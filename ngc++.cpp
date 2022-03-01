#include <iostream>

#include "n-gram_counter.hpp"

/* this program computes n-gram frequencies for all .txt files in the given directory and its subdirectories
Author: Tianjiao Li @ Cornell MAE
Inspired by: Sagar Jha's wc++ program
Highlight: this program is built around the map-reduce pattern in concurrent programming. */
int main(int argc, char* argv[]) {
    if (argc < 4 || std::string(argv[1]).substr(0, 3) != "-n=" || std::string(argv[2]).substr(0, 3) != "-t=") {
        std::cout << "Usage: " << argv[0] << " -n=<#gram> -t=<#threads> <dir>" << std::endl;
        return 1;
    }
    uint32_t n = std::stoi(std::string(argv[1]).substr(3));
    uint32_t t = std::stoi(std::string(argv[2]).substr(3));
    wc::wordCounter word_counter(argv[3], n, t);
    word_counter.process();
}
