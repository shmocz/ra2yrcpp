BUILDDIR ?= build
CMAKE_BUILD_TYPE ?= Release
REPO_FILES = $(shell git ls-tree -r --name-only HEAD) 
TESTS := $(patsubst %.cpp,%.exe,$(subst tests/,bin/,$(filter tests/test_%.cpp, $(REPO_FILES))))
PYTHON := python3
export CMAKE_RUNTIME_OUTPUT_DIRECTORY := $(BUILDDIR)/bin

export PLAYERS_CONFIG := ./test_data/envs.tsv
export CM_FILES = $(filter %CMakeLists.txt, $(REPO_FILES))
export CPP_SOURCES = $(filter %.cpp %.hpp, $(REPO_FILES))
# disables old integration test
#export RA2YRCPP_TUNNEL_URL := 0.0.0.0
export W32_FILES := process.cpp state_parser.cpp dll_inject.cpp network.cpp manager.cpp
GAMEMD_PATCHED := $(CMAKE_RUNTIME_OUTPUT_DIRECTORY)/gamemd-spawn-patched.exe

export UID := $(shell id -u)
export GID := $(shell id -g)

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

build: $(GAMEMD_PATCHED)
	mkdir -p $(BUILDDIR)
	cmake -S . -B $(BUILDDIR)
	cmake --build $(BUILDDIR) --config $(CMAKE_BUILD_TYPE) --target all -j $(nproc)

$(GAMEMD_PATCHED): gamemd-spawn.exe
	$(PYTHON) ./scripts/patch_gamemd.py $< > .gamemd-spawn-patched.exe 
	install -D .gamemd-spawn-patched.exe $@
	rm .gamemd-spawn-patched.exe

test: $(GAMEMD_PATCHED)
	set -e; for f in $(TESTS); do \
		wine $(BUILDDIR)/$$f; done

test_integration: $(GAMEMD_PATCHED)
	./scripts/test_gamemd_tunnel.sh

docker_base:
	docker-compose build --build-arg USER_ID=$(UID)

docker_test:
	docker-compose down -t 5
	set -e; for f in $(TESTS); do \
		COMMAND="sh -c 'make TESTS=$$f test'" docker-compose up --abort-on-container-exit vnc builder; done

docker_build:
	docker-compose run --rm builder make CMAKE_BUILD_TYPE=$(CMAKE_BUILD_TYPE) BUILDDIR=$(BUILDDIR) build

cppcheck:
	./scripts/cppcheck.sh

check: cmake_format lint format
	./scripts/check.sh

clean:
	rm -rf $(BUILDDIR); mkdir -p $(BUILDDIR)
	rm -f test_data/*.status $(GAMEMD_PATCHED)

.PHONY: build doc lint format test docker docker_build check cppcheck clean
