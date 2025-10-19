# COSPAS-SARSAT T.018 (2nd Generation) Beacon Transmitter Makefile

# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=gnu11
INCLUDES = -Iinclude
LIBS = -liio -lm

# Directories
SRC_DIR = src
INC_DIR = include
BUILD_DIR = build
BIN_DIR = bin

# Target executable
TARGET = $(BIN_DIR)/sarsat_sgb

# Source files
SOURCES = $(SRC_DIR)/main.c \
          $(SRC_DIR)/t018_protocol.c \
          $(SRC_DIR)/prn_generator.c \
          $(SRC_DIR)/oqpsk_modulator.c \
          $(SRC_DIR)/rrc_filter.c \
          $(SRC_DIR)/pluto_control.c

# Object files
OBJECTS = $(SOURCES:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

# Header dependencies
HEADERS = $(INC_DIR)/t018_protocol.h \
          $(INC_DIR)/prn_generator.h \
          $(INC_DIR)/oqpsk_modulator.h \
          $(INC_DIR)/rrc_filter.h \
          $(INC_DIR)/pluto_control.h

# Default target
all: directories $(TARGET)

# Create build directories
directories:
	@mkdir -p $(BUILD_DIR) $(BIN_DIR)

# Link executable
$(TARGET): $(OBJECTS)
	@echo "Linking $@"
	@$(CC) $(OBJECTS) $(LIBS) -o $@
	@echo "Build complete: $@"
	@echo ""
	@echo "✓ SARSAT_SGB (2nd Generation Beacon) compiled successfully"
	@echo "  Executable: $@"
	@echo "  Run: $@ -h"

# Compile source files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c $(HEADERS)
	@echo "Compiling $<"
	@$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Clean build artifacts
clean:
	@echo "Cleaning build artifacts..."
	@rm -rf $(BUILD_DIR) $(BIN_DIR)
	@echo "Clean complete"

# Install (copy to /usr/local/bin)
install: $(TARGET)
	@echo "Installing to /usr/local/bin..."
	@sudo cp $(TARGET) /usr/local/bin/
	@echo "Install complete"
	@echo "Run: sarsat_sgb -h"

# Uninstall
uninstall:
	@echo "Uninstalling..."
	@sudo rm -f /usr/local/bin/sarsat_sgb
	@echo "Uninstall complete"

# Run (for testing)
run: $(TARGET)
	@echo "Running $(TARGET) with default settings..."
	@$(TARGET)

# Test transmission (short 10s interval)
test: $(TARGET)
	@echo "Running test transmission (10s interval, test mode)..."
	@$(TARGET) -f 403000000 -g -10 -m 1 -i 10

# Debug build
debug: CFLAGS += -g -DDEBUG
debug: clean all

# Verify build dependencies
check-deps:
	@echo "Checking dependencies..."
	@which $(CC) > /dev/null || (echo "ERROR: gcc not found" && exit 1)
	@pkg-config --exists libiio || (echo "ERROR: libiio not found (install: sudo apt install libiio-dev)" && exit 1)
	@echo "✓ All dependencies found"

# Help
help:
	@echo "COSPAS-SARSAT T.018 (2nd Generation) Beacon Transmitter Build System"
	@echo ""
	@echo "Targets:"
	@echo "  all         - Build the project (default)"
	@echo "  clean       - Remove build artifacts"
	@echo "  install     - Install to /usr/local/bin (requires sudo)"
	@echo "  uninstall   - Remove from /usr/local/bin (requires sudo)"
	@echo "  run         - Build and run with default settings"
	@echo "  test        - Build and run test transmission (10s interval)"
	@echo "  debug       - Build with debug symbols"
	@echo "  check-deps  - Verify build dependencies"
	@echo "  help        - Show this help message"
	@echo ""
	@echo "Dependencies:"
	@echo "  - gcc (GNU Compiler Collection)"
	@echo "  - libiio-dev (PlutoSDR control library)"
	@echo "  - libm (math library, part of glibc)"
	@echo ""
	@echo "Installation (Debian/Ubuntu):"
	@echo "  sudo apt update"
	@echo "  sudo apt install build-essential libiio-dev libiio-utils"
	@echo ""
	@echo "Build & Run Examples:"
	@echo "  make clean && make"
	@echo "  make test"
	@echo "  sudo make install"
	@echo "  sarsat_sgb -f 403000000 -g -10 -m 1 -i 120"
	@echo ""
	@echo "PlutoSDR Connection:"
	@echo "  Default: ip:192.168.2.1"
	@echo "  Custom:  sarsat_sgb -u ip:192.168.3.1"

.PHONY: all clean install uninstall run test debug check-deps help directories
