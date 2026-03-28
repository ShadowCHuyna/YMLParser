CC      = gcc
CFLAGS  = -Wall -Wextra -std=c23
LDFLAGS = -lm

BUILD   = build
SRC     = src/YMLParser.c src/_da.c src/_hm.c src/_lexer.c
OBJ     = $(SRC:src/%.c=$(BUILD)/%.o)
OBJ_PIC = $(SRC:src/%.c=$(BUILD)/%_pic.o)
TESTS   = $(patsubst tests/%.c,$(BUILD)/%,$(wildcard tests/test_*.c))
EXAMPLES = $(patsubst examples/%.c,$(BUILD)/%,$(wildcard examples/*.c))

# ── выбор платформы ───────────────────────────────────────────────────
# Автоопределение: Linux / Darwin → linux, иначе — задать вручную.
# Пример для Windows: make lib-shared PLATFORM=windows CC=x86_64-w64-mingw32-gcc
PLATFORM ?= $(shell uname -s 2>/dev/null | tr '[:upper:]' '[:lower:]' | sed 's/darwin/linux/' || echo linux)

ifeq ($(PLATFORM),windows)
    LIB_STATIC = $(BUILD)/YMLParser.lib
    LIB_SHARED = $(BUILD)/YMLParser.dll
    STATIC_CMD = ar rcs $(LIB_STATIC) $(OBJ)
    SHARED_CMD = $(CC) -shared -o $(LIB_SHARED) $(OBJ_PIC) $(LDFLAGS)
else
    LIB_STATIC = $(BUILD)/libYMLParser.a
    LIB_SHARED = $(BUILD)/libYMLParser.so
    STATIC_CMD = ar rcs $(LIB_STATIC) $(OBJ)
    SHARED_CMD = $(CC) -shared -fPIC -o $(LIB_SHARED) $(OBJ_PIC) $(LDFLAGS)
endif

.PHONY: all test clean run-test run-example lib-static lib-shared

all: $(EXAMPLES)

# ── объектники библиотеки ─────────────────────────────────────────────
$(BUILD)/%.o: src/%.c | $(BUILD)
	$(CC) $(CFLAGS) -Isrc -c -o $@ $<

# ── объектники с -fPIC (для .so / .dll) ───────────────────────────────
$(BUILD)/%_pic.o: src/%.c | $(BUILD)
	$(CC) $(CFLAGS) -fPIC -Isrc -c -o $@ $<

# ── статическая библиотека (.a / .lib) ────────────────────────────────
lib-static: $(OBJ) | $(BUILD)
	$(STATIC_CMD)
	@echo "static lib → $(LIB_STATIC)"

# ── динамическая библиотека (.so / .dll) ──────────────────────────────
lib-shared: $(OBJ_PIC) | $(BUILD)
	$(SHARED_CMD)
	@echo "shared lib → $(LIB_SHARED)"

# ── тесты ─────────────────────────────────────────────────────────────
$(BUILD)/test_%: tests/test_%.c $(OBJ) | $(BUILD)
	$(CC) $(CFLAGS) -Isrc -Itests $(LDFLAGS) -o $@ $^

# ── примеры ───────────────────────────────────────────────────────────
$(BUILD)/%: examples/%.c $(OBJ) | $(BUILD)
	$(CC) $(CFLAGS) -Isrc $(LDFLAGS) -o $@ $^

$(BUILD):
	mkdir -p $(BUILD)

# ── make test  — запустить все тесты ──────────────────────────────────
test: $(TESTS)
	@passed=0; failed=0; \
	for t in $(TESTS); do \
		echo "--- $$t ---"; \
		if $$t; then passed=$$((passed+1)); \
		else failed=$$((failed+1)); fi; \
		echo ""; \
	done; \
	[ $$failed -eq 0 ] \
		&& echo "=== ALL PASSED ($$passed) ===" \
		|| { echo "=== FAILED: $$failed / $$((passed+failed)) ==="; exit 1; }

# ── make run-test T=test_scalars  — запустить один тест ───────────────
run-test: $(BUILD)/$(T)
	@$(BUILD)/$(T)

# ── make run-example [E=example]  — запустить пример ──────────────────
# Примечание: examples/example.c намеренно завершается с assert.
E ?= example
run-example: $(BUILD)/$(E)
	@$(BUILD)/$(E); true

clean:
	rm -rf $(BUILD)
