#pragma once

#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>

namespace exchange::json {

// Object representation used by the HTTP API: flat key -> raw scalar token.
// String values are stored decoded, numbers/booleans/null as their source text.
using Object = std::map<std::string, std::string, std::less<>>;

// Escapes a string for inclusion inside JSON double quotes (without the quotes).
std::string escape(std::string_view value);

// Parses a flat JSON object of scalar values. Returns nullopt for malformed
// input, duplicate keys, nested objects/arrays or trailing garbage. Unicode
// escapes (\uXXXX) are rejected because this API only needs ASCII identifiers
// and decimal numbers.
std::optional<Object> parse_object(std::string_view body);

} // namespace exchange::json
