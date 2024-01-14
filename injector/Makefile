vpath %.cpp ../src
cxx = g++
cxxflags = -c -O1 -I ../inc
cxx32prefix = i686-w64-mingw32-
cxx64prefix = x86_64-w64-mingw32-
objdir = ./obj/
bindir = ../bin/
src = injector.cpp inject.cpp
obj32 = $(patsubst %.cpp, $(objdir)%_32.o, $(src))
obj64 = $(patsubst %.cpp, $(objdir)%_64.o, $(src))
target32 = $(bindir)injector32.exe
target64 = $(bindir)injector64.exe

all: check $(target32) $(target64)

$(target32): $(obj32)
	$(cxx32prefix)$(cxx) -o $@ $^ -static

$(target64): $(obj64)
	$(cxx64prefix)$(cxx) -o $@ $^ -static

$(objdir)%_32.o: %.cpp
	$(cxx32prefix)$(cxx) $(cxxflags) -o $@ $<

$(objdir)%_64.o: %.cpp
	$(cxx64prefix)$(cxx) $(cxxflags) -o $@ $<

.PHONY: check clean

check:
	@ if not exist obj md obj

clean:
	@ if exist obj del obj\*.o
