cxx = g++
cxxflags = -c -I ../../inc
cxx32prefix = i686-w64-mingw32-
cxx64prefix = x86_64-w64-mingw32-
target = testhook.exe
target32 = testhook32.exe
target64 = testhook64.exe
src = testhook.cpp inlinehook.cpp
objdir = ./obj/
obj = $(patsubst %.cpp, $(objdir)%.o, $(src))
obj32 = $(patsubst %.cpp, $(objdir)%_32.o, $(src))
obj64 = $(patsubst %.cpp, $(objdir)%_64.o, $(src))
targets = $(target)
vpath %.cpp ../../src

ifeq ($(cpu32),1)
targets += $(target32)
endif
ifeq ($(cpu64),1)
targets += $(target64)
endif

all: check $(targets)

$(target): $(obj)
	$(cxx) -o $@ $^

$(target32): $(obj32)
	$(cxx32prefix)$(cxx) -o $@ $^

$(target64): $(obj64)
	$(cxx64prefix)$(cxx) -o $@ $^

$(objdir)%.o: %.cpp
	$(cxx) $(cxxflags) -o $@ $<

$(objdir)%_32.o: %.cpp
	$(cxx32prefix)$(cxx) $(cxxflags) -o $@ $<

$(objdir)%_64.o: %.cpp
	$(cxx64prefix)$(cxx) $(cxxflags) -o $@ $<

.PHONY: check clean

check:
	@ if not exist obj md obj

clean:
	@ if exist obj del obj\*.o
	@ del *.exe
