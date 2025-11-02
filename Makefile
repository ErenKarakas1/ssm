CXX = g++
CXXFLAGS = -std=c++23 -Wall -Wextra -pedantic -O3 -march=native -I./include
TARGET = ssm
SOURCE = src/ssm.cpp main.cpp

all: $(SOURCE)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SOURCE)

clean:
	rm -f $(TARGET)

install: $(TARGET)
	mkdir -p $(HOME)/.local/bin
	cp $(TARGET) $(HOME)/.local/bin/

.PHONY: clean install
