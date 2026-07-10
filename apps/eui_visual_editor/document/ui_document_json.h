#pragma once

#include "ui_document.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace visual_editor {

class JsonValue {
public:
    enum class Type { Null, Bool, Number, String, Array, Object };

    Type type = Type::Null;
    bool boolean = false;
    double number = 0.0;
    std::string string;
    std::vector<JsonValue> array;
    std::map<std::string, JsonValue> object;

    bool is(Type expected) const { return type == expected; }
};

class JsonParser {
public:
    explicit JsonParser(std::string source) : source_(std::move(source)) {}

    JsonValue parse() {
        JsonValue value = parseValue();
        skipWhitespace();
        if (!eof()) {
            fail("unexpected trailing content");
        }
        return value;
    }

private:
    JsonValue parseValue() {
        skipWhitespace();
        if (eof()) {
            fail("unexpected end of file");
        }
        const char ch = peek();
        if (ch == '{') {
            return parseObject();
        }
        if (ch == '[') {
            return parseArray();
        }
        if (ch == '"') {
            JsonValue value;
            value.type = JsonValue::Type::String;
            value.string = parseString();
            return value;
        }
        if (ch == '-' || std::isdigit(static_cast<unsigned char>(ch))) {
            return parseNumber();
        }
        if (matchLiteral("true")) {
            JsonValue value;
            value.type = JsonValue::Type::Bool;
            value.boolean = true;
            return value;
        }
        if (matchLiteral("false")) {
            JsonValue value;
            value.type = JsonValue::Type::Bool;
            value.boolean = false;
            return value;
        }
        if (matchLiteral("null")) {
            return JsonValue{};
        }
        fail("expected JSON value");
        return {};
    }

    JsonValue parseObject() {
        consume('{');
        JsonValue value;
        value.type = JsonValue::Type::Object;
        skipWhitespace();
        if (consumeIf('}')) {
            return value;
        }
        while (true) {
            skipWhitespace();
            if (peek() != '"') {
                fail("expected object key");
            }
            std::string key = parseString();
            skipWhitespace();
            consume(':');
            value.object.emplace(std::move(key), parseValue());
            skipWhitespace();
            if (consumeIf('}')) {
                break;
            }
            consume(',');
        }
        return value;
    }

    JsonValue parseArray() {
        consume('[');
        JsonValue value;
        value.type = JsonValue::Type::Array;
        skipWhitespace();
        if (consumeIf(']')) {
            return value;
        }
        while (true) {
            value.array.push_back(parseValue());
            skipWhitespace();
            if (consumeIf(']')) {
                break;
            }
            consume(',');
        }
        return value;
    }

    JsonValue parseNumber() {
        const std::size_t start = pos_;
        consumeIf('-');
        if (consumeIf('0')) {
            // single leading zero
        } else {
            if (eof() || !std::isdigit(static_cast<unsigned char>(peek()))) {
                fail("expected digit");
            }
            while (!eof() && std::isdigit(static_cast<unsigned char>(peek()))) {
                ++pos_;
            }
        }
        if (consumeIf('.')) {
            if (eof() || !std::isdigit(static_cast<unsigned char>(peek()))) {
                fail("expected fractional digit");
            }
            while (!eof() && std::isdigit(static_cast<unsigned char>(peek()))) {
                ++pos_;
            }
        }
        if (!eof() && (peek() == 'e' || peek() == 'E')) {
            ++pos_;
            if (!eof() && (peek() == '+' || peek() == '-')) {
                ++pos_;
            }
            if (eof() || !std::isdigit(static_cast<unsigned char>(peek()))) {
                fail("expected exponent digit");
            }
            while (!eof() && std::isdigit(static_cast<unsigned char>(peek()))) {
                ++pos_;
            }
        }
        JsonValue value;
        value.type = JsonValue::Type::Number;
        value.number = std::stod(source_.substr(start, pos_ - start));
        return value;
    }

    std::string parseString() {
        consume('"');
        std::string result;
        while (!eof()) {
            const char ch = source_[pos_++];
            if (ch == '"') {
                return result;
            }
            if (ch != '\\') {
                result.push_back(ch);
                continue;
            }
            if (eof()) {
                fail("unterminated escape sequence");
            }
            const char escaped = source_[pos_++];
            switch (escaped) {
            case '"': result.push_back('"'); break;
            case '\\': result.push_back('\\'); break;
            case '/': result.push_back('/'); break;
            case 'b': result.push_back('\b'); break;
            case 'f': result.push_back('\f'); break;
            case 'n': result.push_back('\n'); break;
            case 'r': result.push_back('\r'); break;
            case 't': result.push_back('\t'); break;
            default:
                fail("unsupported escape sequence");
            }
        }
        fail("unterminated string");
        return {};
    }

    bool matchLiteral(const char* literal) {
        const std::size_t length = std::char_traits<char>::length(literal);
        if (source_.compare(pos_, length, literal) != 0) {
            return false;
        }
        pos_ += length;
        return true;
    }

    void skipWhitespace() {
        while (!eof() && std::isspace(static_cast<unsigned char>(peek()))) {
            ++pos_;
        }
    }

    bool consumeIf(char expected) {
        skipWhitespace();
        if (!eof() && peek() == expected) {
            ++pos_;
            return true;
        }
        return false;
    }

    void consume(char expected) {
        skipWhitespace();
        if (eof() || peek() != expected) {
            std::string message = "expected '";
            message.push_back(expected);
            message.push_back('\'');
            fail(message);
        }
        ++pos_;
    }

    char peek() const { return source_[pos_]; }
    bool eof() const { return pos_ >= source_.size(); }

    [[noreturn]] void fail(const std::string& message) const {
        throw std::runtime_error(message + " at byte " + std::to_string(pos_));
    }

    std::string source_;
    std::size_t pos_ = 0;
};

inline const JsonValue* findMember(const JsonValue& value, const std::string& key) {
    if (!value.is(JsonValue::Type::Object)) {
        return nullptr;
    }
    const auto it = value.object.find(key);
    return it == value.object.end() ? nullptr : &it->second;
}

inline float jsonNumberOr(const JsonValue& object, const std::string& key, float fallback) {
    const JsonValue* value = findMember(object, key);
    return value && value->is(JsonValue::Type::Number) ? static_cast<float>(value->number) : fallback;
}

inline std::string jsonStringOr(const JsonValue& object, const std::string& key, std::string fallback = {}) {
    const JsonValue* value = findMember(object, key);
    return value && value->is(JsonValue::Type::String) ? value->string : std::move(fallback);
}

inline bool jsonBoolOr(const JsonValue& object, const std::string& key, bool fallback) {
    const JsonValue* value = findMember(object, key);
    return value && value->is(JsonValue::Type::Bool) ? value->boolean : fallback;
}

inline Color parseColorArray(const JsonValue& value, const Color& fallback) {
    if (!value.is(JsonValue::Type::Array) || value.array.size() < 3) {
        return fallback;
    }
    float channels[4] = {fallback.r, fallback.g, fallback.b, fallback.a};
    const std::size_t count = std::min<std::size_t>(4, value.array.size());
    for (std::size_t i = 0; i < count; ++i) {
        if (!value.array[i].is(JsonValue::Type::Number)) {
            return fallback;
        }
        channels[i] = static_cast<float>(value.array[i].number);
    }
    return {channels[0], channels[1], channels[2], channels[3]};
}

inline void readPair(const JsonValue& object, const std::string& key, UiNode& node, const char* first, const char* second) {
    const JsonValue* pair = findMember(object, key);
    if (!pair || !pair->is(JsonValue::Type::Array) || pair->array.size() < 2) {
        return;
    }
    if (pair->array[0].is(JsonValue::Type::Number) && pair->array[1].is(JsonValue::Type::Number)) {
        node.numbers[first] = pair->array[0].number;
        node.numbers[second] = pair->array[1].number;
    }
}

inline UiNode parseNode(const JsonValue& value, const std::string& fallbackId) {
    if (!value.is(JsonValue::Type::Object)) {
        throw std::runtime_error("node must be an object");
    }

    UiNode node;
    node.type = jsonStringOr(value, "type", "stack");
    node.id = jsonStringOr(value, "id", fallbackId);
    readPair(value, "position", node, "x", "y");
    readPair(value, "size", node, "width", "height");

    const char* stringKeys[] = {"text", "source", "title", "subtitle", "description"};
    for (const char* key : stringKeys) {
        if (const JsonValue* member = findMember(value, key); member && member->is(JsonValue::Type::String)) {
            node.strings[key] = member->string;
        }
    }

    const char* numberKeys[] = {
        "x", "y", "width", "height", "radius", "fontSize", "lineHeight", "fontWeight",
        "padding", "gap", "opacity", "duration", "interval", "currentIndex", "cardSpacing"
    };
    for (const char* key : numberKeys) {
        if (const JsonValue* member = findMember(value, key); member && member->is(JsonValue::Type::Number)) {
            node.numbers[key] = member->number;
        }
    }

    const char* boolKeys[] = {"wrap", "autoPlay", "background", "tilt", "primary", "visible"};
    for (const char* key : boolKeys) {
        if (const JsonValue* member = findMember(value, key); member && member->is(JsonValue::Type::Bool)) {
            node.booleans[key] = member->boolean;
        }
    }

    const char* colorKeys[] = {"color", "backgroundColor", "borderColor", "textColor"};
    for (const char* key : colorKeys) {
        if (const JsonValue* member = findMember(value, key)) {
            node.colors[key == std::string("backgroundColor") ? "background" : key] =
                parseColorArray(*member, {0.0f, 0.0f, 0.0f, 1.0f});
        }
    }

    if (const JsonValue* children = findMember(value, "children"); children && children->is(JsonValue::Type::Array)) {
        for (std::size_t i = 0; i < children->array.size(); ++i) {
            node.children.push_back(parseNode(children->array[i], node.id + "." + std::to_string(i)));
        }
    }

    return node;
}

inline UiDocument loadUiDocument(const std::string& path) {
    UiDocument document;
    document.sourcePath = path;
    try {
        JsonParser parser(readTextFile(path));
        document.root = parseNode(parser.parse(), "root");
        document.valid = true;
    } catch (const std::exception& error) {
        document.error = error.what();
        document.valid = false;
    }
    return document;
}

} // namespace visual_editor
