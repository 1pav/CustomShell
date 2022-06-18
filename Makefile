# Paths of directories
PATH_SRC = ./src
PATH_BUILD = ./build
PATH_BIN = $(PATH_BUILD)/bin
PATH_ASSETS = ./assets

# Test mode
# Script used for generating commands
TEST_SCRIPT = ./test.sh
# Path for output file (passed to TEST_SCRIPT)
TEST_FILE = $(PATH_ASSETS)/commands.txt
# Number of commands to generate (passed to TEST_SCRIPT)
TEST_COUNT = 50

# Default compiler
CC = gcc

# Debug mode
DEBUG ?= 0
ifeq ($(DEBUG), 1)
	CFLAGS = -g
endif

.PHONY: help clean build run assets test

help:
	@ cat help.txt

clean:
	if [ -e $(PATH_BUILD) ]; then rm -rf $(PATH_BUILD); fi
	if [ -e $(PATH_ASSETS) ]; then rm -rf $(PATH_ASSETS); fi

build: clean
	mkdir $(PATH_BUILD)
	mkdir $(PATH_BIN)
	$(CC) $(CFLAGS) $(PATH_SRC)/pmanager.c $(PATH_SRC)/common.c $(PATH_SRC)/message.c $(PATH_SRC)/proc_tree.c $(PATH_SRC)/handlers.c -o $(PATH_BUILD)/pmanager
	$(CC) $(CFLAGS) $(PATH_SRC)/phelp.c -o $(PATH_BIN)/phelp
	$(CC) $(CFLAGS) $(PATH_SRC)/pnew.c $(PATH_SRC)/proc_tree.c $(PATH_SRC)/common.c $(PATH_SRC)/message.c $(PATH_SRC)/child.c -o $(PATH_BIN)/pnew
	$(CC) $(CFLAGS) $(PATH_SRC)/pinfo.c $(PATH_SRC)/proc_tree.c $(PATH_SRC)/common.c $(PATH_SRC)/message.c -o $(PATH_BIN)/pinfo
	$(CC) $(CFLAGS) $(PATH_SRC)/pclose.c $(PATH_SRC)/message.c $(PATH_SRC)/common.c -o $(PATH_BIN)/pclose
	$(CC) $(CFLAGS) $(PATH_SRC)/plist.c $(PATH_SRC)/message.c $(PATH_SRC)/proc_tree.c $(PATH_SRC)/common.c -o $(PATH_BIN)/plist
	$(CC) $(CFLAGS) $(PATH_SRC)/ptree.c $(PATH_SRC)/message.c $(PATH_SRC)/common.c $(PATH_SRC)/proc_tree.c -o $(PATH_BIN)/ptree
	$(CC) $(CFLAGS) $(PATH_SRC)/pspawn.c $(PATH_SRC)/message.c $(PATH_SRC)/common.c -o $(PATH_BIN)/pspawn
	$(CC) $(CFLAGS) $(PATH_SRC)/prmall.c $(PATH_SRC)/message.c $(PATH_SRC)/common.c $(PATH_SRC)/proc_tree.c -o $(PATH_BIN)/prmall

run: build
	cd $(PATH_BUILD) && ./pmanager

assets: build
	mkdir $(PATH_ASSETS)
	$(TEST_SCRIPT) $(TEST_COUNT) $(TEST_FILE)

test: assets
	cd $(PATH_BUILD) && ./pmanager ../$(TEST_FILE)
