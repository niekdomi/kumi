.PHONY: build release clean test lint lint-diff check-format format

SOURCES := $(shell find src tests -type f \( -name '*.cpp' -o -name '*.cppm' \) ! -path "*/build/*")

# -----------------------------
# Build Targets
# -----------------------------
build:
	@mkdir -p build/debug && \
		CC=clang CXX=clang++ cmake -G Ninja -Wno-dev -S . -B build/debug -DCMAKE_BUILD_TYPE=Debug && \
		cmake --build build/debug --parallel

release:
	@mkdir -p build/release && \
		CC=clang CXX=clang++ cmake -G Ninja -Wno-dev -S . -B build/release -DCMAKE_BUILD_TYPE=Release && \
		cmake --build build/release --parallel

test: build
	@build/kumi_tests

clean:
	@rm -rf build CMakeFiles CMakeCache.txt .cache

# -----------------------------
# Utility Targets
# -----------------------------
RUN_CLANG_TIDY_CMD := run-clang-tidy
CLANG_FORMAT_CMD := clang-format

LINT_COMMON_FLAGS = -p build/debug -quiet
LINT_TIDY_FLAGS = -warnings-as-errors='*'
LINT_CPUS ?= $(shell nproc)

# Function to check for tool existence
# Usage: $(call check_tool, tool_name)
define check_tool
@if ! command -v $(1) > /dev/null 2>&1; then \
	echo "Error: Required tool '$(1)' not found."; \
	echo "Please ensure it is installed and available in your PATH."; \
	exit 1; \
fi
endef

lint:
	$(call check_tool,$(RUN_CLANG_TIDY_CMD))
	@echo "Linting with $(LINT_CPUS) cores"
	@$(RUN_CLANG_TIDY_CMD) $(LINT_COMMON_FLAGS) $(LINT_TIDY_FLAGS) -j $(LINT_CPUS) $(SOURCES) || exit 1
	@echo "✓ Linting complete"

lint-diff:
	$(call check_tool,$(RUN_CLANG_TIDY_CMD))
	@echo "Linting changed files compared to main branch..."
	@CHANGED_FILES=$$(git diff --name-only --diff-filter=ACM main...HEAD | grep -E '\.(cpp|cppm)$$' || true); \
	if [ -z "$$CHANGED_FILES" ]; then \
		echo "No C++ files changed."; \
		exit 0; \
	fi; \
	echo "Files to lint: $$CHANGED_FILES"; \
	$(RUN_CLANG_TIDY_CMD) $(LINT_COMMON_FLAGS) $(LINT_TIDY_FLAGS) -j $(LINT_CPUS) $$CHANGED_FILES || exit 1; \
	echo "✓ Linting complete"

check-format:
	$(call check_tool,$(CLANG_FORMAT_CMD))
	@echo "Checking code formatting..."
	@if $(CLANG_FORMAT_CMD) --dry-run --Werror $(SOURCES); then \
		echo "✓ All files are properly formatted"; \
	else \
		exit 1; \
	fi

format:
	$(call check_tool,$(CLANG_FORMAT_CMD))
	@echo "Formatting code..."
	@$(CLANG_FORMAT_CMD) -i $(SOURCES)
	@echo "✓ Code formatting complete"
