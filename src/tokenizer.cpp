#include "tokenizer.h"

#include <cctype>
#include <string_view>

namespace {

bool is_tag_name_char(unsigned char c) {
    return (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') ||
           (c == '-');
}

char ascii_lower(unsigned char c) {
    if (c >= 'A' && c <= 'Z') return static_cast<char>(c + ('a' - 'A'));
    return static_cast<char>(c);
}

std::string lower_ascii(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (unsigned char c : s) out.push_back(ascii_lower(c));
    return out;
}

void advance_one(char c, SourceLocation& loc) {
    loc.byte_offset += 1;
    if (c == '\n') {
        loc.line += 1;
        loc.column = 1;
    } else {
        loc.column += 1;
    }
}

void push_error(std::vector<TokenizerError>& errors, std::string message, const SourceLocation& at) {
    TokenizerError e;
    e.type = ErrorType::TokenizationError;
    e.message = std::move(message);
    e.location = at;
    errors.push_back(std::move(e));
}

Token make_text_token(std::string data, const SourceLocation& start) {
    Token t;
    t.type = TokenType::Text;
    t.location = start;
    t.data = TextData{std::move(data)};
    return t;
}

Token make_tag_token(bool is_start_tag, std::string name, bool self_closing, const SourceLocation& start) {
    Token t;
    t.type = is_start_tag ? TokenType::StartTag : TokenType::EndTag;
    t.location = start;

    TagData tag;
    tag.name = std::move(name);
    tag.self_closing = self_closing;
    tag.is_start_tag = is_start_tag;
    t.data = std::move(tag);
    return t;
}

Token make_eof_token(const SourceLocation& at) {
    Token t;
    t.type = TokenType::EOFToken;
    t.location = at;
    t.data = EofData{};
    return t;
}

} // namespace

TokenizerResult Tokenizer::tokenize(const std::string& input) const {
    TokenizerResult result;

    const std::size_t n = input.size();
    std::size_t i = 0;
    SourceLocation loc{};

    auto peek = [&](std::size_t offset = 0) -> char {
        const std::size_t j = i + offset;
        return (j < n) ? input[j] : '\0';
    };

    auto consume = [&]() -> char {
        if (i >= n) return '\0';
        const char c = input[i];
        ++i;
        advance_one(c, loc);
        return c;
    };

    auto skip_whitespace = [&]() {
        while (i < n) {
            const unsigned char c = static_cast<unsigned char>(peek());
            if (!(c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f')) break;
            consume();
        }
    };

    while (i < n) {
        if (peek() != '<') {
            const SourceLocation text_start = loc;
            std::string text;
            while (i < n && peek() != '<') {
                text.push_back(consume());
            }
            if (!text.empty()) result.tokens.push_back(make_text_token(std::move(text), text_start));
            continue;
        }

        const SourceLocation tag_start = loc;
        consume(); // '<'

        if (i >= n) {
            result.tokens.push_back(make_text_token("<", tag_start));
            break;
        }

        const char next = peek();
        const bool is_end_tag = (next == '/');
        if (is_end_tag) consume(); // '/'

        const std::size_t name_begin = i;
        while (i < n && is_tag_name_char(static_cast<unsigned char>(peek()))) {
            consume();
        }
        const std::size_t name_end = i;

        if (name_end == name_begin) {
            push_error(result.errors, "Invalid tag start after '<'", tag_start);
            result.tokens.push_back(make_text_token("<", tag_start));
            continue;
        }

        const std::string tag_name = lower_ascii(std::string_view(input).substr(name_begin, name_end - name_begin));
        skip_whitespace();

        bool self_closing = false;

        if (is_end_tag) {
            // attributes in end tags are an error; we recover by skipping to '>'.
            while (i < n && peek() != '>') {
                const unsigned char c = static_cast<unsigned char>(peek());
                if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f') {
                    consume();
                    continue;
                }
                push_error(result.errors, "Attributes are not allowed in end tags", loc);
                // Consume until '>' or EOF.
                while (i < n && peek() != '>') consume();
                break;
            }

            if (i >= n) {
                push_error(result.errors, "Unterminated tag", tag_start);
                break;
            }

            consume(); // '>'
            result.tokens.push_back(make_tag_token(false, tag_name, false, tag_start));
            continue;
        }

        // Start tag
        while (i < n && peek() != '>') {
            if (peek() == '/') {
                const SourceLocation slash_loc = loc;
                consume(); // '/'
                std::size_t j = i;
                while (j < n) {
                    const unsigned char c = static_cast<unsigned char>(input[j]);
                    if (!(c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f')) break;
                    ++j;
                }
                if (j < n && input[j] == '>') {
                    self_closing = true;
                    skip_whitespace();
                    break;
                }
                push_error(result.errors, "Unexpected '/' in tag", slash_loc);
                continue;
            }

            const unsigned char c = static_cast<unsigned char>(peek());
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f') {
                consume();
                continue;
            }

            push_error(result.errors, "Attributes not supported yet", loc);
            while (i < n && peek() != '>' && peek() != '/') consume();
        }

        if (i >= n) {
            push_error(result.errors, "Unterminated tag", tag_start);
            break;
        }

        if (peek() == '>') consume(); // '>'
        result.tokens.push_back(make_tag_token(true, tag_name, self_closing, tag_start));
    }

    result.tokens.push_back(make_eof_token(loc));
    return result;
}
