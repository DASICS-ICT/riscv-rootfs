BASH_BUILD_DIR=$(shell pwd)/build
HOST=riscv64-unknown-linux-gnu
CC=riscv64-unknown-linux-gnu-gcc
CXX=riscv64-unknown-linux-gnu-g++ 

BASH_TAR_GZ=$(shell pwd)/bash-5.0.tar.gz

BASH_DIR=$(shell pwd)/bash-5.0

BASH_BUILD=$(BASH_BUILD_DIR)/bin/bash
TARGET_BIN_DIR=$(abspath $(shell pwd)/..)/rootfsimg/build

$(BASH_DIR): $(BASH_TAR_GZ)
	tar -zxvf $(BASH_TAR_GZ)

$(BASH_BUILD): $(BASH_DIR)
	cd bash-5.0 && ./configure --host=$(HOST) --prefix=$(BASH_BUILD_DIR) CXX=$(CXX) CC=$(CC)
	cd bash-5.0 && make && make install	
	ln -sf $< $(TARGET_BIN_DIR)/bash

bash: $(BASH_BUILD) 

clean:
	rm -rf build

.PHONY: bash clean $(BASH_DIR) $(BASH_BUILD)
