# mcu-max tests
#
#   make test       Runs all tests (requires: python3 -m pip install chess)
#   make size       Checks the Cortex-M0 code size (requires arm-none-eabi-gcc)
#   make strength   Plays a match against the previous build (slow)
#
# Variables: CC (default cc), GAMES (number of random games, default 100)

CC ?= cc
PYTHON ?= python3
GAMES ?= 100

# Code size budget (bytes): raise only on purpose
SIZE_BUDGET ?= 2129

SRC := ../src/mcu-max.c
HEADER := ../src/mcu-max.h
CFLAGS := -std=c99 -Wall -Wextra -pedantic-errors -O2 -I../src
SANITIZE := -fsanitize=address,undefined -fno-sanitize-recover=all -g
HASH := -DMCUMAX_HASHING_ENABLED -DMCUMAX_HASH_TABLE_SIZE=65536

BUILD := build

.PHONY: test strict api engine engine-hash engine-hash-tiny regression size strength endgame clean

test: strict api engine engine-hash engine-hash-tiny regression

$(BUILD):
	mkdir -p $(BUILD)

# Strict standard C compliance (library, with and without hashing; UCI example)
strict: | $(BUILD)
	$(CC) $(CFLAGS) -c $(SRC) -o $(BUILD)/strict.o
	$(CC) $(CFLAGS) $(HASH) -c $(SRC) -o $(BUILD)/strict_hash.o
	$(CC) $(CFLAGS) -DMCUMAX_PAWN_EVAL_DISABLED -c $(SRC) -o $(BUILD)/strict_nopawn.o
	$(CC) $(CFLAGS) ../examples/mcu-max-uci/main.c $(SRC) -o $(BUILD)/mcu-max-uci
	@echo "Strict build passed"

$(BUILD)/test_api: test_api.c $(SRC) $(HEADER) | $(BUILD)
	$(CC) $(CFLAGS) $(SANITIZE) test_api.c $(SRC) -o $@

$(BUILD)/harness: harness.c $(SRC) $(HEADER) | $(BUILD)
	$(CC) $(CFLAGS) harness.c $(SRC) -o $@

$(BUILD)/harness_hash: harness.c $(SRC) $(HEADER) | $(BUILD)
	$(CC) $(CFLAGS) $(HASH) harness.c $(SRC) -o $@

api: $(BUILD)/test_api
	$(BUILD)/test_api

engine: $(BUILD)/harness
	$(PYTHON) test_engine.py $(BUILD)/harness --games $(GAMES)

engine-hash: $(BUILD)/harness_hash
	$(PYTHON) test_engine.py $(BUILD)/harness_hash --games $(GAMES) --seed 2

# Tiny hash table: frequent index collisions
$(BUILD)/harness_hash_tiny: harness.c $(SRC) $(HEADER) | $(BUILD)
	$(CC) $(CFLAGS) -DMCUMAX_HASHING_ENABLED -DMCUMAX_HASH_TABLE_SIZE=64 harness.c $(SRC) -o $@

engine-hash-tiny: $(BUILD)/harness_hash_tiny
	$(PYTHON) test_engine.py $(BUILD)/harness_hash_tiny --games $(GAMES) --seed 3

regression: $(BUILD)/harness
	$(PYTHON) regression.py $(BUILD)/harness

size:
	$(PYTHON) size.py --budget $(SIZE_BUDGET)

# Strength: current build against a reference UCI engine command
# (e.g. make strength REFERENCE=path/to/old/mcu-max-uci)
REFERENCE ?= stockfish
STRENGTH_GAMES ?= 200
NODES ?= 10000

strength: strict
	$(PYTHON) strength.py $(BUILD)/mcu-max-uci "$(REFERENCE)" \
		--games $(STRENGTH_GAMES) --nodes $(NODES)

# Endgame conversion (KQ-K, KR-K, KP-K) against Stockfish (slow)
endgame: strict
	$(PYTHON) endgame.py $(BUILD)/mcu-max-uci --defender "$(REFERENCE)"

clean:
	rm -rf $(BUILD)
