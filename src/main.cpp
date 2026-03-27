#include <iostream>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

#include "output.h"
#include "tokenizer.h"

namespace {

constexpr std::uintmax_t kDefaultMaxBytes = 5u * 1024u * 1024u;

bool ends_with_case_insensitive(std::string_view s, std::string_view suffix) {
    if (suffix.size() > s.size()) return false;
    const std::size_t start = s.size() - suffix.size();
    for (std::size_t i = 0; i < suffix.size(); ++i) {
        unsigned char a = static_cast<unsigned char>(s[start + i]);
        unsigned char b = static_cast<unsigned char>(suffix[i]);
        if (a >= 'A' && a <= 'Z') a = static_cast<unsigned char>(a + ('a' - 'A'));
        if (b >= 'A' && b <= 'Z') b = static_cast<unsigned char>(b + ('a' - 'A'));
        if (a != b) return false;
    }
    return true;
}

bool is_html_path(std::string_view path) {
    return ends_with_case_insensitive(path, ".html") || ends_with_case_insensitive(path, ".htm");
}

void print_usage(std::ostream& out) {
    out << "Usage: mini_html_tokenizer <path> [--json] [--errors]\n";
}

struct CliOptions {
    std::string path;
    bool json = false;
    bool errors = false;
};

bool parse_args(int argc, char** argv, CliOptions& out_opt) {
    std::vector<std::string_view> args;
    args.reserve(static_cast<std::size_t>(argc));
    for (int i = 1; i < argc; ++i) args.emplace_back(argv[i]);

    for (std::string_view a : args) {
        if (a == "--json") {
            out_opt.json = true;
            continue;
        }
        if (a == "--errors") {
            out_opt.errors = true;
            continue;
        }
        if (!a.empty() && a[0] == '-') {
            std::cerr << "Error: unknown option " << a << "\n";
            return false;
        }
        if (!out_opt.path.empty()) {
            std::cerr << "Error: multiple input paths provided\n";
            return false;
        }
        out_opt.path = std::string(a);
    }

    return !out_opt.path.empty();
}

bool read_file_with_limit(const std::string& path, std::uintmax_t max_bytes, std::string& out_contents) {
    std::error_code ec;
    const std::uintmax_t size = std::filesystem::file_size(path, ec);
    if (ec) {
        std::cerr << "Error: cannot stat file\n";
        return false;
    }
    if (size > max_bytes) {
        std::cerr << "Error: file exceeds size limit (" << max_bytes << " bytes)\n";
        return false;
    }

    std::ifstream in(path, std::ios::binary);
    if (!in) {
        std::cerr << "Error: cannot read file\n";
        return false;
    }

    out_contents.clear();
    out_contents.resize(static_cast<std::size_t>(size));
    if (size > 0) {
        in.read(out_contents.data(), static_cast<std::streamsize>(size));
        if (!in) {
            std::cerr << "Error: file read failed\n";
            return false;
        }
    }
    return true;
}

}
int main(int argc, char** argv) {
    CliOptions opt;
    if (!parse_args(argc, argv, opt)) {
        print_usage(std::cerr);
        return 2;
    }

    if (!is_html_path(opt.path)) {
        std::cerr << "Error: non-HTML file (expected .html or .htm)\n";
        return 2;
    }

    std::string contents;
    if (!read_file_with_limit(opt.path, kDefaultMaxBytes, contents)) {
        return 2;
    }

    const Tokenizer tokenizer;
    const TokenizerResult result = tokenizer.tokenize(contents);

    if (opt.json) {
        print_tokens_json(std::cout, result.tokens);
        std::cout << "\n";
    } else {
        print_tokens_human(std::cout, result.tokens);
    }

    if (opt.errors) {
        print_errors(std::cerr, result.errors);
    }

    return has_fatal_tokenization_error(result.errors) ? 3 : 0;
}
