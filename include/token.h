#ifndef MINI_HTML_TOKENIZER_TOKEN_H
#define MINI_HTML_TOKENIZER_TOKEN_H

#include <cstddef>
#include <string>
#include <variant>
#include <vector>

struct SourceLocation {
    std::size_t line = 1;
    std::size_t column = 1;
    std::size_t byte_offset = 0;
};

enum class TokenType {
    Doctype,
    StartTag,
    EndTag,
    Text,
    Comment,
    EOFToken
};

struct Attribute {
    std::string name;
    std::string value;
    bool has_value = false;
};

struct DoctypeData {
    std::string name;
    bool is_valid = true;
};

struct TagData {
    std::string name;
    std::vector<Attribute> attributes;
    bool self_closing = false;
    bool is_start_tag = true;
};

struct TextData {
    std::string data;
};

struct CommentData {
    std::string data;
};

struct EofData {};

using TokenData = std::variant<DoctypeData, TagData, TextData, CommentData, EofData>;

struct Token {
    TokenType type = TokenType::EOFToken;
    SourceLocation location {};
    TokenData data = EofData {};
};

#endif
