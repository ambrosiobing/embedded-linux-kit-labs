# P04, the host half of the vibration and ultrasound gateway.
#
#   make           build p04 and the suite
#   make check     build and run the suite, which needs no hardware
#   make demo      the lab's own demonstration, end to end, on synthetic data
#   make clean
#
# The warnings are errors on purpose. Every one of them has a cheaper fix now
# than the fault it becomes on a board, and a build that is allowed to be noisy
# is a build nobody reads.

CC      ?= cc
CSTD    ?= -std=c11
# The gate. These are the warnings that are faults wearing a different hat, and
# they are errors so that nobody has to decide in a hurry whether one matters.
WARN     = -Wall -Wextra -Werror -Wshadow -Wpointer-arith \
           -Wstrict-prototypes -Wmissing-prototypes
# A second, stricter pass, run deliberately rather than on every build. Signed
# and unsigned conversions in code that turns counts into floats and back
# deserve a reading rather than a reflex cast, and mixing that reading into the
# gate teaches people to silence it. See: make strict
STRICT   = -Wconversion -Wsign-conversion -Wdouble-promotion
OPT     ?= -O2
CFLAGS  += $(CSTD) $(WARN) $(OPT) -MMD -MP
LDLIBS  += -lm

SRC      = src/block.c src/dsp.c src/analysis.c src/labbook.c src/synth.c
OBJ      = $(SRC:.c=.o)
MAIN_OBJ = src/main.o
TEST_OBJ = tests/test_p04.o

BIN      = p04
TEST_BIN = p04-test

.PHONY: all check demo strict clean

all: $(BIN) $(TEST_BIN)

$(BIN): $(OBJ) $(MAIN_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

$(TEST_BIN): $(OBJ) $(TEST_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

check: $(TEST_BIN)
	./$(TEST_BIN)

# The demonstration the chapter describes: a case touch reads as a rise in the
# band with the microphone unmoved, a voice does the opposite, and a stream
# with holes in it is refused rather than classified.
demo: $(BIN)
	./$(BIN) synth --kind idle   --blocks 8 --out idle.p04
	./$(BIN) synth --kind touch  --blocks 8 --out touch.p04
	./$(BIN) synth --kind speech --blocks 8 --out speech.p04
	./$(BIN) baseline --source idle.p04 --out book.lab \
	    --mounting "synthetic, no probe" --warn 1.5 --fault 3.0
	./$(BIN) validate  --source idle.p04   --book book.lab
	./$(BIN) analyse   --source touch.p04  --book book.lab
	./$(BIN) analyse   --source speech.p04 --book book.lab
	./$(BIN) replay    --source touch.p04  --book book.lab

# The stricter pass, from clean, so that every file is actually recompiled.
# It is expected to have something to say. Reading what it says is the point.
strict:
	$(MAKE) clean
	$(MAKE) CFLAGS="$(CSTD) $(WARN) $(STRICT) $(OPT) -MMD -MP" all

clean:
	rm -f $(OBJ) $(MAIN_OBJ) $(TEST_OBJ) \
	      $(OBJ:.o=.d) $(MAIN_OBJ:.o=.d) $(TEST_OBJ:.o=.d) \
	      $(BIN) $(TEST_BIN) *.p04 *.lab

-include $(OBJ:.o=.d) $(MAIN_OBJ:.o=.d) $(TEST_OBJ:.o=.d)
