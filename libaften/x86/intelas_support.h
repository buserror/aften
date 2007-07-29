/*********************************************************************
 * Copyright (C) 2007 by Prakash Punnoor                             *
 * prakash@punnoor.de                                                *
 *                                                                   *
 * This library is free software; you can redistribute it and/or     *
 * modify it under the terms of the GNU Library General Public       *
 * License as published by the Free Software Foundation;             *
 * version 2 of the License                                          *
 *                                                                   *
 * This library is distributed in the hope that it will be useful,   *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of    *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU *
 * Library General Public License for more details.                  *
 *                                                                   *
 * You should have received a copy of the GNU Library General Public *
 * License along with this library; if not, write to the             *
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,      *
 * Boston, MA  02110-1301, USA.                                      *
 *********************************************************************/

#ifndef INTELAS_SUPPORT_H
#define INTELAS_SUPPORT_H

#define _s(x)		x
#define __s(x,y)	x, y

#define _mov(x, y)	__s(mov y, x)
#define _xor(x, y)	__s(xor y, x)
#define _test(x, y)	__s(test y, x)

#define _(x)		x
#define _l(x)		x##:

#define _eax		EAX
#define _ebx		EBX
#define _ecx		ECX
#define _edx		EDX

#include "asm_common.h"

#if defined(_M_X64) || defined (__LP64__)
#define _a			RAX
#define _b			RBX
#define _c			RCX
#define _d			RDX
#else
#define _a			_eax
#define _b			_ebx
#define _c			_ecx
#define _d			_edx
#endif

#endif /* INTELAS_SUPPORT_H */
