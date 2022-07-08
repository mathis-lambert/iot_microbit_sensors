all: build

patch:
	@sed -i "s/VERSION 2.8.11/VERSION 3.0.0/" ~/.local/lib/python3.6/site-packages/yotta/lib/templates/subdir_CMakeLists.txt
	@sed -i "s/VERSION 2.8.11/VERSION 3.0.0/" ~/.local/lib/python3.6/site-packages/yotta/lib/templates/base_CMakeLists.txt
	@sed -i "s/^cmake_policy(SET CMP0017 OLD)/#cmake_policy(SET CMP0017 OLD)/" ~/.local/lib/python3.6/site-packages/yotta/lib/templates/base_CMakeLists.txt

build: patch
	@yt build

install:
	@cp build/bbc-microbit-classic-gcc/source/microbit-samples-combined.hex /media/$USER/MICROBIT/
	@echo "Install done"

clean:
	@yt clean
	@echo "Cleaning done"
