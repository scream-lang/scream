# Makefile for building Hello
# See ../doc/readme.html for installation and customization instructions.

# == CHANGE THE SETTINGS BELOW TO SUIT YOUR ENVIRONMENT =======================

# Your platform. See PLATS for possible values.
PLAT= mingw

CXX= g++ -std=c++17 -O3 -flto
CXXFLAGS= -Wall -Wextra -DHELLO_COMPAT_5_3 $(SYSCFLAGS) $(MYCFLAGS)
LDFLAGS= $(SYSLDFLAGS) $(MYLDFLAGS)
LIBS= -lm $(SYSLIBS) $(MYLIBS)

# Specifying DEBUG=0 (or any value) will remove any optimizations.
ifdef DEBUG
override CXX= g++ -std=c++17
endif

AR= ar rcu
RANLIB= ranlib
RM= rm -f
UNAME= uname

SYSCFLAGS=
SYSLDFLAGS=
SYSLIBS=

MYCFLAGS=
MYLDFLAGS=
MYLIBS=
MYOBJS=

# Special flags for compiler modules; -Os reduces code size.
CMCFLAGS=

# == END OF USER SETTINGS -- NO NEED TO CHANGE ANYTHING BELOW THIS LINE =======

PLATS= guess aix bsd freebsd generic linux linux-readline macosx mingw posix solaris

HELLO_A=	libhello.a
CORE_O=	lapi.o lcode.o lctype.o ldebug.o ldo.o ldump.o lfunc.o lgc.o llex.o lmem.o lobject.o lopcodes.o lparser.o lstate.o lstring.o ltable.o ltm.o lundump.o lvm.o lzio.o
LIB_O=	lauxlib.o lbaselib.o lcorolib.o ldblib.o liolib.o lmathlib.o loadlib.o loslib.o lstrlib.o lcryptolib.o ltablib.o lutf8lib.o linit.o
BASE_O= $(CORE_O) $(LIB_O) $(MYOBJS)

HELLO_T=	hello
HELLO_O=	hello.o

HELLOC_T=	helloc
HELLOC_O=	helloc.o

ALL_O= $(BASE_O) $(HELLO_O) $(HELLOC_O)
ALL_T= $(HELLO_A) $(HELLO_T) $(HELLOC_T)
ALL_A= $(HELLO_A)

# Targets start here.
default: $(PLAT)

all:	$(ALL_T)

o:	$(ALL_O)

a:	$(ALL_A)

$(HELLO_A): $(BASE_O)
	$(AR) $@ $(BASE_O)
	$(RANLIB) $@

$(HELLO_T): $(HELLO_O) $(HELLO_A)
	$(CXX) -o $@ $(LDFLAGS) $(HELLO_O) $(HELLO_A) $(LIBS)

$(HELLOC_T): $(HELLOC_O) $(HELLO_A)
	$(CXX) -o $@ $(LDFLAGS) $(HELLOC_O) $(HELLO_A) $(LIBS)

test:
	./$(HELLO_T) -v

clean:
	$(RM) $(ALL_T) $(ALL_O)

depend:
	@$(CXX) $(CXXFLAGS) -MM l*.cpp

echo:
	@echo "PLAT= $(PLAT)"
	@echo "CXX= $(CXX)"
	@echo "CFLAGS= $(CXXFLAGS)"
	@echo "LDFLAGS= $(LDFLAGS)"
	@echo "LIBS= $(LIBS)"
	@echo "AR= $(AR)"
	@echo "RANLIB= $(RANLIB)"
	@echo "RM= $(RM)"
	@echo "UNAME= $(UNAME)"

# Convenience targets for popular platforms.
ALL= all

help:
	@echo "Do 'make PLATFORM' where PLATFORM is one of these:"
	@echo "   $(PLATS)"
	@echo "See doc/readme.html for complete instructions."

guess:
	@echo Guessing `$(UNAME)`
	@$(MAKE) `$(UNAME)`

AIX aix:
	$(MAKE) $(ALL) CXX="xlc" CFLAGS="-O2 -DHELLO_USE_POSIX -DHELLO_USE_DLOPEN" SYSLIBS="-ldl" SYSLDFLAGS="-brtl -bexpall"

bsd:
	$(MAKE) $(ALL) SYSCFLAGS="-DHELLO_USE_POSIX -DHELLO_USE_DLOPEN" SYSLIBS="-Wl,-E"

FreeBSD NetBSD OpenBSD freebsd:
	$(MAKE) $(ALL) SYSCFLAGS="-DHELLO_USE_LINUX -DHELLO_USE_READLINE -I/usr/include/edit" SYSLIBS="-ledit" CC="cc"

generic: $(ALL)

Linux linux:	linux-noreadline

linux-noreadline:
	$(MAKE) $(ALL) SYSCFLAGS="-DHELLO_USE_LINUX" SYSLIBS="-ldl"

linux-readline:
	$(MAKE) $(ALL) SYSCFLAGS="-DHELLO_USE_LINUX -DHELLO_USE_READLINE" SYSLIBS="-ldl -lreadline"

Darwin macos macosx:
	$(MAKE) $(ALL) SYSCFLAGS="-DHELLO_USE_MACOSX -DHELLO_USE_READLINE" SYSLIBS="-lreadline"

mingw:
	$(MAKE) "HELLO_A=libhello.dll" "HELLO_T=hello.exe" \
	"AR=$(CXX) -shared -o" "RANLIB=strip --strip-unneeded" \
	"SYSCFLAGS=-DHELLO_BUILD_AS_DLL" "SYSLIBS=" "SYSLDFLAGS=-s" hello.exe
	$(MAKE) "HELLOC_T=helloc.exe" helloc.exe

posix:
	$(MAKE) $(ALL) SYSCFLAGS="-DHELLO_USE_POSIX"

SunOS solaris:
	$(MAKE) $(ALL) SYSCFLAGS="-DHELLO_USE_POSIX -DHELLO_USE_DLOPEN -D_REENTRANT" SYSLIBS="-ldl"

# Targets that do not create files (not all makes understand .PHONY).
.PHONY: all $(PLATS) help test clean default o a depend echo

# Compiler modules may use special flags.
llex.o:
	$(CXX) $(CXXFLAGS) $(CMCFLAGS) -c llex.cpp

lparser.o:
	$(CXX) $(CXXFLAGS) $(CMCFLAGS) -c lparser.cpp

lcode.o:
	$(CXX) $(CXXFLAGS) $(CMCFLAGS) -c lcode.cpp

# DO NOT DELETE

lapi.o: lapi.cpp lprefix.h hello.h helloconf.h lapi.h llimits.h lstate.h \
 lobject.h ltm.h lzio.h lmem.h ldebug.h ldo.h lfunc.h lgc.h lstring.h \
 ltable.h lundump.h lvm.h
lauxlib.o: lauxlib.cpp lprefix.h hello.h helloconf.h lauxlib.h
lbaselib.o: lbaselib.cpp lprefix.h hello.h helloconf.h lauxlib.h hellolib.h
lcode.o: lcode.cpp lprefix.h hello.h helloconf.h lcode.h llex.h lobject.h \
 llimits.h lzio.h lmem.h lopcodes.h lparser.h ldebug.h lstate.h ltm.h \
 ldo.h lgc.h lstring.h ltable.h lvm.h
lcorolib.o: lcorolib.cpp lprefix.h hello.h helloconf.h lauxlib.h hellolib.h
lctype.o: lctype.cpp lprefix.h lctype.h hello.h helloconf.h llimits.h
ldblib.o: ldblib.cpp lprefix.h hello.h helloconf.h lauxlib.h hellolib.h
ldebug.o: ldebug.cpp lprefix.h hello.h helloconf.h lapi.h llimits.h lstate.h \
 lobject.h ltm.h lzio.h lmem.h lcode.h llex.h lopcodes.h lparser.h \
 ldebug.h ldo.h lfunc.h lstring.h lgc.h ltable.h lvm.h
ldo.o: ldo.cpp lprefix.h hello.h helloconf.h lapi.h llimits.h lstate.h \
 lobject.h ltm.h lzio.h lmem.h ldebug.h ldo.h lfunc.h lgc.h lopcodes.h \
 lparser.h lstring.h ltable.h lundump.h lvm.h
ldump.o: ldump.cpp lprefix.h hello.h helloconf.h lobject.h llimits.h lstate.h \
 ltm.h lzio.h lmem.h lundump.h
lfunc.o: lfunc.cpp lprefix.h hello.h helloconf.h ldebug.h lstate.h lobject.h \
 llimits.h ltm.h lzio.h lmem.h ldo.h lfunc.h lgc.h
lgc.o: lgc.cpp lprefix.h hello.h helloconf.h ldebug.h lstate.h lobject.h \
 llimits.h ltm.h lzio.h lmem.h ldo.h lfunc.h lgc.h lstring.h ltable.h
linit.o: linit.cpp lprefix.h hello.h helloconf.h hellolib.h lauxlib.h
liolib.o: liolib.cpp lprefix.h hello.h helloconf.h lauxlib.h hellolib.h
llex.o: llex.cpp lprefix.h hello.h helloconf.h lctype.h llimits.h ldebug.h \
 lstate.h lobject.h ltm.h lzio.h lmem.h ldo.h lgc.h llex.h lparser.h \
 lstring.h ltable.h
lmathlib.o: lmathlib.cpp lprefix.h hello.h helloconf.h lauxlib.h hellolib.h
lmem.o: lmem.cpp lprefix.h hello.h helloconf.h ldebug.h lstate.h lobject.h \
 llimits.h ltm.h lzio.h lmem.h ldo.h lgc.h
loadlib.o: loadlib.cpp lprefix.h hello.h helloconf.h lauxlib.h hellolib.h
lobject.o: lobject.cpp lprefix.h hello.h helloconf.h lctype.h llimits.h \
 ldebug.h lstate.h lobject.h ltm.h lzio.h lmem.h ldo.h lstring.h lgc.h \
 lvm.h
lopcodes.o: lopcodes.cpp lprefix.h lopcodes.h llimits.h hello.h helloconf.h
loslib.o: loslib.cpp lprefix.h hello.h helloconf.h lauxlib.h hellolib.h
lparser.o: lparser.cpp lprefix.h hello.h helloconf.h lcode.h llex.h lobject.h \
 llimits.h lzio.h lmem.h lopcodes.h lparser.h ldebug.h lstate.h ltm.h \
 ldo.h lfunc.h lstring.h lgc.h ltable.h
lstate.o: lstate.cpp lprefix.h hello.h helloconf.h lapi.h llimits.h lstate.h \
 lobject.h ltm.h lzio.h lmem.h ldebug.h ldo.h lfunc.h lgc.h llex.h \
 lstring.h ltable.h
lstring.o: lstring.cpp lprefix.h hello.h helloconf.h ldebug.h lstate.h \
 lobject.h llimits.h ltm.h lzio.h lmem.h ldo.h lstring.h lgc.h
lstrlib.o: lstrlib.cpp lprefix.h hello.h helloconf.h lauxlib.h hellolib.h
lcryptolib.o: lcryptolib.cpp lprefix.h hello.h helloconf.h lauxlib.h hellolib.h lcryptolib.hpp
ltable.o: ltable.cpp lprefix.h hello.h helloconf.h ldebug.h lstate.h lobject.h \
 llimits.h ltm.h lzio.h lmem.h ldo.h lgc.h lstring.h ltable.h lvm.h
ltablib.o: ltablib.cpp lprefix.h hello.h helloconf.h lauxlib.h hellolib.h
ltm.o: ltm.cpp lprefix.h hello.h helloconf.h ldebug.h lstate.h lobject.h \
 llimits.h ltm.h lzio.h lmem.h ldo.h lgc.h lstring.h ltable.h lvm.h
hello.o: hello.cpp lprefix.h hello.h helloconf.h lauxlib.h hellolib.h
helloc.o: helloc.cpp lprefix.h hello.h helloconf.h lauxlib.h ldebug.h lstate.h \
 lobject.h llimits.h ltm.h lzio.h lmem.h lopcodes.h lopnames.h lundump.h
lundump.o: lundump.cpp lprefix.h hello.h helloconf.h ldebug.h lstate.h \
 lobject.h llimits.h ltm.h lzio.h lmem.h ldo.h lfunc.h lstring.h lgc.h \
 lundump.h
lutf8lib.o: lutf8lib.cpp lprefix.h hello.h helloconf.h lauxlib.h hellolib.h
lvm.o: lvm.cpp lprefix.h hello.h helloconf.h ldebug.h lstate.h lobject.h \
 llimits.h ltm.h lzio.h lmem.h ldo.h lfunc.h lgc.h lopcodes.h lstring.h \
 ltable.h lvm.h ljumptab.h ljumptabgcc.h
lzio.o: lzio.cpp lprefix.h hello.h helloconf.h llimits.h lmem.h lstate.h \
 lobject.h ltm.h lzio.h

# (end of Makefile)
