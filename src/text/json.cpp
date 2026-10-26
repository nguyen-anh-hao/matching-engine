#include "exchange/text/json.hpp"

#include <cctype>
#include <cstddef>
#include <utility>

namespace exchange::json {
namespace {

class Parser {
public:
    explicit Parser(std::string_view input) : input_(input) {}

    void skip_whitespace() {
        while (position_ < input_.size() &&
               std::isspace(static_cast<unsigned char>(input_[position_])) != 0) {
            ++position_;
        }
    }

    bool consume(char expected) {
        if (position_ < input_.size() && input_[position_] == expected) {
            ++position_;
            return true;
        }
        return false;
    }

    bool at_end() const { return position_ >= input_.size(); }

    char peek() const { return position_ < input_.size() ? input_[position_] : '\0'; }

    bool parse_string(std::string& out) {
        if (!consume('"'))
            return false;
        out.clear();
        while (position_ < input_.size()) {
            const char c = input_[position_++];
            if (c == '"')
                return true;
            if (c == '\\') {
                if (position_ >= input_.size())
                    return false;
                switch (input_[position_++]) {
                case '"':
                    out.push_back('"');
                    break;
                case '\\':
                    out.push_back('\\');
                    break;
                case '/':
                    out.push_back('/');
                    break;
                case 'b':
                    out.push_back('\b');
                    break;
                case 'f':
                    out.push_back('\f');
                    break;
                case 'n':
                    out.push_back('\n');
                    break;
                case 'r':
                    out.push_back('\r');
                    break;
                case 't':
                    out.push_back('\t');
                    break;
                default:
                    return false;
                }
                continue;
            }
            if (static_cast<unsigned char>(c) < 0x20)
                return false;
            out.push_back(c);
        }
        return false;
    }

    bool parse_scalar(std::string& out) {
        skip_whitespace();
        if (peek() == '"')
            return parse_string(out);

        const size_t start = position_;
        while (position_ < input_.size() && input_[position_] != ',' && input_[position_] != '}') {
            ++position_;
        }
        size_t end = position_;
        while (end > start && std::isspace(static_cast<unsigned char>(input_[end - 1])) != 0) {
            --end;
        }
        out.assign(input_.substr(start, end - start));
        if (out.empty())
            return false;
        return out.find_first_of("{}[]\"") == std::string::npos;
    }

private:
    std::string_view input_;
    size_t position_ = 0;
};

} // namespace

std::string escape(std::string_view value) {
    std::string out;
    out.reserve(value.size());
    for (const char c : value) {
        switch (c) {
        case '"':
            out += "\\\"";
            break;
        case '\\':
            out += "\\\\";
            break;
        case '\b':
            out += "\\b";
            break;
        case '\f':
            out += "\\f";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            out += "\\r";
            break;
        case '\t':
            out += "\\t";
            break;
        default: {
            const auto byte = static_cast<unsigned char>(c);
            if (byte < 0x20) {
                static constexpr char kHex[] = "0123456789abcdef";
                out += "\\u00";
                out.push_back(kHex[byte >> 4]);
                out.push_back(kHex[byte & 0x0F]);
            } else {
                out.push_back(c);
            }
            break;
        }
        }
    }
    return out;
}

std::optional<Object> parse_object(std::string_view body) {
    Parser parser(body);

    parser.skip_whitespace();
    if (!parser.consume('{'))
        return std::nullopt;

    Object object;
    parser.skip_whitespace();
    if (parser.consume('}')) {
        parser.skip_whitespace();
        return parser.at_end() ? std::optional<Object>(std::move(object)) : std::nullopt;
    }

    for (;;) {
        parser.skip_whitespace();
        std::string key;
        if (!parser.parse_string(key))
            return std::nullopt;

        parser.skip_whitespace();
        if (!parser.consume(':'))
            return std::nullopt;

        std::string value;
        if (!parser.parse_scalar(value))
            return std::nullopt;
        if (!object.emplace(std::move(key), std::move(value)).second) {
            return std::nullopt;
        }

        parser.skip_whitespace();
        if (parser.consume(','))
            continue;
        if (parser.consume('}'))
            break;
        return std::nullopt;
    }

    parser.skip_whitespace();
    if (!parser.at_end())
        return std::nullopt;
    return object;
}

} // namespace exchange::json
