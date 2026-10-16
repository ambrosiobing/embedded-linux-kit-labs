# P06, the host half of the 8x8 time-of-flight occupancy kiosk.
#
#   make           build p06 and the suite
#   make check     build and run the suite, which needs no hardware
#   make demo      the lab's own demonstration, end to end, with no display
#   make strict    a second pass with the conversion warnings turned on
#   make clean
#
# The warnings are errors on purpose, for the same reason as in the two
# sibling labs. This one has a further reason of its own: it writes into a
# memory mapping whose size is decided by someone else, and the suite paints
# into a file with a guard area to prove it stays inside.

CC      ?= cc
CSTD    ?= -std=c11
WARN     = -Wall -Wextra -Werror -Wshadow -Wpointer-arith \
           -Wstrict-prototypes -Wmissing-prototypes
STRICT   = -Wconversion -Wsign-conversion -Wdouble-promotion
OPT     ?= -O2
CFLAGS  += $(CSTD) $(WARN) $(OPT) -MMD -MP

SRC      = src/frame.c src/book.c src/fb.c src/synth.c src/run.c
OBJ      = $(SRC:.c=.o)
MAIN_OBJ = src/main.o
TEST_OBJ = tests/test_p06.o

BIN      = p06
TEST_BIN = p06-test

.PHONY: all check demo strict clean

all: $(BIN) $(TEST_BIN)

$(BIN): $(OBJ) $(MAIN_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

$(TEST_BIN): $(OBJ) $(TEST_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

check: $(TEST_BIN)
	./$(TEST_BIN)

# The chapter's own sequence, with a file standing in for the sensor and no
# display anywhere: measure the wall, look at the numbers, then judge a hand
# against the threshold that measurement produced.
demo: $(BIN)
	./$(BIN) synth --frames 30 --scene empty --out empty.stream
	./$(BIN) baseline --source empty.stream --out bench.book \
	    --placement "synthetic, no sensor"
	./$(BIN) show --source empty.stream --book bench.book --frames 1
	./$(BIN) synth --frames 30 --scene hand --hand-zones 6 --out hand.stream
	./$(BIN) run --source hand.stream --book bench.book --expect-zones 4:10
	./$(BIN) run --source empty.stream --book bench.book --expect-zones 0:0

strict:
	$(MAKE) clean
	$(MAKE) CFLAGS="$(CSTD) $(WARN) $(STRICT) $(OPT) -MMD -MP" all

clean:
	rm -f $(OBJ) $(MAIN_OBJ) $(TEST_OBJ) \
	      $(OBJ:.o=.d) $(MAIN_OBJ:.o=.d) $(TEST_OBJ:.o=.d) \
	      $(BIN) $(TEST_BIN) *.stream *.book *.glass
	rm -rf test_p06_sysfs

-include $(OBJ:.o=.d) $(MAIN_OBJ:.o=.d) $(TEST_OBJ:.o=.d)
