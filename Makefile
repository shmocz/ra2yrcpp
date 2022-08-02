BUILDDIR := build
TESTS := $(shell find $(BUILDDIR)/tests -name 'test_*.exe')
CMAKE_BUILD_TYPE := Debug
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
	rm -rf build/*

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
	@mkdir -p .cppcheck
	cppcheck --platform=win32W --enable=all \
		--cppcheck-build-dir=./.cppcheck \
		-I src/ \
		--inline-suppr \
		--suppress=passedByValue \
		--suppress=noExplicitConstructor:src/utility/scope_guard.hpp \
		--suppress=unusedStructMember:src/commands_yr.cpp \
		--suppress=unusedFunction:tests/*.cpp \
		--suppress=uninitMemberVar:src/ra2/objects.cpp \
		src/ tests/

check: cmake_format lint
	./scripts/check.sh

.PHONY: build doc lint format test docker docker_build check cppcheck
