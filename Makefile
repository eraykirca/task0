# Makefile

CC      ?= cc
CFLAGS  ?= -std=c11 -O2 -Wall -Wextra -Werror -pedantic -pthread -Isrc
LDFLAGS ?= -pthread

# Enable sanitizers via: make SAN=tsan 
ifeq ($(SAN),tsan)
  CC      := clang
  CFLAGS  := -std=c11 -O1 -g -Wall -Wextra -Werror -pedantic -pthread -Isrc -fsanitize=thread -fno-omit-frame-pointer
  LDFLAGS := -pthread -fsanitize=thread
endif

SRCS := src/emergency_module.c tests/test_emergency_module.c
BIN  := build/run_tests

all: $(BIN)

$(BIN): $(SRCS)
	@mkdir -p $(dir $(BIN))
	$(CC) $(CFLAGS) -o $@ $(SRCS) $(LDFLAGS)

run: $(BIN)
	./$(BIN)

clean:
	rm -rf build

