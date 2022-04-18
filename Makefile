doc:
	doxygen Doxyfile

lint:
	cpplint --recursive --exclude=src/utility/scope_guard.hpp --filter=-build/include_order,-build/include_subdir,-build/c++11,-legal/copyright,-build/namespaces,-readability/todo,-runtime/int src/

clean:
	mkdir build; cd build
	make clean

build:
	mkdir build; cd build
	cmake .
	make -j 8

.PHONY: build doc lint
