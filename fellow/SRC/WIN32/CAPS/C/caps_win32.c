/* @(#) $Id: caps_win32.c,v 1.1.2.2 2004-06-02 12:09:46 carfesh Exp $ */
/*=========================================================================*/
/* Fellow Amiga Emulator                                                   */
/*                                                                         */
/* Win32 C.A.P.S. Support - The Classic Amiga Preservation Society         */
/* http://www.caps-project.org                                             */
/*                                                                         */
/* (w)1994 by Torsten Enderling (carfesh@gmx.net)                          */
/*                                                                         */
/* Copyright (C) 1991, 1992, 1996 Free Software Foundation, Inc.           */
/*                                                                         */
/* This program is free software; you can redistribute it and/or modify    */
/* it under the terms of the GNU General Public License as published by    */
/* the Free Software Foundation; either version 2, or (at your option)     */
/* any later version.                                                      */
/*                                                                         */
/* This program is distributed in the hope that it will be useful,         */
/* but WITHOUT ANY WARRANTY; without even the implied warranty of          */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the           */
/* GNU General Public License for more details.                            */
/*                                                                         */
/* You should have received a copy of the GNU General Public License       */
/* along with this program; if not, write to the Free Software Foundation, */
/* Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.          */
/*=========================================================================*/

#include "caps_win32.h"

#ifdef FELLOW_SUPPORT_CAPS

#include "wgui.h"
#include "fellow.h"
#include "floppy.h"

#include "ComType.h"
#include "CapsAPI.h"

#define CAPS_USER
#include "CapsLib.h"

#define TRACECAPS 1

static int    capsFlags = DI_LOCK_DENVAR|DI_LOCK_DENNOISE|DI_LOCK_NOISE|DI_LOCK_UPDATEFD;
static BOOLE  capsDriveIsLocked [4]= { FALSE, FALSE, FALSE, FALSE };
static SDWORD capsDriveContainer[4]= { -1,    -1,    -1,    -1    };

static HMODULE capshModule = NULL;          /* handle for the library */
static BOOLE capsIsInitialized = FALSE;     /* is the module initialized? */
static BOOLE capsUserIsNotified = FALSE;    /* if the library is missing, did we already notify the user? */

BOOLE capsStartup(void)
{
    int i;

    if(capsIsInitialized) 
        return TRUE;

    capshModule = LoadLibrary("CAPSImg.dll");
    if(!capshModule) 
    {
	    if(capsUserIsNotified)
	        return FALSE;
        else
        {
            wguiRequester("IPF Images need a current C.A.P.S. Plug-In!", "You can download it from:", "http://www.caps-project.org/download.shtml");
            capsUserIsNotified = TRUE;
            fellowAddLog("capsStartup(): Unable to open the CAPS Plug-In.\n");
	        return FALSE;
        }
    }

    if(!GetProcAddress(capshModule, "CAPSLockImageMemory")) 
    {
	    if(capsUserIsNotified)
	        return FALSE;
        else
        {
	        wguiRequester("IPF Images need a current C.A.P.S. Plug-In!", "You can download it from:", "http://www.caps-project.org/download.shtml");
	        capsUserIsNotified = TRUE;
            fellowAddLog("capsStartup(): Unable to open the CAPS Plug-In.\n");
	        return FALSE;
        }
    }

    for(i = 0; i < 4; i++)
	    capsDriveContainer[i] = CAPSAddImage();

    capsIsInitialized = TRUE;
    fellowAddLog("capsStartup(): CAPS IPF Image library loaded successfully.\n");

    return TRUE;
}

BOOLE capsShutdown(void)
{
    if(capshModule)
    {
        FreeLibrary(capshModule);
        capshModule = NULL;
    }
    capsIsInitialized = FALSE;
    fellowAddLog("capsShutdown(): CAPS IPF Image library unloaded.\n");

    return TRUE;
}

BOOLE capsUnloadImage(ULO drive)
{
     if(!capsDriveIsLocked[drive])
         return FALSE;

    CAPSUnlockAllTracks(capsDriveContainer[drive]);
    CAPSUnlockImage(capsDriveContainer[drive]);
    capsDriveIsLocked[drive] = FALSE;
    fellowAddLog("capsUnloadImage(): Image %s unloaded from drive no %u.\n", floppy[drive].imagename, drive);
    return TRUE;
}

BOOLE capsLoadImage(ULO drive, FILE *F, ULO *tracks)
{
    struct CapsImageInfo capsImageInfo;
    ULO ImageSize, ReturnCode;
    UBY *ImageBuffer;
    char DateString[100];
    struct CapsDateTimeExt *capsDateTimeExt;

    /* make sure we're up and running beforehand */
    if(!capsIsInitialized)
        if(!capsStartup()) 
            return FALSE;

    capsUnloadImage(drive);

    fellowAddLog("capsLoadImage(): Attempting to load IPF Image %s into drive %u.\n", floppy[drive].imagename, drive);

    fseek(F, 0, SEEK_END);
    ImageSize = ftell(F);
    fseek(F, 0, SEEK_SET);

    ImageBuffer = (UBY *) malloc(ImageSize);
    if(!ImageBuffer)
        return FALSE;
    
    if(fread(ImageBuffer, ImageSize, 1, F) == 0)
        return FALSE;

    ReturnCode = CAPSLockImageMemory(capsDriveContainer[drive], ImageBuffer, ImageSize, 0);
    free(ImageBuffer);

    if(ReturnCode != imgeOk)
        return FALSE;
    
    capsDriveIsLocked[drive] = TRUE;

    CAPSGetImageInfo(&capsImageInfo, capsDriveContainer[drive]);
    *tracks = (capsImageInfo.maxcylinder - capsImageInfo.mincylinder + 1) * (capsImageInfo.maxhead - capsImageInfo.minhead + 1);

    CAPSLoadImage(capsDriveContainer[drive], capsFlags);

    /* extract the date from information */
    capsDateTimeExt = &capsImageInfo.crdt;
    sprintf(DateString, "%02d.%02d.%04d %02d:%02d:%02d", 
        capsDateTimeExt->day, 
        capsDateTimeExt->month,
        capsDateTimeExt->year,
        capsDateTimeExt->hour,
        capsDateTimeExt->min,
        capsDateTimeExt->sec);
    fellowAddLog("CAPS Image Information: Type:%d Date:%s Release:%04d Revision:%d\n",
	    capsImageInfo.type, 
        DateString, 
        capsImageInfo.release, 
        capsImageInfo.revision);

    fellowAddLog("capsLoadImage(): Image loaded successfully.\n");
    return TRUE;
}

BOOLE capsLoadTrack(ULO drive, ULO track, UBY *mfm_data, ULO *tracklength, ULO *tracktiming, BOOLE *multirevolution)
{
    ULO i, len, type;
    UBY *current_mfm_data;
    struct CapsTrackInfo capsTrackInfo;

    *tracktiming = 0;
    CAPSLockTrack(&capsTrackInfo, capsDriveContainer[drive], track / 2, track & 1, capsFlags);
    current_mfm_data = mfm_data;
    *multirevolution = (capsTrackInfo.type & CTIT_FLAG_FLAKEY) ? TRUE : FALSE;
    type = capsTrackInfo.type & CTIT_MASK_TYPE;
    len = capsTrackInfo.tracksize[0];
    *tracklength = len;
    for (i = 0; i < len; i++)
        *current_mfm_data++ = *(capsTrackInfo.trackdata[0]+ i);
#if TRACECAPS
    {
	    FILE *f = fopen("C:\\CAPSTest.txt","wb");
	    fwrite(capsTrackInfo.trackdata[0], len, 1, f);
	    fclose(f);
    }
#endif
    if (capsTrackInfo.timelen > 0) {
	for (i = 0; i < capsTrackInfo.timelen; i++)
	    tracktiming[i] = (ULO) capsTrackInfo.timebuf[i];
    }
#if TRACECAPS
    fellowAddLog("CAPS Track Information: drive:%u track:%03u flakey:%s trackcnt:%d timelen:%05d type:%d\n",
        drive, 
        track, 
        *multirevolution ? "TRUE " : "FALSE", 
        capsTrackInfo.trackcnt, 
        capsTrackInfo.timelen, 
        type);
#endif
    return TRUE;
}

BOOLE capsLoadRevolution(ULO drive, ULO track, UBY *mfm_data, ULO *tracklength)
{
    static int revolutioncount = 0;
    ULO revolution, len, i;
    UBY *current_mfm_data;
    struct CapsTrackInfo capsTrackInfo;

    revolutioncount++;
    CAPSLockTrack(&capsTrackInfo, capsDriveContainer[drive], track / 2, track & 1, capsFlags);
    revolution = revolutioncount % capsTrackInfo.trackcnt;
    len = capsTrackInfo.tracksize[revolution];
    *tracklength = len;
    current_mfm_data = mfm_data;
    for (i = 0; i < len; i++)
        *current_mfm_data++ = *(capsTrackInfo.trackdata[revolution] + i);
    return TRUE;
}

#endif