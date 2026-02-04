CXX = g++
CC = gcc
CCFLAGS = -Wall -Wextra -Wpedantic -Wcast-align -Wcast-qual -Wconversion -Wdeprecated -Wctor-dtor-privacy -Wdisabled-optimization \
	      -Wdouble-promotion -Wduplicated-branches -Wduplicated-cond -Wfloat-equal -Wformat=2 -Wformat-signedness -Wlogical-op -Wmissing-noreturn \
	      -Wnon-virtual-dtor -Wmissing-declarations -Wnull-dereference -Wnrvo -Wold-style-cast -Wredundant-decls -Wshadow -Wsign-conversion -Wsign-promo \
	      -Wstrict-aliasing=2 -Wstrict-overflow=2 -Wswitch-default -Wtype-limits -Wsuggest-attribute=returns_nonnull -Wundef -Wno-unknown-warning-option \
	      -Wuseless-cast -fstrict-aliasing

CFLAGS = -I./include -I./thirdparty -DSQLITE_OMIT_LOAD_EXTENSION
CXXFLAGS = -std=c++23 $(CCFLAGS) $(CFLAGS)
DEBUG_FLAGS = -Og -ggdb3
RELEASE_FLAGS = -O3 -march=native
TARGET = ssm

CPP_SOURCES = main.cpp src/ssm.cpp
C_SOURCES = thirdparty/sqlite3.c
C_OBJS = $(C_SOURCES:.c=.o)

all: release

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

release: $(CPP_SOURCES) $(C_OBJS)
	$(CXX) $(CXXFLAGS) $(RELEASE_FLAGS) -o $(TARGET) $(CPP_SOURCES) $(C_OBJS)

debug: $(CPP_SOURCES) $(C_OBJS)
	$(CXX) $(CXXFLAGS) $(DEBUG_FLAGS) -o $(TARGET) $(CPP_SOURCES) $(C_OBJS)

clean:
	rm -f $(TARGET) $(C_OBJS)

install: $(TARGET)
	mkdir -p $(HOME)/.local/bin
	cp $(TARGET) $(HOME)/.local/bin/

.PHONY: all release debug clean install
