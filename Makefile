BUILDDIR := build
TESTS := $(shell find $(BUILDDIR)/tests -name 'test_*.exe')
CPP_SOURCES := $(shell git ls-tree -r --name-only HEAD | grep -E '\.(cpp|hpp)$$')
CMAKE_BUILD_TYPE := Debug

doc:
	doxygen Doxyfile

lint: src/ tests/
	cpplint \
		--recursive \
		--exclude=src/utility/scope_guard.hpp \
		--filter=-build/include_order,-build/include_subdir,-build/c++11,-legal/copyright,-build/namespaces,-readability/todo,-runtime/int,-runtime/string,-runtime/printf \
		$^

cmake_format: CMakeLists.txt src/CMakeLists.txt tests/CMakeLists.txt
	cmake-format -c .cmake-format.yml -i $^

format:
	echo "$(CPP_SOURCES)" | xargs -n 1 clang-format -i

clean:
	rm -rf build

build:
	mkdir -p $(BUILDDIR)
	cmake -S . -B $(BUILDDIR)
	cmake --build $(BUILDDIR) --config $(CMAKE_BUILD_TYPE) --target all -j $(nproc)

test:
	set -e; for f in $(TESTS); do \
		WINEPATH="./build/src" wine $$f; done

docker:
	docker-compose build

docker_build:
	docker-compose run --rm builder make BUILDDIR=$(BUILDDIR) build

.PHONY: build doc lint format test docker docker_build
