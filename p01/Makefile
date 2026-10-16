# P01, the shock event publisher, which P09 also runs.
#
#   make           build p01 and the suite
#   make check     build and run the suite, which needs no hardware
#   make demo      five benches that were never knocked, and the budget
#   make strict    a second pass with the conversion warnings turned on
#   make clean
#
# One program, two chapters. P01 runs it on bus 1 and P09 on bus 0, which is
# an argument rather than a second build. The live read is Linux only and is
# compiled everywhere Linux is, so continuous integration type-checks it even
# though no bus is there to answer.

CC      ?= cc
CSTD    ?= -std=c11
WARN     = -Wall -Wextra -Werror -Wshadow -Wpointer-arith \
           -Wstrict-prototypes -Wmissing-prototypes
STRICT   = -Wconversion -Wsign-conversion -Wdouble-promotion
OPT     ?= -O2
CFLAGS  += $(CSTD) $(WARN) $(OPT) -MMD -MP
LDLIBS  += -lm

SRC      = src/adxl.c src/policy.c src/scene.c
OBJ      = $(SRC:.c=.o)
MAIN_OBJ = src/main.o
TEST_OBJ = tests/test_p01.o

BIN      = p01
TEST_BIN = p01-test

.PHONY: all check demo strict clean

all: $(BIN) $(TEST_BIN)

$(BIN): $(OBJ) $(MAIN_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

$(TEST_BIN): $(OBJ) $(TEST_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

check: $(TEST_BIN)
	./$(TEST_BIN)

# The two scenes that must produce nothing, the one that must produce five,
# and the arithmetic P09 asks for.
demo: $(BIN)
	./$(BIN) decode --bytes "00 00 00 00 00 01"
	./$(BIN) decode --bytes "00 00 00 00 00 ff"
	./$(BIN) scene --kind rest --out rest.samples
	./$(BIN) replay --source rest.samples
	./$(BIN) scene --kind on-its-side --out side.samples
	./$(BIN) replay --source side.samples
	./$(BIN) scene --kind ringing --out ringing.samples
	./$(BIN) replay --source ringing.samples
	./$(BIN) budget --rate 20 --cooloff 2 --fsync 30 --record 48 --hours 24

strict:
	$(MAKE) clean
	$(MAKE) CFLAGS="$(CSTD) $(WARN) $(STRICT) $(OPT) -MMD -MP" all

clean:
	rm -f $(OBJ) $(MAIN_OBJ) $(TEST_OBJ) \
	      $(OBJ:.o=.d) $(MAIN_OBJ:.o=.d) $(TEST_OBJ:.o=.d) \
	      $(BIN) $(TEST_BIN) *.csv *.p01book

-include $(OBJ:.o=.d) $(MAIN_OBJ:.o=.d) $(TEST_OBJ:.o=.d)
