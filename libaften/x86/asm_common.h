#ifndef ASM_COMMON_H
#define ASM_COMMON_H

#define _pushf		_s(pushf)
#define _popf		_s(popf)
#define _push(x)	_s(push x)
#define _pop(x) 	_s(pop x)
#define _jz(x)		_s(jz x)
#define _cpuid		_s(cpuid)

#endif /* ASM_COMMON_H */
