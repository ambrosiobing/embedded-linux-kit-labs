# P19, the offline tilt meter.
#
#   make           build p19 and the suite
#   make check     build and run the suite, which needs no hardware
#   make demo      the acceptance test with nothing to rotate
#   make strict    a second pass with the conversion warnings turned on
#   make clean
#
# Two formulas and a drawing. The sensor is lib/adxl345.h and the panel is
# lib/framebuffer.h, so what is built here is only what belongs to this lab.
# The warnings are errors for the same reason as everywhere else, with one of
# its own: this writes into a mapping whose size is decided by someone else,
# and the suite paints into a file with a guard area to prove it stays
# inside.

CC      ?= cc
CSTD    ?= -std=c11
WARN     = -Wall -Wextra -Werror -Wshadow -Wpointer-arith \
           -Wstrict-prototypes -Wmissing-prototypes
STRICT   = -Wconversion -Wsign-conversion -Wdouble-promotion
OPT     ?= -O2
CFLAGS  += $(CSTD) $(WARN) $(OPT) -MMD -MP
LDLIBS  += -lm

SRC      = src/angles.c src/bars.c
OBJ      = $(SRC:.c=.o)
LIB_OBJ  = lib_framebuffer.o
MAIN_OBJ = src/main.o
TEST_OBJ = tests/test_p19.o

BIN      = p19
TEST_BIN = p19-test

.PHONY: all check demo strict clean

all: $(BIN) $(TEST_BIN)

lib_framebuffer.o: ../lib/framebuffer.c ../lib/framebuffer.h
	$(CC) $(CFLAGS) -c -o $@ $<

$(BIN): $(OBJ) $(LIB_OBJ) $(MAIN_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

$(TEST_BIN): $(OBJ) $(LIB_OBJ) $(TEST_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

check: $(TEST_BIN)
	./$(TEST_BIN)

# The acceptance test with no assembly to rotate, both ways round, and one
# drawing onto a file that stands in for the panel.
demo: $(BIN)
	./$(BIN) angles --bytes "00 00 00 00 00 01"
	./$(BIN) angles --bytes "00 01 00 00 00 00"
	./$(BIN) sweep --axis pitch
	./$(BIN) sweep --axis roll
	dd if=/dev/zero of=panel.glass bs=1 count=307200 status=none
	./$(BIN) draw --fb panel.glass --geometry 480x320x16 --pitch 30 --roll -15

strict:
	$(MAKE) clean
	$(MAKE) CFLAGS="$(CSTD) $(WARN) $(STRICT) $(OPT) -MMD -MP" all

clean:
	rm -f $(OBJ) $(MAIN_OBJ) $(TEST_OBJ) $(LIB_OBJ) \
	      $(OBJ:.o=.d) $(MAIN_OBJ:.o=.d) $(TEST_OBJ:.o=.d) $(LIB_OBJ:.o=.d) \
	      $(BIN) $(TEST_BIN) *.glass

-include $(OBJ:.o=.d) $(MAIN_OBJ:.o=.d) $(TEST_OBJ:.o=.d) $(LIB_OBJ:.o=.d)
