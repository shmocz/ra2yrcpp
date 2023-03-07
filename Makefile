# Reason for relying on environment variables is that it's more convenient to pass them to docker when invoking some of the targets inside a container.
export BUILDDIR ?= cbuild
export CMAKE_BUILD_TYPE ?= Release

REPO_FILES = $(shell git ls-tree -r --name-only HEAD) 
TESTS := $(patsubst %.cpp,%.exe,$(subst tests/,bin/,$(filter tests/test_%.cpp, $(REPO_FILES))))
PYTHON := python3

export CMAKE_TOOLCHAIN_FILE ?= toolchains/mingw-w64-i686.cmake
TC_ID ?= $(notdir $(basename $(CMAKE_TOOLCHAIN_FILE)))
BASE_DIR := $(BUILDDIR)/$(TC_ID)-$(CMAKE_BUILD_TYPE)
BUILD_DIR = $(BASE_DIR)/build
export DEST_DIR = $(BASE_DIR)/pkg
BUILDER ?= builder

export CPPCHECK ?= cppcheck
export PLAYERS_CONFIG := ./test_data/envs.tsv
export CM_FILES = $(filter %CMakeLists.txt, $(REPO_FILES))
export CPP_SOURCES = $(filter %.cpp %.hpp %.c %.h, $(REPO_FILES))
export W32_FILES := process.cpp state_parser.cpp dll_inject.cpp network.cpp addscn/addscn.cpp
GAMEMD_PATCHED := $(DEST_DIR)/bin/gamemd-spawn-patched.exe
EXTRA_PATCHES ?=

export UID := $(shell id -u)
export GID := $(shell id -g)
export NPROC ?= $(nproc)
export CMAKE_TARGET ?= all

DLL_LOADER_UNIX = $(BUILD_DIR)/load_dll.bin
DLL_LOADER = $(subst \,\\,$(shell winepath -w $(DLL_LOADER_UNIX)))

INTEGRATION_TEST ?= docker-compose.integration.yml
INTEGRATION_TEST_TARGET ?= ./pyra2yr/test_sell_mcv.py
COMPOSE_ARGS ?= --abort-on-container-exit pyra2yr tunnel wm vnc novnc game-0 game-1
compose_cmd := docker-compose -f docker-compose.yml -f $(INTEGRATION_TEST)
# need -T flag for this to work properly in shell scripts, but this causes ctrl+c not to work.
# TODO: find a workaround
compose_build = docker-compose run -e BUILDDIR -e CMAKE_TOOLCHAIN_FILE -e CMAKE_TARGET -e NPROC -e CMAKE_BUILD_TYPE -e EXTRA_PATCHES -T --rm $(BUILDER) make


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
		-G "Unix Makefiles" \
		-S . -B $(BUILD_DIR)

# NB. ninja seems broken with clang-cl and protobuf atm.
build_cpp: $(BUILD_DIR)
	# check that these are defined
	test $(NPROC)
	test $(CMAKE_TARGET)

	# TODO: if using custom targets on mingw, the copied libs arent marked as deps!
	cmake --build $(BUILD_DIR) --config $(CMAKE_BUILD_TYPE) --target $(CMAKE_TARGET) -j $(NPROC)
	cmake --build $(BUILD_DIR) --config $(CMAKE_BUILD_TYPE) --target install/fast

build: build_cpp $(GAMEMD_PATCHED)

$(BUILD_DIR)/.gamemd-spawn.exe: gamemd-spawn.exe
	cp $< $@

# FIXME: if building just core lib, ra2yrcppcli.exe is unavailable
$(BUILD_DIR)/p_text2.txt: $(BUILD_DIR)/.gamemd-spawn.exe
	$(DEST_DIR)/bin/ra2yrcppcli.exe \
		--address-GetProcAddr=0x7e1250 \
		--address-LoadLibraryA=0x7e1220 \
		--generate-dll-loader=$(DLL_LOADER)
	$(DEST_DIR)/bin/addscn.exe $< .p_text2 0x1000 0x60000020 > $@
	wineserver -w

# FIXME: autodetect detour address
$(GAMEMD_PATCHED): $(BUILD_DIR)/p_text2.txt
	$(eval s_ptext2 := $(shell cat $(<)))
	$(eval s_ptext2_addr := $(shell cat $(<) | cut -f 3 -d":"))
	$(PYTHON) ./scripts/patch_gamemd.py \
		-p "d0x7cd80f:$(BUILD_DIR)/load_dll.bin" \
		$(EXTRA_PATCHES) \
		-d $(s_ptext2_addr) \
		-s ".p_text:0x00004d66:0x00b7a000:0x0047e000" \
		-s ".text:0x003df38d:0x00401000:0x00001000" \
		-s "$(s_ptext2)" \
		-i $(BUILD_DIR)/.gamemd-spawn.exe > $(BUILD_DIR)/.gamemd-spawn-patched.exe
	install -D $(BUILD_DIR)/.gamemd-spawn-patched.exe $@

test:
	set -e; for f in $(TESTS); do \
		wine $(DEST_DIR)/$$f; done

test_integration: $(GAMEMD_PATCHED)
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
