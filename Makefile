$(shell mkdir -p rootfsimg/build)

APPS = busybox bash hope-RIPE redis sudo strace case-study dasics-test
APPS_DIR = $(addprefix apps/, $(APPS))


.PHONY:$(DIR_DASICS_BUILD) rootfsimg $(APPS_DIR) $(DASICS_DIR) clean

rootfsimg: $(APPS_DIR)


$(APPS_DIR): %:
	-$(MAKE) -s -C $@ install


all: $(APPS_DIR)

clean:
	-$(foreach app, $(APPS_DIR), $(MAKE) -s -C $(app) clean ;)
	-rm -rf apps/dasics-test/build
	-rm -f rootfsimg/build/*
