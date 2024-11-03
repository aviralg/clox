
configure:
	cmake -S . -B build

build:
	cmake --build build

run:
	./build/clox

.PHONY: configure build run
