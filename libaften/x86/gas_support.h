/********************************************************************************
 * Copyright (C) 2007 by Prakash Punnoor                                        *
 * prakash@punnoor.de                                                           *
 *                                                                              *
 * This library is free software; you can redistribute it and/or                *
 * modify it under the terms of the GNU Lesser General Public                   *
 * License as published by the Free Software Foundation; either                 *
 * version 2 of the License                                                     *
 *                                                                              *
 * This library is distributed in the hope that it will be useful,              *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of               *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU            *
 * Lesser General Public License for more details.                              *
 *                                                                              *
 * You should have received a copy of the GNU Lesser General Public             *
 * License along with this library; if not, write to the Free Software          *
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA *
 ********************************************************************************/
#ifndef GAS_SUPPORT_H
#define GAS_SUPPORT_H

#define _s(x)		#x"\n\t"
#define __s(x,y)	#x","#y"\n\t"

#define _mov(x, y)	__s(mov x, y)
#define _xor(x, y)	__s(xor x, y)
#define _test(x, y)	__s(test x, y)

#define _(x)		$##x
#define _l(x)		#x":\n\t"

#define _eax		%%eax
#define _ebx		%%ebx
#define _ecx		%%ecx
#define _edx		%%edx

#include "asm_common.h"

#ifdef __LP64__
#define _a			%%rax
#define _b			%%rbx
#define _c			%%rcx
#define _d			%%rdx
#else
#define _a			_eax
#define _b			_ebx
#define _c			_ecx
#define _d			_edx
#endif

#endif /* GAS_SUPPORT_H */
