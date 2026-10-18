# P20, the fleet sweep's judge.
#
#   make           build p20 and the suite
#   make check     build and run the suite, which reaches no host
#   make demo      one sweep that is an artifact, and five that are not
#   make strict    a second pass with the conversion warnings turned on
#   make clean
#
# From lib/: the line reader. There is no sweep here and no SSH: the sweep is
# the chapter's forty lines of shell and that is the right answer. This reads
# what it wrote.
#
# clean removes named files rather than a wildcard, for the reason P01's
# clean learned the hard way.

CC      ?= cc
CSTD    ?= -std=c11
WARN     = -Wall -Wextra -Werror -Wshadow -Wpointer-arith \
           -Wstrict-prototypes -Wmissing-prototypes
STRICT   = -Wconversion -Wsign-conversion -Wdouble-promotion
OPT     ?= -O2
CFLAGS  += $(CSTD) $(WARN) $(OPT) -MMD -MP

SRC      = src/fleet.c src/scene.c
OBJ      = $(SRC:.c=.o)
MAIN_OBJ = src/main.o
TEST_OBJ = tests/test_p20.o

BIN      = p20
TEST_BIN = p20-test

ARTEFACTS = good.txt blank.txt fifth.txt two.txt undated.txt notemp.txt \
            test_p20_fleet.txt

.PHONY: all check demo strict clean

all: $(BIN) $(TEST_BIN)

$(BIN): $(OBJ) $(MAIN_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

$(TEST_BIN): $(OBJ) $(TEST_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

check: $(TEST_BIN)
	./$(TEST_BIN)

# One sweep that is an acceptance artifact, and five that are each refused
# for their own reason.
demo: $(BIN)
	./$(BIN) scene --kind good       --out good.txt
	./$(BIN) scene --kind blank-hat  --out blank.txt
	./$(BIN) scene --kind fifth      --out fifth.txt
	./$(BIN) scene --kind two-hats   --out two.txt
	./$(BIN) scene --kind undated    --out undated.txt
	./$(BIN) scene --kind pi-no-temp --out notemp.txt
	@echo
	./$(BIN) judge --source good.txt
	@echo
	./$(BIN) judge --source blank.txt || \
	    echo "refused: a missing HAT line reads as a broken script, not as an empty header"
	@echo
	./$(BIN) judge --source fifth.txt || \
	    echo "refused: a fifth name is a host this book has not brought up"
	@echo
	./$(BIN) judge --source two.txt || \
	    echo "refused: one board owns the header, which is the rule every lab here obeys"
	@echo
	./$(BIN) judge --source undated.txt || \
	    echo "refused: an undated sweep cannot be compared with a bench anybody can see"
	@echo
	./$(BIN) judge --source notemp.txt || \
	    echo "refused: a command that failed is not a command that is absent"

strict:
	$(MAKE) clean
	$(MAKE) CFLAGS="$(CSTD) $(WARN) $(STRICT) $(OPT) -MMD -MP" all

clean:
	rm -f $(OBJ) $(MAIN_OBJ) $(TEST_OBJ) \
	      $(OBJ:.o=.d) $(MAIN_OBJ:.o=.d) $(TEST_OBJ:.o=.d) \
	      $(BIN) $(TEST_BIN) $(ARTEFACTS)

-include $(OBJ:.o=.d) $(MAIN_OBJ:.o=.d) $(TEST_OBJ:.o=.d)
