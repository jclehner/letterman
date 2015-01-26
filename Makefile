CXXFLAGS=-Wall -std=c++11 -g -Wextra
LDFLAGS=-lhivex 
CXX=g++

EXEC = letterman
SOURCES = $(wildcard *.cc)
OBJECTS = $(SOURCES:.cc=.o)

UNAME = $(shell uname)

ifeq ($(UNAME), Linux)
	LDFLAGS += -ludev
	CXXFLAGS += -DLETTERMAN_LINUX
endif

ifeq ($(UNAME), Darwin)
	#ARCHES = -arch i386 -arch x86_64
	CXXFLAGS += $(ARCHES)
	LDFLAGS += $(ARCHES) -framework CoreFoundation -framework DiskArbitration -framework IOKit
	CXXFLAGS += -DLETTERMAN_MACOSX

endif

$(EXEC): $(OBJECTS)
	$(CXX) $(OBJECTS) -o $(EXEC) $(LDFLAGS)

%.o: %.cc
	$(CXX) -c $(CXXFLAGS) $< -o $@

clean:
	rm -f *.o $(EXEC)
