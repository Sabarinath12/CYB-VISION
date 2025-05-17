CXX = g++
TARGET = main
SRCS = main.cpp
OBJS = $(SRCS:.cpp=.o)

# OpenCV flags - get these from pkg-config
OPENCV_CFLAGS = $(shell pkg-config --cflags opencv4)
OPENCV_LIBS = $(shell pkg-config --libs opencv4)

# Compiler flags
# -Wall: Enable all warnings
# -Wextra: Enable extra warnings
# -O2: Optimize for speed
# -g: Add debugging info
CXXFLAGS = -Wall -Wextra -O2 -g $(OPENCV_CFLAGS) -std=c++17

# Linker flags
LDFLAGS = $(OPENCV_LIBS) -lpthread

# Default target
all: $(TARGET)

# Link rule
$(TARGET): $(OBJS)
	$(CXX) $^ -o $@ $(LDFLAGS)

# Compile rule
%.o: %.cpp
	$(CXX) -c $< -o $@ $(CXXFLAGS)

# Build and run
run: $(TARGET)
	./$(TARGET)

# Clean up
clean:
	rm -f $(OBJS) $(TARGET)
	rm -rf snapshot

# Create snapshot directory
snapshot:
	mkdir -p snapshot

# Install the necessary dependencies on Debian/Ubuntu
deps-ubuntu:
	sudo apt-get update
	sudo apt-get install -y libopencv-dev build-essential cmake git pkg-config

# Install the necessary dependencies on Fedora
deps-fedora:
	sudo dnf install -y opencv opencv-devel gcc-c++ make cmake git pkgconfig

# Install the necessary dependencies on Arch Linux
deps-arch:
	sudo pacman -S --needed opencv opencv-samples gcc make cmake git pkg-config

.PHONY: all clean run snapshot deps-ubuntu deps-fedora deps-arch
