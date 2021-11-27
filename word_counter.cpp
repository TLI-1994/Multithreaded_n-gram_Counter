#include "word_counter.hpp"

#include <algorithm>
#include <atomic>
#include <fstream>
#include <iostream>
#include <mutex>
#include <regex>
#include <thread>
#include <vector>

#include "utils.hpp"

wc::wordCounter::wordCounter(const std::string& dir, uint32_t num_threads)
    : dir(dir),
      num_threads(num_threads) {
}

void wc::wordCounter::compute() {
    // this tracks which files have already been processed or are being processed
    std::mutex wc_mtx;

    std::vector<fs::path> files_to_sweep = utils::find_all_files(dir, [](const std::string& extension) {
        return extension == ".h" || extension == ".c";
    });

    // threads use this atomic as fetch and add to decide on which files to process
    std::atomic<uint64_t> global_index = 0;

    auto sweep = [this, &files_to_sweep, &global_index, &wc_mtx]() {
        std::map<std::string, uint64_t> local_freq;

        uint64_t file_index;
        while ((file_index = global_index++) < files_to_sweep.size()) {
            process_file(files_to_sweep[file_index], local_freq);
        }

        // update this->freq and exit
        std::lock_guard<std::mutex> lock(wc_mtx);
        for (auto [word, cnt] : local_freq) {
            freq[word] += cnt;
        }
    };

    // start all threads and wait for them to finish
    std::vector<std::thread> workers;
    for (uint32_t i = 0; i < num_threads; ++i) {
        workers.push_back(std::thread(sweep));
    }
    for (auto& worker : workers) {
        worker.join();
    }
}

void wc::wordCounter::display() {
    // to print in sorted value order (frequency), convert the map to a vector of pairs and then sort the vector
    using pair_t = std::pair<std::string, uint64_t>;
    std::vector<pair_t> freq_vec(freq.size());
    uint32_t index = 0;
    for (auto [word, cnt] : freq) {
        freq_vec[index++] = {word, cnt};
    }
    std::sort(freq_vec.begin(), freq_vec.end(), [](const pair_t& p1, const pair_t& p2) {
        // decreasing order, of course
        return p1.second > p2.second || (p1.second == p2.second && p1.first < p2.first);
    });

    for (auto [word, cnt] : freq_vec) {
        std::cout << word << ": " << cnt << std::endl;
    }
}

void wc::wordCounter::process_file(fs::path& file, std::map<std::string, uint64_t>& local_freq) {
    // read the entire file and update local_freq
    std::ifstream fin(file);
    std::stringstream buffer;
    buffer << fin.rdbuf();
    std::string contents = buffer.str();
    // break the word into sequences of alphanumeric characters, ignoring other characters
    std::regex rgx("\\W+");
    std::sregex_token_iterator iter(contents.begin(), contents.end(), rgx, -1);
    std::sregex_token_iterator end;
    for (; iter != end; ++iter) {
        if (*iter != "") {
            local_freq[*iter]++;
        }
    }
}
