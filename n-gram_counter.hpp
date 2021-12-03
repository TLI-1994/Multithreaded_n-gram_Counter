#include <filesystem>
#include <map>
#include <regex>
#include <set>
#include <string>

namespace fs = std::filesystem;

namespace wc {
class wordCounter {
    fs::path dir;
    uint32_t n;
    uint32_t num_threads;
    const uint32_t header = 5;
    char default_punct = '|';

    void process_file(fs::path& file, std::map<std::string, uint64_t>& local_n_gram_freq);
    void retrieve_n_gram(std::sregex_token_iterator iter, std::sregex_token_iterator end,
                         std::map<std::string, uint64_t>& local_n_gram_freq);

   public:
    wordCounter(const std::string& dir, uint32_t n, uint32_t num_threads);
    void process();
};
}  // namespace wc
