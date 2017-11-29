
LUACONTRIB=contrib/lua-5.1.5/
LUASRC=$(LUACONTRIB)src/

CFLAGS=$(MYCFLAGS) -Wall -Wextra -ansi -pedantic -O3 -I$(LUASRC) -Icontrib/pluto
CPPFLAGS=-DDEPLOY -DNDEBUG -DLUA_USE_LINUX -D_XOPEN_SOURCE=600
CXXFLAGS=$(CFLAGS) -I$(LUACONTRIB)etc
LDFLAGS=-s -Wl,--gc-sections -Wl,-E
LDLIBS=-lm -lrt -lpthread -ldl
RM=rm -f

CXXSRCS=src/chemical.cpp src/compartment.cpp src/compartmenttype.cpp \
	src/distribution.cpp src/event.cpp src/hiercompartment.cpp src/main.cpp \
	src/multithread.cpp src/parser.cpp src/parsestream.cpp src/rate.cpp \
	src/reaction.cpp src/reactionbank.cpp src/rng.cpp src/samplertarget.cpp \
	src/sbmlreader.cpp src/simulation.cpp src/simulationinit.cpp \
	src/simulationloader.cpp src/simulationsampler.cpp src/split.cpp \
	src/waitlist.cpp
CSRCS=$(LUASRC)lapi.c $(LUASRC)lauxlib.c $(LUASRC)lbaselib.c $(LUASRC)lcode.c \
	$(LUASRC)ldblib.c $(LUASRC)ldebug.c $(LUASRC)ldo.c $(LUASRC)ldump.c \
	$(LUASRC)lfunc.c $(LUASRC)lgc.c $(LUASRC)linit.c $(LUASRC)liolib.c \
	$(LUASRC)llex.c $(LUASRC)lmathlib.c $(LUASRC)lmem.c $(LUASRC)loadlib.c \
	$(LUASRC)lobject.c $(LUASRC)lopcodes.c $(LUASRC)loslib.c \
	$(LUASRC)lparser.c $(LUASRC)lstate.c $(LUASRC)lstring.c \
	$(LUASRC)lstrlib.c $(LUASRC)ltable.c $(LUASRC)ltablib.c $(LUASRC)ltm.c \
	$(LUASRC)lundump.c $(LUASRC)lvm.c $(LUASRC)lzio.c $(LUASRC)print.c \
	contrib/pluto/pluto.c
OBJS=$(subst .cpp,.o,$(CXXSRCS)) $(subst .c,.o,$(CSRCS))

all: sgns2

32bit:
	$(MAKE) MYCFLAGS=-m32

64bit:
	$(MAKE) MYCFLAGS=-m64

macosx: macosx10.5

macosx10.5:
	$(MAKE) CC='gcc -arch x86_64 -arch i386' CXX='g++ -arch x86_64 -arch i386' LDFLAGS='-Wl,-unexported_symbol,*,-x,-dead_strip -rdynamic' LDLIBS="" MYCFLAGS='-D_DARWIN_C_SOURCE -isysroot /Developer/SDKs/MacOSX10.5.sdk -mmacosx-version-min=10.5'

sgns2: $(OBJS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o sgns2 $(OBJS) $(LDLIBS)

clean:
	$(RM) $(OBJS) ./sgns2

depend: .depend

.depend: $(CXXSRCS) $(CSRCS)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MM $(CXXSRCS) > .depend;
	$(CC) $(CPPFLAGS) $(CFLAGS) -MM $(CSRCS) >> .depend;

.PHONY: all 32bit 64bit macosx macosx10.5 clean depend

include .depend

