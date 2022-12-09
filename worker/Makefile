MODULES = ../HashTable
MODULES2 = ../AVLTree
MODULES3 = ../List
MODULES4 = ../SymbolTable


CXXFLAGS = -g -std=c++11 -Wall -I.

PROGRAM = worker

CC = g++

OBJS = worker.o 

include $(MODULES)/make.inc
include $(MODULES2)/make.inc
include $(MODULES3)/make.inc
include $(MODULES4)/make.inc


$(PROGRAM): clean $(OBJS)
	$(CXX) $(OBJS) -o $(PROGRAM) -lm

clean:
	rm -f $(PROGRAM) $(OBJS)

run: $(PROGRAM)
	./$(PROGRAM)
