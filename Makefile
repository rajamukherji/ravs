.PHONY: clean all install

PLATFORM = $(shell uname)
MACHINE = $(shell uname -m)

all: libravs.a

*.o: *.h

CFLAGS += -std=gnu99 -fstrict-aliasing -Wstrict-aliasing -Wall \
	-I. -DGC_THREADS -D_GNU_SOURCE -D$(PLATFORM)

ifdef DEBUG
	CFLAGS += -g -DGC_DEBUG -DDEBUG
else
	CFLAGS += -O3 -g
endif

common_objects = \
	ravs.o \
	bsdiff.o \
	bspatch.o

platform_objects =

ifeq ($(MACHINE), i686)
	CFLAGS += -fno-pic
endif

ifeq ($(PLATFORM), Linux)
	platform_objects += 
endif

ifeq ($(PLATFORM), FreeBSD)
	platform_objects += 
	CFLAGS += -I/usr/local/include
endif

ifeq ($(PLATFORM), Darwin)
	platform_objects += 
endif

$(common_objects):

radb/libradb.a:
	$(MAKE) -C radb

libravs.a: radb/libradb.a $(common_objects) $(platform_objects)
	ar rcs $@ radb/libradb.a $(common_objects) $(platform_objects)

clean:
	$(MAKE) -C radb clean
	rm -f *.o
	rm -f libravs.a

PREFIX = /usr
install_include = $(DESTDIR)$(PREFIX)/include/ravs
install_lib = $(DESTDIR)$(PREFIX)/lib

install_h = \
	$(install_include)/ravs.h \
	$(install_include)/config.h

install_a = $(install_lib)/libravs.a

$(install_h): $(install_include)/%: %
	mkdir -p $(install_include)
	cp $< $@

$(install_a): $(install_lib)/%: %
	mkdir -p $(install_lib)
	cp $< $@

install: $(install_h) $(install_a)
