#include "tokenizer.h"

#include <cctype>
#include <string_view>

namespace {

enum class State {
    ReadingText,
    ReadingTagOpen,
    ReadingTagName,
    PreparingToReadAttributeName,
    ReadingAttributeName,
    AfterReadingAttributeName,
    PreparingToReadAttributeValue,
    ReadingQuotedAttributeValue,
    ReadingUnquotedAttributeValue,
    ProcessingSelfClosingTag,
    ReadingEndTagOpen,
    ReadingMarkupDeclaration,
};

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
        if (ascii_lower(static_cast<unsigned char>(a[k])) !=
            ascii_lower(static_cast<unsigned char>(b[k])))
            return false;
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

void push_error(std::vector<TokenizerError>& errors, std::string message,
                const SourceLocation& at) {
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

Token make_tag_token(bool is_start, std::string name, bool self_closing,
                     const SourceLocation& start) {
    Token t;
    t.type = is_start ? TokenType::StartTag : TokenType::EndTag;
    t.location = start;
    TagData tag;
    tag.name = std::move(name);
    tag.self_closing = self_closing;
    tag.is_start_tag = is_start;
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

Token make_doctype_token(std::string name, bool is_valid,
                         const SourceLocation& start) {
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
            if (!is_whitespace(static_cast<unsigned char>(peek()))) break;
            consume();
        }
    };

    State state = State::ReadingText;

    std::string text_buffer;
    SourceLocation text_start = loc;

    SourceLocation tag_start{};
    TagData current_tag{};

    Attribute current_attr{};
    SourceLocation attr_start{};
    bool current_attr_is_duplicate = false;

    char quote_char = '"';
    SourceLocation quote_start{};
    std::string value_buffer;

    while (i < n) {
        switch (state) {

        case State::ReadingText: {
            if (peek() == '<') {
                if (!text_buffer.empty()) {
                    result.tokens.push_back(
                        make_text_token(std::move(text_buffer), text_start));
                    text_buffer.clear();
                }
                tag_start = loc;
                consume();
                state = State::ReadingTagOpen;
            } else {
                if (text_buffer.empty()) text_start = loc;
                text_buffer.push_back(consume());
            }
            break;
        }

        case State::ReadingTagOpen: {
            if (peek() == '!') {
                consume();
                state = State::ReadingMarkupDeclaration;
            } else if (peek() == '/') {
                consume();
                state = State::ReadingEndTagOpen;
            } else if (is_tag_name_char(static_cast<unsigned char>(peek()))) {
                current_tag = TagData{};
                current_tag.is_start_tag = true;
                state = State::ReadingTagName;
            } else {
                push_error(result.errors, "Invalid tag start after '<'",
                           tag_start);
                result.tokens.push_back(make_text_token("<", tag_start));
                state = State::ReadingText;
            }
            break;
        }

        case State::ReadingTagName: {
            const std::size_t name_begin = i;
            while (i < n &&
                   is_tag_name_char(static_cast<unsigned char>(peek()))) {
                consume();
            }
            current_tag.name = lower_ascii(
                std::string_view(input).substr(name_begin, i - name_begin));
            state = State::PreparingToReadAttributeName;
            break;
        }

        case State::ReadingEndTagOpen: {
            const std::size_t name_begin = i;
            while (i < n &&
                   is_tag_name_char(static_cast<unsigned char>(peek()))) {
                consume();
            }

            if (i == name_begin) {
                push_error(result.errors, "Invalid tag start after '<'",
                           tag_start);
                result.tokens.push_back(make_text_token("<", tag_start));
                state = State::ReadingText;
                break;
            }

            std::string tag_name = lower_ascii(
                std::string_view(input).substr(name_begin, i - name_begin));

            skip_whitespace();

            while (i < n && peek() != '>') {
                if (is_whitespace(static_cast<unsigned char>(peek()))) {
                    consume();
                    continue;
                }
                push_error(result.errors,
                           "Attributes are not allowed in end tags", loc);
                while (i < n && peek() != '>') consume();
                break;
            }

            if (i >= n) {
                push_error(result.errors, "Unterminated tag", tag_start);
                state = State::ReadingText;
                break;
            }

            consume();
            result.tokens.push_back(
                make_tag_token(false, std::move(tag_name), false, tag_start));
            state = State::ReadingText;
            break;
        }

        case State::ReadingMarkupDeclaration: {
            if (i + 1 < n && peek() == '-' && peek(1) == '-') {
                consume();
                consume();

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
                    push_error(result.errors, "Unterminated comment",
                               tag_start);
                }

                result.tokens.push_back(
                    make_comment_token(std::move(data), tag_start));
                state = State::ReadingText;
                break;
            }

            const std::size_t word_begin = i;
            while (i < n && ((peek() >= 'A' && peek() <= 'Z') ||
                             (peek() >= 'a' && peek() <= 'z'))) {
                consume();
            }
            const std::string_view word =
                std::string_view(input).substr(word_begin, i - word_begin);

            if (iequals_ascii(word, "DOCTYPE")) {
                skip_whitespace();

                bool is_valid = true;
                const SourceLocation name_loc = loc;
                const std::size_t name_begin = i;
                while (i < n &&
                       !is_whitespace(static_cast<unsigned char>(peek())) &&
                       peek() != '>') {
                    consume();
                }

                std::string name;
                if (i == name_begin) {
                    is_valid = false;
                    push_error(result.errors, "Malformed doctype", name_loc);
                } else {
                    name = lower_ascii(std::string_view(input).substr(
                        name_begin, i - name_begin));
                }

                skip_whitespace();

                if (i >= n) {
                    is_valid = false;
                    push_error(result.errors, "Malformed doctype", tag_start);
                    result.tokens.push_back(make_doctype_token(
                        std::move(name), is_valid, tag_start));
                    state = State::ReadingText;
                    break;
                }

                if (peek() != '>') {
                    is_valid = false;
                    push_error(result.errors, "Malformed doctype", loc);
                    while (i < n && peek() != '>') consume();
                }

                if (i < n && peek() == '>') consume();
                result.tokens.push_back(make_doctype_token(
                    std::move(name), is_valid, tag_start));
                state = State::ReadingText;
                break;
            }

            push_error(result.errors, "Invalid markup declaration", tag_start);
            while (i < n && peek() != '>') consume();
            if (i < n && peek() == '>') consume();
            state = State::ReadingText;
            break;
        }

        case State::PreparingToReadAttributeName: {
            skip_whitespace();
            if (i >= n) break;

            if (peek() == '>') {
                consume();
                Token t;
                t.type = TokenType::StartTag;
                t.location = tag_start;
                t.data = std::move(current_tag);
                result.tokens.push_back(std::move(t));
                state = State::ReadingText;
            } else if (peek() == '/') {
                consume();
                state = State::ProcessingSelfClosingTag;
            } else if (is_attr_name_char(static_cast<unsigned char>(peek()))) {
                attr_start = loc;
                current_attr = Attribute{};
                current_attr_is_duplicate = false;
                state = State::ReadingAttributeName;
            } else {
                push_error(result.errors, "Malformed attribute name", loc);
                consume();
            }
            break;
        }

        case State::ReadingAttributeName: {
            const std::size_t name_begin = i;
            while (i < n &&
                   is_attr_name_char(static_cast<unsigned char>(peek()))) {
                consume();
            }
            current_attr.name = std::string(
                std::string_view(input).substr(name_begin, i - name_begin));

            current_attr_is_duplicate = false;
            for (const auto& a : current_tag.attributes) {
                if (a.name == current_attr.name) {
                    current_attr_is_duplicate = true;
                    break;
                }
            }

            if (current_attr_is_duplicate) {
                push_error(result.errors,
                           "Duplicate attribute name: " + current_attr.name,
                           attr_start);
            }

            state = State::AfterReadingAttributeName;
            break;
        }

        case State::AfterReadingAttributeName: {
            skip_whitespace();

            if (i >= n) {
                if (!current_attr_is_duplicate) {
                    current_tag.attributes.push_back(std::move(current_attr));
                }
                break;
            }

            if (peek() == '=') {
                consume();
                state = State::PreparingToReadAttributeValue;
            } else {
                if (!current_attr_is_duplicate) {
                    current_tag.attributes.push_back(std::move(current_attr));
                }
                state = State::PreparingToReadAttributeName;
            }
            break;
        }

        case State::PreparingToReadAttributeValue: {
            skip_whitespace();
            if (i >= n) break;

            if (peek() == '"' || peek() == '\'') {
                quote_char = peek();
                quote_start = loc;
                consume();
                value_buffer.clear();
                state = State::ReadingQuotedAttributeValue;
            } else {
                value_buffer.clear();
                state = State::ReadingUnquotedAttributeValue;
            }
            break;
        }

        case State::ReadingQuotedAttributeValue: {
            while (i < n && peek() != quote_char) {
                value_buffer.push_back(consume());
            }

            if (i >= n) {
                push_error(result.errors, "Unterminated attribute quote",
                           quote_start);
                break;
            }

            consume();
            if (!current_attr_is_duplicate) {
                current_attr.value = std::move(value_buffer);
                current_attr.has_value = true;
                current_tag.attributes.push_back(std::move(current_attr));
            }
            value_buffer.clear();
            state = State::PreparingToReadAttributeName;
            break;
        }

        case State::ReadingUnquotedAttributeValue: {
            while (i < n) {
                const unsigned char c = static_cast<unsigned char>(peek());
                if (is_whitespace(c) || peek() == '>') break;
                if (peek() == '/') {
                    std::size_t j = i + 1;
                    while (j < n &&
                           is_whitespace(static_cast<unsigned char>(input[j])))
                        ++j;
                    if (j < n && input[j] == '>') break;
                }
                value_buffer.push_back(consume());
            }

            if (!current_attr_is_duplicate) {
                current_attr.value = std::move(value_buffer);
                current_attr.has_value = true;
                current_tag.attributes.push_back(std::move(current_attr));
            }
            value_buffer.clear();
            state = State::PreparingToReadAttributeName;
            break;
        }

        case State::ProcessingSelfClosingTag: {
            skip_whitespace();
            if (i < n && peek() == '>') {
                current_tag.self_closing = true;
                consume();
                Token t;
                t.type = TokenType::StartTag;
                t.location = tag_start;
                t.data = std::move(current_tag);
                result.tokens.push_back(std::move(t));
                state = State::ReadingText;
            } else {
                push_error(result.errors, "Unexpected '/' in tag", loc);
                state = State::PreparingToReadAttributeName;
            }
            break;
        }

        } // switch
    } // while

    // Handle EOF based on which state was active when input ended.
    switch (state) {
    case State::ReadingText:
        if (!text_buffer.empty()) {
            result.tokens.push_back(
                make_text_token(std::move(text_buffer), text_start));
        }
        break;

    case State::ReadingTagOpen:
        result.tokens.push_back(make_text_token("<", tag_start));
        break;

    case State::ReadingTagName:
    case State::PreparingToReadAttributeName:
    case State::ReadingAttributeName:
    case State::AfterReadingAttributeName:
    case State::PreparingToReadAttributeValue:
    case State::ReadingQuotedAttributeValue:
    case State::ReadingUnquotedAttributeValue:
    case State::ProcessingSelfClosingTag:
        push_error(result.errors, "Unterminated tag", tag_start);
        break;

    case State::ReadingEndTagOpen:
    case State::ReadingMarkupDeclaration:
        break;
    }

    result.tokens.push_back(make_eof_token(loc));
    return result;
}
