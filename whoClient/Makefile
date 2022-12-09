CXXFLAGS = -g -std=c++11 -Wall -I.

PROGRAM = whoClient

CC = g++

OBJS = whoClient.o 

$(PROGRAM): clean $(OBJS)
	$(CXX) $(OBJS) -o $(PROGRAM) -lm -lpthread

clean:
	rm -f $(PROGRAM) $(OBJS)

run: $(PROGRAM)
	./$(PROGRAM)