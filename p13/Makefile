# P13, the supervisor of the dual-radio signalling failover.
#
#   make           build p13 and the suite
#   make check     build and run the suite, which needs no hardware
#   make demo      five runs that did not happen, judged
#   make strict    a second pass with the conversion warnings turned on
#   make clean
#
# No AT engine here. This lab's host side is a subscriber with a clock, and
# the only thing it shares with the cellular labs is the line reader.

CC      ?= cc
CSTD    ?= -std=c11
WARN     = -Wall -Wextra -Werror -Wshadow -Wpointer-arith \
           -Wstrict-prototypes -Wmissing-prototypes
STRICT   = -Wconversion -Wsign-conversion -Wdouble-promotion
OPT     ?= -O2
CFLAGS  += $(CSTD) $(WARN) $(OPT) -MMD -MP

SRC      = src/machine.c src/replay.c src/scene.c
OBJ      = $(SRC:.c=.o)
MAIN_OBJ = src/main.o
TEST_OBJ = tests/test_p13.o

BIN      = p13
TEST_BIN = p13-test

.PHONY: all check demo strict clean

all: $(BIN) $(TEST_BIN)

$(BIN): $(OBJ) $(MAIN_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

$(TEST_BIN): $(OBJ) $(TEST_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

check: $(TEST_BIN)
	./$(TEST_BIN)

# The one run that demonstrates the lab, and four that do not, each refused
# for its own reason.
demo: $(BIN)
	./$(BIN) scene --kind outage --out outage.p13
	./$(BIN) replay --source outage.p13
	./$(BIN) verify --source outage.p13
	./$(BIN) scene --kind steady --out steady.p13
	./$(BIN) verify --source steady.p13 || \
	    echo "refused a healthy run: nobody pulled an antenna, so nothing was shown"
	./$(BIN) scene --kind short-gap --out shortgap.p13
	./$(BIN) verify --source shortgap.p13 || \
	    echo "refused: one missed sample is inside the window, which is the point of it"
	./$(BIN) scene --kind no-return --out noreturn.p13
	./$(BIN) verify --source noreturn.p13 || \
	    echo "refused: it fell and never came back, which is half a test"
	./$(BIN) scene --kind backup-only --out backuponly.p13
	./$(BIN) verify --source backuponly.p13 || \
	    echo "refused: one source is not two"

strict:
	$(MAKE) clean
	$(MAKE) CFLAGS="$(CSTD) $(WARN) $(STRICT) $(OPT) -MMD -MP" all

clean:
	rm -f $(OBJ) $(MAIN_OBJ) $(TEST_OBJ) \
	      $(OBJ:.o=.d) $(MAIN_OBJ:.o=.d) $(TEST_OBJ:.o=.d) \
	      $(BIN) $(TEST_BIN) *.p13

-include $(OBJ:.o=.d) $(MAIN_OBJ:.o=.d) $(TEST_OBJ:.o=.d)
