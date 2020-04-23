PROJECT=server
SOURCES=
SOURCES-CPP=server.cpp message.cpp
SUBSCRIBER-CPP=client.cpp
SUBSCRIBER=subscriber
LIBRARY=nope
INCPATHS=include
LIBPATHS=.
LDFLAGS=
CFLAGS=-c -Wall
CC=gcc
CXX=g++
# Automatic generation of some important lists
OBJECTS=$(SOURCES:.c=.o)
OBJECTS-CPP=$(SOURCES-CPP:.cpp=.o)
INCFLAGS=$(foreach TMP,$(INCPATHS),-I$(TMP))
LIBFLAGS=$(foreach TMP,$(LIBPATHS),-L$(TMP))
# Set up the output file names for the different output types
BINARY=$(PROJECT)

all: $(SOURCES) $(SOURCES-CPP) $(BINARY)

$(BINARY): $(OBJECTS) $(OBJECTS-CPP) $(SUBSCRIBER)
	$(CXX) $(LIBFLAGS) $(OBJECTS-CPP) $(OBJECTS) $(LDFLAGS) -o $@

$(SUBSCRIBER): $(SOURCES-CPP) $(OBJECTS-CPP) $(SUBSCRIBER-CPP)
	$(CXX) $(LIBFLAGS) $(INCFLAGS) message.o $(LDFLAGS) $(SUBSCRIBER-CPP) -o $(SUBSCRIBER)

.c.o:
	$(CXX) $(INCFLAGS) $(CFLAGS) -fPIC $< -o $@

.cpp.o:
	$(CXX) $(INCFLAGS) $(CFLAGS) -fPIC $< -o $@

distclean: clean
	rm -f $(BINARY)

clean:
	rm -f $(OBJECTS)
	rm -f $(OBJECTS-CPP)
	rm -f subscriber

run-server: all
	./server 12345

run-client: all
	./subscriber 238 127.0.0.1 12345
