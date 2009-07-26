/* @(#) $Id: CpuModule_Instructions.c,v 1.4 2009-07-26 22:56:07 peschau Exp $ */
/*=========================================================================*/
/* Fellow                                                                  */
/* CPU 68k functions                                                       */
/*                                                                         */
/* Author: Petter Schau                                                    */
/*                                                                         */
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

#ifdef UAE_FILESYS
#include "uae2fell.h"
#include "autoconf.h"
#endif

/*============================================================================*/
/* profiling help functions                                                   */
/*============================================================================*/

#ifndef X64
static __inline void cpuTscBefore(LLO* a)
{
  LLO local_a = *a;
  __asm 
  {
      push    eax
      push    edx
      push    ecx
      mov     ecx,10h
      rdtsc
      pop     ecx
      mov     dword ptr [local_a], eax
      mov     dword ptr [local_a + 4], edx
      pop     edx
      pop     eax
  }
  *a = local_a;
}

static __inline void cpuTscAfter(LLO* a, LLO* b, ULO* c)
{
  LLO local_a = *a;
  LLO local_b = *b;
  ULO local_c = *c;

  __asm 
  {
      push    eax
      push    edx
      push    ecx
      mov     ecx, 10h
      rdtsc
      pop     ecx
      sub     eax, dword ptr [local_a]
      sbb     edx, dword ptr [local_a + 4]
      add     dword ptr [local_b], eax
      adc     dword ptr [local_b + 4], edx
      inc     dword ptr [local_c]
      pop     edx
      pop     eax
  }
  *a = local_a;
  *b = local_b;
  *c = local_c;
}
#endif

/* Maintains the integrity of the super/user state */

void cpuUpdateSr(UWO new_sr)
{
  BOOLE supermode_was_set = cpuGetFlagSupervisor();
  BOOLE master_was_set = (cpuGetModelMajor() >= 2) && cpuGetFlagMaster();

  BOOLE supermode_is_set = !!(new_sr & 0x2000);
  BOOLE master_is_set = (cpuGetModelMajor() >= 2) && !!(new_sr & 0x1000);

  ULO runlevel_old = (cpuGetSR() >> 8) & 7;
  ULO runlevel_new = (new_sr >> 8) & 7;

  if (!supermode_was_set) cpuSetUspDirect(cpuGetAReg(7));
  else if (master_was_set) cpuSetMspDirect(cpuGetAReg(7));
  else cpuSetSspDirect(cpuGetAReg(7));

  if (!supermode_is_set) cpuSetAReg(7, cpuGetUspDirect());
  else if (master_is_set) cpuSetAReg(7, cpuGetMspDirect());
  else cpuSetAReg(7, cpuGetSspDirect());

  cpuSetSR(new_sr);

  if (runlevel_old != runlevel_new)
  {
    cpuCallCheckPendingInterruptsFunc();
  }
}

static void cpuIllegal(void)
{
  UWO opcode = memoryReadWord(cpuGetPC() - 2);
  if ((opcode & 0xf000) == 0xf000)
  {
    cpuThrowFLineException();
  }
  else if ((opcode & 0xa000) == 0xa000)
  {
#ifdef UAE_FILESYS
    if ((cpuGetPC() & 0xff0000) == 0xf00000)
    {
      call_calltrap(opcode & 0xfff);
      cpuReadPrefetch();
      cpuSetInstructionTime(512);
    }
    else
#endif
    {
      cpuThrowALineException();
    }
  }
  else
  {
    cpuThrowIllegalInstructionException(FALSE);
  }
}

/// <summary>
/// Illegal instruction handler.
/// </summary>
static void cpuIllegalInstruction(ULO *opcode_data)
{
  cpuIllegal(); 
}

/// <summary>
/// BKPT
/// </summary>
static void cpuBkpt(ULO vector)
{
  cpuIllegal();
}

/// <summary>
/// Adds bytes src1 to src2. Sets all flags.
/// </summary>
/// <returns>The result</returns>
static UBY cpuAddB(UBY src2, UBY src1)
{
  UBY res = src2 + src1;
  cpuSetFlagsAdd(cpuIsZeroB(res), cpuMsbB(res), cpuMsbB(src2), cpuMsbB(src1));
  return res;
}

/// <summary>
/// Adds words src1 to src2. Sets all flags.
/// </summary>
/// <returns>The result</returns>
static UWO cpuAddW(UWO src2, UWO src1)
{
  UWO res = src2 + src1;
  cpuSetFlagsAdd(cpuIsZeroW(res), cpuMsbW(res), cpuMsbW(src2), cpuMsbW(src1));
  return res;
}

/// <summary>
/// Adds dwords src1 to src2. Sets all flags.
/// </summary>
/// <returns>The result</returns>
static ULO cpuAddL(ULO src2, ULO src1)
{
  ULO res = src2 + src1;
  cpuSetFlagsAdd(cpuIsZeroL(res), cpuMsbL(res), cpuMsbL(src2), cpuMsbL(src1));
  return res;
}

/// <summary>
/// Adds src1 to src2 (For address registers). No flags.
/// </summary>
/// <returns>The result</returns>
static ULO cpuAddaW(ULO src2, ULO src1)
{
  return src2 + src1;
}

/// <summary>
/// Adds src1 to src2 (For address registers). No flags.
/// </summary>
/// <returns>The result</returns>
static ULO cpuAddaL(ULO src2, ULO src1)
{
  return src2 + src1;
}

/// <summary>
/// Subtracts src1 from src2. Sets all flags.
/// </summary>
/// <returns>The result</returns>
static UBY cpuSubB(UBY src2, UBY src1)
{
  UBY res = src2 - src1;
  cpuSetFlagsSub(cpuIsZeroB(res), cpuMsbB(res), cpuMsbB(src2), cpuMsbB(src1));
  return res;
}

/// <summary>
/// Subtracts src1 from src2. Sets all flags.
/// </summary>
/// <returns>The result</returns>
static UWO cpuSubW(UWO src2, UWO src1)
{
  UWO res = src2 - src1;
  cpuSetFlagsSub(cpuIsZeroW(res), cpuMsbW(res), cpuMsbW(src2), cpuMsbW(src1));
  return res;
}

/// <summary>
/// Subtracts src1 from src2. Sets all flags.
/// </summary>
/// <returns>The result</returns>
static ULO cpuSubL(ULO src2, ULO src1)
{
  ULO res = src2 - src1;
  cpuSetFlagsSub(cpuIsZeroL(res), cpuMsbL(res), cpuMsbL(src2), cpuMsbL(src1));
  return res;
}

/// <summary>
/// Subtracts src1 from src2 (For address registers). No flags.
/// </summary>
/// <returns>The result</returns>
static ULO cpuSubaW(ULO src2, ULO src1)
{
  return src2 - src1;
}

/// <summary>
/// Subtracts src1 from src2 (For address registers). No flags.
/// </summary>
/// <returns>The result</returns>
static ULO cpuSubaL(ULO src2, ULO src1)
{
  return src2 - src1;
}

/// <summary>
/// Subtracts src1 from src2. Sets all flags.
/// </summary>
static void cpuCmpB(UBY src2, UBY src1)
{
  UBY res = src2 - src1;
  cpuSetFlagsCmp(cpuIsZeroB(res), cpuMsbB(res), cpuMsbB(src2), cpuMsbB(src1));
}

/// <summary>
/// Subtracts src1 from src2. Sets all flags.
/// </summary>
static void cpuCmpW(UWO src2, UWO src1)
{
  UWO res = src2 - src1;
  cpuSetFlagsCmp(cpuIsZeroW(res), cpuMsbW(res), cpuMsbW(src2), cpuMsbW(src1));
}

/// <summary>
/// Subtracts src1 from src2. Sets all flags.
/// </summary>
static void cpuCmpL(ULO src2, ULO src1)
{
  ULO res = src2 - src1;
  cpuSetFlagsCmp(cpuIsZeroL(res), cpuMsbL(res), cpuMsbL(src2), cpuMsbL(src1));
}

/// <summary>
/// Ands src1 to src2. Sets NZ00 flags.
/// </summary>
/// <returns>The result</returns>
static UBY cpuAndB(UBY src2, UBY src1)
{
  UBY res = src2 & src1;
  cpuSetFlagsNZ00(cpuIsZeroB(res), cpuMsbB(res));
  return res;
}

/// <summary>
/// Ands src1 to src2. Sets NZ00 flags.
/// </summary>
/// <returns>The result</returns>
static UWO cpuAndW(UWO src2, UWO src1)
{
  UWO res = src2 & src1;
  cpuSetFlagsNZ00(cpuIsZeroW(res), cpuMsbW(res));
  return res;
}

/// <summary>
/// Ands src1 to src2. Sets NZ00 flags.
/// </summary>
/// <returns>The result</returns>
static ULO cpuAndL(ULO src2, ULO src1)
{
  ULO res = src2 & src1;
  cpuSetFlagsNZ00(cpuIsZeroL(res), cpuMsbL(res));
  return res;
}

/// <summary>
/// Eors src1 to src2. Sets NZ00 flags.
/// </summary>
/// <returns>The result</returns>
static UBY cpuEorB(UBY src2, UBY src1)
{
  UBY res = src2 ^ src1;
  cpuSetFlagsNZ00(cpuIsZeroB(res), cpuMsbB(res));
  return res;
}

/// <summary>
/// Eors src1 to src2. Sets NZ00 flags.
/// </summary>
/// <returns>The result</returns>
static UWO cpuEorW(UWO src2, UWO src1)
{
  UWO res = src2 ^ src1;
  cpuSetFlagsNZ00(cpuIsZeroW(res), cpuMsbW(res));
  return res;
}

/// <summary>
/// Eors src1 to src2. Sets NZ00 flags.
/// </summary>
/// <returns>The result</returns>
static ULO cpuEorL(ULO src2, ULO src1)
{
  ULO res = src2 ^ src1;
  cpuSetFlagsNZ00(cpuIsZeroL(res), cpuMsbL(res));
  return res;
}

/// <summary>
/// Ors src1 to src2. Sets NZ00 flags.
/// </summary>
/// <returns>The result</returns>
static UBY cpuOrB(UBY src2, UBY src1)
{
  UBY res = src2 | src1;
  cpuSetFlagsNZ00(cpuIsZeroB(res), cpuMsbB(res));
  return res;
}

/// <summary>
/// Ors src1 to src2. Sets NZ00 flags.
/// </summary>
/// <returns>The result</returns>
static UWO cpuOrW(UWO src2, UWO src1)
{
  UWO res = src2 | src1;
  cpuSetFlagsNZ00(cpuIsZeroW(res), cpuMsbW(res));
  return res;
}

/// <summary>
/// Ors src1 to src2. Sets NZ00 flags.
/// </summary>
/// <returns>The result</returns>
static ULO cpuOrL(ULO src2, ULO src1)
{
  ULO res = src2 | src1;
  cpuSetFlagsNZ00(cpuIsZeroL(res), cpuMsbL(res));
  return res;
}

/// <summary>
/// Changes bit in src. Sets Z flag.
/// </summary>
/// <returns>The result</returns>
static UBY cpuBchgB(UBY src, UBY bit)
{
  UBY bit_mask = 1 << (bit & 7);
  cpuSetFlagZ(!(src & bit_mask));
  return src ^ bit_mask;
}

/// <summary>
/// Changes bit in src. Sets Z flag.
/// </summary>
/// <returns>The result</returns>
static ULO cpuBchgL(ULO src, ULO bit)
{
  ULO bit_mask = 1 << (bit & 31);
  cpuSetFlagZ(!(src & bit_mask));
  return src ^ bit_mask;
}

/// <summary>
/// Clears bit in src. Sets Z flag.
/// </summary>
/// <returns>The result</returns>
static UBY cpuBclrB(UBY src, UBY bit)
{
  UBY bit_mask = 1 << (bit & 7);
  cpuSetFlagZ(!(src & bit_mask));
  return src & ~bit_mask;
}

/// <summary>
/// Clears bit in src. Sets Z flag.
/// </summary>
/// <returns>The result</returns>
static ULO cpuBclrL(ULO src, ULO bit)
{
  ULO bit_mask = 1 << (bit & 31);
  cpuSetFlagZ(!(src & bit_mask));
  return src & ~bit_mask;
}

/// <summary>
/// Sets bit in src. Sets Z flag.
/// </summary>
/// <returns>The result</returns>
static UBY cpuBsetB(UBY src, UBY bit)
{
  UBY bit_mask = 1 << (bit & 7);
  cpuSetFlagZ(!(src & bit_mask));
  return src | bit_mask;
}

/// <summary>
/// Sets bit in src. Sets Z flag.
/// </summary>
/// <returns>The result</returns>
static ULO cpuBsetL(ULO src, ULO bit)
{
  ULO bit_mask = 1 << (bit & 31);
  cpuSetFlagZ(!(src & bit_mask));
  return src | bit_mask;
}

/// <summary>
/// Tests bit in src. Sets Z flag.
/// </summary>
static void cpuBtstB(UBY src, UBY bit)
{
  UBY bit_mask = 1 << (bit & 7);
  cpuSetFlagZ(!(src & bit_mask));
}

/// <summary>
/// Tests bit in src. Sets Z flag.
/// </summary>
static void cpuBtstL(ULO src, ULO bit)
{
  ULO bit_mask = 1 << (bit & 31);
  cpuSetFlagZ(!(src & bit_mask));
}

/// <summary>
/// Set flags for clr operation.  X0100.
/// </summary>
static void cpuClr()
{
  cpuSetFlags0100();
}

/// <summary>
/// Neg src1. Sets sub flags.
/// </summary>
/// <returns>The result</returns>
static UBY cpuNegB(UBY src1)
{
  UBY res = (UBY)-(BYT)src1;
  cpuSetFlagsNeg(cpuIsZeroB(res), cpuMsbB(res), cpuMsbB(src1));
  return res;
}

/// <summary>
/// Neg src1. Sets sub flags.
/// </summary>
/// <returns>The result</returns>
static UWO cpuNegW(UWO src1)
{
  UWO res = (UWO)-(WOR)src1;
  cpuSetFlagsNeg(cpuIsZeroW(res), cpuMsbW(res), cpuMsbW(src1));
  return res;
}

/// <summary>
/// Neg src1. Sets sub flags.
/// </summary>
/// <returns>The result</returns>
static ULO cpuNegL(ULO src1)
{
  ULO res = (ULO)-(LON)src1;
  cpuSetFlagsNeg(cpuIsZeroL(res), cpuMsbL(res), cpuMsbL(src1));
  return res;
}

/// <summary>
/// Negx src1.
/// </summary>
/// <returns>The result</returns>
static UBY cpuNegxB(UBY src1)
{
  BYT x = (cpuGetFlagX()) ? 1 : 0;
  UBY res = (UBY)-(BYT)src1 - x;
  cpuSetFlagsNegx(cpuIsZeroB(res), cpuMsbB(res), cpuMsbB(src1));
  return res;
}

/// <summary>
/// Negx src1.
/// </summary>
/// <returns>The result</returns>
static UWO cpuNegxW(UWO src1)
{
  WOR x = (cpuGetFlagX()) ? 1 : 0;
  UWO res = (UWO)-(WOR)src1 - x;
  cpuSetFlagsNegx(cpuIsZeroW(res), cpuMsbW(res), cpuMsbW(src1));
  return res;
}

/// <summary>
/// Negx src1.
/// </summary>
/// <returns>The result</returns>
static ULO cpuNegxL(ULO src1)
{
  LON x = (cpuGetFlagX()) ? 1 : 0;
  ULO res = (ULO)-(LON)src1 - x;
  cpuSetFlagsNegx(cpuIsZeroL(res), cpuMsbL(res), cpuMsbL(src1));
  return res;
}

/// <summary>
/// Not src1. 
/// </summary>
/// <returns>The result</returns>
static UBY cpuNotB(UBY src1)
{
  UBY res = ~src1;
  cpuSetFlagsNZ00(cpuIsZeroB(res), cpuMsbB(res));
  return res;
}

/// <summary>
/// Not src1. 
/// </summary>
/// <returns>The result</returns>
static UWO cpuNotW(UWO src1)
{
  UWO res = ~src1;
  cpuSetFlagsNZ00(cpuIsZeroW(res), cpuMsbW(res));
  return res;
}

/// <summary>
/// Not src1. 
/// </summary>
/// <returns>The result</returns>
static ULO cpuNotL(ULO src1)
{
  ULO res = ~src1;
  cpuSetFlagsNZ00(cpuIsZeroL(res), cpuMsbL(res));
  return res;
}

/// <summary>
/// Tas src1. 
/// </summary>
/// <returns>The result</returns>
static UBY cpuTas(UBY src1)
{
  cpuSetFlagsNZ00(cpuIsZeroB(src1), cpuMsbB(src1));
  return src1 | 0x80;
}

/// <summary>
/// Tst res. 
/// </summary>
static void cpuTestB(UBY res)
{
  cpuSetFlagsNZ00(cpuIsZeroB(res), cpuMsbB(res));
}

/// <summary>
/// Tst res. 
/// </summary>
static void cpuTestW(UWO res)
{
  cpuSetFlagsNZ00(cpuIsZeroW(res), cpuMsbW(res));
}

/// <summary>
/// Tst res. 
/// </summary>
static void cpuTestL(ULO res)
{
  cpuSetFlagsNZ00(cpuIsZeroL(res), cpuMsbL(res));
}

/// <summary>
/// PEA ea. 
/// </summary>
static void cpuPeaL(ULO ea)
{
  cpuSetAReg(7, cpuGetAReg(7) - 4);
  memoryWriteLong(ea, cpuGetAReg(7));
}

/// <summary>
/// JMP ea. 
/// </summary>
static void cpuJmp(ULO ea)
{
  cpuSetPC(ea);
  cpuReadPrefetch();
}

/// <summary>
/// JSR ea. 
/// </summary>
static void cpuJsr(ULO ea)
{
  cpuSetAReg(7, cpuGetAReg(7) - 4);
  memoryWriteLong(cpuGetPC(), cpuGetAReg(7));
  cpuSetPC(ea);
  cpuReadPrefetch();
}

/// <summary>
/// Move res
/// </summary>
/// <returns>The result</returns>
static void cpuMoveB(UBY res)
{
  cpuSetFlagsNZ00(cpuIsZeroB(res), cpuMsbB(res));
}

/// <summary>
/// Move res
/// </summary>
/// <returns>The result</returns>
static void cpuMoveW(UWO res)
{
  cpuSetFlagsNZ00(cpuIsZeroW(res), cpuMsbW(res));
}

/// <summary>
/// Move res 
/// </summary>
/// <returns>The result</returns>
static void cpuMoveL(ULO res)
{
  cpuSetFlagsNZ00(cpuIsZeroL(res), cpuMsbL(res));
}

/// <summary>
/// Bra byte offset. 
/// </summary>
static void cpuBraB(ULO offset)
{
  cpuSetPC(cpuGetPC() + offset);
  cpuReadPrefetch();
  cpuSetInstructionTime(10);
}

/// <summary>
/// Bra word offset. 
/// </summary>
static void cpuBraW()
{
  ULO tmp_pc = cpuGetPC();
  cpuSetPC(tmp_pc + cpuGetNextOpcode16SignExt());
  cpuReadPrefetch();
  cpuSetInstructionTime(10);
}

/// <summary>
/// Bra long offset. 
/// </summary>
static void cpuBraL()
{
  if (cpuGetModelMajor() < 2) cpuBraB(0xffffffff);
  else
  {
    ULO tmp_pc = cpuGetPC();
    cpuSetPC(tmp_pc + cpuGetNextOpcode32());
    cpuReadPrefetch();
    cpuSetInstructionTime(4);
  }
}

/// <summary>
/// Bsr byte offset. 
/// </summary>
static void cpuBsrB(ULO offset)
{
  cpuSetAReg(7, cpuGetAReg(7) - 4);
  memoryWriteLong(cpuGetPC(), cpuGetAReg(7));
  cpuSetPC(cpuGetPC() + offset);
  cpuReadPrefetch();
  cpuSetInstructionTime(18);
}

/// <summary>
/// Bsr word offset. 
/// </summary>
static void cpuBsrW()
{
  ULO tmp_pc = cpuGetPC();
  ULO offset = cpuGetNextOpcode16SignExt();
  cpuSetAReg(7, cpuGetAReg(7) - 4);
  memoryWriteLong(cpuGetPC(), cpuGetAReg(7));
  cpuSetPC(tmp_pc + offset);
  cpuReadPrefetch();
  cpuSetInstructionTime(18);
}

/// <summary>
/// Bsr long offset. (020+) 
/// </summary>
static void cpuBsrL()
{
  if (cpuGetModelMajor() < 2) cpuBsrB(0xffffffff);
  else
  {
    ULO tmp_pc = cpuGetPC();
    ULO offset = cpuGetNextOpcode32();
    cpuSetAReg(7, cpuGetAReg(7) - 4);
    memoryWriteLong(cpuGetPC(), cpuGetAReg(7));
    cpuSetPC(tmp_pc + offset);
    cpuReadPrefetch();
  }
}

/// <summary>
/// Bcc byte offset. 
/// </summary>
static void cpuBccB(BOOLE cc, ULO offset)
{
  if (cc)
  {
    cpuSetPC(cpuGetPC() + offset);
    cpuReadPrefetch();
    cpuSetInstructionTime(10);
  }
  else
  {
    cpuSetInstructionTime(8);
  }
}

/// <summary>
/// Bcc word offset. 
/// </summary>
static void cpuBccW(BOOLE cc)
{
  if (cc)
  {
    ULO tmp_pc = cpuGetPC();
    cpuSetPC(tmp_pc + cpuGetNextOpcode16SignExt());
    cpuSetInstructionTime(10);
  }
  else
  {
    cpuSetPC(cpuGetPC() + 2);
    cpuSetInstructionTime(12);
  }
  cpuReadPrefetch();
}

/// <summary>
/// Bcc long offset. (020+)
/// </summary>
static void cpuBccL(BOOLE cc)
{
  if (cpuGetModelMajor() < 2) cpuBccB(cc, 0xffffffff);
  else
  {
    if (cc)
    {
      ULO tmp_pc = cpuGetPC();
      cpuSetPC(tmp_pc + cpuGetNextOpcode32());
    }
    else
    {
      cpuSetPC(cpuGetPC() + 4);
    }
    cpuReadPrefetch();
    cpuSetInstructionTime(4);
  }
}

/// <summary>
/// DBcc word offset. 
/// </summary>
static void cpuDbcc(BOOLE cc, ULO reg)
{
  if (!cc)
  {
    WOR val = (WOR)cpuGetDRegWord(reg);
    val--;
    cpuSetDRegWord(reg, val);
    if (val != -1)
    {
      ULO tmp_pc = cpuGetPC();
      cpuSetPC(tmp_pc + cpuGetNextOpcode16SignExt());
      cpuSetInstructionTime(10);
    }
    else
    {
      cpuSetPC(cpuGetPC() + 2);
      cpuSetInstructionTime(14);
    }
  }
  else
  {
    cpuSetPC(cpuGetPC() + 2);
    cpuSetInstructionTime(12);
  }
  cpuReadPrefetch();
}

/// <summary>
/// And #imm, ccr 
/// </summary>
static void cpuAndCcrB()
{
  UWO imm = cpuGetNextOpcode16();
  cpuSetSR(cpuGetSR() & (0xffe0 | (imm & 0x1f)));
}

/// <summary>
/// And #imm, sr 
/// </summary>
static void cpuAndSrW()
{
  if (cpuGetFlagSupervisor())
  {
    UWO imm = cpuGetNextOpcode16();
    cpuUpdateSr(cpuGetSR() & imm);
  }
  else
  {
    cpuThrowPrivilegeViolationException();
  }
}

/// <summary>
/// Or #imm, ccr 
/// </summary>
static void cpuOrCcrB()
{
  UWO imm = cpuGetNextOpcode16();
  cpuSetSR(cpuGetSR() | (imm & 0x1f));
}

/// <summary>
/// Or #imm, sr 
/// </summary>
static void cpuOrSrW()
{
  if (cpuGetFlagSupervisor())
  {
    UWO imm = cpuGetNextOpcode16();
    cpuUpdateSr(cpuGetSR() | imm);
  }
  else
  {
    cpuThrowPrivilegeViolationException();
  }
}

/// <summary>
/// Eor #imm, ccr 
/// </summary>
static void cpuEorCcrB()
{
  UWO imm = cpuGetNextOpcode16();
  cpuSetSR(cpuGetSR() ^ (imm & 0x1f));
}

/// <summary>
/// Eor #imm, sr 
/// </summary>
static void cpuEorSrW()
{
  if (cpuGetFlagSupervisor())
  {
    UWO imm = cpuGetNextOpcode16();
    cpuUpdateSr(cpuGetSR() ^ imm);
  }
  else
  {
    cpuThrowPrivilegeViolationException();
  }
}

/// <summary>
/// Move ea, ccr 
/// </summary>
static void cpuMoveToCcr(UWO src)
{
  cpuSetSR((cpuGetSR() & 0xff00) | (src & 0x1f));
}

/// <summary>
/// Move <ea>, sr 
/// </summary>
static void cpuMoveToSr(UWO src)
{
  if (cpuGetFlagSupervisor())
  {
    cpuUpdateSr(src);
  }
  else
  {
    cpuThrowPrivilegeViolationException();
  }
}

/// <summary>
/// Move ccr, ea 
/// </summary>
static UWO cpuMoveFromCcr()
{
  return cpuGetSR() & 0x1f;
}

/// <summary>
/// Move <ea>, sr 
/// </summary>
static UWO cpuMoveFromSr()
{
  if (cpuGetModelMajor() == 0 || (cpuGetModelMajor() > 0 && cpuGetFlagSupervisor()))
  {
    return cpuGetSR();
  }
  else
  {
    cpuThrowPrivilegeViolationException();
  } 
  return 0;
}

/// <summary>
/// Scc byte. 
/// </summary>
static UBY cpuScc(ULO cc)
{
  return (cpuCalculateConditionCode(cc)) ? 0xff : 0;
}

/// <summary>
/// Rts 
/// </summary>
static void cpuRts()
{
  cpuSetPC(memoryReadLong(cpuGetAReg(7)));
  cpuSetAReg(7, cpuGetAReg(7) + 4);
  cpuReadPrefetch();
  cpuSetInstructionTime(16);
}

/// <summary>
/// Rtr 
/// </summary>
static void cpuRtr()
{
  cpuSetSR((cpuGetSR() & 0xffe0) | (memoryReadWord(cpuGetAReg(7)) & 0x1f));
  cpuSetAReg(7, cpuGetAReg(7) + 2);
  cpuSetPC(memoryReadLong(cpuGetAReg(7)));
  cpuSetAReg(7, cpuGetAReg(7) + 4);
  cpuReadPrefetch();
  cpuSetInstructionTime(20);
}

/// <summary>
/// Nop 
/// </summary>
static void cpuNop()
{
  cpuSetInstructionTime(4);
}

/// <summary>
/// Trapv
/// </summary>
static void cpuTrapv()
{
  if (cpuGetFlagV())
  {
    cpuThrowTrapVException();
    return;
  }
  cpuSetInstructionTime(4);
}

/// <summary>
/// Muls/u.l
/// </summary>
static void cpuMulL(ULO src1, UWO extension)
{
  ULO dl = (extension >> 12) & 7;
  if (extension & 0x0800) // muls.l
  {
    LLO result = ((LLO)(LON) src1) * ((LLO)(LON)cpuGetDReg(dl));
    if (extension & 0x0400) // 32bx32b=64b
    {  
      ULO dh = extension & 7;
      cpuSetDReg(dh, (ULO)(result >> 32));
      cpuSetDReg(dl, (ULO)result);
      cpuSetFlagsNZ00(result == 0, result < 0);
    }
    else // 32bx32b=32b
    {
      BOOLE o;
      if (result >= 0)
	o = (result & 0xffffffff00000000) != 0;
      else
	o = (result & 0xffffffff00000000) != 0xffffffff00000000;
      cpuSetDReg(dl, (ULO)result);
      cpuSetFlagsNZVC(result == 0, result < 0, o, FALSE);
    }
  }
  else // mulu.l
  {
    ULL result = ((ULL) src1) * ((ULL) cpuGetDReg(dl));
    if (extension & 0x0400) // 32bx32b=64b
    {  
      ULO dh = extension & 7;
      cpuSetDReg(dh, (ULO)(result >> 32));
      cpuSetDReg(dl, (ULO)result);
      cpuSetFlagsNZ00(result == 0, !!(result & 0x8000000000000000));
    }
    else // 32bx32b=32b
    {
      cpuSetDReg(dl, (ULO)result);
      cpuSetFlagsNZVC(result == 0, !!(result & 0x8000000000000000), (result >> 32) != 0, FALSE);
    }
  }
  cpuSetInstructionTime(4);
}

/// <summary>
/// Muls.w
/// </summary>
static ULO cpuMulsW(UWO src2, UWO src1)
{
  ULO res = (ULO)(((LON)(WOR)src2)*((LON)(WOR)src1));
  cpuSetFlagsNZ00(cpuIsZeroL(res), cpuMsbL(res));
  return res;
}

/// <summary>
/// Mulu.w
/// </summary>
static ULO cpuMuluW(UWO src2, UWO src1)
{
  ULO res = ((ULO)src2)*((ULO)src1);
  cpuSetFlagsNZ00(cpuIsZeroL(res), cpuMsbL(res));
  return res;
}

/// <summary>
/// Divsw, src1 / src2
/// </summary>
static ULO cpuDivsW(ULO dst, UWO src1)
{
  ULO result;
  if (src1 == 0)
  {
    // Alcatraz odyssey assumes that PC in this exception points after the instruction.
    cpuThrowDivisionByZeroException(TRUE);
    result = dst;
  }
  else
  {
    LON x = (LON) dst;
    LON y = (LON)(WOR) src1;
    LON res = x / y;
    LON rem = x % y;
    if (res > 32767 || res < -32768)
    {
      result = dst;
      cpuSetFlagsVC(TRUE, FALSE);
    }
    else
    {
      result = (rem << 16) | (res & 0xffff);
      cpuSetFlagsNZVC(cpuIsZeroW((UWO) res), cpuMsbW((UWO) res), FALSE, FALSE);
    }
  }
  return result;
}

/// <summary>
/// Divuw, src1 / src2
/// </summary>
static ULO cpuDivuW(ULO dst, UWO src1)
{
  ULO result;
  if (src1 == 0)
  {
    // Alcatraz odyssey assumes that PC in this exception points after the instruction.
    cpuThrowDivisionByZeroException(TRUE);
    result = dst;
  }
  else
  {
    ULO x = dst;
    ULO y = (ULO) src1;
    ULO res = x / y;
    ULO rem = x % y;
    if (res > 65535)
    {
      result = dst;
      cpuSetFlagsVC(TRUE, FALSE);
    }
    else
    {
      result = (rem << 16) | (res & 0xffff);
      cpuSetFlagsNZVC(cpuIsZeroW((UWO) res), cpuMsbW((UWO) res), FALSE, FALSE);
    }
  }
  return result;
}

static void cpuDivL(ULO divisor, ULO ext)
{
  if (divisor != 0)
  {
    ULO dq_reg = (ext>>12) & 7; /* Get operand registers, size and sign */
    ULO dr_reg = ext & 7;
    BOOLE size64 = (ext>>10) & 1;
    BOOLE sign = (ext>>11) & 1;
    BOOLE resultsigned = FALSE, restsigned = FALSE;
    ULL result, rest;
    ULL x, y;
    LLO x_signed, y_signed; 

    if (sign)
    { 
      if (size64) x_signed = ((LLO) (LON) cpuGetDReg(dq_reg)) | (((LLO) cpuGetDReg(dr_reg))<<32);
      else x_signed = (LLO) (LON) cpuGetDReg(dq_reg);
      y_signed = (LLO) (LON) divisor;

      if (y_signed < 0)
      {
	y = (ULL) -y_signed;
	resultsigned = !resultsigned;
      }
      else y = y_signed;
      if (x_signed < 0)
      {
	x = (ULL) -x_signed;
	resultsigned = !resultsigned;
	restsigned = TRUE;
      }
      else x = (ULL) x_signed;
    }
    else
    {
      if (size64) x = ((ULL) cpuGetDReg(dq_reg)) | (((ULL) cpuGetDReg(dr_reg))<<32);
      else x = (ULL) cpuGetDReg(dq_reg);
      y = (ULL) divisor;
    }

    result = x / y;
    rest = x % y;

    if (sign)
    {
      if ((resultsigned && result > 0x80000000) || (!resultsigned && result > 0x7fffffff))
      {
	/* Overflow */
	cpuSetFlagsVC(TRUE, FALSE);
      }
      else
      {
	LLO result_signed = (resultsigned) ? (-(LLO)result) : ((LLO)result);
	LLO rest_signed = (restsigned) ? (-(LLO)rest) : ((LLO)rest);
	cpuSetDReg(dr_reg, (ULO) rest_signed);
	cpuSetDReg(dq_reg, (ULO) result_signed);
	cpuSetFlagsNZ00(cpuIsZeroL(cpuGetDReg(dq_reg)), cpuMsbL(cpuGetDReg(dq_reg)));
      }
    }
    else
    {
      if (result > 0xffffffff)
      {
	/* Overflow */
	cpuSetFlagsVC(TRUE, FALSE);
      }
      else
      {
	cpuSetDReg(dr_reg, (ULO) rest);
	cpuSetDReg(dq_reg, (ULO) result);
	cpuSetFlagsNZ00(cpuIsZeroL(cpuGetDReg(dq_reg)), cpuMsbL(cpuGetDReg(dq_reg)));
      }
    }
  }
  else
  {
    cpuThrowDivisionByZeroException(FALSE);
  }
}

/// <summary>
/// Lslb
/// </summary>
static UBY cpuLslB(UBY dst, ULO sh, ULO cycles)
{
  UBY res;
  sh &= 0x3f;
  if (sh == 0)
  {
    cpuSetFlagsShiftZero(cpuIsZeroB(dst), cpuMsbB(dst));
    res = dst;
  }
  else if (sh >= 8)
  {
    res = 0;
    cpuSetFlagsShift(TRUE, FALSE, (sh == 8) ? (dst & 1) : FALSE, FALSE);
  }
  else
  {
    res = dst << sh;
    cpuSetFlagsShift(cpuIsZeroB(res), cpuMsbB(res), dst & (0x80>>(sh-1)), FALSE);
  }
  cpuSetInstructionTime(cycles + sh*2);
  return res;
}

/// <summary>
/// Lslw
/// </summary>
static UWO cpuLslW(UWO dst, ULO sh, ULO cycles)
{
  UWO res;
  sh &= 0x3f;
  if (sh == 0)
  {
    cpuSetFlagsShiftZero(cpuIsZeroW(dst), cpuMsbW(dst));
    res = dst;
  }
  else if (sh >= 16)
  {
    res = 0;
    cpuSetFlagsShift(TRUE, FALSE, (sh == 16) ? (dst & 1) : FALSE, FALSE);
  }
  else
  {
    res = dst << sh;
    cpuSetFlagsShift(cpuIsZeroW(res), cpuMsbW(res), dst & (0x8000>>(sh-1)), FALSE);
  }
  cpuSetInstructionTime(cycles + sh*2);
  return res;
}

/// <summary>
/// Lsll
/// </summary>
static ULO cpuLslL(ULO dst, ULO sh, ULO cycles)
{
  ULO res;
  sh &= 0x3f;
  if (sh == 0)
  {
    cpuSetFlagsShiftZero(cpuIsZeroL(dst), cpuMsbL(dst));
    res = dst;
  }
  else if (sh >= 32)
  {
    res = 0;
    cpuSetFlagsShift(TRUE, FALSE, (sh == 32) ? (dst & 1) : FALSE, FALSE);
  }
  else
  {
    res = dst << sh;
    cpuSetFlagsShift(cpuIsZeroL(res), cpuMsbL(res), dst & (0x80000000>>(sh-1)), FALSE);
  }
  cpuSetInstructionTime(cycles + sh*2);
  return res;
}

/// <summary>
/// Lsrb
/// </summary>
static UBY cpuLsrB(UBY dst, ULO sh, ULO cycles)
{
  UBY res;
  sh &= 0x3f;
  if (sh == 0)
  {
    cpuSetFlagsShiftZero(cpuIsZeroB(dst), cpuMsbB(dst));
    res = dst;
  }
  else if (sh >= 8)
  {
    res = 0;
    cpuSetFlagsShift(TRUE, FALSE, (sh == 8) ? cpuMsbB(dst) : FALSE, FALSE);
  }
  else
  {
    res = dst >> sh;
    cpuSetFlagsShift(cpuIsZeroB(res), FALSE, dst & (1<<(sh-1)), FALSE);
  }
  cpuSetInstructionTime(cycles + sh*2);
  return res;
}

/// <summary>
/// Lsrw
/// </summary>
static UWO cpuLsrW(UWO dst, ULO sh, ULO cycles)
{
  UWO res;
  sh &= 0x3f;
  if (sh == 0)
  {
    cpuSetFlagsShiftZero(cpuIsZeroW(dst), cpuMsbW(dst));
    res = dst;
  }
  else if (sh >= 16)
  {
    res = 0;
    cpuSetFlagsShift(TRUE, FALSE, (sh == 16) ? cpuMsbW(dst) : FALSE, FALSE);
  }
  else
  {
    res = dst >> sh;
    cpuSetFlagsShift(cpuIsZeroW(res), FALSE, dst & (1<<(sh-1)), FALSE);
  }
  cpuSetInstructionTime(cycles + sh*2);
  return res;
}

/// <summary>
/// Lsrl
/// </summary>
static ULO cpuLsrL(ULO dst, ULO sh, ULO cycles)
{
  ULO res;
  sh &= 0x3f;
  if (sh == 0)
  {
    cpuSetFlagsShiftZero(cpuIsZeroL(dst), cpuMsbL(dst));
    res = dst;
  }
  else if (sh >= 32)
  {
    res = 0;
    cpuSetFlagsShift(TRUE, FALSE, (sh == 32) ? cpuMsbL(dst) : FALSE, FALSE);
  }
  else
  {
    res = dst >> sh;
    cpuSetFlagsShift(cpuIsZeroL(res), FALSE, dst & (1<<(sh-1)), FALSE);
  }
  cpuSetInstructionTime(cycles + sh*2);
  return res;
}

/// <summary>
/// Aslb
/// </summary>
static UBY cpuAslB(UBY dst, ULO sh, ULO cycles)
{
  BYT res;
  sh &= 0x3f;
  if (sh == 0)
  {
    cpuSetFlagsShiftZero(cpuIsZeroB(dst), cpuMsbB(dst));
    res = (BYT) dst;
  }
  else if (sh >= 8)
  {
    res = 0;
    cpuSetFlagsShift(TRUE, FALSE, (sh == 8) ? (dst & 1) : FALSE, dst != 0);
  }
  else
  {
    UBY mask = 0xff << (7-sh);
    UBY out = dst & mask;
    BOOLE n;
    res = ((BYT)dst) << sh;
    n = cpuMsbB(res);
    cpuSetFlagsShift(cpuIsZeroB(res), n, dst & (0x80>>(sh-1)), (cpuMsbB(dst)) ? (out != mask) : (out != 0));
  }
  cpuSetInstructionTime(cycles + sh*2);
  return (UBY) res;
}

/// <summary>
/// Aslw
/// </summary>
static UWO cpuAslW(UWO dst, ULO sh, ULO cycles)
{
  WOR res;
  sh &= 0x3f;
  if (sh == 0)
  {
    cpuSetFlagsShiftZero(cpuIsZeroW(dst), cpuMsbW(dst));
    res = (WOR) dst;
  }
  else if (sh >= 16)
  {
    res = 0;
    cpuSetFlagsShift(TRUE, FALSE, (sh == 16) ? (dst & 1) : FALSE, dst != 0);
  }
  else
  {
    UWO mask = 0xffff << (15-sh);
    UWO out = dst & mask;
    BOOLE n;
    res = ((WOR)dst) << sh;
    n = cpuMsbW(res);
    cpuSetFlagsShift(cpuIsZeroW(res), n, dst & (0x8000>>(sh-1)), (cpuMsbW(dst)) ? (out != mask) : (out != 0));
  }
  cpuSetInstructionTime(cycles + sh*2);
  return (UWO) res;
}

/// <summary>
/// Asll
/// </summary>
static ULO cpuAslL(ULO dst, ULO sh, ULO cycles)
{
  LON res;
  sh &= 0x3f;
  if (sh == 0)
  {
    cpuSetFlagsShiftZero(cpuIsZeroL(dst), cpuMsbL(dst));
    res = (LON) dst;
  }
  else if (sh >= 32)
  {
    res = 0;
    cpuSetFlagsShift(TRUE, FALSE, (sh == 32) ? (dst & 1) : FALSE, dst != 0);
  }
  else
  {
    ULO mask = 0xffffffff << (31-sh);
    ULO out = dst & mask;
    BOOLE n;
    res = ((LON)dst) << sh;
    n = cpuMsbL(res);
    cpuSetFlagsShift(cpuIsZeroL(res), n, dst & (0x80000000>>(sh-1)), (cpuMsbL(dst)) ? (out != mask) : (out != 0));
  }
  cpuSetInstructionTime(cycles + sh*2);
  return (ULO) res;
}

/// <summary>
/// Asrb
/// </summary>
static UBY cpuAsrB(UBY dst, ULO sh, ULO cycles)
{
  BYT res;
  sh &= 0x3f;
  if (sh == 0)
  {
    cpuSetFlagsShiftZero(cpuIsZeroB(dst), cpuMsbB(dst));
    res = (BYT) dst;
  }
  else if (sh >= 8)
  {
    res = (cpuMsbB(dst)) ? 0xff : 0;
    cpuSetFlagsShift(cpuIsZeroB(res), cpuMsbB(res), cpuMsbB(res), FALSE);
  }
  else
  {
    res = ((BYT)dst) >> sh;
    cpuSetFlagsShift(cpuIsZeroB(res), cpuMsbB(res), dst & (1<<(sh-1)), FALSE);
  }
  cpuSetInstructionTime(cycles + sh*2);
  return (UBY) res;
}

/// <summary>
/// Asrw
/// </summary>
static UWO cpuAsrW(UWO dst, ULO sh, ULO cycles)
{
  WOR res;
  sh &= 0x3f;
  if (sh == 0)
  {
    cpuSetFlagsShiftZero(cpuIsZeroW(dst), cpuMsbW(dst));
    res = (WOR) dst;
  }
  else if (sh >= 16)
  {
    res = (cpuMsbW(dst)) ? 0xffff : 0;
    cpuSetFlagsShift(cpuIsZeroW(res), cpuMsbW(res), cpuMsbW(res), FALSE);
  }
  else
  {
    res = ((WOR)dst) >> sh;
    cpuSetFlagsShift(cpuIsZeroW(res), cpuMsbW(res), dst & (1<<(sh-1)), FALSE);
  }
  cpuSetInstructionTime(cycles + sh*2);
  return (UWO) res;
}

/// <summary>
/// Asrl
/// </summary>
static ULO cpuAsrL(ULO dst, ULO sh, ULO cycles)
{
  LON res;
  sh &= 0x3f;

  if (sh == 0)
  {
    cpuSetFlagsShiftZero(cpuIsZeroL(dst), cpuMsbL(dst));
    res = (LON) dst;
  }
  else if (sh >= 32)
  {
    res = (cpuMsbL(dst)) ? 0xffffffff : 0;
    cpuSetFlagsShift(cpuIsZeroL(res), cpuMsbL(res), cpuMsbL(res), FALSE);
  }
  else
  {
    res = ((LON)dst) >> sh;
    cpuSetFlagsShift(cpuIsZeroL(res), cpuMsbL(res), dst & (1<<(sh-1)), FALSE);
  }
  cpuSetInstructionTime(cycles + sh*2);
  return (ULO) res;
}

/// <summary>
/// Rolb
/// </summary>
static UBY cpuRolB(UBY dst, ULO sh, ULO cycles)
{
  UBY res;
  sh &= 0x3f;
  cycles += sh*2;
  if (sh == 0)
  {
    cpuSetFlagsShiftZero(cpuIsZeroB(dst), cpuMsbB(dst));
    res = dst;
  }
  else
  {
    sh &= 7;
    res = (dst << sh) | (dst >> (8-sh));
    cpuSetFlagsRotate(cpuIsZeroB(res), cpuMsbB(res), res & 1);
  }
  cpuSetInstructionTime(cycles);
  return res;
}

/// <summary>
/// Rolw
/// </summary>
static UWO cpuRolW(UWO dst, ULO sh, ULO cycles)
{
  UWO res;
  sh &= 0x3f;
  cycles += sh*2;
  if (sh == 0)
  {
    cpuSetFlagsShiftZero(cpuIsZeroW(dst), cpuMsbW(dst));
    res = dst;
  }
  else
  {
    sh &= 15;
    res = (dst << sh) | (dst >> (16-sh));
    cpuSetFlagsRotate(cpuIsZeroW(res), cpuMsbW(res), res & 1);
  }
  cpuSetInstructionTime(cycles);
  return res;
}

/// <summary>
/// Roll
/// </summary>
static ULO cpuRolL(ULO dst, ULO sh, ULO cycles)
{
  ULO res;
  sh &= 0x3f;
  cycles += sh*2;
  if (sh == 0)
  {
    cpuSetFlagsShiftZero(cpuIsZeroL(dst), cpuMsbL(dst));
    res = dst;
  }
  else
  {
    sh &= 31;
    res = (dst << sh) | (dst >> (32-sh));
    cpuSetFlagsRotate(cpuIsZeroL(res), cpuMsbL(res), res & 1);
  }
  cpuSetInstructionTime(cycles);
  return res;
}

/// <summary>
/// Rorb
/// </summary>
static UBY cpuRorB(UBY dst, ULO sh, ULO cycles)
{
  UBY res;
  sh &= 0x3f;
  cycles += sh*2;
  if (sh == 0)
  {
    cpuSetFlagsShiftZero(cpuIsZeroB(dst), cpuMsbB(dst));
    res = dst;
  }
  else
  {
    sh &= 7;
    res = (dst >> sh) | (dst << (8-sh));
    cpuSetFlagsRotate(cpuIsZeroB(res), cpuMsbB(res), cpuMsbB(res));
  }
  cpuSetInstructionTime(cycles);
  return res;
}

/// <summary>
/// Rorw
/// </summary>
static UWO cpuRorW(UWO dst, ULO sh, ULO cycles)
{
  UWO res;
  sh &= 0x3f;
  cycles += sh*2;
  if (sh == 0)
  {
    cpuSetFlagsShiftZero(cpuIsZeroW(dst), cpuMsbW(dst));
    res = dst;
  }
  else
  {
    sh &= 15;
    res = (dst >> sh) | (dst << (16-sh));
    cpuSetFlagsRotate(cpuIsZeroW(res), cpuMsbW(res), cpuMsbW(res));
  }
  cpuSetInstructionTime(cycles);
  return res;
}

/// <summary>
/// Rorl
/// </summary>
static ULO cpuRorL(ULO dst, ULO sh, ULO cycles)
{
  ULO res;
  sh &= 0x3f;
  cycles += sh*2;
  if (sh == 0)
  {
    cpuSetFlagsShiftZero(cpuIsZeroL(dst), cpuMsbL(dst));
    res = dst;
  }
  else
  {
    sh &= 31;
    res = (dst >> sh) | (dst << (32-sh));
    cpuSetFlagsRotate(cpuIsZeroL(res), cpuMsbL(res), cpuMsbL(res));
  }
  cpuSetInstructionTime(cycles);
  return res;
}

/// <summary>
/// Roxlb
/// </summary>
static UBY cpuRoxlB(UBY dst, ULO sh, ULO cycles)
{
  BOOLE x = cpuGetFlagX();
  BOOLE x_temp;
  UBY res = dst;
  ULO i;
  sh &= 0x3f;
  for (i = 0; i < sh; ++i)
  {
    x_temp = cpuMsbB(res);
    res = (res << 1) | ((x) ? 1:0);
    x = x_temp;
  }
  cpuSetFlagsRotateX(cpuGetZFlagB(res), cpuGetNFlagB(res), (x) ? 0x11 : 0);
  cpuSetInstructionTime(cycles + sh*2);
  return res;
}

/// <summary>
/// Roxlw
/// </summary>
static UWO cpuRoxlW(UWO dst, ULO sh, ULO cycles)
{
  BOOLE x = cpuGetFlagX();
  BOOLE x_temp;
  UWO res = dst;
  ULO i;
  sh &= 0x3f;
  for (i = 0; i < sh; ++i)
  {
    x_temp = cpuMsbW(res);
    res = (res << 1) | ((x) ? 1:0);
    x = x_temp;
  }
  cpuSetFlagsRotateX(cpuGetZFlagW(res), cpuGetNFlagW(res), (x) ? 0x11 : 0);
  cpuSetInstructionTime(cycles + sh*2);
  return res;
}

/// <summary>
/// Roxll
/// </summary>
static ULO cpuRoxlL(ULO dst, ULO sh, ULO cycles)
{
  BOOLE x = cpuGetFlagX();
  BOOLE x_temp;
  ULO res = dst;
  ULO i;
  sh &= 0x3f;
  for (i = 0; i < sh; ++i)
  {
    x_temp = cpuMsbL(res);
    res = (res << 1) | ((x) ? 1:0);
    x = x_temp;
  }
  cpuSetFlagsRotateX(cpuGetZFlagL(res), cpuGetNFlagL(res), (x) ? 0x11 : 0);
  cpuSetInstructionTime(cycles + sh*2);
  return res;
}

/// <summary>
/// Roxrb
/// </summary>
static UBY cpuRoxrB(UBY dst, ULO sh, ULO cycles)
{
  BOOLE x = cpuGetFlagX();
  BOOLE x_temp;
  UBY res = dst;
  ULO i;
  sh &= 0x3f;
  for (i = 0; i < sh; ++i)
  {
    x_temp = res & 1;
    res = (res >> 1) | ((x) ? 0x80:0);
    x = x_temp;
  }
  cpuSetFlagsRotateX(cpuGetZFlagB(res), cpuGetNFlagB(res), (x) ? 0x11 : 0);
  cpuSetInstructionTime(cycles + sh*2);
  return res;
}

/// <summary>
/// Roxrw
/// </summary>
static UWO cpuRoxrW(UWO dst, ULO sh, ULO cycles)
{
  BOOLE x = cpuGetFlagX();
  BOOLE x_temp;
  UWO res = dst;
  ULO i;
  sh &= 0x3f;
  for (i = 0; i < sh; ++i)
  {
    x_temp = res & 1;
    res = (res >> 1) | ((x) ? 0x8000:0);
    x = x_temp;
  }
  cpuSetFlagsRotateX(cpuGetZFlagW(res), cpuGetNFlagW(res), (x) ? 0x11 : 0);
  cpuSetInstructionTime(cycles + sh*2);
  return res;
}

/// <summary>
/// Roxrl
/// </summary>
static ULO cpuRoxrL(ULO dst, ULO sh, ULO cycles)
{
  BOOLE x = cpuGetFlagX();
  BOOLE x_temp;
  ULO res = dst;
  ULO i;
  sh &= 0x3f;
  for (i = 0; i < sh; ++i)
  {
    x_temp = res & 1;
    res = (res >> 1) | ((x) ? 0x80000000:0);
    x = x_temp;
  }
  cpuSetFlagsRotateX(cpuGetZFlagL(res), cpuGetNFlagL(res), (x) ? 0x11 : 0);
  cpuSetInstructionTime(cycles + sh*2);
  return res;
}

/// <summary>
/// Stop
/// </summary>
static void cpuStop(UWO flags)
{
  if (cpuGetFlagSupervisor())
  {
    cpuSetStop(TRUE);
    cpuUpdateSr(flags);
    cpuSetInstructionTime(16);
  }
  else
  {
    cpuThrowPrivilegeViolationException();
  }
}

/// <summary>
/// Reset
/// </summary>
static void cpuReset()
{
  cpuCallResetExceptionFunc();
  cpuSetInstructionTime(132);
}

/// <summary>
/// Rtd
/// </summary>
static void cpuRtd()
{
  ULO displacement = cpuGetNextOpcode16SignExt();
  cpuSetAReg(7, cpuGetAReg(7) + 4 + displacement);
  cpuSetInstructionTime(4);
}

static ULO cpuRteStackInc[16] = {0, 0, 4, 4, 8, 0, 0, 52, 50, 10, 24, 84, 16, 18, 0, 0};

/// <summary>
/// Rte
/// </summary>
static void cpuRte()
{
  if (cpuGetFlagSupervisor())
  {
    BOOLE redo = TRUE;
    UWO newsr;
    do
    {
      newsr = memoryReadWord(cpuGetAReg(7));
      cpuSetAReg(7, cpuGetAReg(7) + 2);

      cpuSetPC(memoryReadLong(cpuGetAReg(7)));
      cpuSetAReg(7, cpuGetAReg(7) + 4);

      if (cpuGetModelMajor() > 0)
      {
	ULO frame_type = (memoryReadWord(cpuGetAReg(7)) >> 12) & 0xf;
	cpuSetAReg(7, cpuGetAReg(7) + 2);
	cpuSetAReg(7, cpuGetAReg(7) + cpuRteStackInc[frame_type]);
	redo = (frame_type == 1 && cpuGetModelMajor() >= 2 && cpuGetModelMajor() < 6);
      }
      else redo = FALSE;

      cpuUpdateSr(newsr); // Because we can go from isp to msp here.

    } while (redo);

    cpuReadPrefetch();
  }
  else
  {
    cpuThrowPrivilegeViolationException();
  }
  cpuSetInstructionTime(20);
}

/// <summary>
/// Swap
/// </summary>
static void cpuSwap(ULO reg)
{
  cpuSetDReg(reg, cpuJoinWordToLong((UWO)cpuGetDReg(reg), (UWO) (cpuGetDReg(reg) >> 16)));
  cpuSetFlagsNZ00(cpuIsZeroL(cpuGetDReg(reg)), cpuMsbL(cpuGetDReg(reg)));
  cpuSetInstructionTime(4);
}

/// <summary>
/// Unlk
/// </summary>
static void cpuUnlk(ULO reg)
{
  cpuSetAReg(7, cpuGetAReg(reg));
  cpuSetAReg(reg, memoryReadLong(cpuGetAReg(7)));
  cpuSetAReg(7, cpuGetAReg(7) + 4);
  cpuSetInstructionTime(12);
}

/// <summary>
/// Link
/// </summary>
static void cpuLinkW(ULO reg)
{
  ULO disp = cpuGetNextOpcode16SignExt();
  cpuSetAReg(7, cpuGetAReg(7) - 4);
  memoryWriteLong(cpuGetAReg(reg), cpuGetAReg(7));
  cpuSetAReg(reg, cpuGetAReg(7));
  cpuSetAReg(7, cpuGetAReg(7) + disp);
  cpuSetInstructionTime(18);
}

/// <summary>
/// Link.
/// 68020, 68030 and 68040 only.
/// </summary>
static void cpuLinkL(ULO reg)
{
  ULO disp = cpuGetNextOpcode32();
  cpuSetAReg(7, cpuGetAReg(7) - 4);
  memoryWriteLong(cpuGetAReg(reg), cpuGetAReg(7));
  cpuSetAReg(reg, cpuGetAReg(7));
  cpuSetAReg(7, cpuGetAReg(7) + disp);
  cpuSetInstructionTime(4);
}

/// <summary>
/// Ext.w (byte to word)
/// </summary>
static void cpuExtW(ULO reg)
{
  cpuSetDRegWord(reg, cpuGetDRegByteSignExtWord(reg));
  cpuSetFlagsNZ00(cpuIsZeroW(cpuGetDRegWord(reg)), cpuMsbW(cpuGetDRegWord(reg)));
  cpuSetInstructionTime(4);
}

/// <summary>
/// Ext.l (word to long)
/// </summary>
static void cpuExtL(ULO reg)
{
  cpuSetDReg(reg, cpuGetDRegWordSignExtLong(reg));
  cpuSetFlagsNZ00(cpuIsZeroL(cpuGetDReg(reg)), cpuMsbL(cpuGetDReg(reg)));
  cpuSetInstructionTime(4);
}

/// <summary>
/// ExtB.l (byte to long) (020+)
/// </summary>
static void cpuExtBL(ULO reg)
{
  cpuSetDReg(reg, cpuGetDRegByteSignExtLong(reg));
  cpuSetFlagsNZ00(cpuIsZeroL(cpuGetDReg(reg)), cpuMsbL(cpuGetDReg(reg)));
  cpuSetInstructionTime(4);
}

/// <summary>
/// Exg Rx,Ry
/// </summary>
static void cpuExgAll(ULO reg1_type, ULO reg1, ULO reg2_type, ULO reg2)
{
  ULO tmp = cpuGetReg(reg1_type, reg1);
  cpuSetReg(reg1_type, reg1, cpuGetReg(reg2_type, reg2));
  cpuSetReg(reg2_type, reg2, tmp);
  cpuSetInstructionTime(6);
}

/// <summary>
/// Exg Dx,Dy
/// </summary>
static void cpuExgDD(ULO reg1, ULO reg2)
{
  cpuExgAll(0, reg1, 0, reg2);
}

/// <summary>
/// Exg Ax,Ay
/// </summary>
static void cpuExgAA(ULO reg1, ULO reg2)
{
  cpuExgAll(1, reg1, 1, reg2);
}

/// <summary>
/// Exg Dx,Ay
/// </summary>
static void cpuExgDA(ULO reg1, ULO reg2)
{
  cpuExgAll(0, reg1, 1, reg2);
}

/// <summary>
/// Movem.w regs, -(Ax)
/// Order: d0-d7,a0-a7   a7 first
/// </summary>
static void cpuMovemwPre(UWO regs, ULO reg)
{
  ULO cycles = 8;
  ULO dstea = cpuGetAReg(reg);
  ULO index = 1;
  LON i, j;
  BOOLE ea_reg_seen = FALSE;
  ULO ea_reg_ea = 0;

  for (i = 1; i >= 0; i--)
  {
    for (j = 7; j >= 0; j--)
    {
      if (regs & index)
      {
	dstea -= 2;
	if (cpuGetModelMajor() >= 2 && i == 1 && j == reg)
	{
	  ea_reg_seen = TRUE;
	  ea_reg_ea = dstea;
	}
	else
	{
	  memoryWriteWord(cpuGetRegWord(i, j), dstea);
	}
	cycles += 4;
      }
      index = index << 1;
    }
  }
  if (cpuGetModelMajor() >= 2 && ea_reg_seen)
  {
    memoryWriteWord((UWO)dstea, ea_reg_ea);
  }
  cpuSetAReg(reg, dstea);
  cpuSetInstructionTime(cycles);
}

/// <summary>
/// Movem.l regs, -(Ax)
/// Order: d0-d7,a0-a7   a7 first
/// </summary>
static void cpuMovemlPre(UWO regs, ULO reg)
{
  ULO cycles = 8;
  ULO dstea = cpuGetAReg(reg);
  ULO index = 1;
  LON i, j;
  BOOLE ea_reg_seen = FALSE;
  ULO ea_reg_ea = 0;

  for (i = 1; i >= 0; i--)
  {
    for (j = 7; j >= 0; j--)
    {
      if (regs & index)
      {
	dstea -= 4;
	if (cpuGetModelMajor() >= 2 && i == 1 && j == reg)
	{
	  ea_reg_seen = TRUE;
	  ea_reg_ea = dstea;
	}
	else
	{
	  memoryWriteLong(cpuGetReg(i, j), dstea);
	}
	cycles += 8;
      }
      index = index << 1;
    }
  }
  if (cpuGetModelMajor() >= 2 && ea_reg_seen)
  {
    memoryWriteLong(dstea, ea_reg_ea);
  }
  cpuSetAReg(reg, dstea);
  cpuSetInstructionTime(cycles);
}

/// <summary>
/// Movem.w (Ax)+, regs
/// Order: a7-a0,d7-d0   d0 first
/// </summary>
static void cpuMovemwPost(UWO regs, ULO reg)
{
  ULO cycles = 12;
  ULO dstea = cpuGetAReg(reg);
  ULO index = 1;
  ULO i, j;

  for (i = 0; i < 2; ++i)
  {
    for (j = 0; j < 8; ++j)
    {
      if (regs & index)
      {
	// Each word, for both data and address registers, is sign-extended before stored.
	cpuSetReg(i, j, (ULO)(LON)(WOR) memoryReadWord(dstea));
	dstea += 2;
	cycles += 4;
      }
      index = index << 1;
    }
  }
  cpuSetAReg(reg, dstea);
  cpuSetInstructionTime(cycles);
}

/// <summary>
/// Movem.l (Ax)+, regs
/// Order: a7-a0,d7-d0   d0 first
/// </summary>
static void cpuMovemlPost(UWO regs, ULO reg)
{
  ULO cycles = 12;
  ULO dstea = cpuGetAReg(reg);
  ULO index = 1;
  ULO i, j;

  for (i = 0; i < 2; ++i)
  {
    for (j = 0; j < 8; ++j)
    {
      if (regs & index)
      {
	cpuSetReg(i, j, memoryReadLong(dstea));
	dstea += 4;
	cycles += 8;
      }
      index = index << 1;
    }
  }
  cpuSetAReg(reg, dstea);
  cpuSetInstructionTime(cycles);
}

/// <summary>
/// Movem.w <Control>, regs
/// Order: a7-a0,d7-d0   d0 first
/// </summary>
static void cpuMovemwEa2R(UWO regs, ULO ea, ULO eacycles)
{
  ULO cycles = eacycles;
  ULO dstea = ea;
  ULO index = 1;
  ULO i, j;

  for (i = 0; i < 2; ++i)
  {
    for (j = 0; j < 8; ++j)
    {
      if (regs & index)
      {
	// Each word, for both data and address registers, is sign-extended before stored.
	cpuSetReg(i, j, (ULO)(LON)(WOR) memoryReadWord(dstea));
	dstea += 2;
	cycles += 4;
      }
      index = index << 1;
    }
  }
  cpuSetInstructionTime(cycles);
}

/// <summary>
/// Movem.l <Control>, regs
/// Order: a7-a0,d7-d0   d0 first
/// </summary>
static void cpuMovemlEa2R(UWO regs, ULO ea, ULO eacycles)
{
  ULO cycles = eacycles;
  ULO dstea = ea;
  ULO index = 1;
  ULO i, j;

  for (i = 0; i < 2; ++i)
  {
    for (j = 0; j < 8; ++j)
    {
      if (regs & index)
      {
	cpuSetReg(i, j, memoryReadLong(dstea));
	dstea += 4;
	cycles += 8;
      }
      index = index << 1;
    }
  }
  cpuSetInstructionTime(cycles);
}

/// <summary>
/// Movem.w regs, <Control>
/// Order: a7-a0,d7-d0   d0 first
/// </summary>
static void cpuMovemwR2Ea(UWO regs, ULO ea, ULO eacycles)
{
  ULO cycles = eacycles;
  ULO dstea = ea;
  ULO index = 1;
  ULO i, j;

  for (i = 0; i < 2; ++i)
  {
    for (j = 0; j < 8; ++j)
    {
      if (regs & index)
      {
	memoryWriteWord(cpuGetRegWord(i, j), dstea);
	dstea += 2;
	cycles += 4;
      }
      index = index << 1;
    }
  }
  cpuSetInstructionTime(cycles);
}

/// <summary>
/// Movem.l regs, <Control>
/// Order: a7-a0,d7-d0   d0 first
/// </summary>
static void cpuMovemlR2Ea(UWO regs, ULO ea, ULO eacycles)
{
  ULO cycles = eacycles;
  ULO dstea = ea;
  ULO index = 1;
  ULO i, j;

  for (i = 0; i < 2; ++i)
  {
    for (j = 0; j < 8; ++j)
    {
      if (regs & index)
      {
	memoryWriteLong(cpuGetReg(i, j), dstea);
	dstea += 4;
	cycles += 8;
      }
      index = index << 1;
    }
  }
  cpuSetInstructionTime(cycles);
}

/// <summary>
/// Trap #vectorno
/// </summary>
static void cpuTrap(ULO vectorno)
{
  // PC written to the exception frame must be pc + 2, the address of the next instruction.
  cpuThrowTrapException(vectorno);
}

/// <summary>
/// move.l  Ax,Usp
/// </summary>
static void cpuMoveToUsp(ULO reg)
{
  if (cpuGetFlagSupervisor())
  {
    // In supervisor mode, usp does not affect a7
    cpuSetUspDirect(cpuGetAReg(reg));
    cpuSetInstructionTime(4);
  }
  else
  {
    cpuThrowPrivilegeViolationException();
  }
}

/// <summary>
/// move.l  Usp,Ax
/// </summary>
static void cpuMoveFromUsp(ULO reg)
{
  if (cpuGetFlagSupervisor())
  {
    // In supervisor mode, usp is up to date
    cpuSetAReg(reg, cpuGetUspDirect());
    cpuSetInstructionTime(4);
  }
  else
  {
    cpuThrowPrivilegeViolationException();
  }
}

/// <summary>
/// cmp.b (Ay)+,(Ax)+
/// </summary>
static void cpuCmpMB(ULO regx, ULO regy)
{
  UBY src = memoryReadByte(cpuEA03(regy, 1));
  UBY dst = memoryReadByte(cpuEA03(regx, 1));
  UBY res = dst - src;
  cpuSetFlagsCmp(cpuIsZeroB(res), cpuMsbB(res), cpuMsbB(dst), cpuMsbB(src));
  cpuSetInstructionTime(12);
}

/// <summary>
/// cmp.w (Ay)+,(Ax)+
/// </summary>
static void cpuCmpMW(ULO regx, ULO regy)
{
  UWO src = memoryReadWord(cpuEA03(regy, 2));
  UWO dst = memoryReadWord(cpuEA03(regx, 2));
  UWO res = dst - src;
  cpuSetFlagsCmp(cpuIsZeroW(res), cpuMsbW(res), cpuMsbW(dst), cpuMsbW(src));
  cpuSetInstructionTime(12);
}

/// <summary>
/// cmp.l (Ay)+,(Ax)+
/// </summary>
static void cpuCmpML(ULO regx, ULO regy)
{
  ULO src = memoryReadLong(cpuEA03(regy, 4));
  ULO dst = memoryReadLong(cpuEA03(regx, 4));
  ULO res = dst - src;
  cpuSetFlagsCmp(cpuIsZeroL(res), cpuMsbL(res), cpuMsbL(dst), cpuMsbL(src));
  cpuSetInstructionTime(20);
}

/// <summary>
/// chk.w Dx, ea
/// Undocumented features:
/// Z is set from the register operand,
/// V and C is always cleared.
/// </summary>
static void cpuChkW(UWO value, UWO ub)
{
  cpuSetFlagZ(value == 0);
  cpuSetFlagsVC(FALSE, FALSE);
  if (((WOR)value) < 0)
  {
    cpuSetFlagN(TRUE);
    cpuThrowChkException();
  }
  else if (((WOR)value) > ((WOR)ub))
  {
    cpuSetFlagN(FALSE);
    cpuThrowChkException();
  }
}

/// <summary>
/// chk.l Dx, ea
/// 68020+
/// Undocumented features:
/// Z is set from the register operand,
/// V and C is always cleared.
/// </summary>
static void cpuChkL(ULO value, ULO ub)
{
  cpuSetFlagZ(value == 0);
  cpuSetFlagsVC(FALSE, FALSE);
  if (((LON)value) < 0)
  {
    cpuSetFlagN(TRUE);
    cpuThrowChkException();
  }
  else if (((LON)value) > ((LON)ub))
  {
    cpuSetFlagN(FALSE);
    cpuThrowChkException();
  }
}

/// <summary>
/// addx.b dx,dy
/// </summary>
static UBY cpuAddXB(UBY dst, UBY src)
{
  UBY res = dst + src + ((cpuGetFlagX()) ? 1:0);
  cpuSetFlagsAddX(cpuIsZeroB(res), cpuMsbB(res), cpuMsbB(dst), cpuMsbB(src));
  return res;
}

/// <summary>
/// addx.w dx,dy
/// </summary>
static UWO cpuAddXW(UWO dst, UWO src)
{
  UWO res = dst + src + ((cpuGetFlagX()) ? 1:0);
  cpuSetFlagsAddX(cpuIsZeroW(res), cpuMsbW(res), cpuMsbW(dst), cpuMsbW(src));
  return res;
}

/// <summary>
/// addx.l dx,dy
/// </summary>
static ULO cpuAddXL(ULO dst, ULO src)
{
  ULO res = dst + src + ((cpuGetFlagX()) ? 1:0);
  cpuSetFlagsAddX(cpuIsZeroL(res), cpuMsbL(res), cpuMsbL(dst), cpuMsbL(src));
  return res;
}

/// <summary>
/// subx.b dx,dy
/// </summary>
static UBY cpuSubXB(UBY dst, UBY src)
{
  UBY res = dst - src - ((cpuGetFlagX()) ? 1:0);
  cpuSetFlagsSubX(cpuIsZeroB(res), cpuMsbB(res), cpuMsbB(dst), cpuMsbB(src));
  return res;
}

/// <summary>
/// subx.w dx,dy
/// </summary>
static UWO cpuSubXW(UWO dst, UWO src)
{
  UWO res = dst - src - ((cpuGetFlagX()) ? 1:0);
  cpuSetFlagsSubX(cpuIsZeroW(res), cpuMsbW(res), cpuMsbW(dst), cpuMsbW(src));
  return res;
}

/// <summary>
/// subx.l dx,dy
/// </summary>
static ULO cpuSubXL(ULO dst, ULO src)
{
  ULO res = dst - src - ((cpuGetFlagX()) ? 1:0);
  cpuSetFlagsSubX(cpuIsZeroL(res), cpuMsbL(res), cpuMsbL(dst), cpuMsbL(src));
  return res;
}

/// <summary>
/// abcd.b src,dst
/// Implemented using the information from:
///     68000 Undocumented Behavior Notes
///                Fourth Edition
///    by Bart Trzynadlowski, May 12, 2003
/// </summary>
static UBY cpuAbcdB(UBY dst, UBY src)
{
  UBY xflag = (cpuGetFlagX()) ? 1:0;
  UWO res = dst + src + xflag;
  UWO res_unadjusted = res;
  UBY res_bcd;
  UBY low_nibble = (dst & 0xf) + (src & 0xf) + xflag;

  if (low_nibble > 9)
  {
    res += 6;
  }

  if (res > 0x99)
  {
    res += 0x60;
    cpuSetFlagXC(TRUE);
  }
  else
  {
    cpuSetFlagXC(FALSE);
  }

  res_bcd = (UBY) res;

  if (res_bcd != 0)
  {
    cpuSetFlagZ(FALSE);
  }
  if (res_bcd & 0x80)
  {
    cpuSetFlagN(TRUE);
  }
  cpuSetFlagV(((res_unadjusted & 0x80) == 0) && (res_bcd & 0x80));
  return res_bcd;
}

/// <summary>
/// sbcd.b src,dst
/// nbcd.b src   (set dst=0)
/// Implemented using the information from:
///     68000 Undocumented Behavior Notes
///                Fourth Edition
///    by Bart Trzynadlowski, May 12, 2003
/// </summary>
static UBY cpuSbcdB(UBY dst, UBY src)
{
  UBY xflag = (cpuGetFlagX()) ? 1:0;
  UWO res = dst - src - xflag;
  UWO res_unadjusted = res;
  UBY res_bcd;

  if (((src & 0xf) + xflag) > (dst & 0xf))
  {
    res -= 6;
  }
  if (res & 0x80)
  {
    res -= 0x60;
    cpuSetFlagXC(TRUE);
  }
  else
  {
    cpuSetFlagXC(FALSE);
  }
  res_bcd = (UBY) res;

  if (res_bcd != 0)
  {
    cpuSetFlagZ(FALSE);
  }
  if (res_bcd & 0x80)
  {
    cpuSetFlagN(TRUE);
  }
  cpuSetFlagV(((res_unadjusted & 0x80) == 0x80) && !(res_bcd & 0x80));
  return res_bcd;
}

/// <summary>
/// nbcd.b dst
/// </summary>
static UBY cpuNbcdB(UBY dst)
{
  return cpuSbcdB(0, dst);
}

// Bit field functions
static void cpuGetBfRegBytes(UBY *bytes, ULO regno)
{
  bytes[0] = (UBY)(cpuGetDReg(regno) >> 24);
  bytes[1] = (UBY)(cpuGetDReg(regno) >> 16);
  bytes[2] = (UBY)(cpuGetDReg(regno) >> 8);
  bytes[3] = (UBY)cpuGetDReg(regno);
}

static void cpuGetBfEaBytes(UBY *bytes, ULO address, ULO count)
{
  ULO i;
  for (i = 0; i < count; ++i)
  {
    bytes[i] = memoryReadByte(address + i);
  }
}

static void cpuSetBfRegBytes(UBY *bytes, ULO regno)
{
  cpuSetDReg(regno, cpuJoinByteToLong(bytes[0], bytes[1], bytes[2], bytes[3]));
}

static void cpuSetBfEaBytes(UBY *bytes, ULO address, ULO count)
{
  ULO i;
  for (i = 0; i < count; ++i)
  {
    memoryWriteByte(bytes[i], address + i);
  }
}

static LON cpuGetBfOffset(UWO ext, BOOLE offsetIsDr)
{
  LON offset = (ext >> 6) & 0x1f;
  if (offsetIsDr)
  {
    offset = (LON) cpuGetDReg(offset & 7);
  }
  return offset;
}

static ULO cpuGetBfWidth(UWO ext, BOOLE widthIsDr)
{
  ULO width = (ext & 0x1f);
  if (widthIsDr)
  {
    width = (cpuGetDReg(width & 7) & 0x1f);
  }
  if (width == 0)
  {
    width = 32;
  }
  return width;
}

static ULO cpuGetBfField(UBY *bytes, ULO end_offset, ULO byte_count, ULO field_mask)
{
  ULO i;
  ULO field = ((ULO)bytes[byte_count - 1]) >> end_offset;

  for (i = 1; i < byte_count; i++)
  {
    field |= ((ULO)bytes[byte_count - i - 1]) << (8*i - end_offset); 
  }
  return field & field_mask;
}

static void cpuSetBfField(UBY *bytes, ULO end_offset, ULO byte_count, ULO field, ULO field_mask)
{
  ULO i;

  bytes[byte_count - 1] = (UBY)((field << end_offset) | (bytes[byte_count - 1] & (UBY)~(field_mask << end_offset)));
  for (i = 1; i < byte_count - 1; ++i)
  {
    bytes[byte_count - i - 1] = (UBY)(field >> (end_offset + 8*i));
  }
  if (i < byte_count)
  {
    bytes[0] = (bytes[0] & (UBY)~(field_mask >> (end_offset + 8*i)) | (UBY)(field >> (end_offset + 8*i)));
  }
}

struct cpuBfData
{
  UWO ext;
  BOOLE offsetIsDr;
  BOOLE widthIsDr;
  LON offset;
  ULO width;
  ULO base_address;
  ULO bit_offset;
  ULO end_offset;
  ULO byte_count;
  ULO field;
  ULO field_mask;
  ULO dn;
  UBY b[5];
};

void cpuBfExtWord(struct cpuBfData *bf_data, ULO val, BOOLE has_dn, BOOLE has_ea, UWO ext)
{
  bf_data->ext = ext;
  bf_data->offsetIsDr = (bf_data->ext & 0x0800);
  bf_data->widthIsDr = (bf_data->ext & 0x20);
  bf_data->offset = cpuGetBfOffset(bf_data->ext, bf_data->offsetIsDr);
  bf_data->width = cpuGetBfWidth(bf_data->ext, bf_data->widthIsDr);
  bf_data->bit_offset = bf_data->offset & 7;
  bf_data->byte_count = ((bf_data->bit_offset + bf_data->width + 7) >> 3);
  bf_data->end_offset = (bf_data->byte_count*8 - (bf_data->offset + bf_data->width)) & 7;
  bf_data->field = 0;
  bf_data->field_mask = 0xffffffff >> (32 - bf_data->width);
  if (has_dn)
  {
    bf_data->dn = (bf_data->ext & 0x7000) >> 12;
  }
  if (has_ea)
  {
    bf_data->base_address = val + (bf_data->offset >> 3);
    cpuGetBfEaBytes(&bf_data->b[0], bf_data->base_address, bf_data->byte_count);
  }
  else
  {
    cpuGetBfRegBytes(&bf_data->b[0], val);
  }
}

/// <summary>
/// bfchg common logic
/// </summary>
static void cpuBfChgCommon(ULO val, BOOLE has_ea, UWO ext)
{
  struct cpuBfData bf_data;
  cpuBfExtWord(&bf_data, val, FALSE, has_ea, ext);
  bf_data.field = cpuGetBfField(&bf_data.b[0], bf_data.end_offset, bf_data.byte_count, bf_data.field_mask);
  cpuSetFlagsNZVC(bf_data.field == 0, bf_data.field & (1 << (bf_data.width - 1)), FALSE, FALSE);
  cpuSetBfField(&bf_data.b[0], bf_data.end_offset, bf_data.byte_count, (~bf_data.field) & bf_data.field_mask, bf_data.field_mask);
  if (has_ea)
  {
    cpuSetBfEaBytes(&bf_data.b[0], bf_data.base_address, bf_data.byte_count);
  }
  else
  {
    cpuSetBfRegBytes(&bf_data.b[0], val);
  }
}

/// <summary>
/// bfchg dx {offset:width}
/// </summary>
static void cpuBfChgReg(ULO regno, UWO ext)
{
  cpuBfChgCommon(regno, FALSE, ext);
}

/// <summary>
/// bfchg ea {offset:width}
/// </summary>
static void cpuBfChgEa(ULO ea, UWO ext)
{
  cpuBfChgCommon(ea, TRUE, ext);
}

/// <summary>
/// bfclr common logic
/// </summary>
static void cpuBfClrCommon(ULO val, BOOLE has_ea, UWO ext)
{
  struct cpuBfData bf_data;
  cpuBfExtWord(&bf_data, val, FALSE, has_ea, ext);
  bf_data.field = cpuGetBfField(&bf_data.b[0], bf_data.end_offset, bf_data.byte_count, bf_data.field_mask);
  cpuSetFlagsNZVC(bf_data.field == 0, bf_data.field & (1 << (bf_data.width - 1)), FALSE, FALSE);
  cpuSetBfField(&bf_data.b[0], bf_data.end_offset, bf_data.byte_count, 0, bf_data.field_mask);
  if (has_ea)
  {
    cpuSetBfEaBytes(&bf_data.b[0], bf_data.base_address, bf_data.byte_count);
  }
  else
  {
    cpuSetBfRegBytes(&bf_data.b[0], val);
  }
}

/// <summary>
/// bfclr dx {offset:width}
/// </summary>
static void cpuBfClrReg(ULO regno, UWO ext)
{
  cpuBfClrCommon(regno, FALSE, ext);
}

/// <summary>
/// bfclr ea {offset:width}
/// </summary>
static void cpuBfClrEa(ULO ea, UWO ext)
{
  cpuBfClrCommon(ea, TRUE, ext);
}

/// <summary>
/// bfexts common logic
/// </summary>
static void cpuBfExtsCommon(ULO val, BOOLE has_ea, UWO ext)
{
  struct cpuBfData bf_data;
  BOOLE n_flag;
  cpuBfExtWord(&bf_data, val, TRUE, has_ea, ext);
  bf_data.field = cpuGetBfField(&bf_data.b[0], bf_data.end_offset, bf_data.byte_count, bf_data.field_mask);
  n_flag = bf_data.field & (1 << (bf_data.width - 1));
  cpuSetFlagsNZVC(bf_data.field == 0, n_flag, FALSE, FALSE);
  if (n_flag)
  {
    bf_data.field = ~bf_data.field_mask | bf_data.field;
  }
  cpuSetDReg(bf_data.dn, bf_data.field);
}

/// <summary>
/// bfexts dx {offset:width}, Dn
/// </summary>
static void cpuBfExtsReg(ULO regno, UWO ext)
{
  cpuBfExtsCommon(regno, FALSE, ext);
}

/// <summary>
/// bfexts ea {offset:width}, Dn
/// </summary>
static void cpuBfExtsEa(ULO ea, UWO ext)
{
  cpuBfExtsCommon(ea, TRUE, ext);
}

/// <summary>
/// bfextu ea {offset:width}, Dn
/// </summary>
static void cpuBfExtuCommon(ULO val, BOOLE has_ea, UWO ext)
{
  struct cpuBfData bf_data;
  cpuBfExtWord(&bf_data, val, TRUE, has_ea, ext);
  bf_data.field = cpuGetBfField(&bf_data.b[0], bf_data.end_offset, bf_data.byte_count, bf_data.field_mask);
  cpuSetFlagsNZVC(bf_data.field == 0, bf_data.field & (1 << (bf_data.width - 1)), FALSE, FALSE);
  cpuSetDReg(bf_data.dn, bf_data.field);
}

/// <summary>
/// bfextu dx {offset:width}, Dn
/// </summary>
static void cpuBfExtuReg(ULO regno, UWO ext)
{
  cpuBfExtuCommon(regno, FALSE, ext);
}

/// <summary>
/// bfextu ea {offset:width}, Dn
/// </summary>
static void cpuBfExtuEa(ULO ea, UWO ext)
{
  cpuBfExtuCommon(ea, TRUE, ext);
}

/// <summary>
/// bfffo common logic
/// </summary>
static void cpuBfFfoCommon(ULO val, BOOLE has_ea, UWO ext)
{
  struct cpuBfData bf_data;
  ULO i;
  cpuBfExtWord(&bf_data, val, TRUE, has_ea, ext);
  bf_data.field = cpuGetBfField(&bf_data.b[0], bf_data.end_offset, bf_data.byte_count, bf_data.field_mask);
  cpuSetFlagsNZVC(bf_data.field == 0, bf_data.field & (1 << (bf_data.width - 1)), FALSE, FALSE);
  for (i = 0; i < bf_data.width; ++i)
  {
    if (bf_data.field & (0x1 << (bf_data.width - i - 1)))
      break;
  }
  cpuSetDReg(bf_data.dn, bf_data.offset + i);
}

/// <summary>
/// bfffo dx {offset:width}, Dn
/// </summary>
static void cpuBfFfoReg(ULO regno, UWO ext)
{
  cpuBfFfoCommon(regno, FALSE, ext);
}

/// <summary>
/// bfffo ea {offset:width}, Dn
/// </summary>
static void cpuBfFfoEa(ULO ea, UWO ext)
{
  cpuBfFfoCommon(ea, TRUE, ext);
}

/// <summary>
/// bfins common logic
/// </summary>
static void cpuBfInsCommon(ULO val, BOOLE has_ea, UWO ext)
{
  struct cpuBfData bf_data;
  cpuBfExtWord(&bf_data, val, TRUE, has_ea, ext);
  bf_data.field = cpuGetBfField(&bf_data.b[0], bf_data.end_offset, bf_data.byte_count, bf_data.field_mask);
  cpuSetFlagsNZVC(bf_data.field == 0, bf_data.field & (1 << (bf_data.width - 1)), FALSE, FALSE);
  bf_data.field = cpuGetDReg(bf_data.dn) & bf_data.field_mask;
  cpuSetBfField(&bf_data.b[0], bf_data.end_offset, bf_data.byte_count, bf_data.field, bf_data.field_mask);
  if (has_ea)
  {
    cpuSetBfEaBytes(&bf_data.b[0], bf_data.base_address, bf_data.byte_count);
  }
  else
  {
    cpuSetBfRegBytes(&bf_data.b[0], val);
  }
}

/// <summary>
/// bfins Dn, ea {offset:width}
/// </summary>
static void cpuBfInsReg(ULO regno, UWO ext)
{
  cpuBfInsCommon(regno, FALSE, ext);
}

/// <summary>
/// bfins Dn, ea {offset:width}
/// </summary>
static void cpuBfInsEa(ULO ea, UWO ext)
{
  cpuBfInsCommon(ea, TRUE, ext);
}

/// <summary>
/// bfset common logic
/// </summary>
static void cpuBfSetCommon(ULO val, BOOLE has_ea, UWO ext)
{
  struct cpuBfData bf_data;
  cpuBfExtWord(&bf_data, val, FALSE, has_ea, ext);
  bf_data.field = cpuGetBfField(&bf_data.b[0], bf_data.end_offset, bf_data.byte_count, bf_data.field_mask);
  cpuSetFlagsNZVC(bf_data.field == 0, bf_data.field & (1 << (bf_data.width - 1)), FALSE, FALSE);
  bf_data.field = bf_data.field_mask;
  cpuSetBfField(&bf_data.b[0], bf_data.end_offset, bf_data.byte_count, bf_data.field, bf_data.field_mask);
  if (has_ea)
  {
    cpuSetBfEaBytes(&bf_data.b[0], bf_data.base_address, bf_data.byte_count);
  }
  else
  {
    cpuSetBfRegBytes(&bf_data.b[0], val);
  }
}

/// <summary>
/// bfset dx {offset:width}
/// </summary>
static void cpuBfSetReg(ULO regno, UWO ext)
{
  cpuBfSetCommon(regno, FALSE, ext);
}

/// <summary>
/// bfset ea {offset:width}
/// </summary>
static void cpuBfSetEa(ULO ea, UWO ext)
{
  cpuBfSetCommon(ea, TRUE, ext);
}

/// <summary>
/// bftst common logic
/// </summary>
static void cpuBfTstCommon(ULO val, BOOLE has_ea, UWO ext)
{
  struct cpuBfData bf_data;
  cpuBfExtWord(&bf_data, val, FALSE, has_ea, ext);
  bf_data.field = cpuGetBfField(&bf_data.b[0], bf_data.end_offset, bf_data.byte_count, bf_data.field_mask);
  cpuSetFlagsNZVC(bf_data.field == 0, bf_data.field & (1 << (bf_data.width - 1)), FALSE, FALSE);
}

/// <summary>
/// bftst dx {offset:width}
/// </summary>
static void cpuBfTstReg(ULO regno, UWO ext)
{
  cpuBfTstCommon(regno, FALSE, ext);
}

/// <summary>
/// bftst ea {offset:width}
/// </summary>
static void cpuBfTstEa(ULO ea, UWO ext)
{
  cpuBfTstCommon(ea, TRUE, ext);
}

/// <summary>
/// movep.w (d16, Ay), Dx
/// </summary>
static void cpuMovepWReg(ULO areg, ULO dreg)
{
  ULO ea = cpuGetAReg(areg) + cpuGetNextOpcode16SignExt();
  memoryWriteByte((UBY) (cpuGetDReg(dreg) >> 8), ea);
  memoryWriteByte(cpuGetDRegByte(dreg), ea + 2);
  cpuSetInstructionTime(16);
}

/// <summary>
/// movep.l (d16, Ay), Dx
/// </summary>
static void cpuMovepLReg(ULO areg, ULO dreg)
{
  ULO ea = cpuGetAReg(areg) + cpuGetNextOpcode16SignExt();
  memoryWriteByte((UBY)(cpuGetDReg(dreg) >> 24), ea);
  memoryWriteByte((UBY)(cpuGetDReg(dreg) >> 16), ea + 2);
  memoryWriteByte((UBY)(cpuGetDReg(dreg) >> 8), ea + 4);
  memoryWriteByte(cpuGetDRegByte(dreg), ea + 6);
  cpuSetInstructionTime(24);
}

/// <summary>
/// movep.w Dx, (d16, Ay)
/// </summary>
static void cpuMovepWEa(ULO areg, ULO dreg)
{
  ULO ea = cpuGetAReg(areg) + cpuGetNextOpcode16SignExt();
  cpuSetDRegWord(dreg, cpuJoinByteToWord(memoryReadByte(ea), memoryReadByte(ea + 2)));
  cpuSetInstructionTime(16);
}

/// <summary>
/// movep.l Dx, (d16, Ay)
/// </summary>
static void cpuMovepLEa(ULO areg, ULO dreg)
{
  ULO ea = cpuGetAReg(areg) + cpuGetNextOpcode16SignExt();
  cpuSetDReg(dreg, cpuJoinByteToLong(memoryReadByte(ea), memoryReadByte(ea + 2), memoryReadByte(ea + 4), memoryReadByte(ea + 6)));
  cpuSetInstructionTime(24);
}

/// <summary>
/// pack Dx, Dy, #adjustment
/// </summary>
static void cpuPackReg(ULO yreg, ULO xreg)
{
  UWO adjustment = cpuGetNextOpcode16();
  UWO src = cpuGetDRegWord(xreg) + adjustment;
  cpuSetDRegByte(yreg, (UBY) (((src >> 4) & 0xf0) | (src & 0xf)));
}

/// <summary>
/// pack -(Ax), -(Ay), #adjustment
/// </summary>
static void cpuPackEa(ULO yreg, ULO xreg)
{
  UWO adjustment = cpuGetNextOpcode16();
  UBY b1 = memoryReadByte(cpuEA04(xreg, 1));
  UBY b2 = memoryReadByte(cpuEA04(xreg, 1));
  UWO result = ((((UWO)b1) << 8) | (UWO) b2) + adjustment;
  memoryWriteByte((UBY) (((result >> 4) & 0xf0) | (result & 0xf)), cpuEA04(yreg, 1));
}

/// <summary>
/// unpk Dx, Dy, #adjustment
/// </summary>
static void cpuUnpkReg(ULO yreg, ULO xreg)
{
  UWO adjustment = cpuGetNextOpcode16();
  UBY b1 = cpuGetDRegByte(xreg);
  UWO result = ((((UWO)(b1 & 0xf0)) << 4) | ((UWO)(b1 & 0xf))) + adjustment;
  cpuSetDRegWord(yreg, result);
}

/// <summary>
/// unpk -(Ax), -(Ay), #adjustment
/// </summary>
static void cpuUnpkEa(ULO yreg, ULO xreg)
{
  UWO adjustment = cpuGetNextOpcode16();
  UBY b1 = memoryReadByte(cpuEA04(xreg, 1));
  UWO result = ((((UWO)(b1 & 0xf0)) << 4) | ((UWO)(b1 & 0xf))) + adjustment;
  memoryWriteByte((UBY) (result >> 8), cpuEA04(yreg, 1));
  memoryWriteByte((UBY) result, cpuEA04(yreg, 1));
}

/// <summary>
/// movec
/// </summary>
static void cpuMoveCFrom()
{
  if (cpuGetFlagSupervisor())
  {
    UWO extension = (UWO) cpuGetNextOpcode16();
    ULO da = (extension >> 15) & 1;
    ULO regno = (extension >> 12) & 7;
    ULO ctrl_regno = extension & 0xfff;
    if (cpuGetModelMajor() == 1)
    {
      switch (ctrl_regno)
      {
	case 0x000: cpuSetReg(da, regno, cpuGetSfc()); break;
	case 0x001: cpuSetReg(da, regno, cpuGetDfc()); break;
	case 0x800: cpuSetReg(da, regno, cpuGetUspDirect()); break; // In supervisor mode, usp is up to date.
	case 0x801: cpuSetReg(da, regno, cpuGetVbr()); break;
	default:  cpuThrowIllegalInstructionException(FALSE); return;	  // Illegal instruction
      }
    }
    else if (cpuGetModelMajor() == 2)
    {
      switch (ctrl_regno)
      {
	case 0x000: cpuSetReg(da, regno, cpuGetSfc()); break;
	case 0x001: cpuSetReg(da, regno, cpuGetDfc()); break;
	case 0x002: cpuSetReg(da, regno, cpuGetCacr() & 3); break;
	case 0x800: cpuSetReg(da, regno, cpuGetUspDirect()); break; // In supervisor mode, usp is up to date.
	case 0x801: cpuSetReg(da, regno, cpuGetVbr()); break;
	case 0x802: cpuSetReg(da, regno, cpuGetCaar() & 0xfc); break;
	case 0x803: cpuSetReg(da, regno, cpuGetMspAutoMap()); break;
	case 0x804: cpuSetReg(da, regno, cpuGetIspAutoMap()); break;
	default:  cpuThrowIllegalInstructionException(FALSE); return;	  // Illegal instruction
      }
    }
    else if (cpuGetModelMajor() == 3)
    {
      switch (ctrl_regno)
      {
	case 0x000: cpuSetReg(da, regno, cpuGetSfc()); break;
	case 0x001: cpuSetReg(da, regno, cpuGetDfc()); break;
	case 0x002: cpuSetReg(da, regno, cpuGetCacr()); break;
	case 0x800: cpuSetReg(da, regno, cpuGetUspDirect()); break; // In supervisor mode, usp is up to date.
	case 0x801: cpuSetReg(da, regno, cpuGetVbr()); break;
	case 0x802: cpuSetReg(da, regno, cpuGetCaar() & 0xfc); break;
	case 0x803: cpuSetReg(da, regno, cpuGetMspAutoMap()); break;
	case 0x804: cpuSetReg(da, regno, cpuGetIspAutoMap()); break;
	default:  cpuThrowIllegalInstructionException(FALSE); return;	  // Illegal instruction
      }
    }
  }
  else
  {
    cpuThrowPrivilegeViolationException();
    return;
  }
  cpuSetInstructionTime(4);
}

/// <summary>
/// movec
/// </summary>
static void cpuMoveCTo()
{
  if (cpuGetFlagSupervisor())
  {
    UWO extension = (UWO) cpuGetNextOpcode16();
    ULO da = (extension >> 15) & 1;
    ULO regno = (extension >> 12) & 7;
    ULO ctrl_regno = extension & 0xfff;
    if (cpuGetModelMajor() == 1)
    {
      switch (ctrl_regno)
      {
	case 0x000: cpuSetSfc(cpuGetReg(da, regno) & 7); break;
	case 0x001: cpuSetDfc(cpuGetReg(da, regno) & 7); break;
	case 0x800: cpuSetUspDirect(cpuGetReg(da, regno)); break;
	case 0x801: cpuSetVbr(cpuGetReg(da, regno)); break;
	default:  cpuThrowIllegalInstructionException(FALSE); return;	  // Illegal instruction
      }
    }
    else if (cpuGetModelMajor() == 2)
    {
      switch (ctrl_regno)
      {
	case 0x000: cpuSetSfc(cpuGetReg(da, regno) & 7); break;
	case 0x001: cpuSetDfc(cpuGetReg(da, regno) & 7); break;
	case 0x002: cpuSetCacr(cpuGetReg(da, regno) & 0x3); break;
	case 0x800: cpuSetUspDirect(cpuGetReg(da, regno)); break;
	case 0x801: cpuSetVbr(cpuGetReg(da, regno)); break;
	case 0x802: cpuSetCaar(cpuGetReg(da, regno) & 0x00fc); break;
	case 0x803: cpuSetMspAutoMap(cpuGetReg(da, regno)); break;
	case 0x804: cpuSetIspAutoMap(cpuGetReg(da, regno)); break;
	default:  cpuThrowIllegalInstructionException(FALSE); return;	  // Illegal instruction
      }
    }
    else if (cpuGetModelMajor() == 3)
    {
      switch (ctrl_regno)
      {
	case 0x000: cpuSetSfc(cpuGetReg(da, regno) & 7); break;
	case 0x001: cpuSetDfc(cpuGetReg(da, regno) & 7); break;
	case 0x002: cpuSetCacr(cpuGetReg(da, regno) & 0x3313); break;
	case 0x800: cpuSetUspDirect(cpuGetReg(da, regno)); break;
	case 0x801: cpuSetVbr(cpuGetReg(da, regno)); break;
	case 0x802: cpuSetCaar(cpuGetReg(da, regno) & 0x00fc); break;
	case 0x803: cpuSetMspAutoMap(cpuGetReg(da, regno)); break;
	case 0x804: cpuSetIspAutoMap(cpuGetReg(da, regno)); break;
	default:  cpuThrowIllegalInstructionException(FALSE); return;	  // Illegal instruction
      }
    }
  }
  else
  {
    cpuThrowPrivilegeViolationException();
    return;
  }
  cpuSetInstructionTime(4);
}

/// <summary>
/// moves.b Rn, ea / moves.b ea, Rn
/// </summary>
static void cpuMoveSB(ULO ea, UWO extension)
{
  if (cpuGetFlagSupervisor())
  {
    ULO da = (extension >> 15) & 1;
    ULO regno = (extension >> 12) & 7;
    if (extension & 0x0800) // From Rn to ea (in dfc)
    {
      memoryWriteByte((UBY)cpuGetReg(da, regno), ea);
    }
    else  // From ea to Rn (in sfc)
    {
      UBY data = memoryReadByte(ea);
      if (da == 0)
      {
	cpuSetDRegByte(regno, data);
      }
      else
      {
	cpuSetAReg(regno, (ULO)(LON)(BYT) data);
      }
    }
  }
  else
  {
    cpuThrowPrivilegeViolationException();
    return;
  }
  cpuSetInstructionTime(4);	  
}

/// <summary>
/// moves.w Rn, ea / moves.w ea, Rn
/// </summary>
static void cpuMoveSW(ULO ea, UWO extension)
{
  if (cpuGetFlagSupervisor())
  {
    ULO da = (extension >> 15) & 1;
    ULO regno = (extension >> 12) & 7;
    if (extension & 0x0800) // From Rn to ea (in dfc)
    {
      memoryWriteWord((UWO)cpuGetReg(da, regno), ea);
    }
    else  // From ea to Rn (in sfc)
    {
      UWO data = memoryReadWord(ea);
      if (da == 0)
      {
	cpuSetDRegWord(regno, data);
      }
      else
      {
	cpuSetAReg(regno, (ULO)(LON)(WOR) data);
      }
    }
  }
  else
  {
    cpuThrowPrivilegeViolationException();
    return;
  }
  cpuSetInstructionTime(4);	  
}

/// <summary>
/// moves.l Rn, ea / moves.l ea, Rn
/// </summary>
static void cpuMoveSL(ULO ea, UWO extension)
{
  if (cpuGetFlagSupervisor())
  {
    ULO da = (extension >> 15) & 1;
    ULO regno = (extension >> 12) & 7;
    if (extension & 0x0800) // From Rn to ea (in dfc)
    {
      memoryWriteLong(cpuGetReg(da, regno), ea);
    }
    else  // From ea to Rn (in sfc)
    {
      cpuSetDReg(regno, memoryReadLong(ea));
    }
  }
  else
  {
    cpuThrowPrivilegeViolationException();
    return;
  }
  cpuSetInstructionTime(4);	  
}

/// <summary>
/// Trapcc
/// </summary>
static void cpuTrapcc(ULO cc)
{
  if (cc)
  {
    cpuThrowTrapVException(); // TrapV and Trapcc share the exception vector
    return;
  }
  cpuSetInstructionTime(4);	  
}

/// <summary>
/// Trapcc.w #
/// </summary>
static void cpuTrapccW(ULO cc)
{
  UWO imm = cpuGetNextOpcode16();
  if (cc)
  {
    cpuThrowTrapVException(); // TrapV and Trapcc share the exception vector
    return;
  }
  cpuSetInstructionTime(4);	  
}

/// <summary>
/// trapcc.l #
/// </summary>
static void cpuTrapccL(ULO cc)
{
  ULO imm = cpuGetNextOpcode32();
  if (cc)
  {
    cpuThrowTrapVException(); // TrapV and Trapcc share the exception vector
    return;
  }
  cpuSetInstructionTime(4);	  
}

/// <summary>
/// cas.b Dc,Du, ea
/// </summary>
static void cpuCasB(ULO ea, UWO extension)
{
  UBY dst = memoryReadByte(ea);
  ULO cmp_regno = extension & 7;
  UBY res = dst - cpuGetDRegByte(cmp_regno);

  cpuSetFlagsCmp(cpuIsZeroB(res), cpuMsbB(res), cpuMsbB(dst), cpuMsbB(cpuGetDRegByte(cmp_regno)));

  if (cpuIsZeroB(res))
  {
    memoryWriteByte(cpuGetDRegByte((extension >> 6) & 7), ea);
  }
  else
  {
    cpuSetDRegByte(cmp_regno, dst);
  }
  cpuSetInstructionTime(4);
}

/// <summary>
/// cas.w Dc,Du, ea
/// </summary>
static void cpuCasW(ULO ea, UWO extension)
{
  UWO dst = memoryReadWord(ea);
  ULO cmp_regno = extension & 7;
  UWO res = dst - cpuGetDRegWord(cmp_regno);

  cpuSetFlagsCmp(cpuIsZeroW(res), cpuMsbW(res), cpuMsbW(dst), cpuMsbW(cpuGetDRegWord(cmp_regno)));

  if (cpuIsZeroW(res))
  {
    memoryWriteWord(cpuGetDRegWord((extension >> 6) & 7), ea);
  }
  else
  {
    cpuSetDRegWord(cmp_regno, dst);
  }
  cpuSetInstructionTime(4);
}

/// <summary>
/// cas.l Dc,Du, ea
/// </summary>
static void cpuCasL(ULO ea, UWO extension)
{
  ULO dst = memoryReadLong(ea);
  ULO cmp_regno = extension & 7;
  ULO res = dst - cpuGetDReg(cmp_regno);

  cpuSetFlagsCmp(cpuIsZeroL(res), cpuMsbL(res), cpuMsbL(dst), cpuMsbL(cpuGetDReg(cmp_regno)));

  if (cpuIsZeroL(res))
  {
    memoryWriteLong(cpuGetDReg((extension >> 6) & 7), ea);
  }
  else
  {
    cpuSetDReg(cmp_regno, dst);
  }
  cpuSetInstructionTime(4);
}

/// <summary>
/// cas2.w Dc1:Dc2,Du1:Du2,(Rn1):(Rn2)
/// </summary>
static void cpuCas2W()
{
  UWO extension1 = cpuGetNextOpcode16();
  UWO extension2 = cpuGetNextOpcode16();
  ULO ea1 = cpuGetReg(extension1 >> 15, (extension1 >> 12) & 7);
  ULO ea2 = cpuGetReg(extension2 >> 15, (extension2 >> 12) & 7);
  UWO dst1 = memoryReadWord(ea1);
  UWO dst2 = memoryReadWord(ea2);
  ULO cmp1_regno = extension1 & 7;
  ULO cmp2_regno = extension2 & 7;
  UWO res1 = dst1 - cpuGetDRegWord(cmp1_regno);
  UWO res2 = dst2 - cpuGetDRegWord(cmp2_regno);

  if (cpuIsZeroW(res1))
  {
    cpuSetFlagsCmp(cpuIsZeroW(res2), cpuMsbW(res2), cpuMsbW(dst2), cpuMsbW(cpuGetDRegWord(cmp2_regno)));
  }
  else
  {
    cpuSetFlagsCmp(cpuIsZeroW(res1), cpuMsbW(res1), cpuMsbW(dst1), cpuMsbW(cpuGetDRegWord(cmp1_regno)));
  }

  if (cpuIsZeroW(res1) && cpuIsZeroW(res2))
  {
    memoryWriteWord(cpuGetDRegWord((extension1 >> 6) & 7), ea1);
    memoryWriteWord(cpuGetDRegWord((extension2 >> 6) & 7), ea2);
  }
  else
  {
    cpuSetDRegWord(cmp1_regno, dst1);
    cpuSetDRegWord(cmp2_regno, dst2);
  }
  cpuSetInstructionTime(4);
}

/// <summary>
/// cas2.l Dc1:Dc2,Du1:Du2,(Rn1):(Rn2)
/// </summary>
static void cpuCas2L()
{
  UWO extension1 = cpuGetNextOpcode16();
  UWO extension2 = cpuGetNextOpcode16();
  ULO ea1 = cpuGetReg(extension1 >> 15, (extension1 >> 12) & 7);
  ULO ea2 = cpuGetReg(extension2 >> 15, (extension2 >> 12) & 7);
  ULO dst1 = memoryReadLong(ea1);
  ULO dst2 = memoryReadLong(ea2);
  ULO cmp1_regno = extension1 & 7;
  ULO cmp2_regno = extension2 & 7;
  ULO res1 = dst1 - cpuGetDReg(cmp1_regno);
  ULO res2 = dst2 - cpuGetDReg(cmp2_regno);

  if (cpuIsZeroL(res1))
  {
    cpuSetFlagsCmp(cpuIsZeroL(res2), cpuMsbL(res2), cpuMsbL(dst2), cpuMsbL(cpuGetDReg(cmp2_regno)));
  }
  else
  {
    cpuSetFlagsCmp(cpuIsZeroL(res1), cpuMsbL(res1), cpuMsbL(dst1), cpuMsbL(cpuGetDReg(cmp1_regno)));
  }

  if (cpuIsZeroL(res1) && cpuIsZeroL(res2))
  {
    memoryWriteLong(cpuGetDReg((extension1 >> 6) & 7), ea1);
    memoryWriteLong(cpuGetDReg((extension2 >> 6) & 7), ea2);
  }
  else
  {
    cpuSetDReg(cmp1_regno, dst1);
    cpuSetDReg(cmp2_regno, dst2);
  }
  cpuSetInstructionTime(4);
}

/// <summary>
/// Common code for chk2 ea, Rn / cmp2 ea, Rn
/// </summary>
static void cpuChkCmp(ULO lb, ULO ub, ULO val, BOOLE is_chk2)
{
  BOOLE z = (val == lb || val == ub);
  BOOLE c = ((lb <= ub) && (val < lb || val > ub)) || ((lb > ub) && (val < lb) && (val > ub)); 
  cpuSetFlagZ(z);
  cpuSetFlagC(c);
  cpuSetInstructionTime(4);
  if (is_chk2 && c)
  {
    cpuThrowChkException();
  }
}

/// <summary>
/// chk2.b ea, Rn / cmp2.b ea, Rn
/// </summary>
static void cpuChkCmp2B(ULO ea, UWO extension)
{
  ULO da = (ULO) (extension >> 15);
  ULO rn = (ULO) (extension >> 12) & 7;
  BOOLE is_chk2 = (extension & 0x0800);
  if (da == 1)
  {
    cpuChkCmp((ULO)(LON)(BYT)memoryReadByte(ea), (ULO)(LON)(BYT)memoryReadByte(ea + 1), cpuGetAReg(rn), is_chk2);
  }
  else
  {
    cpuChkCmp((ULO)memoryReadByte(ea), (ULO)memoryReadByte(ea + 1), (ULO)(UBY)cpuGetDReg(rn), is_chk2);
  }
}

/// <summary>
/// chk2.w ea, Rn / cmp2.w ea, Rn
/// </summary>
static void cpuChkCmp2W(ULO ea, UWO extension)
{
  ULO da = (ULO) (extension >> 15);
  ULO rn = (ULO) (extension >> 12) & 7;
  BOOLE is_chk2 = (extension & 0x0800);
  if (da == 1)
  {
    cpuChkCmp((ULO)(LON)(WOR)memoryReadWord(ea), (ULO)(LON)(WOR)memoryReadWord(ea + 1), cpuGetAReg(rn), is_chk2);
  }
  else
  {
    cpuChkCmp((ULO)memoryReadWord(ea), (ULO)memoryReadWord(ea + 2), (ULO)(UWO)cpuGetDReg(rn), is_chk2);
  }
}

/// <summary>
/// chk2.l ea, Rn / cmp2.l ea, Rn
/// </summary>
static void cpuChkCmp2L(ULO ea, UWO extension)
{
  ULO da = (ULO) (extension >> 15);
  ULO rn = (ULO) (extension >> 12) & 7;
  BOOLE is_chk2 = (extension & 0x0800);
  cpuChkCmp(memoryReadLong(ea), memoryReadLong(ea + 4), cpuGetReg(da, rn), is_chk2);
}

/// <summary>
/// callm
/// Since this is a coprocessor instruction, this is NOP.
/// This will likely fail, but anything we do here will be wrong anyhow.
/// </summary>
static void cpuCallm(ULO ea, UWO extension)
{
  cpuSetInstructionTime(4);
}

/// <summary>
/// rtm
/// Since this is a coprocessor instruction, this is NOP.
/// This will likely fail, but anything we do here will be wrong anyhow.
/// </summary>
static void cpuRtm(ULO da, ULO regno)
{
  cpuSetInstructionTime(4);
}

/// <summary>
/// 68030 version only.
///
/// Extension word: 001xxx00xxxxxxxx
/// pflusha
/// pflush fc, mask
/// pflush fc, mask, ea
///
/// Extension word: 001000x0000xxxxx
/// ploadr fc, ea
/// ploadw fc, ea
///
/// Extension word: 010xxxxx00000000 (SRp, CRP, TC)
/// Extension word: 011000x000000000 (MMU status register)
/// Extension word: 000xxxxx00000000 (TT)
/// pmove mrn, ea
/// pmove ea, mrn
/// pmovefd ea, mrn
///
/// Extension word: 100xxxxxxxxxxxxx
/// ptestr fc, ea, #level
/// ptestr fc, ea, #level, An
/// ptestw fc, ea, #level
/// ptestw fc, ea, #level, An
///
/// Since this is a coprocessor instruction, this is NOP.
/// </summary>
static void cpuPflush030(ULO ea, UWO extension)
{
  if (cpuGetFlagSupervisor())
  {
    if ((extension & 0xfde0) == 0x2000)
    {
      // ploadr, ploadw
    }
    else if ((extension & 0xe300) == 0x2000)
    {
      // pflusha, pflush
      ULO mode = (extension >> 10) & 7;
      ULO mask = (extension >> 5) & 7;
      ULO fc = extension & 0x1f;
    }
  }
  else
  {
    cpuThrowPrivilegeViolationException();
    return;
  }
  cpuSetInstructionTime(4);
}

/// <summary>
/// pflusha
/// pflush fc, mask
/// pflush fc, mask, ea
///
/// 68040 version only.
///
/// Since this is a coprocessor instruction, this is NOP.
/// </summary>
static void cpuPflush040(ULO opmode, ULO regno)
{
  if (cpuGetFlagSupervisor())
  {
    if (cpuGetModelMajor() != 2)	// This is NOP on 68EC040
    {
      switch (opmode)
      {
	case 0: //PFLUSHN (An)
	  break;
	case 1: //PFLUSH (An)
	  break;
	case 2: //PFLUSHAN
	  break;
	case 3: //PFLUSHA
	  break;
      }
    }
  }
  else
  {
    cpuThrowPrivilegeViolationException();
    return;
  }
  cpuSetInstructionTime(4);
}

/// <summary>
/// ptestr (An)
/// ptestw (An)
///
/// 68040 version only.
///
/// Since this is a coprocessor instruction, this is NOP.
/// </summary>
static void cpuPtest040(ULO rw, ULO regno)
{
  if (cpuGetFlagSupervisor())
  {
    if (cpuGetModelMajor() != 2)	// This is NOP on 68EC040
    {
      if (rw == 0)
      {
	// ptestr
      }
      else 
      {
	// ptestw
      }
    }
  } 
  else
  {
    cpuThrowPrivilegeViolationException();
    return;
  }
  cpuSetInstructionTime(4);
}

#include "CpuModule_Decl.h"
#include "CpuModule_Data.h"
#include "CpuModule_Profile.h"
#include "CpuModule_Code.h"

ULO cpuExecuteInstruction(void)
{
  if (cpuGetRaiseInterrupt())
  {
    cpuSetUpInterrupt();
    cpuCheckPendingInterrupts();
    return 44;
  }
  else
  {
    UWO oldSr = cpuGetSR();
    UWO opcode;

    cpuCallInstructionLoggingFunc();

    cpuSetOriginalPC(cpuGetPC()); // Store pc and opcode for exception logging
    opcode = cpuGetNextOpcode16();
    cpuSetCurrentOpcode(opcode);

    cpuSetInstructionTime(0);

    if (cpu_opcode_model_mask[opcode] & cpuGetModelMask())
    {
      cpu_opcode_data[opcode].instruction_func(cpu_opcode_data[opcode].data);
    }
    else
    {
      cpuIllegalInstruction(cpu_opcode_data[opcode].data);
    }
    if (oldSr & 0xc000)
    {
      // This instruction was traced
      ULO cycles = cpuGetInstructionTime();
      cpuThrowTraceException();
      cpuSetInstructionTime(cpuGetInstructionTime() + cycles);
    }
    return cpuGetInstructionTime();
  }
}
