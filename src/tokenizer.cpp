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

bool iequals_ascii(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    for (std::size_t k = 0; k < a.size(); ++k) {
        if (ascii_lower(static_cast<unsigned char>(a[k])) != ascii_lower(static_cast<unsigned char>(b[k]))) {
            return false;
        }
    }
    return true;
}

bool is_whitespace(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f';
}

bool is_attr_name_char(unsigned char c) {
    return (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') ||
           (c == '-') ||
           (c == '_') ||
           (c == ':');
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

Token make_comment_token(std::string data, const SourceLocation& start) {
    Token t;
    t.type = TokenType::Comment;
    t.location = start;
    t.data = CommentData{std::move(data)};
    return t;
}

Token make_doctype_token(std::string name, bool is_valid, const SourceLocation& start) {
    Token t;
    t.type = TokenType::Doctype;
    t.location = start;
    DoctypeData d;
    d.name = std::move(name);
    d.is_valid = is_valid;
    t.data = std::move(d);
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
            if (!is_whitespace(c)) break;
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

        // Markup declarations: comments and doctype.
        if (peek() == '!') {
            consume(); // '!'

            // Comment: <!-- ... -->
            if (i + 1 < n && peek() == '-' && peek(1) == '-') {
                consume(); // '-'
                consume(); // '-'

                std::string data;
                bool found_end = false;
                while (i < n) {
                    if (peek() == '-' && peek(1) == '-' && peek(2) == '>') {
                        found_end = true;
                        consume();
                        consume();
                        consume();
                        break;
                    }
                    data.push_back(consume());
                }

                if (!found_end) {
                    push_error(result.errors, "Unterminated comment", tag_start);
                }

                result.tokens.push_back(make_comment_token(std::move(data), tag_start));
                continue;
            }

            // Doctype: <!DOCTYPE name> (case-insensitive)
            const std::size_t word_begin = i;
            while (i < n && ((peek() >= 'A' && peek() <= 'Z') || (peek() >= 'a' && peek() <= 'z'))) {
                consume();
            }
            const std::size_t word_end = i;
            const std::string_view word = std::string_view(input).substr(word_begin, word_end - word_begin);

            if (iequals_ascii(word, "DOCTYPE")) {
                skip_whitespace();

                bool is_valid = true;
                const SourceLocation name_loc = loc;
                const std::size_t name_begin = i;
                while (i < n && !is_whitespace(static_cast<unsigned char>(peek())) && peek() != '>') {
                    consume();
                }
                const std::size_t name_end = i;
                std::string name;
                if (name_end == name_begin) {
                    is_valid = false;
                    push_error(result.errors, "Malformed doctype", name_loc);
                } else {
                    name = lower_ascii(std::string_view(input).substr(name_begin, name_end - name_begin));
                }

                skip_whitespace();
                if (i >= n) {
                    is_valid = false;
                    push_error(result.errors, "Malformed doctype", tag_start);
                    result.tokens.push_back(make_doctype_token(std::move(name), is_valid, tag_start));
                    break;
                }

                if (peek() != '>') {
                    is_valid = false;
                    push_error(result.errors, "Malformed doctype", loc);
                    while (i < n && peek() != '>') consume();
                }

                if (i < n && peek() == '>') consume();
                result.tokens.push_back(make_doctype_token(std::move(name), is_valid, tag_start));
                continue;
            }

            // Unknown markup declaration: recover by skipping to '>' or EOF.
            push_error(result.errors, "Invalid markup declaration", tag_start);
            while (i < n && peek() != '>') consume();
            if (i < n && peek() == '>') consume();
            continue;
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
                if (is_whitespace(c)) {
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
        TagData tag;
        tag.name = tag_name;
        tag.self_closing = false;
        tag.is_start_tag = true;

        auto attr_is_duplicate = [&](const std::string& name) -> bool {
            for (const Attribute& a : tag.attributes) {
                if (a.name == name) return true;
            }
            return false;
        };

        auto parse_attribute_value = [&](Attribute& attr, bool store_value) -> bool {
            // Returns false if we hit EOF (unterminated quote), true otherwise.
            skip_whitespace();
            if (i >= n) return false;

            const char q = peek();
            if (q == '"' || q == '\'') {
                const SourceLocation quote_loc = loc;
                consume(); // opening quote
                std::string value;
                while (i < n && peek() != q) {
                    value.push_back(consume());
                }
                if (i >= n) {
                    push_error(result.errors, "Unterminated attribute quote", quote_loc);
                    if (store_value) {
                        attr.value = std::move(value);
                        attr.has_value = true;
                    }
                    return false;
                }
                consume(); // closing quote
                if (store_value) {
                    attr.value = std::move(value);
                    attr.has_value = true;
                }
                return true;
            }

            // Unquoted: ends at whitespace, '>', or a self-closing delimiter.
            std::string value;
            while (i < n) {
                const unsigned char c = static_cast<unsigned char>(peek());
                if (is_whitespace(c) || peek() == '>') break;
                if (peek() == '/') {
                    std::size_t j = i + 1;
                    while (j < n && is_whitespace(static_cast<unsigned char>(input[j]))) ++j;
                    if (j < n && input[j] == '>') break;
                }
                value.push_back(consume());
            }
            if (store_value) {
                attr.value = std::move(value);
                attr.has_value = true;
            }
            return true;
        };

        while (i < n) {
            skip_whitespace();

            if (i >= n) break;
            if (peek() == '>') break;

            if (peek() == '/') {
                consume(); // '/'
                skip_whitespace();
                if (i < n && peek() == '>') {
                    self_closing = true;
                    tag.self_closing = true;
                    break;
                }
                push_error(result.errors, "Unexpected '/' in tag", loc);
                continue;
            }

            // Parse attribute name.
            const SourceLocation attr_start = loc;
            const std::size_t attr_name_begin = i;
            while (i < n && is_attr_name_char(static_cast<unsigned char>(peek()))) {
                consume();
            }
            const std::size_t attr_name_end = i;

            if (attr_name_end == attr_name_begin) {
                push_error(result.errors, "Malformed attribute name", attr_start);
                // Avoid infinite loop.
                consume();
                continue;
            }

            const std::string attr_name = std::string(std::string_view(input).substr(attr_name_begin, attr_name_end - attr_name_begin));
            const bool duplicate = attr_is_duplicate(attr_name);
            if (duplicate) {
                push_error(result.errors, "Duplicate attribute name: " + attr_name, attr_start);
            }

            Attribute attr;
            attr.name = attr_name;
            attr.value = "";
            attr.has_value = false;

            // Optional value.
            skip_whitespace();
            if (i < n && peek() == '=') {
                consume(); // '='
                const bool ok = parse_attribute_value(attr, !duplicate);
                if (!ok) {
                    // EOF during quoted value; stop parsing this tag.
                    break;
                }
            } else {
                // Boolean attribute.
            }

            if (!duplicate) tag.attributes.push_back(std::move(attr));
        }

        if (i >= n) {
            push_error(result.errors, "Unterminated tag", tag_start);
            break;
        }

        if (peek() == '>') consume(); // '>'
        tag.self_closing = self_closing;
        Token t;
        t.type = TokenType::StartTag;
        t.location = tag_start;
        t.data = std::move(tag);
        result.tokens.push_back(std::move(t));
    }

    result.tokens.push_back(make_eof_token(loc));
    return result;
}
