CCFLAGS=-Wall -std=c++11 -g
LDFLAGS=-lhivex 
CC=g++

EXEC = letterman
SOURCES = $(wildcard *.cc)
OBJECTS = $(SOURCES:.cc=.o)

UNAME = $(shell uname)

ifeq ($(UNAME), Linux)
	LDFLAGS += -ludev
endif

ifeq ($(UNAME), Darwin)
	LDFLAGS += -framework CoreFoundation -framework DiskArbitration -framework IOKit
endif

$(EXEC): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(EXEC) $(LDFLAGS)

%.o: %.cc
	$(CC) -c $(CCFLAGS) $< -o $@

clean:
	rm -f *.o $(EXEC)

