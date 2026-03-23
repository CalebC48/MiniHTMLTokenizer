#ifndef MINI_HTML_TOKENIZER_TOKENIZER_H
#define MINI_HTML_TOKENIZER_TOKENIZER_H

#include <string>
#include <vector>

#include "error.h"
#include "token.h"

struct TokenizerResult {
    std::vector<Token> tokens;
    std::vector<TokenizerError> errors;
};

class Tokenizer {
public:
    TokenizerResult tokenize(const std::string& input) const;
};

#endif
