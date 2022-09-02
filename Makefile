BUILDDIR := build
TESTS = $(shell find $(BUILDDIR)/tests -name 'test_*.exe')
CMAKE_BUILD_TYPE := Release
export CPP_SOURCES = $(shell git ls-tree -r --name-only HEAD | grep -E '\.(cpp|hpp)$$')
export CM_FILES = $(shell git ls-tree -r --name-only HEAD | grep -E 'CMakeLists\.txt$$')
export W32_FILES := process.cpp state_parser.cpp dll_inject.cpp network.cpp

doc:
	doxygen Doxyfile

lint: src/ tests/
	cpplint \
		--recursive \
		--exclude=src/utility/scope_guard.hpp \
		--filter=-build/include_order,-build/include_subdir,-build/c++11,-legal/copyright,-build/namespaces,-readability/todo,-runtime/int,-runtime/string,-runtime/printf \
		$^

cmake_format: $(CM_FILES)
	cmake-format -c .cmake-format.yml -i $^

format:
	echo "$(CPP_SOURCES)" | xargs -n 1 clang-format -i

clean:
	rm -rf $(BUILDDIR); mkdir -p $(BUILDDIR)

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

cppcheck:
	mkdir -p .cppcheck
	./scripts/cppcheck.sh

check: cmake_format lint
	./scripts/check.sh

.PHONY: build doc lint format test docker docker_build check cppcheck
