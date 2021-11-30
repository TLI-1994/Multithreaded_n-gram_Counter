#include "word_counter.hpp"

#include <algorithm>
#include <atomic>
#include <fstream>
#include <future>
#include <iostream>
#include <mutex>
#include <regex>
#include <thread>
#include <vector>

#include "utils.hpp"

wc::wordCounter::wordCounter(const std::string& dir, uint32_t n, uint32_t num_threads)
    : dir(dir),
      n(n),
      num_threads(num_threads) {
}

void wc::wordCounter::compute() {
    // this tracks which files have already been processed or are being processed
    std::mutex wc_mtx;

    using group_t = std::vector<std::map<std::string, uint64_t>>;
    std::vector<std::promise<group_t>> promises(num_threads);
    std::vector<std::future<group_t>> futures;
    for (size_t i = 0; i < num_threads; i++) {
        futures.push_back(promises[i].get_future());
    }

    std::vector<fs::path> files_to_sweep = utils::find_all_files(dir, [](const std::string& extension) {
        return extension == ".txt";
    });

    std::vector<std::vector<fs::path>> files_per_thread(num_threads);
    for (uint32_t i = 0; i < files_to_sweep.size(); i++) {
        files_per_thread[i % num_threads].push_back(files_to_sweep[i]);
    }

    auto my_sweep = [this, &files_per_thread, &wc_mtx](const uint32_t thread_id, std::promise<group_t>&& my_promise) {
        std::vector<fs::path> local_files_to_sweep = files_per_thread[thread_id];
        std::map<std::string, uint64_t> local_freq;
        std::map<std::string, uint64_t> local_n_gram_freq;
        std::vector<std::string> local_n_grams;

        for (fs::path file : local_files_to_sweep) {
            process_file(file, local_freq, local_n_gram_freq, local_n_grams);
        }

        // group by
        group_t group_by_thread(num_threads);
        for (std::string n_gram : local_n_grams) {
            size_t t = std::hash<std::string>{}(n_gram) % num_threads;
            group_by_thread[t][n_gram] = local_n_gram_freq[n_gram];
        }

        my_promise.set_value(group_by_thread);

        // update this->freq and exit
        std::lock_guard<std::mutex> lock(wc_mtx);
        for (auto [word, cnt] : local_freq) {
            freq[word] += cnt;
        }
    };

    // threads use this atomic as fetch and add to decide on which files to process
    // std::atomic<uint64_t> global_index = 0;

    // auto sweep = [this, &files_to_sweep, &global_index, &wc_mtx]() {
    //     std::map<std::string, uint64_t> local_freq;

    //     uint64_t file_index;
    //     while ((file_index = global_index++) < files_to_sweep.size()) {
    //         process_file(files_to_sweep[file_index], local_freq);
    //     }

    //     // update this->freq and exit
    //     std::lock_guard<std::mutex> lock(wc_mtx);
    //     for (auto [word, cnt] : local_freq) {
    //         freq[word] += cnt;
    //     }
    // };

    // start all threads and wait for them to finish
    std::vector<std::thread> workers;
    for (uint32_t i = 0; i < num_threads; ++i) {
        // workers.push_back(std::thread(sweep));
        workers.push_back(std::thread(my_sweep, i, std::move(promises[i])));
    }

    std::vector<group_t> results;
    for (size_t i = 0; i < num_threads; i++) {
        results.push_back(futures[i].get());
        {
            std::lock_guard<std::mutex> lock(wc_mtx);
            std::cout << "Thread " << i << " results back" << std::endl;
            for (auto map : results[i]) {
                std::cout << map.size() << " ";
            }
            std::cout << std::endl;
        }
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

void wc::wordCounter::process_file(fs::path& file, std::map<std::string, uint64_t>& local_freq,
                                   std::map<std::string, uint64_t>& local_n_gram_freq,
                                   std::vector<std::string>& local_n_grams) {
    // read the entire file and update local_freq
    std::ifstream fin(file);
    std::stringstream buffer;
    buffer << fin.rdbuf();
    std::string contents = buffer.str();
    std::transform(contents.begin(), contents.end(), contents.begin(),
                   [this](unsigned char c) {
                       if (c == '\n' || c == '\t') return (int)' ';
                       if ((c >= '0' && c <= '9') || std::ispunct(c)) return (int)default_punct;
                       return std::tolower(c);
                   });

    // process the file
    std::string my_string = std::string(contents);
    while (my_string.size() > 0) {
        std::stringstream my_sentence_stream;
        size_t i = 0;
        while (my_string[i] != default_punct && i < my_string.size()) {
            my_sentence_stream << my_string[i];
            i++;
        }

        // recursively process the sentence
        const std::regex rgx("\\W+");
        std::string my_sentence = my_sentence_stream.str();
        std::sregex_token_iterator iter(my_sentence.begin(), my_sentence.end(), rgx, -1);
        std::sregex_token_iterator end;
        // in case the first character of my_sentence is space, skip the parsed empty string
        if (*iter == "") iter++;
        retrieve_n_gram(iter, end, local_n_gram_freq, local_n_grams);

        // discard from the beginning of my_string to the appearance of the first punctuation mark
        my_string = i == my_string.size() ? my_string.substr(i) : my_string.substr(i + 1);
    }

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

void wc::wordCounter::retrieve_n_gram(std::sregex_token_iterator iter, std::sregex_token_iterator end,
                                      std::map<std::string, uint64_t>& local_n_gram_freq,
                                      std::vector<std::string>& local_n_grams) {
    static std::mutex io_mutex;
    if (std::distance(iter, end) >= n) {
        std::stringstream my_n_gram_stream;
        uint32_t i = 0;
        while (i < n) {
            my_n_gram_stream << *(std::next(iter, i));
            if (i != n - 1) {
                my_n_gram_stream << " ";
            }
            i++;
        }
        std::string my_n_gram = my_n_gram_stream.str();

        // update local n-gram freq and n-gram table
        if (local_n_gram_freq.find(my_n_gram) == local_n_gram_freq.end()) {
            local_n_grams.push_back(my_n_gram);
        }
        local_n_gram_freq[my_n_gram]++;

        {
            std::scoped_lock lock(io_mutex);
            std::cout << my_n_gram_stream.str() << std::endl;
        }
        retrieve_n_gram(++iter, end, local_n_gram_freq, local_n_grams);
    }
}