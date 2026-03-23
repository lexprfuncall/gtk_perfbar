# Targets for solaris and linux. feel free to add others
# Thanks to Michael Rumpf for helping to simplify this

CFLAGS := $(shell pkg-config --cflags gtk4)
LDFLAGS := $(shell pkg-config --libs gtk4)

linux_perfbar: perfbar.c Makefile
	gcc -O -DLINUX $(CFLAGS) $(LDFLAGS) -o linux_perfbar perfbar.c


solaris_sparc_perfbar: perfbar.c Makefile
	gcc -O -DSOLARIS $(CFLAGS) $(LDFLAGS) -lkstat -o solaris_sparc_perfbar perfbar.c 
solaris_x86_perfbar: perfbar.c Makefile
	gcc -O -DSOLARIS $(CFLAGS) $(LDFLAGS) -lkstat -o solaris_x86_perfbar perfbar.c 


