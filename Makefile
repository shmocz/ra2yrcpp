# Reason for relying on environment variables is that it's more convenient to pass them to docker when invoking some of the targets inside a container.
export BUILDDIR ?= cbuild
export CMAKE_BUILD_TYPE ?= Release

VERSION = SOFT_VERSION-$(shell git rev-parse --short HEAD)

REPO_FILES = $(shell git ls-tree -r --name-only HEAD) 
TESTS := $(patsubst %.cpp,%.exe,$(subst tests/,bin/,$(shell find tests/ -iname "test_*.cpp")))

export CMAKE_TOOLCHAIN_FILE ?= toolchains/mingw-w64-i686.cmake
BASE_DIR := $(BUILDDIR)/$(notdir $(basename $(CMAKE_TOOLCHAIN_FILE)))-$(CMAKE_BUILD_TYPE)
BUILD_DIR = $(BASE_DIR)/build
export DEST_DIR = $(BASE_DIR)/pkg
BUILDER ?= builder

export CM_FILES = $(filter %CMakeLists.txt, $(REPO_FILES))
export CPP_SOURCES = $(filter %.cpp %.hpp %.c %.h, $(REPO_FILES))
export W32_FILES := process.cpp state_parser.cpp dll_inject.cpp network.cpp addscn/addscn.cpp

export UID := $(shell id -u)
export GID := $(shell id -g)
export NPROC ?= $(shell nproc)
export CMAKE_TARGET ?= all
export CMAKE_EXTRA_ARGS ?=
export CXXFLAGS ?= -Wall -Wextra

# need -T flag for this to work properly in shell scripts, but this causes ctrl+c not to work.
# TODO: find a workaround
compose_build = docker-compose run -e BUILDDIR -e CMAKE_TOOLCHAIN_FILE -e CMAKE_TARGET -e NPROC -e CMAKE_BUILD_TYPE -e CMAKE_EXTRA_ARGS -e CXXFLAGS -e TAG_NAME -T --rm $(BUILDER)


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

$(BUILD_DIR): 
	# TODO: should probably pass TC file explicitly - relative paths wont work since we cd to BUILD_DIR
	mkdir -p $(BUILD_DIR)
	CMAKE_TOOLCHAIN_FILE=$(abspath $(CMAKE_TOOLCHAIN_FILE)) cmake \
		-DCMAKE_INSTALL_PREFIX=$(DEST_DIR) \
		-DRA2YRCPP_VERSION=$(VERSION) \
		$(CMAKE_EXTRA_ARGS) \
		-G "Unix Makefiles" \
		-S . -B $(BUILD_DIR)

# NB. ninja seems broken with clang-cl and protobuf atm.
build_cpp: $(BUILD_DIR)
	# check that these are defined
	test $(NPROC)
	test $(CMAKE_TARGET)
	test $(VERSION)

	# TODO: if using custom targets on mingw, the copied libs arent marked as deps!
	cmake --build $(BUILD_DIR) --config $(CMAKE_BUILD_TYPE) --target $(CMAKE_TARGET) -j $(NPROC)
	cmake --build $(BUILD_DIR) --config $(CMAKE_BUILD_TYPE) --target install/fast

build: build_cpp

build_protobuf:
	mkdir -p $(BUILD_DIR)
	CMAKE_TOOLCHAIN_FILE=$(abspath $(CMAKE_TOOLCHAIN_FILE)) cmake \
		-DCMAKE_INSTALL_PREFIX=$(DEST_DIR) \
		$(CMAKE_EXTRA_ARGS) \
		-G "Unix Makefiles" \
		-Dprotobuf_BUILD_LIBPROTOC=ON \
		-Dprotobuf_WITH_ZLIB=ON \
		-DProtobuf_USE_STATIC_LIBS=ON \
		-Dprotobuf_MSVC_STATIC_RUNTIME=OFF \
		-Dprotobuf_BUILD_EXAMPLES=OFF \
		-Dprotobuf_INSTALL=ON \
		-Dprotobuf_BUILD_TESTS=OFF  \
		-DZLIB_LIB=/usr/i686-w64-mingw32/lib \
		-DZLIB_INCLUDE_DIR=/usr/i686-w64-mingw32/include \
		-S 3rdparty/protobuf -B $(BUILD_DIR)
	cmake --build $(BUILD_DIR) --parallel $(nproc)
	cmake --build $(BUILD_DIR) --config $(CMAKE_BUILD_TYPE) --target install

test:
	set -e; for f in $(TESTS); do \
		wineboot -s; \
		wine $(DEST_DIR)/$$f; done

docker-base:
	docker-compose build builder pyra2yr tunnel vnc

docker_test:
	set -e; for f in $(TESTS); do \
		docker-compose down --remove-orphans; \
		COMMAND="sh -c 'UID=$(UID) BUILDDIR=$(BUILDDIR) make BUILDDIR=$(BUILDDIR) DEST_DIR=$(DEST_DIR) TESTS=$$f test'" docker-compose up --abort-on-container-exit builder; done

# NB. using "run" the env. vars need to be specified with -e flag
docker-build:
	$(compose_build) make build

cppcheck:
	./scripts/cppcheck.sh

check: cmake_format lint format
	./scripts/check.sh

clean:
	rm -rf $(BUILDDIR); mkdir -p $(BUILDDIR)
	rm -f test_data/*.status

# TODO: check out the special $(MAKE) variable that presumably passes flags
docker-release:
	$(compose_build) ./scripts/create-release.sh

.PHONY: build build_cpp doc lint format test docker docker_build check cppcheck clean
