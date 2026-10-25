// Tests for the strict flat-object JSON parser used by the HTTP API.
#include "json.hpp"

#include "test_util.hpp"

#include <string>

using exchange::json::escape;
using exchange::json::parse_object;

int main() {
    const auto basic = parse_object(R"({"symbol":"AAPL","quantity":100,"ok":true})");
    CHECK(basic.has_value());
    CHECK((*basic).at("symbol") == "AAPL");
    CHECK((*basic).at("quantity") == "100");
    CHECK((*basic).at("ok") == "true");

    const auto escaped = parse_object(R"({"text":"a\"b\\c\n\t"})");
    CHECK(escaped.has_value());
    CHECK((*escaped).at("text") == "a\"b\\c\n\t");

    const auto spaced = parse_object("  { \"a\" : 1 , \"b\" : \"x\" }  ");
    CHECK(spaced.has_value());
    CHECK((*spaced).at("a") == "1");
    CHECK((*spaced).at("b") == "x");

    const auto empty = parse_object("{}");
    CHECK(empty.has_value());
    CHECK(empty->empty());

    // Malformed input must be rejected instead of silently returning a partial
    // object (which previously allowed truncated requests to place orders).
    CHECK(!parse_object("").has_value());
    CHECK(!parse_object("null").has_value());
    CHECK(!parse_object("{").has_value());
    CHECK(!parse_object("}").has_value());
    CHECK(!parse_object(R"({"a":1,})").has_value());
    CHECK(!parse_object(R"({"a":1} trailing)").has_value());
    CHECK(!parse_object(R"({"a":1,"a":2})").has_value());  // duplicate key
    CHECK(!parse_object(R"({"a":{"b":1}})").has_value());  // nested object
    CHECK(!parse_object(R"({"a":[1]})").has_value());      // nested array
    CHECK(!parse_object(R"({"a":"\u0041"})").has_value()); // unicode escape unsupported
    CHECK(!parse_object(R"({"a" 1})").has_value());
    CHECK(!parse_object(R"({"a":})").has_value());
    CHECK(!parse_object(R"({a:1})").has_value());

    CHECK(escape("plain") == "plain");
    CHECK(escape("a\"b") == "a\\\"b");
    CHECK(escape("a\\b") == "a\\\\b");
    CHECK(escape("line\nnext") == "line\\nnext");
    CHECK(escape(std::string("\x01", 1)) == "\\u0001");

    return test::summary();
}
