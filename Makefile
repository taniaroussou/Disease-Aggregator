MODULES = HashTable
MODULES4 = SymbolTable

CXXFLAGS = -g -std=c++11 -Wall -I.

PROGRAM = master

CC = g++

OBJS = master.o main.o

include $(MODULES)/make.inc
include $(MODULES4)/make.inc

$(PROGRAM): clean $(OBJS)
	$(CXX) $(OBJS) -o $(PROGRAM) -lm 
	$(MAKE) -C worker
	$(MAKE) -C whoServer
	$(MAKE) -C whoClient

clean:
	rm -f $(PROGRAM) $(OBJS) 

run: $(PROGRAM)
	./$(PROGRAM)
