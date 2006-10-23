#
# Aften Program Makefile
#
include ../config.mak

VPATH=$(SRC_PATH)

CFLAGS=$(OPTFLAGS) -I. -I.. -I../libaften -I$(SRC_PATH)/libaften \
	-D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_ISOC9X_SOURCE
LDFLAGS+= -g

PROGS_G=aften_g$(EXESUF)

OBJS = aften.o wav.o
SRCS = $(OBJS:.o=.c)
AFTLIBDIRS = -L../libaften
AFTLIBS = -laften$(BUILDSUF)

all: $(PROGS_G)

aften_g$(EXESUF): aften.o wav.o libaften/libaften.a
	$(CC) $(AFTLIBDIRS) $(LDFLAGS) -o $@ aften.o wav.o $(AFTLIBS) $(EXTRALIBS)
	cp -p aften_g$(EXESUF) aften$(EXESUF)
	$(STRIP) aften$(EXESUF)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

install: install-progs

install-progs: $(PROGS)
	install -d "$(bindir)"
	install -c -m 755 $(PROGS) "$(bindir)"

uninstall: uninstall-progs

uninstall-progs:
	rm -f $(addprefix $(bindir)/, $(PROGS))

dep:	depend

depend: .depend

.depend: $(SRCS)
	$(CC) -MM $(CFLAGS) $^ 1>.depend

clean:
	rm -f *.o *~ $(PROGS) $(PROGS_G)

distclean: clean
	rm -f .depend ../config.*

../config.mak:
	touch ../config.mak

ifneq ($(wildcard .depend),)
include .depend
endif
