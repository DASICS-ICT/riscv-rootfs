$(shell mkdir -p rootfsimg/build)

APPS = busybox dasics-test mpk
APPS_DIR = $(addprefix apps/, $(APPS))

.PHONY: rootfsimg $(APPS_DIR) clean

all: $(APPS_DIR)

rootfsimg: $(APPS_DIR)

$(APPS_DIR): %:
	-$(MAKE) -s -C $@ install

clean:
	-$(foreach app, $(APPS_DIR), $(MAKE) -s -C $(app) clean ;)
	-rm -f rootfsimg/build/*
