BUILDDIR := build
CMAKE_BUILD_TYPE := Release
REPO_FILES = $(shell git ls-tree -r --name-only HEAD) 
TESTS = $(patsubst %.cpp,$(BUILDDIR)/%.exe,$(filter tests/test_%.cpp, $(REPO_FILES)))

export PLAYERS_CONFIG := ./test_data/envs.tsv
export CM_FILES = $(filter %CMakeLists.txt, $(REPO_FILES))
export CPP_SOURCES = $(filter %.cpp %.hpp, $(REPO_FILES))
export RA2YRCPP_GAME_DIR := game/main
export RA2YRCPP_TEST_INSTANCES_DIR := $(realpath game)
# disables old integration test
#export RA2YRCPP_TUNNEL_URL := 0.0.0.0
export W32_FILES := process.cpp state_parser.cpp dll_inject.cpp network.cpp
GAMEMD_PATCHED := $(RA2YRCPP_GAME_DIR)/gamemd-spawn-patched.exe

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

build:
	mkdir -p $(BUILDDIR)
	cmake -S . -B $(BUILDDIR)
	cmake --build $(BUILDDIR) --config $(CMAKE_BUILD_TYPE) --target all -j $(nproc)

$(GAMEMD_PATCHED): $(RA2YRCPP_GAME_DIR)/gamemd-spawn.exe
	python ./scripts/patch_gamemd.py $< > $@

patch: $(GAMEMD_PATCHED)

test_data/wine_envs.status: $(PLAYERS_CONFIG) patch
	for name in $(shell tail -n+2 $< | cut -f1); do \
		export WINEPREFIX=$(RA2YRCPP_TEST_INSTANCES_DIR)/$$name/.wine; \
		mkdir -p $$WINEPREFIX; \
		( export WINEARCH=win32; wineboot -ik; wine regedit test_data/env.reg ); \
	done
	./scripts/prep_instance_dirs.sh
	touch $@

test_environment: test_data/wine_envs.status

test:
	set -e; for f in $(TESTS); do \
		WINEPATH="./$(BUILDDIR)/src" wine $$f; done

test_integration:
	./scripts/test_gamemd_tunnel.sh

docker:
	docker-compose build

docker_build:
	docker-compose run --rm builder make BUILDDIR=$(BUILDDIR) build

cppcheck:
	./scripts/cppcheck.sh

check: cmake_format lint format
	./scripts/check.sh

clean:
	rm -rf $(BUILDDIR); mkdir -p $(BUILDDIR)
	rm -f test_data/*.status $(GAMEMD_PATCHED)

.PHONY: build doc lint format test docker docker_build check cppcheck clean
