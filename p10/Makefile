# P10, the energy characterisation bench.
#
#   make           build p10 and the suite
#   make check     build and run the suite, which needs no instrument
#   make demo      a run that passes, and the run this lab exists to refuse
#   make strict    a second pass with the conversion warnings turned on
#   make clean
#
# From lib/: the line reader, and the fixed-point formatter that moved there
# when this lab turned out to need P15's arithmetic on different units.
#
# clean removes named files rather than a wildcard. P01's clean once took
# another lab's artefacts because it swept *.csv, and every file this lab
# writes is named below instead.

CC      ?= cc
CSTD    ?= -std=c11
WARN     = -Wall -Wextra -Werror -Wshadow -Wpointer-arith \
           -Wstrict-prototypes -Wmissing-prototypes
STRICT   = -Wconversion -Wsign-conversion -Wdouble-promotion
OPT     ?= -O2
CFLAGS  += $(CSTD) $(WARN) $(OPT) -MMD -MP

SRC      = src/reduce.c src/book.c src/scene.c
OBJ      = $(SRC:.c=.o)
MAIN_OBJ = src/main.o
TEST_OBJ = tests/test_p10.o

BIN      = p10
TEST_BIN = p10-test

ARTEFACTS = sleep.csv idle.csv ble.csv usb.csv repeat.csv \
            run.p10book usbrun.p10book \
            test_p10_capture.csv test_p10.p10book

.PHONY: all check demo strict clean

all: $(BIN) $(TEST_BIN)

$(BIN): $(OBJ) $(MAIN_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

$(TEST_BIN): $(OBJ) $(TEST_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

check: $(TEST_BIN)
	./$(TEST_BIN)

# One run that satisfies every item this program can decide, and one that is
# refused for the reason the whole lab exists to catch.
demo: $(BIN)
	./$(BIN) scene --kind sleep --samples 20000 --out sleep.csv
	./$(BIN) scene --kind idle  --samples 20000 --out idle.csv
	./$(BIN) scene --kind ble   --samples 20000 --out ble.csv
	./$(BIN) scene --kind sleep --samples 20000 --out repeat.csv
	./$(BIN) reduce --source sleep.csv  --state sleep --book run.p10book \
	    --why "a dev-board, so the USB-UART silicon leaks and this is not a bare-module figure"
	./$(BIN) reduce --source idle.csv   --state idle  --book run.p10book
	./$(BIN) reduce --source ble.csv    --state ble   --book run.p10book
	./$(BIN) reduce --source repeat.csv --state sleep --book run.p10book
	./$(BIN) report --book run.p10book
	./$(BIN) judge  --book run.p10book
	@echo
	./$(BIN) scene --kind usb --samples 20000 --out usb.csv
	./$(BIN) reduce --source usb.csv --state sleep --book usbrun.p10book \
	    --why "a dev-board, so the USB-UART silicon leaks"
	./$(BIN) reduce --source usb.csv --state idle  --book usbrun.p10book
	./$(BIN) reduce --source usb.csv --state ble   --book usbrun.p10book
	./$(BIN) reduce --source usb.csv --state sleep --book usbrun.p10book
	./$(BIN) judge --book usbrun.p10book || \
	    echo "refused: three traces that overlay, which is the DUT USB cable and not the instrument"

strict:
	$(MAKE) clean
	$(MAKE) CFLAGS="$(CSTD) $(WARN) $(STRICT) $(OPT) -MMD -MP" all

clean:
	rm -f $(OBJ) $(MAIN_OBJ) $(TEST_OBJ) \
	      $(OBJ:.o=.d) $(MAIN_OBJ:.o=.d) $(TEST_OBJ:.o=.d) \
	      $(BIN) $(TEST_BIN) $(ARTEFACTS)

-include $(OBJ:.o=.d) $(MAIN_OBJ:.o=.d) $(TEST_OBJ:.o=.d)
