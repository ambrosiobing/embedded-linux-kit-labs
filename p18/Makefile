# P18, the host half of the USB-CDC recorder.
#
#   make           build p18 and the suite
#   make check     build and run the suite, which needs no hardware
#   make demo      the lab's own demonstration, end to end, on a synthetic stream
#   make strict    a second pass with the conversion warnings turned on
#   make clean
#
# The warnings are errors on purpose, for the same reason as in the sibling
# lab: every one of them has a cheaper fix here than the fault it becomes on a
# board, and a build that is allowed to be noisy is a build nobody reads.

CC      ?= cc
CSTD    ?= -std=c11
WARN     = -Wall -Wextra -Werror -Wshadow -Wpointer-arith \
           -Wstrict-prototypes -Wmissing-prototypes
# Run deliberately rather than on every build. See the sibling lab's Makefile:
# a reflex cast taught by a gating warning is worse than the warning.
STRICT   = -Wconversion -Wsign-conversion -Wdouble-promotion
OPT     ?= -O2
CFLAGS  += $(CSTD) $(WARN) $(OPT) -MMD -MP
LDLIBS  += -lm

SRC      = src/lines.c src/synth.c src/run.c src/report.c
OBJ      = $(SRC:.c=.o)
MAIN_OBJ = src/main.o
TEST_OBJ = tests/test_p18.o

BIN      = p18
TEST_BIN = p18-test

.PHONY: all check demo strict clean

all: $(BIN) $(TEST_BIN)

$(BIN): $(OBJ) $(MAIN_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

$(TEST_BIN): $(OBJ) $(TEST_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

check: $(TEST_BIN)
	./$(TEST_BIN)

# The two faults this lab keeps apart, shown one after the other on streams
# whose contents are known in advance: a clean capture, then one with four
# samples withheld, then the acceptance test applied to both.
demo: $(BIN)
	./$(BIN) synth --samples 2000 --out demo.stream
	./$(BIN) sink --source demo.stream --out demo.capture \
	    --report demo.p18 --seconds 0 --rate 1000
	./$(BIN) count --source demo.capture
	./$(BIN) verify --report demo.p18 --allow-unmeasured-rate
	./$(BIN) synth --samples 2000 --drop 10,11,12,13 --out holed.stream
	./$(BIN) sink --source holed.stream --report holed.p18 --seconds 0
	./$(BIN) verify --report holed.p18 --allow-unmeasured-rate || \
	    echo "refused, as it should be: four samples in two thousand is above the limit"

strict:
	$(MAKE) clean
	$(MAKE) CFLAGS="$(CSTD) $(WARN) $(STRICT) $(OPT) -MMD -MP" all

clean:
	rm -f $(OBJ) $(MAIN_OBJ) $(TEST_OBJ) \
	      $(OBJ:.o=.d) $(MAIN_OBJ:.o=.d) $(TEST_OBJ:.o=.d) \
	      $(BIN) $(TEST_BIN) *.stream *.capture *.p18

-include $(OBJ:.o=.d) $(MAIN_OBJ:.o=.d) $(TEST_OBJ:.o=.d)
