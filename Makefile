export BUILDDIR ?= cbuild
export CMAKE_BUILD_TYPE ?= Release

REPO_FILES = $(shell git ls-tree -r --name-only HEAD) 
TESTS := $(patsubst %.cpp,%.exe,$(subst tests/,bin/,$(filter tests/test_%.cpp, $(REPO_FILES))))
PYTHON := python3

CMAKE_TOOLCHAIN_FILE ?= toolchains/mingw-w64-i686.cmake
TC_ID ?= $(notdir $(basename $(CMAKE_TOOLCHAIN_FILE)))
BASE_DIR := $(BUILDDIR)/$(TC_ID)-$(CMAKE_BUILD_TYPE)
BUILD_DIR = $(BASE_DIR)/build
export DEST_DIR = $(BASE_DIR)/pkg
BUILDER ?= builder

export CPPCHECK ?= cppcheck
export PLAYERS_CONFIG := ./test_data/envs.tsv
export CM_FILES = $(filter %CMakeLists.txt, $(REPO_FILES))
export CPP_SOURCES = $(filter %.cpp %.hpp %.c %.h, $(REPO_FILES))
export W32_FILES := process.cpp state_parser.cpp dll_inject.cpp network.cpp manager.cpp
GAMEMD_PATCHED := $(DEST_DIR)/bin/gamemd-spawn-patched.exe
EXTRA_PATCHES ?=

export UID := $(shell id -u)
export GID := $(shell id -g)
export NPROC ?= $(nproc)

INTEGRATION_TEST ?= docker-compose.integration.yml
INTEGRATION_TEST_TARGET ?= ./pyra2yr/test_sell_mcv.py
COMPOSE_ARGS ?= --abort-on-container-exit pyra2yr tunnel wm vnc novnc game-0 game-1
compose_cmd := docker-compose -f docker-compose.yml -f $(INTEGRATION_TEST)
# need -T flag for this to work properly in shell scripts
compose_build = docker-compose run -e "NPROC=$(NPROC)" -T --rm $(BUILDER) make CMAKE_TOOLCHAIN_FILE=$(CMAKE_TOOLCHAIN_FILE) CMAKE_BUILD_TYPE=$(CMAKE_BUILD_TYPE) BUILDDIR=cbuild_docker EXTRA_PATCHES="$(EXTRA_PATCHES)"

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

# NB. ninja seems broken with clang-cl and protobuf atm.
build_cpp:
	# TODO: should probably pass TC file explicitly - relative paths wont work since we cd to BUILD_DIR
	mkdir -p $(BUILD_DIR)
	CMAKE_TOOLCHAIN_FILE=$(abspath $(CMAKE_TOOLCHAIN_FILE)) cmake \
		-DCMAKE_INSTALL_PREFIX=$(DEST_DIR) \
		-G "Unix Makefiles" \
		-S . -B $(BUILD_DIR)
	cmake --build $(BUILD_DIR) --config $(CMAKE_BUILD_TYPE) --target all -j $(NPROC)
	cmake --build $(BUILD_DIR) --config $(CMAKE_BUILD_TYPE) --target install

build: build_cpp $(GAMEMD_PATCHED)

$(GAMEMD_PATCHED): gamemd-spawn.exe
	$(DEST_DIR)/bin/ra2yrcppcli.exe \
		--address-GetProcAddr=0x7e1250 \
		--address-LoadLibraryA=0x7e1220 \
		--generate-dll-loader  \
		> $(BUILD_DIR)/load_dll.bin
	$(PYTHON) ./scripts/patch_gamemd.py \
		-p "d0x7cd80f:$(BUILD_DIR)/load_dll.bin" \
		$(EXTRA_PATCHES) \
		-i $< > .gamemd-spawn-patched.exe
	install -D .gamemd-spawn-patched.exe $@
	rm .gamemd-spawn-patched.exe

test: $(GAMEMD_PATCHED)
	set -e; for f in $(TESTS); do \
		wine $(DEST_DIR)/$$f; done

test_integration:
	BUILDDIR=$(BUILDDIR) COMMAND='./scripts/run_gamemd.sh' COMMAND_PYRA2YR='python3 $(INTEGRATION_TEST_TARGET)' $(compose_cmd) up $(COMPOSE_ARGS)

docker_base:
	docker-compose build --build-arg USER_ID=$(UID)

docker_test:
	set -e; for f in $(TESTS); do \
		docker-compose down --remove-orphans; \
		COMMAND="sh -c 'BUILDDIR=$(BUILDDIR) make BUILDDIR=$(BUILDDIR) DEST_DIR=$(DEST_DIR) TESTS=$$f test'" docker-compose up --abort-on-container-exit vnc builder; done

# NB. using "run" the env. vars need to be specified with -e flag
# actually we dont wanna pass TC in env var, because it overrides the --toolchain flag, which we use to transform relative path
docker_build:
	$(compose_build) build

cppcheck:
	./scripts/cppcheck.sh

check: cmake_format lint format
	./scripts/check.sh

clean:
	rm -rf $(BUILDDIR); mkdir -p $(BUILDDIR)
	rm -f test_data/*.status $(GAMEMD_PATCHED)

.PHONY: build build_cpp doc lint format test docker docker_build check cppcheck clean
