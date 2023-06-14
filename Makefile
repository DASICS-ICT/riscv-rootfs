$(shell mkdir -p rootfsimg/build)

APPS = busybox 
APPS_DIR = $(addprefix apps/, $(APPS))

DASICS = dasics-test
DASICS_DIR = apps/dasics-test
RIPE_DIR = apps/hope-RIPE
BASH_DIR = bash

.PHONY: rootfsimg $(APPS_DIR) $(DASICS_DIR) $(RIPE_DIR) $(BASH_DIR) clean

rootfsimg: $(APPS_DIR)

$(APPS_DIR): %:
	-$(MAKE) -s -C $@ install

$(DASICS_DIR):
	-$(MAKE) -s -C $@ all

$(RIPE_DIR):
	-$(MAKE) -s -C $@ all 

$(BASH_DIR):
	-$(MAKE) -C $@ bash

all: $(APPS_DIR) $(DASICS_DIR) $(RIPE_DIR) $(BASH_DIR)

clean:
	-$(foreach app, $(APPS_DIR), $(MAKE) -s -C $(app) clean ;)
	-rm -rf apps/dasics-test/build
	-$(MAKE) -s -C $(RIPE_DIR) clean
	-rm -f rootfsimg/build/*

