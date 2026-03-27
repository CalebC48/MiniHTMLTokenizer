#ifndef MINI_HTML_TOKENIZER_OUTPUT_H
#define MINI_HTML_TOKENIZER_OUTPUT_H

#include <iosfwd>
#include <vector>

#include "error.h"
#include "token.h"

void print_tokens_human(std::ostream& out, const std::vector<Token>& tokens);
void print_tokens_json(std::ostream& out, const std::vector<Token>& tokens);
void print_errors(std::ostream& out, const std::vector<TokenizerError>& errors);

bool has_fatal_tokenization_error(const std::vector<TokenizerError>& errors);

#endif
