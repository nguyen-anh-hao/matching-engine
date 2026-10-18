.DEFAULT_GOAL := help
SHELL := /bin/bash

ifneq (,$(wildcard .env))
include .env
export
endif

BUILD_DIR ?= build
CMAKE ?= cmake
CTEST ?= ctest
PORT ?= 9001
IMAGE ?= matching-engine:local

.PHONY: help env configure build run test bench lint check docker clean

help: ## Show available targets
	@grep -E '^[a-zA-Z_-]+:.*?## ' $(MAKEFILE_LIST) | awk 'BEGIN {FS = ":.*?## "}; {printf "  %-10s %s\n", $$1, $$2}'

env: ## Create .env from .env.example when missing
	@test -f .env || cp .env.example .env

configure: ## Configure CMake
	$(CMAKE) -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Release

build: configure ## Compile the engine
	$(CMAKE) --build $(BUILD_DIR) -j

run: build ## Run the engine on $(PORT)
	./$(BUILD_DIR)/matching-engine $(PORT)

test: build ## Run unit tests
	$(CTEST) --test-dir $(BUILD_DIR) --output-on-failure

bench: build ## Run the matching benchmark
	./$(BUILD_DIR)/benchmark_matching

lint: ## No linter configured
	@echo "matching-engine has no linter configured"

check: test ## Build and test

docker: ## Build the container image
	docker build -t $(IMAGE) .

clean: ## Remove the build directory
	rm -rf $(BUILD_DIR)
