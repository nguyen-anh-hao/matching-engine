.DEFAULT_GOAL := help
SHELL := /bin/bash

ifneq (,$(wildcard .env))
include .env
export
endif

BUILD_DIR ?= build
ASAN_BUILD_DIR ?= build-asan
CMAKE ?= cmake
CTEST ?= ctest
PORT ?= 9001
IMAGE ?= matching-engine:local
FORMAT ?= clang-format
SOURCES := $(shell git ls-files '*.cpp' '*.hpp')

.PHONY: help env configure build run test smoke bench lint format asan check docker clean

help: ## Show available targets
	@grep -E '^[a-zA-Z_-]+:.*?## ' $(MAKEFILE_LIST) | awk 'BEGIN {FS = ":.*?## "}; {printf "  %-10s %s\n", $$1, $$2}'

env: ## Create .env from .env.example when missing
	@test -f .env || cp .env.example .env

configure: ## Configure CMake (Release, warnings as errors)
	$(CMAKE) -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Release -DENGINE_WARNINGS_AS_ERRORS=ON

build: configure ## Compile the engine
	$(CMAKE) --build $(BUILD_DIR) -j

run: build ## Run the engine on $(PORT)
	./$(BUILD_DIR)/src/matching-engine $(PORT)

test: build ## Run unit tests
	$(CTEST) --test-dir $(BUILD_DIR) --output-on-failure

smoke: build ## Run the HTTP end-to-end smoke test
	bash tests/integration/http_smoke_test.sh ./$(BUILD_DIR)/src/matching-engine

bench: build ## Run the matching benchmark
	./$(BUILD_DIR)/benchmarks/benchmark_matching

lint: ## Check source formatting with clang-format
	@command -v $(FORMAT) >/dev/null 2>&1 || { echo "$(FORMAT) not found"; exit 1; }
	$(FORMAT) --dry-run --Werror $(SOURCES)

format: ## Format sources with clang-format
	@command -v $(FORMAT) >/dev/null 2>&1 || { echo "$(FORMAT) not found"; exit 1; }
	$(FORMAT) -i $(SOURCES)

asan: ## Build and test with AddressSanitizer/UBSan (Debug)
	$(CMAKE) -S . -B $(ASAN_BUILD_DIR) -DCMAKE_BUILD_TYPE=Debug \
		-DENGINE_SANITIZERS=ON -DENGINE_WARNINGS_AS_ERRORS=ON
	$(CMAKE) --build $(ASAN_BUILD_DIR) -j
	$(CTEST) --test-dir $(ASAN_BUILD_DIR) --output-on-failure

check: test smoke ## Build, run unit tests and the HTTP smoke test

docker: ## Build the container image
	docker build -t $(IMAGE) .

clean: ## Remove the build directories
	rm -rf $(BUILD_DIR) $(ASAN_BUILD_DIR)
