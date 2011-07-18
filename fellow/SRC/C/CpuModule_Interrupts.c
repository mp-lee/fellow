/* @(#) $Id: CpuModule_Interrupts.c,v 1.3 2011-07-18 17:22:55 peschau Exp $ */
/*=========================================================================*/
/* Fellow                                                                  */
/* 68000 interrupt handling                                                */
/*                                                                         */
/* Author: Petter Schau                                                    */
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
#include "defs.h"
#include "fellow.h"
#include "fmem.h"

#include "CpuModule.h"
#include "CpuModule_Internal.h"

/* Function for checking pending interrupts */
cpuCheckPendingInterruptsFunc cpu_check_pending_interrupts_func;

void cpuCallCheckPendingInterruptsFunc(void)
{
  if (cpuGetRaiseInterrupt()) return;
  cpuSetRaiseInterrupt(cpu_check_pending_interrupts_func());
}

void cpuCheckPendingInterrupts(void)
{
  cpuCallCheckPendingInterruptsFunc();
}

void cpuSetCheckPendingInterruptsFunc(cpuCheckPendingInterruptsFunc func)
{
  cpu_check_pending_interrupts_func = func;
}

ULO cpuActivateSSP(void)
{
  ULO currentSP = cpuGetAReg(7);

  // check supervisor bit number (bit 13) within the system byte of the status register
  if (!cpuGetFlagSupervisor())
  {
    // we are in user mode, thus save user stack pointer (USP)
    cpuSetUspDirect(currentSP);
    currentSP = cpuGetSspDirect();

    if (cpuGetModelMajor() >= 2)
    {
      if (cpuGetFlagMaster())
      {
        currentSP = cpuGetMspDirect();
      }
    }
    cpuSetAReg(7, currentSP);
  }
  return currentSP;
}

/*============================================================
  Transfers control to an interrupt routine
  ============================================================*/

// Returns TRUE if the cpu was stopped
void cpuSetUpInterrupt(void)
{
  UWO vector_offset = (UWO) (0x60 + cpuGetIrqLevel()*4);
  cpuActivateSSP(); // Switch to using ssp or msp. Loads a7 and preserves usp if we came from user-mode.

  cpuStackFrameGenerate(vector_offset, cpuGetPC()); // This will end up on msp if master is enabled, or on the ssp/isp if not.

  cpuSetSR(cpuGetSR() & 0x38ff);  // Clear interrupt level
  cpuSetSR(cpuGetSR() | 0x2000);  // Set supervisor mode
  cpuSetSR(cpuGetSR() | (UWO)(cpuGetIrqLevel() << 8)); // Set interrupt level

#ifdef ENABLE_INSTRUCTION_LOGGING
  cpuCallInterruptLoggingFunc(cpuGetIrqLevel(), cpuGetIrqAddress());
#endif

  if (cpuGetModelMajor() >= 2 && cpuGetModelMajor() < 6)
  {
    if (cpuGetFlagMaster())
    { // If the cpu was in master mode, preserve msp, and switch to using ssp (isp) in a7.
      ULO oldA7 = cpuGetAReg(7);
      cpuSetMspDirect(oldA7);
      cpuSetAReg(7, cpuGetSspDirect());
      cpuFrame1(vector_offset, cpuGetPC());   // Make the throwaway frame on ssp/isp
      cpuSetSR(cpuGetSR() & 0xefff);  // Clear master bit
    }
  }
  cpuSetPC(cpuGetIrqAddress());
  cpuReadPrefetch();
  cpuSetStop(FALSE);
  cpuSetRaiseInterrupt(FALSE);
}
