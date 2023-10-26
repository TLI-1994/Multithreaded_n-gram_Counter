#include "n-gram_counter.hpp"

#include <algorithm>
#include <atomic>
#include <fstream>
#include <future>
#include <iostream>
#include <mutex>
#include <queue>
#include <regex>
#include <thread>
#include <vector>

#include "utils.hpp"

wc::wordCounter::wordCounter(const std::string& dir, uint32_t n,
                             uint32_t num_threads)
    : dir(dir), n(n), num_threads(num_threads) {}

void wc::wordCounter::process() {
    std::vector<std::vector<std::promise<fmap>>> promises;
    for (uint32_t i = 0; i != num_threads; i++)
        promises.push_back(std::vector<std::promise<fmap>>(num_threads));

    std::vector<std::vector<std::future<fmap>>> futures(num_threads);
    for (uint32_t i = 0; i != num_threads; i++) {
        for (uint32_t j = 0; j != num_threads; j++)
            futures[i].push_back(promises[j][i].get_future());
    }

    std::vector<fs::path> all_files = utils::find_all_files(
        dir, [](const std::string& extension) { return extension == ".txt"; });

    // map each file to a thread
    std::vector<std::vector<fs::path>> files_to_sweep(num_threads);
    for (uint32_t i = 0; i < all_files.size(); i++)
        files_to_sweep[i % num_threads].push_back(all_files[i]);

    std::mutex wc_mtx;
    std::atomic<uint32_t> display_id = 0;
    std::condition_variable cv;
    auto sweep = [this, &wc_mtx, &display_id, &cv](
                     uint32_t thread_id,
                     std::vector<fs::path>&& my_files_to_sweep,
                     std::vector<std::promise<fmap>>&& my_promises,
                     std::vector<std::future<fmap>>&& my_futures) {
        fmap local_freq;
        for (fs::path file : my_files_to_sweep) {
            process_file(file, local_freq);
        }

        // group by
        std::vector<fmap> subsets(num_threads);
        for (const auto& [word, count] : local_freq) {
            size_t target_thread = std::hash<std::string>{}(word) % num_threads;
            subsets[target_thread][word] = count;
        }

        // shuffle
        for (uint32_t i = 0; i != num_threads; i++) {
            my_promises[i].set_value(subsets[i]);
        }

        // reduce
        fmap final_map;
        std::queue<std::future<fmap>> futures_queue;
        for (auto& fut : my_futures) {
            futures_queue.push(std::move(fut));
        }
        while (!futures_queue.empty()) {
            std::future<fmap>& front_future = futures_queue.front();
            if (front_future.wait_for(std::chrono::seconds(0)) ==
                std::future_status::ready) {
                fmap my_fmap = front_future.get();
                for (auto& p : my_fmap) final_map[p.first] += p.second;
                futures_queue.pop();
            } else {
                futures_queue.push(std::move(front_future));
                futures_queue.pop();
            }
        }

        // sorting
        using pair_t = std::pair<std::string, uint64_t>;
        std::vector<pair_t> freq_vec(final_map.size());
        uint32_t index = 0;
        for (auto [word, cnt] : final_map) {
            freq_vec[index++] = {word, cnt};
        }
        std::sort(freq_vec.begin(), freq_vec.end(),
                  [](const pair_t& p1, const pair_t& p2) {
                      // decreasing order, of course
                      return p1.second > p2.second ||
                             (p1.second == p2.second && p1.first < p2.first);
                  });

        // display
        std::unique_lock<std::mutex> lock(wc_mtx);
        while (display_id.load() != thread_id) cv.wait(lock);
        std::cout << " * =================================== Thread "
                  << thread_id << std::endl;
        for (size_t i = 0; i != freq_vec.size() && i != header; i++)
            std::cout << " | " << freq_vec[i].first << ": "
                      << freq_vec[i].second << std::endl;
        std::cout << " * --------------------------------------------- "
                  << std::endl;
        display_id++;
        cv.notify_all();
    };
    // start all threads and wait for them to finish
    std::vector<std::thread> workers;
    for (uint32_t i = 0; i < num_threads; ++i)
        workers.push_back(std::thread(sweep, i, std::move(files_to_sweep[i]),
                                      std::move(promises[i]),
                                      std::move(futures[i])));
    for (auto& worker : workers) worker.join();
}

void wc::wordCounter::process_file(fs::path& file, fmap& local_freq) {
    // read the entire file and update local_freq
    std::ifstream fin(file);
    std::stringstream buffer;
    buffer << fin.rdbuf();
    std::string contents = buffer.str();
    std::transform(contents.begin(), contents.end(), contents.begin(),
                   [this](unsigned char c) {
                       if (c == '\n' || c == '\t')
                           return (int) ' ';
                       if ((c >= '0' && c <= '9') || std::ispunct(c))
                           return (int) default_punct;
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
        const std::regex rgx("\\W+");
        std::string my_sentence = my_sentence_stream.str();
        std::sregex_token_iterator iter(my_sentence.begin(), my_sentence.end(),
                                        rgx, -1);
        std::sregex_token_iterator end;
        // in case the first character of my_sentence is space, skip the parsed
        // empty string
        if (*iter == "")
            iter++;
        retrieve_n_gram(iter, end, local_freq);
        my_string = i == my_string.size() ? my_string.substr(i)
                                          : my_string.substr(i + 1);
    }
}

void wc::wordCounter::retrieve_n_gram(std::sregex_token_iterator iter,
                                      std::sregex_token_iterator end,
                                      fmap& local_freq) {
    if (std::distance(iter, end) < n)
        return;
    std::stringstream my_n_gram_stream;
    uint32_t i = 0;
    while (i < n) {
        my_n_gram_stream << *(std::next(iter, i));
        if (i != n - 1)
            my_n_gram_stream << " ";
        i++;
    }
    std::string my_n_gram = my_n_gram_stream.str();
    local_freq[my_n_gram]++;
    retrieve_n_gram(++iter, end, local_freq);
}