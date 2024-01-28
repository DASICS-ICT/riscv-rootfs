$(shell mkdir -p rootfsimg/build)

APPS = busybox 
APPS_DIR = $(addprefix apps/, $(APPS))

DASICS = dasics-test
DASICS_DIR = apps/dasics-test

DASICS_DYNAMIC_LIB 	= DASICS_dynamic-lib
DIR_DASICS_BUILD	= $(DASICS_DYNAMIC_LIB)/build

.PHONY:$(DIR_DASICS_BUILD) rootfsimg $(APPS_DIR) $(DASICS_DIR) clean

rootfsimg: $(APPS_DIR)

$(DIR_DASICS_BUILD):
ifeq ($(wildcard $(DASICS_DYNAMIC_LIB)/*),)
	git submodule update --init $(DASICS_DYNAMIC_LIB)
endif
	make -C $(DASICS_DYNAMIC_LIB) all

$(APPS_DIR): %:
	-$(MAKE) -s -C $@ install

$(DASICS_DIR):
	-$(MAKE) -s -C $@ all

all: $(APPS_DIR) $(DASICS_DIR)

clean:
	-$(foreach app, $(APPS_DIR), $(MAKE) -s -C $(app) clean ;)
	-rm -rf apps/dasics-test/build
	-rm -f rootfsimg/build/*
