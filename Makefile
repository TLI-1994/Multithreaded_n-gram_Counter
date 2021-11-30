OBJS	= wc++.o word_counter.o utils.o
SOURCE	= wc++.cpp word_counter.cpp utils.cpp
HEADER	= word_counter.hpp utils.hpp
OUT	= hw4
CC	 = g++
FLAGS	 = -g -c -Wall -O3
LFLAGS	 = -lpthread -lstdc++fs

all: $(OBJS)
	$(CC) -g $(OBJS) -o $(OUT) $(LFLAGS)

wc++.o: wc++.cpp
	$(CC) $(FLAGS) wc++.cpp -std=c++17

word_counter.o: word_counter.cpp
	$(CC) $(FLAGS) word_counter.cpp -std=c++17

utils.o: utils.cpp
	$(CC) $(FLAGS) utils.cpp -std=c++17


clean:
	rm -f $(OBJS) $(OUT)