#
# Malloc Lab - build and scoring
#
# Targets
#   make            Build the driver linked against your mm.c
#   make test       Build and score YOUR allocator on the default traces
#   make textbook   Build and score the textbook allocator (the baseline)
#   make naive      Build and score the naive allocator (no reuse at all)
#   make score      Measure both and tell you whether you beat the baseline
#   make debug      Build your allocator with -g and the address/UB sanitizers
#   make clean      Delete build/
#
# Run one trace instead of the whole default set:
#   make test TRACE=traces/realloc-bal.rep
#

CC = gcc
CFLAGS = -Wall -O2
DEBUG_CFLAGS = -Wall -O0 -g -fsanitize=address,undefined

# Every object file and every driver binary is written under build/, which
# is git-ignored, so the source directory stays clean.
BUILD = build

COMMON_OBJS = $(BUILD)/memlib.o $(BUILD)/fsecs.o $(BUILD)/fcyc.o $(BUILD)/clock.o $(BUILD)/ftimer.o

TRACE =

all: $(BUILD)/mdriver

# Each allocator gets its own driver binary, compiled from the same mdriver.c
# but with a different -DMM_NAME so the results banner names what was tested.
$(BUILD)/mdriver: $(BUILD)/mdriver.o $(BUILD)/mm.o $(COMMON_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

$(BUILD)/mdriver-naive: $(BUILD)/mdriver-naive.o $(BUILD)/mm-naive.o $(COMMON_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

$(BUILD)/mdriver-textbook: $(BUILD)/mdriver-textbook.o $(BUILD)/mm-textbook.o $(COMMON_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

$(BUILD)/mdriver.o: mdriver.c fsecs.h fcyc.h clock.h memlib.h config.h mm.h | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ mdriver.c

$(BUILD)/mdriver-naive.o: mdriver.c fsecs.h fcyc.h clock.h memlib.h config.h mm.h | $(BUILD)
	$(CC) $(CFLAGS) -DMM_NAME='"mm-naive"' -c -o $@ mdriver.c

$(BUILD)/mdriver-textbook.o: mdriver.c fsecs.h fcyc.h clock.h memlib.h config.h mm.h | $(BUILD)
	$(CC) $(CFLAGS) -DMM_NAME='"mm-textbook"' -c -o $@ mdriver.c

# Everything else is a one-source-file object.
$(BUILD)/%.o: %.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/mm.o $(BUILD)/mm-naive.o $(BUILD)/mm-textbook.o: mm.h memlib.h
$(BUILD)/fsecs.o $(BUILD)/ftimer.o: config.h

$(BUILD):
	mkdir -p $(BUILD)

# ---------------------------------------------------------------------------
# Scoring.  Each target runs the driver over the default trace set defined in
# config.h.  Pass TRACE=... to score a single trace instead.
# ---------------------------------------------------------------------------

test: $(BUILD)/mdriver
	$(BUILD)/mdriver -v $(if $(TRACE),-f $(TRACE),)

textbook: $(BUILD)/mdriver-textbook
	$(BUILD)/mdriver-textbook -v $(if $(TRACE),-f $(TRACE),)

naive: $(BUILD)/mdriver-naive
	$(BUILD)/mdriver-naive -v $(if $(TRACE),-f $(TRACE),)

# Measure both allocators in the same run and compare.  Because the baseline
# is measured on this machine at the same moment, the comparison is fair on
# any CPU.  Exits 0 only if your allocator scores strictly higher.
score: $(BUILD)/mdriver $(BUILD)/mdriver-textbook
	@your_out=`$(BUILD)/mdriver -g $(if $(TRACE),-f $(TRACE),) 2>&1`; \
	base_out=`$(BUILD)/mdriver-textbook -g $(if $(TRACE),-f $(TRACE),) 2>&1`; \
	your=`printf '%s\n' "$$your_out" | sed -n 's/^perfidx://p'`; \
	base=`printf '%s\n' "$$base_out" | sed -n 's/^perfidx://p'`; \
	your_ok=`printf '%s\n' "$$your_out" | sed -n 's/^correct://p'`; \
	base_ok=`printf '%s\n' "$$base_out" | sed -n 's/^correct://p'`; \
	echo ""; \
	echo "                     traces valid   perf index"; \
	echo "  your mm.c          $$your_ok            $$your"; \
	echo "  mm-textbook        $$base_ok            $$base"; \
	echo ""; \
	if [ -z "$$your" ] || [ -z "$$base" ]; then \
	  echo "FAIL: the driver produced no score - did it crash?"; \
	  exit 1; \
	elif [ "$$your" -gt "$$base" ]; then \
	  echo "PASS: your allocator beats the textbook baseline ($$your > $$base)"; \
	else \
	  echo "FAIL: your allocator does not beat the textbook baseline ($$your <= $$base)"; \
	  exit 1; \
	fi

# ---------------------------------------------------------------------------
# Debugging.  Same driver, built with -g and the sanitizers.
# ---------------------------------------------------------------------------

debug: $(BUILD)/mdriver-debug
	$(BUILD)/mdriver-debug -v $(if $(TRACE),-f $(TRACE),)

$(BUILD)/mdriver-debug: $(BUILD)/mdriver-debug.o $(BUILD)/mm-debug.o $(COMMON_OBJS)
	$(CC) $(DEBUG_CFLAGS) -o $@ $^

$(BUILD)/mdriver-debug.o: mdriver.c fsecs.h fcyc.h clock.h memlib.h config.h mm.h | $(BUILD)
	$(CC) $(DEBUG_CFLAGS) -DMM_NAME='"mm (debug)"' -c -o $@ mdriver.c

$(BUILD)/mm-debug.o: mm.c mm.h memlib.h | $(BUILD)
	$(CC) $(DEBUG_CFLAGS) -c -o $@ $<

clean:
	rm -rf $(BUILD)

.PHONY: all test textbook naive score debug clean
