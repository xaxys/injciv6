vpath %.cpp ../src
cxx = g++
cxxflags = -c -O1 -I ../inc
cxx64prefix = x86_64-w64-mingw32-
objdir = ./obj/
bindir = ../bin/
src_inject = injciv6.cpp inject.cpp
src_remove = civ6remove.cpp inject.cpp
obj_inject = $(patsubst %.cpp, $(objdir)%.o, $(src_inject))
obj_remove = $(patsubst %.cpp, $(objdir)%.o, $(src_remove))
target_inject = $(bindir)injciv6.exe
target_remove = $(bindir)civ6remove.exe

all: check $(target_inject) $(target_remove)

$(target_inject): $(obj_inject)
	$(cxx64prefix)$(cxx) -o $@ $^ -static -mwindows

$(target_remove): $(obj_remove)
	$(cxx64prefix)$(cxx) -o $@ $^ -static -mwindows

$(objdir)%.o: %.cpp
	$(cxx64prefix)$(cxx) $(cxxflags) -o $@ $<

.PHONY: check clean

check:
	@ if not exist obj md obj

clean:
	@ if exist obj del obj\*.o
