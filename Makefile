
LIBTOOL ?= libtool
RANLIB ?= ranlib
BUILD := build-${shell $(CC) -dumpmachine}

OBJ := ${BUILD}/obj
LIB := ${BUILD}/lib
BIN := ${BUILD}/bin

ARCH 		= ${shell $(CC) -dumpmachine | sed -e 's/^x/i/' -e 's/\(.\).*/\1/'}

CPPFLAGS += -I. -Ipcm -Ilibaften
CPPFLAGS += -DHAVE_BYTESWAP_H
CPPFLAGS += -DHAVE_INTTYPES_H
CPPFLAGS += -DHAVE_POSIX_THREADS_H
CPPFLAGS += -DMAX_NUM_THREADS=32

ifeq (${ARCH},i)
CPPFLAGS += -DHAVE_MMX -DUSE_MMX -DHAVE_SSE -DUSE_SSE \
			-DHAVE_SSE2 -DUSE_SSE2 \
			-DHAVE_SSE3 -DUSE_SSE3 \
			-DHAVE_CPU_CAPS_DETECTION
CFLAGS		+= -mtune=core2 -mmmx -msse2 -msse3
endif

CFLAGS	+= -fPIC -O2 -g
LDFLAGS :=	-L${LIB} -Wl,-rpath-link,${LIB}
LDFLAGS +=	-laften_pcm -laften -lm

all : libaften_pcm libaften
all : ${BIN}/aften

${LIB} ${OBJ} ${BIN}:
	mkdir -p ${LIB} ${OBJ} ${BIN}

VPATH = pcm
VPATH += libaften
VPATH += aften

libaften_pcm : ${LIB} ${OBJ}
libaften_pcm : ${LIB}/libaften_pcm.a ${LIB}/libaften_pcm.so

libaften : ${LIB} ${OBJ}
libaften : ${LIB}/libaften.a ${LIB}/libaften.so

libpcm		:= ${wildcard pcm/*.c}
libpcm_o 	:= ${patsubst pcm/%.c, ${OBJ}/%.o, ${libpcm}}

${LIB}/libaften_pcm.a : ${libpcm_o}
${LIB}/libaften_pcm.so.1 : ${libpcm_o}

libaften	:= ${wildcard libaften/*.c}
libaften_o	:= ${patsubst libaften/%.c, ${OBJ}/%.o, ${libaften}}
ifeq (${ARCH},i)
VPATH += libaften/x86
libaften_i	:= ${wildcard libaften/x86/*.c}
libaften_io	:= ${patsubst libaften/x86/%.c, ${OBJ}/%.o, ${libaften_i}}
libaften_o	+= ${libaften_io}
endif

${LIB}/libaften.a : ${libaften_o}
${LIB}/libaften.so.1 : ${libaften_o}

${BIN}/aften : CPPFLAGS += -Iaften
${BIN}/aften : ${OBJ}/aften.o
${BIN}/aften : ${OBJ}/opts.o
${BIN}/aften : ${LIB}/libaften.so ${LIB}/libaften_pcm.so

${BIN}/% : ${BIN}
	$(CC) -MMD $(CPPFLAGS) $(CPPFLAGS_EXTRAS) \
		$(CFLAGS) $(CFLAGS_EXTRAS) \
		-o $@ \
		-Wl,--start-group ${filter %.o %.a,$^} -Wl,--end-group \
		$(LDFLAGS) $(LDFLAGS_EXTRAS) $(LIBS)

${LIB}/%.a :
	$(AR) cru $@ $^ && $(RANLIB) $@
${LIB}/%.so : ${LIB}/%.so.1
	ln -sf ${shell basename ${filter %.1, $^}} $@
${LIB}/%.so.1 :
	$(CC) -shared -Wl,-soname,${shell basename $@} -o $@ \
		-Wl,--start-group $^ -Wl,--end-group
${OBJ}/%.o : %.c
	$(CC) -MMD $(CPPFLAGS) $(CPPFLAGS_EXTRAS) \
		$(CFLAGS) $(CFLAGS_EXTRAS) \
		-c -o $@ $^

.PHONY: install clean all

install:
	mkdir -p ${DESTDIR}/lib
	cp -a ${LIB}/* ${DESTDIR}/lib/
	mkdir -p ${DESTDIR}/include/aften
	cp pcm/*.h libaften/*.h ${DESTDIR}/include/aften

clean:
	rm -rf ${BUILD}

-include ${wildcard ${OBJ}/*.d}
