NAME = bash

include $(RISCV_ROOTFS_HOME)/Makefile.app

TARBALL = bash-5.2.21.tar.gz


BASH_PATH = bash-5.2.21

$(TARBALL):
	wget https://ftp.gnu.org/gnu/bash/$(TARBALL)

$(BASH_PATH): $(TARBALL)
	tar -xvf $(TARBALL)
	rm $(TARBALL)

$(APP): | $(BASH_PATH)
	mkdir -p build
	cd $(BASH_PATH) && \
	./configure \
		--host=riscv64-unknown-linux-gnu \
		--prefix=$(shell pwd)/build \
		CXX=riscv64-unknown-linux-gnu-g++ \
		CC=riscv64-unknown-linux-gnu-gcc && \
	make -j`nproc` && \
	make install && \
	ln -s $(shell pwd)/build/bin/bash ../build/bash

