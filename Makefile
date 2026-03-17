CC ?= cc
CPPFLAGS += -D_POSIX_C_SOURCE=200809L -Iinclude -Isrc
CFLAGS ?= -std=c17 -O2 -Wall -Wextra -Wpedantic
LDFLAGS ?=

SRC := \
	src/main.c \
	src/autolib.c \
	src/build.c \
	src/cache.c \
	src/classifier.c \
	src/cli.c \
	src/compdb.c \
	src/common.c \
	src/config.c \
	src/diag.c \
	src/doctor.c \
	src/profile.c \
	src/scanner.c \
	src/standard.c \
	src/target.c

OBJ := $(SRC:.c=.o)

.PHONY: all clean install test test-regression test-perf

all: mazen

mazen: $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDFLAGS)

clean:
	rm -f $(OBJ) mazen

install: mazen
	install -d $(DESTDIR)/usr/local/bin
	install -m 0755 mazen $(DESTDIR)/usr/local/bin/mazen


test: test-regression

test-regression: mazen
	bash ./tests/regression.sh


test-perf: mazen
	bash ./tests/perf_matrix.sh
