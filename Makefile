.PHONY:clean all
CFLAGS=-g -Wall -pthread -w -D_BIN
CFLAGS :=$(CFLAGS) $(shell pkg-config --cflags --libs libva libva-drm libmfx) 
BIN=hwenc

SUBDIR=$(shell ls -d */)     

ROOTSRC=$(wildcard *.cpp)
ROOTOBJ=$(ROOTSRC:%.cpp=%.o)

SUBSRC=$(shell find $(SUBDIR) -name '*.cpp')
SUBOBJ=$(SUBSRC:%.cpp=%.o)
all:clean bin
bin:$(ROOTOBJ) $(SUBOBJ)
#	$(CXX) -o $@ $^ $(CFLAGS)
	$(CXX) -o $(BIN) $^ $(CFLAGS)
.cpp.o:
	$(CXX) -c $< -o $@ $(CFLAGS)
clean:
	rm -f *.o $(BIN) $(ROOTOBJ) $(SUBOBJ)

