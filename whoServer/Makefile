MODULES4 = ../SymbolTable
MODULES5 = ../RingBuffer

CXXFLAGS = -g -std=c++11 -Wall -I.

PROGRAM = whoServer

CC = g++

OBJS = whoServer.o 

include $(MODULES4)/make.inc
include $(MODULES5)/make.inc


$(PROGRAM): clean $(OBJS)
	$(CXX) $(OBJS) -o $(PROGRAM) -lm -lpthread

clean:
	rm -f $(PROGRAM) $(OBJS)

run: $(PROGRAM)
	./$(PROGRAM)
