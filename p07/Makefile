# P07 and P08, which share one program.
#
#   make           build p07 and the suite
#   make check     build and run the suite, which needs no shield
#   make demo      three captures that pass, four that do not, and the pair
#   make strict    a second pass with the conversion warnings turned on
#   make clean
#
# One directory for two labs, as p01 is for P01 and P09. The reason differs:
# there it is one program with a different bus number, here it is that P08's
# third acceptance item is an item about P07, and a check that spans two labs
# has to be written once in a place that knows both.
#
# From lib/: the line reader. The JSON key extractor is this lab's own and is
# not lifted, because nothing else here reads JSON yet.

CC      ?= cc
CSTD    ?= -std=c11
WARN     = -Wall -Wextra -Werror -Wshadow -Wpointer-arith \
           -Wstrict-prototypes -Wmissing-prototypes
STRICT   = -Wconversion -Wsign-conversion -Wdouble-promotion
OPT     ?= -O2
CFLAGS  += $(CSTD) $(WARN) $(OPT) -MMD -MP

SRC      = src/keys.c src/log.c src/scene.c
OBJ      = $(SRC:.c=.o)
MAIN_OBJ = src/main.o
TEST_OBJ = tests/test_p07.o

BIN      = p07
TEST_BIN = p07-test

ARTEFACTS = p07.jsonl p08_lowg.jsonl p08_highg.jsonl \
            drift.jsonl collapsed.jsonl hubbed.jsonl noscale.jsonl \
            test_p07_a.jsonl test_p07_b.jsonl

.PHONY: all check demo strict clean

all: $(BIN) $(TEST_BIN)

$(BIN): $(OBJ) $(MAIN_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

$(TEST_BIN): $(OBJ) $(TEST_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

check: $(TEST_BIN)
	./$(TEST_BIN)

demo: $(BIN)
	./$(BIN) profiles
	@echo
	./$(BIN) scene --kind iks4a1    --records 50 --out p07.jsonl
	./$(BIN) scene --kind lowg      --records 50 --out p08_lowg.jsonl
	./$(BIN) scene --kind highg     --records 50 --out p08_highg.jsonl
	./$(BIN) scene --kind drift     --records 50 --out drift.jsonl
	./$(BIN) scene --kind collapsed --records 50 --out collapsed.jsonl
	./$(BIN) scene --kind hubbed    --records 50 --out hubbed.jsonl
	./$(BIN) scene --kind noscale   --records 50 --out noscale.jsonl
	@echo
	./$(BIN) verify --source p07.jsonl       --profile iks4a1
	@echo
	./$(BIN) verify --source p08_lowg.jsonl  --profile lowg
	@echo
	./$(BIN) verify --source p08_highg.jsonl --profile highg
	@echo
	./$(BIN) verify --source drift.jsonl --profile iks4a1 || \
	    echo "refused: a schema that drifts leaves a file no parser reads end to end"
	@echo
	./$(BIN) verify --source hubbed.jsonl --profile iks4a1 || \
	    echo "refused: short from the first line, which is a sensor-hub mode and not a drift"
	@echo
	./$(BIN) verify --source collapsed.jsonl --profile lowg || \
	    echo "refused: the two streams merged into one field, which is the error P08 names"
	@echo
	./$(BIN) verify --source noscale.jsonl --profile lowg || \
	    echo "refused: a pressure on a two-scale part, with no range beside it"
	@echo
	./$(BIN) pair --a p08_lowg.jsonl --b p08_highg.jsonl
	@echo
	./$(BIN) pair --a p08_lowg.jsonl --b p08_lowg.jsonl || \
	    echo "refused: one file given twice is one log"

strict:
	$(MAKE) clean
	$(MAKE) CFLAGS="$(CSTD) $(WARN) $(STRICT) $(OPT) -MMD -MP" all

clean:
	rm -f $(OBJ) $(MAIN_OBJ) $(TEST_OBJ) \
	      $(OBJ:.o=.d) $(MAIN_OBJ:.o=.d) $(TEST_OBJ:.o=.d) \
	      $(BIN) $(TEST_BIN) $(ARTEFACTS)

-include $(OBJ:.o=.d) $(MAIN_OBJ:.o=.d) $(TEST_OBJ:.o=.d)
