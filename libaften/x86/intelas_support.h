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
