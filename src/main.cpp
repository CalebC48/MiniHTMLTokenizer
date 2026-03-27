#include <iostream>

#include <string>

#include "tokenizer.h"

static const char* token_type_name(TokenType t) {
    switch (t) {
        case TokenType::Doctype: return "Doctype";
        case TokenType::StartTag: return "StartTag";
        case TokenType::EndTag: return "EndTag";
        case TokenType::Text: return "Text";
        case TokenType::Comment: return "Comment";
        case TokenType::EOFToken: return "EOFToken";
    }
    return "Unknown";
}

int main(int argc, char** argv) {
    std::cout << "Mini HTML Tokenizer\n";

    if (argc != 2) {
        std::cout << "Usage: mini_html_tokenizer \"<html>...\"\n";
        return 0;
    }

    const std::string input = argv[1];
    const Tokenizer tokenizer;
    const TokenizerResult result = tokenizer.tokenize(input);

    for (const Token& tok : result.tokens) {
        std::cout << token_type_name(tok.type)
                  << " @(" << tok.location.line << "," << tok.location.column
                  << ") +" << tok.location.byte_offset;

        if (tok.type == TokenType::Text) {
            const auto& td = std::get<TextData>(tok.data);
            std::cout << " data=\"" << td.data << "\"";
        } else if (tok.type == TokenType::StartTag || tok.type == TokenType::EndTag) {
            const auto& td = std::get<TagData>(tok.data);
            std::cout << " name=\"" << td.name << "\""
                      << " selfClosing=" << (td.self_closing ? "true" : "false");
        }

        std::cout << "\n";
    }

    for (const TokenizerError& e : result.errors) {
        std::cerr << "Error: " << e.message
                  << " @(" << e.location.line << "," << e.location.column
                  << ") +" << e.location.byte_offset << "\n";
    }

    return result.errors.empty() ? 0 : 3;
}
