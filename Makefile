CCFLAGS=-Wall -std=c++11 -g
LDFLAGS=-lhivex -ludev
CC=g++

EXEC = letterman
SOURCES = $(wildcard *.cc)
OBJECTS = $(SOURCES:.cc=.o)

$(EXEC): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(EXEC) $(LDFLAGS)

%.o: %.cc
	$(CC) -c $(CCFLAGS) $< -o $@

clean:
	rm -f $(EXEC) $(OBJECTS)

