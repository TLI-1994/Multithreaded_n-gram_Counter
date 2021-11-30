#include <filesystem>
#include <map>
#include <regex>
#include <set>
#include <string>

namespace fs = std::filesystem;

namespace wc {
class wordCounter {
    // main storage structure for word frequencies
    std::map<std::string, uint64_t> freq;

    fs::path dir;
    uint32_t n;
    uint32_t num_threads;
    char default_punct = '|';

    void process_file(fs::path& file, std::map<std::string, uint64_t>& local_freq,
                      std::map<std::string, uint64_t>& local_n_gram_freq,
                      std::vector<std::string>& local_n_grams);
    void retrieve_n_gram(std::sregex_token_iterator iter, std::sregex_token_iterator end,
                         std::map<std::string, uint64_t>& local_n_gram_freq,
                         std::vector<std::string>& local_n_grams);

   public:
    wordCounter(const std::string& dir, uint32_t n, uint32_t num_threads);
    void compute();
    void display();
};
}  // namespace wc
