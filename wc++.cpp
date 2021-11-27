#include <iostream>

#include "word_counter.hpp"

// this program computes word frequencies for all .h and .c files in the given directory and its subdirectories
int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cout << "Usage: " << argv[0] << " <dir> <num-threads>" << std::endl;
        return 1;
    }

    wc::wordCounter word_counter(argv[1], std::stoi(argv[2]));
    word_counter.compute();
    word_counter.display();
}
