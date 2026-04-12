CC = gcc
# Strict compilation flags
CFLAGS = -std=c11 \
         -pedantic \
         -pedantic-errors \
         -Wall \
         -Wextra \
         -Werror \
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
         -Wold-style-definition

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
          -pie

# Optimization
OPTFLAGS = -O2

# GTK flags
GTK_FLAGS = `pkg-config --cflags --libs gtk+-3.0`

# Combine all flags
ALL_CFLAGS = $(CFLAGS) $(HARDENING) $(OPTFLAGS)

TARGET = calc
OBJS = main.o engine.o ui.o resources.o
SRCS = main.c engine.c ui.c

.PHONY: all clean install test run format lint asan

all: clean format lint $(TARGET)

run: $(TARGET)
	./$(TARGET)

# Build with AddressSanitizer
asan: clean
	$(MAKE) ALL_CFLAGS="$(ALL_CFLAGS) -fsanitize=address -g" LDFLAGS="$(LDFLAGS) -fsanitize=address" $(TARGET)
	@echo "Build complete. Run './calc' with 'ASAN_OPTIONS=detect_leaks=1' to check for leaks."

# Formatting using clang-format
format:
	@echo "Formatting code..."
	clang-format -i $(SRCS) engine.h ui.h

# Linting using clang (static analysis)
lint:
	@echo "Linting with clang..."
	clang --analyze -Xanalyzer -analyzer-output=text $(SRCS) `pkg-config --cflags gtk+-3.0`

$(TARGET): $(OBJS)
	$(CC) $(ALL_CFLAGS) $(LDFLAGS) -o $(TARGET) $(OBJS) $(GTK_FLAGS) -lm

resources.c: calc.gresource.xml style.css
	glib-compile-resources calc.gresource.xml --target=resources.c --generate-source

%.o: %.c
	$(CC) $(ALL_CFLAGS) `pkg-config --cflags gtk+-3.0` -c $< -o $@

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
	rm -f $(TARGET) $(OBJS) resources.c
