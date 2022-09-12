config:
	cmake -S . -B build

build:
	cmake --build build -j8

client:
	./build/db_client_test

.PHONY: build