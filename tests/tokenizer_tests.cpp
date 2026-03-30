#include <iostream>
#include <string>
#include <vector>

#include "tokenizer.h"

namespace {

bool has_error_message(const TokenizerResult& r, const std::string& msg_substr) {
    for (const auto& e : r.errors) {
        if (e.message.find(msg_substr) != std::string::npos) return true;
    }
    return false;
}

bool require(bool cond, const std::string& label, int& failures) {
    if (cond) return true;
    std::cerr << "FAIL: " << label << "\n";
    ++failures;
    return false;
}

}

int main() {
    int failures = 0;
    Tokenizer tokenizer;

    {
        auto r = tokenizer.tokenize("hello");
        require(r.errors.empty(), "plain text has no errors", failures);
        require(r.tokens.size() == 2, "plain text token count", failures);
        require(r.tokens[0].type == TokenType::Text, "plain text first token type", failures);
    }

    {
        auto r = tokenizer.tokenize("<div>x</div>");
        require(r.tokens.size() == 4, "simple tags token count", failures);
        require(r.tokens[0].type == TokenType::StartTag, "simple tags start tag", failures);
        require(r.tokens[2].type == TokenType::EndTag, "simple tags end tag", failures);
    }

    {
        auto r = tokenizer.tokenize("<a href=\"x\">");
        const auto& td = std::get<TagData>(r.tokens[0].data);
        require(td.attributes.size() == 1, "quoted attribute count", failures);
        require(td.attributes[0].name == "href", "quoted attribute name", failures);
        require(td.attributes[0].value == "x", "quoted attribute value", failures);
    }

    {
        auto r = tokenizer.tokenize("<img src=test />");
        const auto& td = std::get<TagData>(r.tokens[0].data);
        require(td.self_closing, "unquoted self-closing true", failures);
        require(td.attributes.size() == 1 && td.attributes[0].value == "test", "unquoted value parsed", failures);
    }

    {
        auto r = tokenizer.tokenize("<input disabled>");
        const auto& td = std::get<TagData>(r.tokens[0].data);
        require(td.attributes.size() == 1, "boolean attr count", failures);
        require(!td.attributes[0].has_value, "boolean attr has no value", failures);
    }

    {
        auto r = tokenizer.tokenize("<!-- c -->");
        require(r.tokens[0].type == TokenType::Comment, "comment token type", failures);
    }

    {
        auto r = tokenizer.tokenize("<!-- c");
        require(has_error_message(r, "Unterminated comment"), "unterminated comment error", failures);
    }

    {
        auto r = tokenizer.tokenize("<!doctype html>");
        require(r.tokens[0].type == TokenType::Doctype, "doctype token type", failures);
        const auto& d = std::get<DoctypeData>(r.tokens[0].data);
        require(d.is_valid, "doctype valid", failures);
        require(d.name == "html", "doctype name", failures);
    }

    {
        auto r = tokenizer.tokenize("<!DOCTYPE>");
        require(has_error_message(r, "Malformed doctype"), "malformed doctype error", failures);
        const auto& d = std::get<DoctypeData>(r.tokens[0].data);
        require(!d.is_valid, "malformed doctype invalid token", failures);
    }

    {
        auto r = tokenizer.tokenize("a < b");
        require(has_error_message(r, "Invalid tag start after '<'"), "random < in text error", failures);
    }

    {
        auto r = tokenizer.tokenize("<div");
        require(has_error_message(r, "Unterminated tag"), "unterminated tag error", failures);
    }

    {
        auto r = tokenizer.tokenize("<a href=\"x>");
        require(has_error_message(r, "Unterminated attribute quote"), "unterminated attr quote error", failures);
    }

    {
        auto r = tokenizer.tokenize("</div class=x>");
        require(has_error_message(r, "Attributes are not allowed in end tags"), "malformed end tag error", failures);
    }

    {
        auto r = tokenizer.tokenize("<a href=x href=y>");
        require(has_error_message(r, "Duplicate attribute name"), "duplicate attribute error", failures);
        const auto& td = std::get<TagData>(r.tokens[0].data);
        require(td.attributes.size() == 1, "duplicate keep-first policy", failures);
    }

    {
        auto r = tokenizer.tokenize("");
        require(r.tokens.size() == 1, "empty input eof-only", failures);
        require(r.tokens[0].type == TokenType::EOFToken, "empty input eof token", failures);
        require(r.errors.empty(), "empty input no errors", failures);
    }

    if (failures > 0) {
        std::cerr << "Tokenizer tests failed: " << failures << "\n";
        return 1;
    }

    std::cout << "Tokenizer tests passed\n";
    return 0;
}
