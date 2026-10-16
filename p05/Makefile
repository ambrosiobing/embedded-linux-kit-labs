# P05, the file half of the calibrated voltage recorder.
#
#   make           build p05 and the suite
#   make check     build and run the suite, which needs no hardware
#   make demo      six files the board did not produce, judged
#   make strict    a second pass with the conversion warnings turned on
#   make clean
#
# The recorder is not here. The acquisition is four calls into the vendor's
# library, which owns the calibration held in the board's EEPROM, and the
# chapter prints them. This builds the half that starts at the file.

CC      ?= cc
CSTD    ?= -std=c11
WARN     = -Wall -Wextra -Werror -Wshadow -Wpointer-arith \
           -Wstrict-prototypes -Wmissing-prototypes
STRICT   = -Wconversion -Wsign-conversion -Wdouble-promotion
OPT     ?= -O2
CFLAGS  += $(CSTD) $(WARN) $(OPT) -MMD -MP
LDLIBS  += -lm

SRC      = src/scan.c src/book.c src/synth.c
OBJ      = $(SRC:.c=.o)
MAIN_OBJ = src/main.o
TEST_OBJ = tests/test_p05.o

BIN      = p05
TEST_BIN = p05-test

.PHONY: all check demo strict clean

all: $(BIN) $(TEST_BIN)

$(BIN): $(OBJ) $(MAIN_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

$(TEST_BIN): $(OBJ) $(TEST_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

check: $(TEST_BIN)
	./$(TEST_BIN)

# One capture that is whole, five that are not, and the verdict this bench
# can actually reach.
demo: $(BIN)
	./$(BIN) scene --kind good --out good.csv
	./$(BIN) check --source good.csv
	./$(BIN) scene --kind repeat --out repeat.csv
	./$(BIN) check --source repeat.csv || \
	    echo "refused: more rows than a whole file, and one index written twice"
	./$(BIN) book --out bench.p05book --address 0 --header-clear \
	    --survived-reboot --second-run-clean --date "a day at this bench"
	./$(BIN) verify --source good.csv --book bench.p05book || \
	    echo "stopped one item short, and the item is a multimeter"

strict:
	$(MAKE) clean
	$(MAKE) CFLAGS="$(CSTD) $(WARN) $(STRICT) $(OPT) -MMD -MP" all

clean:
	rm -f $(OBJ) $(MAIN_OBJ) $(TEST_OBJ) \
	      $(OBJ:.o=.d) $(MAIN_OBJ:.o=.d) $(TEST_OBJ:.o=.d) \
	      $(BIN) $(TEST_BIN) *.csv *.p05book

-include $(OBJ:.o=.d) $(MAIN_OBJ:.o=.d) $(TEST_OBJ:.o=.d)
