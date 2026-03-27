#include "output.h"

#include <ostream>
#include <string>
#include <variant>

namespace {

const char* token_type_name(TokenType t) {
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

std::string json_escape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (unsigned char c : s) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    static const char* hex = "0123456789abcdef";
                    out += "\\u00";
                    out.push_back(hex[(c >> 4) & 0xF]);
                    out.push_back(hex[c & 0xF]);
                } else {
                    out.push_back(static_cast<char>(c));
                }
        }
    }
    return out;
}

}

void print_tokens_human(std::ostream& out, const std::vector<Token>& tokens) {
    for (const Token& tok : tokens) {
        out << token_type_name(tok.type)
            << " @(" << tok.location.line << "," << tok.location.column
            << ") +" << tok.location.byte_offset;

        if (tok.type == TokenType::Text) {
            const auto& td = std::get<TextData>(tok.data);
            out << " data=\"" << td.data << "\"";
        } else if (tok.type == TokenType::StartTag || tok.type == TokenType::EndTag) {
            const auto& td = std::get<TagData>(tok.data);
            out << " name=\"" << td.name << "\""
                << " selfClosing=" << (td.self_closing ? "true" : "false");
            if (tok.type == TokenType::StartTag && !td.attributes.empty()) {
                out << " attrs=[";
                bool first = true;
                for (const auto& a : td.attributes) {
                    if (!first) out << ", ";
                    first = false;
                    out << a.name;
                    if (a.has_value) out << "=\"" << a.value << "\"";
                }
                out << "]";
            }
        } else if (tok.type == TokenType::Comment) {
            const auto& td = std::get<CommentData>(tok.data);
            out << " data=\"" << td.data << "\"";
        } else if (tok.type == TokenType::Doctype) {
            const auto& td = std::get<DoctypeData>(tok.data);
            out << " name=\"" << td.name << "\""
                << " valid=" << (td.is_valid ? "true" : "false");
        }

        out << "\n";
    }
}

void print_tokens_json(std::ostream& out, const std::vector<Token>& tokens) {
    out << "[";
    bool first_tok = true;
    for (const Token& tok : tokens) {
        if (!first_tok) out << ",";
        first_tok = false;

        out << "{";
        out << "\"type\":\"" << token_type_name(tok.type) << "\",";
        out << "\"location\":{"
            << "\"line\":" << tok.location.line << ","
            << "\"column\":" << tok.location.column << ","
            << "\"byte_offset\":" << tok.location.byte_offset
            << "}";

        if (tok.type == TokenType::Text) {
            const auto& td = std::get<TextData>(tok.data);
            out << ",\"data\":\"" << json_escape(td.data) << "\"";
        } else if (tok.type == TokenType::Comment) {
            const auto& td = std::get<CommentData>(tok.data);
            out << ",\"data\":\"" << json_escape(td.data) << "\"";
        } else if (tok.type == TokenType::Doctype) {
            const auto& td = std::get<DoctypeData>(tok.data);
            out << ",\"name\":\"" << json_escape(td.name) << "\"";
            out << ",\"is_valid\":" << (td.is_valid ? "true" : "false");
        } else if (tok.type == TokenType::StartTag || tok.type == TokenType::EndTag) {
            const auto& td = std::get<TagData>(tok.data);
            out << ",\"name\":\"" << json_escape(td.name) << "\"";
            out << ",\"self_closing\":" << (td.self_closing ? "true" : "false");
            out << ",\"is_start_tag\":" << (td.is_start_tag ? "true" : "false");
            out << ",\"attributes\":[";
            bool first_attr = true;
            for (const auto& a : td.attributes) {
                if (!first_attr) out << ",";
                first_attr = false;
                out << "{"
                    << "\"name\":\"" << json_escape(a.name) << "\""
                    << ",\"has_value\":" << (a.has_value ? "true" : "false");
                if (a.has_value) out << ",\"value\":\"" << json_escape(a.value) << "\"";
                out << "}";
            }
            out << "]";
        }

        out << "}";
    }
    out << "]";
}

void print_errors(std::ostream& out, const std::vector<TokenizerError>& errors) {
    for (const TokenizerError& e : errors) {
        out << "Error: " << e.message
            << " @(" << e.location.line << "," << e.location.column
            << ") +" << e.location.byte_offset << "\n";
    }
}

bool has_fatal_tokenization_error(const std::vector<TokenizerError>& errors) {
    for (const auto& e : errors) {
        if (e.message == "Unterminated tag" ||
            e.message == "Unterminated comment" ||
            e.message == "Unterminated attribute quote") {
            return true;
        }
    }
    return false;
}
