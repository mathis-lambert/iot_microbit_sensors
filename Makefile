all: build

VERSION := $(shell yt --version 2> /dev/null)

check:
ifeq($VERSION),)
	@echo you should use before trying anything: source /sync/Module_Dev_App_Mobile/yotta/bin/activate
	@false
endif

build:
	@yt build

install:
	@cp build/bbc-microbit-classic-gcc/source/microbit-samples-combined.hex /media/$USER/MICROBIT/
	@echo "Install done"

clean:
	@yt clean
	@echo "Cleaning done"
