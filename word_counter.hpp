#include <filesystem>
#include <map>
#include <regex>
#include <string>

namespace fs = std::filesystem;

namespace wc {
class wordCounter {
    // main storage structure for word frequencies
    std::map<std::string, uint64_t> freq;

    fs::path dir;
    uint32_t n;
    uint32_t num_threads;

    void process_file(fs::path& file, std::map<std::string, uint64_t>& local_freq);
    void process_file_string(const std::string& file_string);
    void process_file_string_helper(std::sregex_token_iterator iter, std::sregex_token_iterator end);

   public:
    wordCounter(const std::string& dir, uint32_t n, uint32_t num_threads);
    void compute();
    void display();
};
}  // namespace wc
