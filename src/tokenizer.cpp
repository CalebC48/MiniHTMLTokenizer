#include "tokenizer.h"

TokenizerResult Tokenizer::tokenize(const std::string& input) const {
    (void) input;

    TokenizerResult result;
    Token eof_token;
    eof_token.type = TokenType::EOFToken;
    eof_token.location = SourceLocation {};
    eof_token.data = EofData {};
    result.tokens.push_back(eof_token);
    return result;
}
