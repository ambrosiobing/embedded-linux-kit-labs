# P03, the host half of the Cat-M node that stamps its payload from GNSS.
#
#   make           build p03 and the suite
#   make check     build and run the suite, which needs no hardware
#   make demo      the scenes replayed, and the ordering audited
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

SRC      = src/fix.c src/session.c src/scene.c src/audit.c
OBJ      = $(SRC:.c=.o)
LIB_OBJ  = lib_atchat.o
MAIN_OBJ = src/main.o
TEST_OBJ = tests/test_p03.o

BIN      = p03
TEST_BIN = p03-test

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

# Every scene replayed, then the same ordering question asked of a recording
# this program did not make and could not have produced.
demo: $(BIN)
	./$(BIN) fix --line "+CGNSINF: 1,1,20261016103012.000,47.070714,15.439504,353.0,0.0,0.0"
	./$(BIN) transcript --scene fix --out fix.transcript
	./$(BIN) session --transcript fix.transcript
	./$(BIN) transcript --scene cached --out cached.transcript
	./$(BIN) session --transcript cached.transcript
	./$(BIN) transcript --scene no-fix --out nofix.transcript
	./$(BIN) session --transcript nofix.transcript
	./$(BIN) transcript --scene wrong-bearer --out bearer.transcript
	./$(BIN) session --transcript bearer.transcript || \
	    echo "stopped on the wrong bearer, which this lab exists to catch"
	./$(BIN) audit --transcript fix.transcript
	./$(BIN) transcript --scene gnss-left-on --out broken.transcript
	./$(BIN) audit --transcript broken.transcript || \
	    echo "refused a recording that transmitted with the receiver powered"

strict:
	$(MAKE) clean
	$(MAKE) CFLAGS="$(CSTD) $(WARN) $(STRICT) $(OPT) -MMD -MP" all

clean:
	rm -f $(OBJ) $(MAIN_OBJ) $(TEST_OBJ) $(LIB_OBJ) \
	      $(OBJ:.o=.d) $(MAIN_OBJ:.o=.d) $(TEST_OBJ:.o=.d) $(LIB_OBJ:.o=.d) \
	      $(BIN) $(TEST_BIN) *.transcript *.log

-include $(OBJ:.o=.d) $(MAIN_OBJ:.o=.d) $(TEST_OBJ:.o=.d) $(LIB_OBJ:.o=.d)
