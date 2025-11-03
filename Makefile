CXX = g++
CXXFLAGS = -std=c++23 -Wall -Wextra -Wpedantic -I./include
DEBUG_FLAGS = -Og -ggdb3
RELEASE_FLAGS = -O3 -march=native
TARGET = ssm
SOURCE = src/ssm.cpp main.cpp

all: $(SOURCE)
	$(CXX) $(CXXFLAGS) $(RELEASE_FLAGS) -o $(TARGET) $(SOURCE)

debug: $(SOURCE)
	$(CXX) $(CXXFLAGS) $(DEBUG_FLAGS) -o $(TARGET) $(SOURCE)

clean:
	rm -f $(TARGET)

install: $(TARGET)
	mkdir -p $(HOME)/.local/bin
	cp $(TARGET) $(HOME)/.local/bin/

.PHONY: clean install
