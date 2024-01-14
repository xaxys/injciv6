subdir = hookdll injector injciv6
cleandir = $(addprefix _clean_, $(subdir))

all: $(subdir)
	@ echo Make done !

$(subdir):
	$(MAKE) -C $@

$(cleandir):
	$(MAKE) clean -C $(patsubst _clean_%, %, $@)

.PHONY: clean $(subdir)

clean: $(cleandir)
	@ del bin\*.exe bin\*.dll
	@ echo Clean done !
