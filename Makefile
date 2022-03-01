OBJS	= ngc++.o n-gram_counter.o utils.o
SOURCE	= ngc++.cpp n-gram_counter.cpp utils.cpp
HEADER	= n-gram_counter.hpp utils.hpp
OUT	= ngc++
CC	 = g++
FLAGS	 = -g -c -Wall -O3
LFLAGS	 = -lpthread -lstdc++fs

all: $(OBJS)
	$(CC) -g $(OBJS) -o $(OUT) $(LFLAGS)

ngc++.o: ngc++.cpp
	$(CC) $(FLAGS) ngc++.cpp -std=c++17

n-gram_counter.o: n-gram_counter.cpp
	$(CC) $(FLAGS) n-gram_counter.cpp -std=c++17

utils.o: utils.cpp
	$(CC) $(FLAGS) utils.cpp -std=c++17


clean:
	rm -f $(OBJS) $(OUT)
