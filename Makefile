doc:
	doxygen Doxyfile

lint:
	cpplint --recursive --filter=-build/c++11,-build/namespaces src/


clean:
	mkdir build; cd build
	make clean

build:
	mkdir build; cd build
	cmake .
	make -j 8

.PHONY: build doc lint
