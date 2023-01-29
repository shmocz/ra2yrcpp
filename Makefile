BUILDDIR ?= build
CMAKE_BUILD_TYPE ?= Release
REPO_FILES = $(shell git ls-tree -r --name-only HEAD) 
TESTS := $(patsubst %.cpp,%.exe,$(subst tests/,bin/,$(filter tests/test_%.cpp, $(REPO_FILES))))
PYTHON := python3
export CMAKE_RUNTIME_OUTPUT_DIRECTORY := $(BUILDDIR)/bin

export CPPCHECK ?= cppcheck
export PLAYERS_CONFIG := ./test_data/envs.tsv
export CM_FILES = $(filter %CMakeLists.txt, $(REPO_FILES))
export CPP_SOURCES = $(filter %.cpp %.hpp %.c %.h, $(REPO_FILES))
# disables old integration test
#export RA2YRCPP_TUNNEL_URL := 0.0.0.0
export W32_FILES := process.cpp state_parser.cpp dll_inject.cpp network.cpp manager.cpp
GAMEMD_PATCHED := $(CMAKE_RUNTIME_OUTPUT_DIRECTORY)/gamemd-spawn-patched.exe
EXTRA_PATCHES ?=

export UID := $(shell id -u)
export GID := $(shell id -g)

INTEGRATION_TEST ?= docker-compose.integration.yml
compose_cmd := docker-compose -f docker-compose.yml -f $(INTEGRATION_TEST)

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

build_cpp:
	mkdir -p $(BUILDDIR)
	cmake -S . -B $(BUILDDIR)
	cmake --build $(BUILDDIR) --config $(CMAKE_BUILD_TYPE) --target all -j $(nproc)

build: build_cpp $(GAMEMD_PATCHED)

$(GAMEMD_PATCHED): gamemd-spawn.exe
	$(BUILDDIR)/bin/ra2yrcppcli.exe --generate-dll-loader > $(BUILDDIR)/load_dll.bin
	$(PYTHON) ./scripts/patch_gamemd.py \
		-p "d0x7cd80f:$(BUILDDIR)/load_dll.bin" \
		$(EXTRA_PATCHES) \
		-i $< > .gamemd-spawn-patched.exe
	install -D .gamemd-spawn-patched.exe $@
	rm .gamemd-spawn-patched.exe

test: $(GAMEMD_PATCHED)
	set -e; for f in $(TESTS); do \
		wine $(BUILDDIR)/$$f; done

test_integration: $(GAMEMD_PATCHED)
	COMMAND='sh -c "WINEPREFIX=$${HOME}/project/$${BUILDDIR}/test_instances/$${PLAYER_ID}/.wine ./scripts/run_gamemd.sh"' COMMAND_PYRA2YR='python3 ./pyra2yr/test_sell_mcv.py' $(compose_cmd) up --abort-on-container-exit pyra2yr tunnel wm vnc novnc game-0 game-1

docker_base:
	docker-compose build --build-arg USER_ID=$(UID)

docker_test:
	set -e; for f in $(TESTS); do \
		docker-compose down --remove-orphans; \
		COMMAND="sh -c 'make TESTS=$$f test'" docker-compose up --abort-on-container-exit vnc builder; done

docker_build:
	docker-compose run --rm builder make CMAKE_BUILD_TYPE=$(CMAKE_BUILD_TYPE) BUILDDIR=$(BUILDDIR) EXTRA_PATCHES="$(EXTRA_PATCHES)" build

cppcheck:
	./scripts/cppcheck.sh

check: cmake_format lint format
	./scripts/check.sh

clean:
	rm -rf $(BUILDDIR); mkdir -p $(BUILDDIR)
	rm -f test_data/*.status $(GAMEMD_PATCHED)

.PHONY: build build_cpp doc lint format test docker docker_build check cppcheck clean
