NAME = ripe_attack_generator

include $(RISCV_ROOTFS_HOME)/Makefile.app

REPO_PATH = repo
$(REPO_PATH):
	mkdir -p $@
	git clone --depth 1 -b hope-ripe-xs-dasics git@github.com:DASICS-ICT/hope-RIPE.git $@


$(APP): | $(REPO_PATH)
	make -C $(REPO_PATH) all 
	ln -sf $(abspath $(REPO_PATH)/build/ripe_attack_generator) $@
	cp $(REPO_PATH)/ripe_tester.sh ./build

