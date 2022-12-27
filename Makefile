$(shell mkdir -p rootfsimg/build)

APPS = busybox 
APPS_DIR = $(addprefix apps/, $(APPS))

DASICS = dasics-test
DASICS_DIR = apps/dasics-test

.PHONY: rootfsimg $(APPS_DIR) $(DASICS_DIR) clean

rootfsimg: $(APPS_DIR)

$(APPS_DIR): %:
	-$(MAKE) -s -C $@ install

$(DASICS_DIR):
	-$(MAKE) -s -C $@ all

all: $(APPS_DIR) $(DASICS_DIR)

clean:
	-$(foreach app, $(APPS_DIR), $(MAKE) -s -C $(app) clean ;)
	-rm -rf apps/dasics-test/build
	-rm -f rootfsimg/build/*
