BUILDDIR := build
TESTS := $(shell find $(BUILDDIR)/tests -name 'test_*.exe')

doc:
	doxygen Doxyfile

lint: src/ tests/
	cpplint \
		--recursive \
		--exclude=src/utility/scope_guard.hpp \
		--filter=-build/include_order,-build/include_subdir,-build/c++11,-legal/copyright,-build/namespaces,-readability/todo,-runtime/int,-runtime/string,-runtime/printf \
		$^

format: CMakeLists.txt src/CMakeLists.txt tests/CMakeLists.txt
	cmake-format -c .cmake-format.yml -i $^

clean:
	mkdir build; cd build
	make clean

build:
	mkdir build; cd build
	cmake .
	make -j 8

test:
	set -e; for f in $(TESTS); do \
		wine $$f; done

.PHONY: build doc lint format test
