CC      = gcc
CFLAGS  = -Wall -Wextra -std=c11 -D_POSIX_C_SOURCE=200809L
LDFLAGS = -lm

BUILD   = build
SRC     = src/YMLParser.c src/_da.c src/_hm.c src/_lexer.c
OBJ     = $(SRC:src/%.c=$(BUILD)/%.o)
TESTS   = $(patsubst tests/%.c,$(BUILD)/%,$(wildcard tests/test_*.c))
EXAMPLES = $(patsubst examples/%.c,$(BUILD)/%,$(wildcard examples/*.c))

.PHONY: all test clean run-test run-example

all: $(EXAMPLES)

# ── объектники библиотеки ─────────────────────────────────────────────
$(BUILD)/%.o: src/%.c | $(BUILD)
	$(CC) $(CFLAGS) -Isrc -c -o $@ $<

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
