vpath %.cpp ../src
cxx = g++
cxxflags = -c -O2 -I ../inc -I ../minhook/include -Wall -Werror
linkflags = -shared -static
linkflags32 = -L ../minhook/lib -l MinHook32
linkflags64 = -L ../minhook/lib -l MinHook64
cxx32prefix = i686-w64-mingw32-
cxx64prefix = x86_64-w64-mingw32-
objdir = ./obj/
bindir = ../bin/
src = hookdll.cpp fkhook.cpp inlinehook.cpp
obj32 = $(patsubst %.cpp, $(objdir)%_32.o, $(src))
obj64 = $(patsubst %.cpp, $(objdir)%_64.o, $(src))
target32 = $(bindir)hookdll32.dll
target64 = $(bindir)hookdll64.dll

all: check $(target32) $(target64)

$(target32): $(obj32)
	$(cxx32prefix)$(cxx) -o $@ $^ $(linkflags) $(linkflags32)

$(target64): $(obj64)
	$(cxx64prefix)$(cxx) -o $@ $^ $(linkflags) $(linkflags64)

$(objdir)%_32.o: %.cpp
	$(cxx32prefix)$(cxx) $(cxxflags) -o $@ $<

$(objdir)%_64.o: %.cpp
	$(cxx64prefix)$(cxx) $(cxxflags) -o $@ $<

.PHONY: check clean

check:
	@ if not exist obj md obj

clean:
	@ if exist obj del obj\*.o
