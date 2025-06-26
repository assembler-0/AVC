# AVC - Archive Version Control
# High-performance VCS with Git compatibility

CC = gcc
CFLAGS = -Wall -Wextra -O3 -std=c99 -fopenmp
LIBS = -lssl -lcrypto -lz -ldeflate -lblake3 -fopenmp -lm

# Directories
SRC_DIR = src
AGCL_DIR = agcl
BUILD_DIR = build
BIN_DIR = bin

# Source files
CORE_SOURCES = $(wildcard $(SRC_DIR)/*.c)
AGCL_SOURCES = $(wildcard $(AGCL_DIR)/*.c)
ALL_SOURCES = $(CORE_SOURCES) $(AGCL_SOURCES)

# Object files
CORE_OBJECTS = $(CORE_SOURCES:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
AGCL_OBJECTS = $(AGCL_SOURCES:$(AGCL_DIR)/%.c=$(BUILD_DIR)/%.o)
ALL_OBJECTS = $(CORE_OBJECTS) $(AGCL_OBJECTS)

# Target executable
TARGET = $(BIN_DIR)/avc

# Default target
all: directories $(TARGET)

# Create directories
directories:
	@mkdir -p $(BUILD_DIR) $(BIN_DIR)

# Build main executable
$(TARGET): $(ALL_OBJECTS)
	@echo "Linking AVC..."
	@$(CC) $(ALL_OBJECTS) -o $@ $(LIBS)
	@echo "✓ AVC built successfully: $@"

# Compile core source files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@echo "Compiling $<..."
	@$(CC) $(CFLAGS) -I$(SRC_DIR) -I$(AGCL_DIR) -c $< -o $@

# Compile AGCL source files
$(BUILD_DIR)/%.o: $(AGCL_DIR)/%.c
	@echo "Compiling $<..."
	@$(CC) $(CFLAGS) -I$(SRC_DIR) -I$(AGCL_DIR) -c $< -o $@

# Install system-wide
install: $(TARGET)
	@echo "Installing AVC to /usr/local/bin..."
	@sudo cp $(TARGET) /usr/local/bin/avc
	@sudo chmod +x /usr/local/bin/avc
	@echo "✓ AVC installed successfully"

# Uninstall
uninstall:
	@echo "Removing AVC from /usr/local/bin..."
	@sudo rm -f /usr/local/bin/avc
	@echo "✓ AVC uninstalled"

# Clean build files
clean:
	@echo "Cleaning build files..."
	@rm -rf $(BUILD_DIR) $(BIN_DIR)
	@echo "✓ Clean complete"

# Debug build
debug: CFLAGS += -g -DDEBUG -O0
debug: clean all

# Release build (default)
release: CFLAGS += -DNDEBUG -march=native
release: clean all

# Check dependencies
deps:
	@echo "Checking dependencies..."
	@pkg-config --exists openssl || (echo "❌ OpenSSL not found" && exit 1)
	@pkg-config --exists zlib || (echo "❌ zlib not found" && exit 1)
	@echo "✓ Core dependencies found"
	@echo "Note: Ensure libdeflate and libblake3 are installed"

# Test build
test: $(TARGET)
	@echo "Running basic AVC test..."
	@$(TARGET) version
	@echo "✓ AVC test passed"

# Help
help:
	@echo "AVC Build System"
	@echo ""
	@echo "Targets:"
	@echo "  all      - Build AVC (default)"
	@echo "  install  - Install AVC system-wide"
	@echo "  uninstall- Remove AVC from system"
	@echo "  clean    - Remove build files"
	@echo "  debug    - Build with debug symbols"
	@echo "  release  - Build optimized release"
	@echo "  deps     - Check dependencies"
	@echo "  test     - Test built executable"
	@echo "  help     - Show this help"
	@echo ""
	@echo "Dependencies:"
	@echo "  - gcc with OpenMP support"
	@echo "  - OpenSSL development headers"
	@echo "  - zlib development headers"
	@echo "  - libdeflate library"
	@echo "  - libblake3 library"

# Phony targets
.PHONY: all directories install uninstall clean debug release deps test help

# Default goal
.DEFAULT_GOAL := all