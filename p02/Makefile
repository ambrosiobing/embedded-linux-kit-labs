# P02, the host half of the NB-IoT power-saving field node.
#
#   make           build p02 and the suite
#   make check     build and run the suite, which needs no hardware
#   make demo      the four scenes, replayed, with no module and no SIM
#   make strict    a second pass with the conversion warnings turned on
#   make clean
#
# The shared AT engine is in ../lib and is compiled into this directory under
# a name of its own. Two labs use it and each builds its own object, so a
# strict pass here cannot collide with an ordinary build next door.

CC      ?= cc
CSTD    ?= -std=c11
WARN     = -Wall -Wextra -Werror -Wshadow -Wpointer-arith \
           -Wstrict-prototypes -Wmissing-prototypes
STRICT   = -Wconversion -Wsign-conversion -Wdouble-promotion
OPT     ?= -O2
CFLAGS  += $(CSTD) $(WARN) $(OPT) -MMD -MP

SRC      = src/timers.c src/session.c src/scene.c
OBJ      = $(SRC:.c=.o)
LIB_OBJ  = lib_atchat.o
MAIN_OBJ = src/main.o
TEST_OBJ = tests/test_p02.o

BIN      = p02
TEST_BIN = p02-test

.PHONY: all check demo strict clean

all: $(BIN) $(TEST_BIN)

lib_atchat.o: ../lib/atchat.c ../lib/atchat.h
	$(CC) $(CFLAGS) -c -o $@ $<

$(BIN): $(OBJ) $(LIB_OBJ) $(MAIN_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

$(TEST_BIN): $(OBJ) $(LIB_OBJ) $(TEST_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

check: $(TEST_BIN)
	./$(TEST_BIN)

# The timers decoded, the schedule judged, and the four scenes replayed. The
# last one is expected to fail and is run with a guard that says so.
demo: $(BIN)
	./$(BIN) timers
	./$(BIN) plan --period 600
	./$(BIN) transcript --scene attached --out attached.transcript
	./$(BIN) session --transcript attached.transcript
	./$(BIN) transcript --scene searching --out searching.transcript
	./$(BIN) session --transcript searching.transcript
	./$(BIN) transcript --scene psm-refused --out refused.transcript
	./$(BIN) session --transcript refused.transcript
	./$(BIN) transcript --scene silent --out silent.transcript
	./$(BIN) session --transcript silent.transcript || \
	    echo "stopped on a module that said nothing, which is the point of that scene"

strict:
	$(MAKE) clean
	$(MAKE) CFLAGS="$(CSTD) $(WARN) $(STRICT) $(OPT) -MMD -MP" all

clean:
	rm -f $(OBJ) $(MAIN_OBJ) $(TEST_OBJ) $(LIB_OBJ) \
	      $(OBJ:.o=.d) $(MAIN_OBJ:.o=.d) $(TEST_OBJ:.o=.d) $(LIB_OBJ:.o=.d) \
	      $(BIN) $(TEST_BIN) *.transcript *.log

-include $(OBJ:.o=.d) $(MAIN_OBJ:.o=.d) $(TEST_OBJ:.o=.d) $(LIB_OBJ:.o=.d)
