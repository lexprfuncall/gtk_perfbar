# Targets for solaris and linux. feel free to add others
# Thanks to Michael Rumpf for helping to simplify this

CFLAGS := $(shell pkg-config --cflags gtk4)
LDFLAGS := $(shell pkg-config --libs gtk4)

linux_perfbar: perfbar.c Makefile
	gcc -O -DLINUX $(CFLAGS) -o linux_perfbar perfbar.c $(LDFLAGS)


solaris_sparc_perfbar: perfbar.c Makefile
	gcc -O -DSOLARIS $(CFLAGS) -o solaris_sparc_perfbar perfbar.c $(LDFLAGS) -lkstat
solaris_x86_perfbar: perfbar.c Makefile
	gcc -O -DSOLARIS $(CFLAGS) -o solaris_x86_perfbar perfbar.c $(LDFLAGS) -lkstat


