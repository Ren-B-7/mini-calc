CC = gcc
# Strict compilation flags
CFLAGS = -std=c99 \
         -pedantic \
         -pedantic-errors \
         -Wall \
         -Wextra \
         -Wformat=2 \
         -Wformat-security \
         -Wnull-dereference \
         -Wstack-protector \
         -Wtrampolines \
         -Walloca \
         -Wvla \
         -Warray-bounds=2 \
         -Wimplicit-fallthrough=3 \
         -Wshift-overflow=2 \
         -Wcast-qual \
         -Wcast-align=strict \
         -Wconversion \
         -Wsign-conversion \
         -Wlogical-op \
         -Wduplicated-cond \
         -Wduplicated-branches \
         -Wrestrict \
         -Wnested-externs \
         -Winline \
         -Wundef \
         -Wstrict-prototypes \
         -Wmissing-prototypes \
         -Wmissing-declarations \
         -Wredundant-decls \
         -Wshadow \
         -Wwrite-strings \
         -Wfloat-equal \
         -Wpointer-arith \
         -Wbad-function-cast \
         -Wold-style-definition \
         -Isrc

# Security hardening flags
HARDENING = -D_FORTIFY_SOURCE=2 \
            -fstack-protector-strong \
            -fPIE \
            -fstack-clash-protection \
            -fcf-protection

# Linker hardening flags
LDFLAGS = -Wl,-z,relro \
          -Wl,-z,now \
          -Wl,-z,noexecstack \
          -Wl,-z,separate-code \
          -pie \
          -flto

# Optimization
OPTFLAGS = -O3 -march=native -flto

# GTK flags
GTK_FLAGS = `pkg-config --cflags --libs gtk+-3.0`

# Source file search path
VPATH = src

# Combine all flags
ALL_CFLAGS = $(CFLAGS) $(HARDENING) $(OPTFLAGS)

# Output directories
BIN_DIR = bin
OBJ_DIR = bin/obj
SRC_DIR = src
ASSETS_DIR = assets

TARGET = $(BIN_DIR)/calc
SRCS = $(SRC_DIR)/main.c $(SRC_DIR)/engine.c $(SRC_DIR)/ui.c
OBJS = $(OBJ_DIR)/main.o $(OBJ_DIR)/engine.o $(OBJ_DIR)/ui.o $(OBJ_DIR)/resources.o
HEADERS = $(SRC_DIR)/engine.h $(SRC_DIR)/ui.h
RESOURCES_SRC = $(OBJ_DIR)/resources.c

.PHONY: all clean install test run format lint asan

all: clean format lint $(TARGET)

run: $(TARGET)
	./$(TARGET)

# Build with AddressSanitizer
asan: clean
	$(MAKE) ALL_CFLAGS="$(ALL_CFLAGS) -fsanitize=address -g" LDFLAGS="$(LDFLAGS) -fsanitize=address" $(TARGET)
	@echo "Build complete. Run './$(TARGET)' with 'ASAN_OPTIONS=detect_leaks=1' to check for leaks."

format:
	clang-format -style=file:./.clang-format -i $(SRCS) $(HEADERS)
	mbake format --config ./.bake.toml Makefile

CLANG_TIDY_CHECKS = -checks=-bugprone-easily-swappable-parameters,-clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling
CLANG_TIDY_FLAGS = -std=c99 -pedantic -Wall -Wextra -Isrc -Isrc/include

lint:
	clang-tidy $(CLANG_TIDY_CHECKS) $(SRCS) -- $(GTK_CFLAGS_SYSTEM) $(CLANG_TIDY_FLAGS) || true
	mbake validate --config ./.bake.toml Makefile

fix:
	clang-tidy --fix $(CLANG_TIDY_CHECKS) $(SRCS) -- $(GTK_CFLAGS_SYSTEM) $(CLANG_TIDY_FLAGS)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(TARGET): $(BIN_DIR) $(OBJ_DIR) $(OBJS)
	$(CC) $(ALL_CFLAGS) $(LDFLAGS) -o $(TARGET) $(OBJS) $(GTK_FLAGS) -lm

$(RESOURCES_SRC): $(ASSETS_DIR)/calc.gresource.xml $(ASSETS_DIR)/style.css | $(OBJ_DIR)
	cd $(ASSETS_DIR) && glib-compile-resources calc.gresource.xml --target=../$(RESOURCES_SRC) --generate-source

# Convert GTK include paths to system includes to suppress external library warnings
GTK_CFLAGS_SYSTEM = $(shell pkg-config --cflags gtk+-3.0 | sed 's/-I/-isystem /g')

$(OBJ_DIR)/resources.o: $(RESOURCES_SRC) | $(OBJ_DIR)
	$(CC) $(ALL_CFLAGS) $(GTK_CFLAGS_SYSTEM) -c $(RESOURCES_SRC) -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(ALL_CFLAGS) $(GTK_CFLAGS_SYSTEM) -c $< -o $@

# Tests for CLI mode
test: $(TARGET)
	@echo "Testing CLI calculations..."
	@if [ "$$(./$(TARGET) 10 + 5)" = "15" ]; then echo "✓ Addition passed"; else echo "✗ Addition failed"; exit 1; fi
	@if [ "$$(./$(TARGET) 20 - 7)" = "13" ]; then echo "✓ Subtraction passed"; else echo "✗ Subtraction failed"; exit 1; fi
	@if [ "$$(./$(TARGET) 6 '*' 7)" = "42" ]; then echo "✓ Multiplication passed"; else echo "✗ Multiplication failed"; exit 1; fi
	@if [ "$$(./$(TARGET) 100 / 4)" = "25" ]; then echo "✓ Division passed"; else echo "✗ Division failed"; exit 1; fi
	@if [ "$$(./$(TARGET) 5 '*' '(' 10 + 2 ')' )" = "60" ]; then echo "✓ Parentheses passed"; else echo "✗ Parentheses failed"; exit 1; fi
	@if [ "$$(./$(TARGET) 10 + 2 '*' 3)" = "16" ]; then echo "✓ Precedence passed"; else echo "✗ Precedence failed"; exit 1; fi
	@if [ "$$(./$(TARGET) 'sqrt(16)')" = "4" ]; then echo "✓ sqrt passed"; else echo "✗ sqrt failed"; exit 1; fi
	@if [ "$$(./$(TARGET) 2 '^' 3)" = "8" ]; then echo "✓ power passed"; else echo "✗ power failed"; exit 1; fi
	@# Test division by zero
	@if ./$(TARGET) 10 / 0 2>/dev/null; then echo "✗ Division by zero check failed"; exit 1; else echo "✓ Division by zero check passed"; fi
	@echo "All CLI tests passed."

install: $(TARGET)
	mkdir -p $(HOME)/.local/bin
	install -m 755 $(TARGET) $(HOME)/.local/bin/

clean:
	rm -rf $(BIN_DIR)
