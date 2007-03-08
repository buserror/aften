;********************************************************************************
;* Copyright (C) 2005-2007 by Prakash Punnoor                                   *
;* prakash@punnoor.de                                                           *
;*                                                                              *
;* This library is free software; you can redistribute it and/or                *
;* modify it under the terms of the GNU Lesser General Public                   *
;* License as published by the Free Software Foundation; either                 *
;* version 2 of the License                                                     *
;*                                                                              *
;* This library is distributed in the hope that it will be useful,              *
;* but WITHOUT ANY WARRANTY; without even the implied warranty of               *
;* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU            *
;* Lesser General Public License for more details.                              *
;*                                                                              *
;* You should have received a copy of the GNU Lesser General Public             *
;* License along with this library; if not, write to the Free Software          *
;* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA *
;********************************************************************************

; int cpu_caps_detect_x86(uint* caps1, uint* caps2, uint* caps3);
; derived from loki_cpuinfo.c, 1997-98 by H. Dietz and R. Fisher
; using infos from sandpile.org

; returns 0 if no CPUID available

%ifdef HAVE_NASM_VISIBILITY
global _cpu_caps_detect_x86:function hidden
global cpu_caps_detect_x86:function hidden
%else
global _cpu_caps_detect_x86
global cpu_caps_detect_x86
%endif

segment .text
_cpu_caps_detect_x86:
cpu_caps_detect_x86:

pushf
pop eax
mov ecx, eax

xor eax, 0x200000
push eax
popf

pushf
pop eax

xor ecx, eax
xor eax, eax
test ecx, 0x200000
jz .Return

; standard CPUID
push ebx
mov eax, 1
cpuid
mov eax, [esp + 8]	;caps1 - MMX, SSE, SSE2
mov [eax], edx
mov eax, [esp + 12]	;caps2 - SSE3
mov [eax], ecx

; extended CPUID
mov	eax, 0x80000001
cpuid
mov eax, [esp + 16]	;caps3 - 3DNOW!, 3DNOW!EXT, CYRIX-MMXEXT, AMD-MMX-SSE
mov [eax], edx
pop ebx

; End
.Return
ret

; prevent executable stack
%ifidn __OUTPUT_FORMAT__,elf
section .note.GNU-stack noalloc noexec nowrite progbits
%endif

%ifidn __YASM_OBJFMT__,elf
section ".note.GNU-stack" noalloc noexec nowrite progbits
%endif

%ifidn __YASM_OBJFMT__,elf32
section ".note.GNU-stack" noalloc noexec nowrite progbits
%endif
