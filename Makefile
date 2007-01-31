#
# Aften Main Makefile
#
# DEPRECATED - use CMake build system
#
include config.mak

VPATH=$(SRC_PATH)

CFLAGS=$(OPTFLAGS) -I. -I$(SRC_PATH) -I$(SRC_PATH)/libaften \
       -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_ISOC9X_SOURCE

DEP_LIBS=libaften/$(LIBPREF)aften$(LIBSUF)

AFTLIBDIRS = -L./libaften
AFTLIBS = -laften$(BUILDSUF)

all: lib progs utils

lib:
	$(MAKE) -C libaften all

progs: lib
	$(MAKE) -C aften all

utils: lib
	$(MAKE) -C util all

.PHONY: install

install: install-progs install-libs install-headers

install-progs: progs
	$(MAKE) -C aften install
	$(MAKE) -C util install

install-libs:
	$(MAKE) -C libaften install-libs

install-headers:
	$(MAKE) -C libaften install-headers

uninstall: uninstall-progs uninstall-libs uninstall-headers

uninstall-progs:
	$(MAKE) -C aften uninstall-progs
	$(MAKE) -C util uninstall-progs

uninstall-libs:
	$(MAKE) -C libaften uninstall-libs

uninstall-headers:
	$(MAKE) -C libaften uninstall-headers
	-rmdir "$(incdir)"

dep:	depend

depend:
	$(MAKE) -C libaften depend
	$(MAKE) -C aften    depend
	$(MAKE) -C util     depend

.libs: lib
	@test -f .libs || touch .libs
	@for i in $(DEP_LIBS) ; do if test $$i -nt .libs ; then touch .libs; fi ; done

clean:
	$(MAKE) -C libaften clean
	$(MAKE) -C aften    clean
	$(MAKE) -C util     clean
	rm -f *.o *.d *~

distclean: clean
	$(MAKE) -C libaften distclean
	$(MAKE) -C aften    distclean
	$(MAKE) -C util     distclean
	rm -f .depend config.*

# tar release (use 'make -k tar' on a checkouted tree)
FILE=aften-$(shell grep "\#define AFTEN_VERSION " libaften/aften.h | \
                    cut -d " " -f 3 )

tar: distclean
	rm -rf /tmp/$(FILE)
	cp -r . /tmp/$(FILE)
	(tar --exclude config.mak --exclude .svn -C /tmp -jcvf $(FILE).tar.bz2 $(FILE) )
	rm -rf /tmp/$(FILE)

config.mak:
	touch config.mak

.PHONY: lib

ifneq ($(wildcard .depend),)
include .depend
endif
