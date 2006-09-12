#
# Aften Library Makefile
#
include ../config.mak

CFLAGS=$(OPTFLAGS) -I. -I.. -I$(SRC_PATH)/libaften \
       -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_ISOC9X_SOURCE

OBJS= a52enc.o bitalloc.o bitio.o crc.o mdct.o filter.o util.o exponent.o \
	window.o \


HEADERS = aften.h

NAME=aften

SRC_DIR = $(SRC_PATH)/lib$(NAME)
VPATH = $(SRC_DIR)

SRCS := $(OBJS:.o=.c)
OBJS := $(OBJS)
STATIC_OBJS := $(OBJS) $(STATIC_OBJS)

all: $(EXTRADEPS) $(LIB)

$(LIB): $(STATIC_OBJS)
	rm -f $@
	$(AR) rc $@ $^ $(EXTRAOBJS)
	$(RANLIB) $@

%.o: %.c
	$(CC) $(CFLAGS) $(LIBOBJFLAGS) -c -o $@ $<

%.o: %.S
	$(CC) $(CFLAGS) $(LIBOBJFLAGS) -c -o $@ $<

# BeOS: remove -Wall to get rid of all the "multibyte constant" warnings
%.o: %.cpp
	g++ $(subst -Wall,,$(CFLAGS)) -c -o $@ $<

depend: $(SRCS)
	$(CC) -MM $(CFLAGS) $^ 1>.depend

dep:	depend

clean::
	rm -f *.o *.d *~ *.a *.lib *.so *.so.* *.dylib *.dll \
	      *.lib *.def *.dll.a *.exp

distclean: clean
	rm -f .depend

INSTLIBTARGETS += install-lib-static

install: install-libs install-headers

install-libs: $(INSTLIBTARGETS)

install-lib-static: $(LIB)
	install -d "$(libdir)"
	install -m 644 $(LIB) "$(libdir)"

install-headers:
	install -d "$(incdir)"
	install -d "$(libdir)/pkgconfig"
	install -m 644 $(addprefix "$(SRC_DIR)"/,$(HEADERS)) "$(incdir)"

uninstall: uninstall-libs uninstall-headers

uninstall-libs:
	-rm -f $(libdir)/$(LIB)

uninstall-headers:
	rm -f $(addprefix $(incdir)/,$(HEADERS))

../config.mak:
	touch ../config.mak

#
# include dependency files if they exist
#
ifneq ($(wildcard .depend),)
include .depend
endif
