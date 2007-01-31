#
# Aften Util Makefile
#
# DEPRECATED - use CMake build system
#
include ../config.mak

VPATH=$(SRC_PATH)

CFLAGS=$(OPTFLAGS) -I. -I.. -I../libaften -I$(SRC_PATH)/libaften \
       -I../aften -I$(SRC_PATH)/aften \
       -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_ISOC9X_SOURCE

PROGS=wavinfo$(EXESUF) wavrms$(EXESUF) wavfilter$(EXESUF)

OBJS = wavinfo.o wavrms.o wavfilter.o ../aften/wav.o ../libaften/filter.o
SRCS = $(OBJS:.o=.c)

all: $(PROGS)

wavinfo$(EXESUF): wavinfo.o ../aften/wav.o
	$(CC) $(LDFLAGS) -o $@ wavinfo.o ../aften/wav.o $(EXTRALIBS)

wavrms$(EXESUF): wavrms.o ../aften/wav.o
	$(CC) $(LDFLAGS) -o $@ wavrms.o ../aften/wav.o $(EXTRALIBS)

wavfilter$(EXESUF): wavfilter.o ../aften/wav.o ../libaften/filter.o
	$(CC) $(LDFLAGS) -o $@ wavfilter.o ../aften/wav.o ../libaften/filter.o $(EXTRALIBS)

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
	rm -f *.o *~ $(PROGS)

distclean: clean
	rm -f .depend ../config.*

../config.mak:
	touch ../config.mak

ifneq ($(wildcard .depend),)
include .depend
endif
