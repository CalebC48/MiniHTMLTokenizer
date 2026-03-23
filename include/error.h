#ifndef MINI_HTML_TOKENIZER_ERROR_H
#define MINI_HTML_TOKENIZER_ERROR_H

#include <string>

#include "token.h"

enum class ErrorType {
    InputError,
    TokenizationError
};

struct TokenizerError {
    ErrorType type = ErrorType::TokenizationError;
    std::string message;
    SourceLocation location {};
};

#endif
