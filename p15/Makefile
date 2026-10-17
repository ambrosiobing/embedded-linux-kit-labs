# P15, the semaphore of the mixed-voltage discipline lab.
#
#   make           build p15 and the suite
#   make check     build and run the suite, which needs no chip
#   make demo      the three subcommands that need no bench, and one refusal
#   make strict    a second pass with the conversion warnings turned on
#   make clean
#
# No library from lib/ here. This lab's host side is three output lines and a
# plan, and nothing in it has a second user yet. The GPIO character device is
# reached through linux/gpio.h directly, so the suite builds wherever these
# programs are compiled rather than only on the board.
#
# This program writes no files, so clean removes only what the compiler made.

CC      ?= cc
CSTD    ?= -std=c11
WARN     = -Wall -Wextra -Werror -Wshadow -Wpointer-arith \
           -Wstrict-prototypes -Wmissing-prototypes
STRICT   = -Wconversion -Wsign-conversion -Wdouble-promotion
OPT     ?= -O2
CFLAGS  += $(CSTD) $(WARN) $(OPT) -MMD -MP

SRC      = src/pattern.c src/plan.c src/lines.c
OBJ      = $(SRC:.c=.o)
MAIN_OBJ = src/main.o
TEST_OBJ = tests/test_p15.o

BIN      = p15
TEST_BIN = p15-test

.PHONY: all check demo strict clean

all: $(BIN) $(TEST_BIN)

$(BIN): $(OBJ) $(MAIN_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

$(TEST_BIN): $(OBJ) $(TEST_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

check: $(TEST_BIN)
	./$(TEST_BIN)

# What the lab book can be prepared from, on a laptop with no board, and one
# command that is supposed to fail here.
demo: $(BIN)
	./$(BIN) plan
	@echo
	./$(BIN) budget
	@echo
	./$(BIN) wiring
	@echo
	./$(BIN) probe || \
	    echo "refused: no chip on this machine, which is the right answer and not a defect"

strict:
	$(MAKE) clean
	$(MAKE) CFLAGS="$(CSTD) $(WARN) $(STRICT) $(OPT) -MMD -MP" all

clean:
	rm -f $(OBJ) $(MAIN_OBJ) $(TEST_OBJ) \
	      $(OBJ:.o=.d) $(MAIN_OBJ:.o=.d) $(TEST_OBJ:.o=.d) \
	      $(BIN) $(TEST_BIN)

-include $(OBJ:.o=.d) $(MAIN_OBJ:.o=.d) $(TEST_OBJ:.o=.d)
