#pragma once

#include "ui_document.h"

#include <algorithm>
#include <cctype>
#include <map>
#include <stdexcept>
#include <string>

namespace visual_editor {

class XmlParser {
public:
    explicit XmlParser(std::string source) : source_(std::move(source)) {}

    UiNode parse() {
        skipMisc();
        UiNode root = parseElement("root");
        skipMisc();
        if (!eof()) {
            fail("unexpected trailing XML content");
        }
        return root;
    }

private:
    UiNode parseElement(const std::string& fallbackId) {
        consume('<');
        if (consumeIf('/')) {
            fail("unexpected closing tag");
        }
        const std::string tag = parseName();
        UiNode node;
        node.type = normalizeType(tag);
        node.id = fallbackId;

        while (true) {
            skipWhitespace();
            if (consumeIf('/')) {
                consume('>');
                applyDefaults(node, tag);
                return node;
            }
            if (consumeIf('>')) {
                break;
            }
            const std::string key = parseName();
            skipWhitespace();
            consume('=');
            skipWhitespace();
            const std::string value = parseQuotedValue();
            applyAttribute(node, key, value);
        }

        std::size_t childIndex = 0;
        while (true) {
            skipWhitespace();
            if (startsWith("</")) {
                pos_ += 2;
                const std::string closing = parseName();
                if (closing != tag) {
                    fail("mismatched closing tag: " + closing);
                }
                skipWhitespace();
                consume('>');
                applyDefaults(node, tag);
                return node;
            }
            if (startsWith("<!--")) {
                skipComment();
                continue;
            }
            if (eof()) {
                fail("unterminated tag: " + tag);
            }
            if (peek() == '<') {
                node.children.push_back(parseElement(node.id + "." + std::to_string(childIndex++)));
                continue;
            }
            parseTextContent(node);
        }
    }

    static std::string normalizeType(const std::string& tag) {
        if (tag == "Stack") return "stack";
        if (tag == "Row") return "row";
        if (tag == "Column") return "column";
        if (tag == "Rect") return "rect";
        if (tag == "Text") return "text";
        if (tag == "Image") return "image";
        if (tag == "Button") return "button";
        if (tag == "Card") return "card";
        if (tag == "Panel") return "panel";
        if (tag == "CardSlider") return "cardSlider";
        if (tag == "Item") return "item";
        return tag;
    }

    static void applyDefaults(UiNode& node, const std::string& tag) {
        if (node.id.empty()) {
            node.id = tag;
        }
        if (node.type == "item") {
            node.type = "cardSliderItem";
        }
    }

    static void applyAttribute(UiNode& node, const std::string& key, const std::string& value) {
        if (key == "id") {
            node.id = value;
            return;
        }
        if (key == "text" || key == "source" || key == "title" || key == "subtitle" || key == "description") {
            node.strings[key] = value;
            return;
        }
        if (key == "color" || key == "backgroundColor" || key == "borderColor" || key == "textColor") {
            node.colors[key == "backgroundColor" ? "background" : key] = parseColor(value);
            return;
        }
        if (key == "wrap" || key == "autoPlay" || key == "background" ||
            key == "backgroundEnabled" || key == "tilt" || key == "primary" || key == "visible") {
            node.booleans[key == "backgroundEnabled" ? "background" : key] = parseBool(value);
            return;
        }
        if (isNumberKey(key)) {
            node.numbers[key] = parseNumber(value);
            return;
        }
        node.strings[key] = value;
    }

    static bool isNumberKey(const std::string& key) {
        static const char* keys[] = {
            "x", "y", "width", "height", "radius", "fontSize", "lineHeight", "fontWeight",
            "padding", "gap", "opacity", "duration", "interval", "currentIndex", "cardSpacing"
        };
        return std::find(std::begin(keys), std::end(keys), key) != std::end(keys);
    }

    static double parseNumber(const std::string& value) {
        std::size_t consumed = 0;
        const double number = std::stod(value, &consumed);
        if (consumed != value.size()) {
            throw std::runtime_error("invalid number: " + value);
        }
        return number;
    }

    static bool parseBool(const std::string& value) {
        return value == "true" || value == "1" || value == "yes";
    }

    static int hexDigit(char ch) {
        if (ch >= '0' && ch <= '9') return ch - '0';
        if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
        if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
        throw std::runtime_error("invalid hex color");
    }

    static float hexByte(const std::string& value, std::size_t offset) {
        return static_cast<float>(hexDigit(value[offset]) * 16 + hexDigit(value[offset + 1])) / 255.0f;
    }

    static Color parseColor(const std::string& value) {
        if (!value.empty() && value[0] == '#') {
            if (value.size() != 7 && value.size() != 9) {
                throw std::runtime_error("expected #RRGGBB or #RRGGBBAA color");
            }
            return {
                hexByte(value, 1),
                hexByte(value, 3),
                hexByte(value, 5),
                value.size() == 9 ? hexByte(value, 7) : 1.0f
            };
        }

        float channels[4] = {0.0f, 0.0f, 0.0f, 1.0f};
        std::size_t start = 0;
        for (int i = 0; i < 4 && start < value.size(); ++i) {
            const std::size_t comma = value.find(',', start);
            const std::string part = value.substr(start, comma == std::string::npos ? std::string::npos : comma - start);
            channels[i] = static_cast<float>(std::stod(part));
            if (comma == std::string::npos) {
                break;
            }
            start = comma + 1;
        }
        return {channels[0], channels[1], channels[2], channels[3]};
    }

    void parseTextContent(UiNode& node) {
        const std::size_t start = pos_;
        while (!eof() && peek() != '<') {
            ++pos_;
        }
        std::string text = trim(source_.substr(start, pos_ - start));
        if (!text.empty() && node.strings.find("text") == node.strings.end()) {
            node.strings["text"] = decodeEntities(text);
        }
    }

    static std::string trim(std::string value) {
        auto notSpace = [](unsigned char ch) { return !std::isspace(ch); };
        value.erase(value.begin(), std::find_if(value.begin(), value.end(), notSpace));
        value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
        return value;
    }

    static std::string decodeEntities(std::string value) {
        replaceAll(value, "&quot;", "\"");
        replaceAll(value, "&apos;", "'");
        replaceAll(value, "&lt;", "<");
        replaceAll(value, "&gt;", ">");
        replaceAll(value, "&amp;", "&");
        return value;
    }

    static void replaceAll(std::string& value, const std::string& from, const std::string& to) {
        std::size_t pos = 0;
        while ((pos = value.find(from, pos)) != std::string::npos) {
            value.replace(pos, from.size(), to);
            pos += to.size();
        }
    }

    void skipMisc() {
        while (true) {
            skipWhitespace();
            if (startsWith("<?")) {
                skipUntil("?>");
                continue;
            }
            if (startsWith("<!--")) {
                skipComment();
                continue;
            }
            break;
        }
    }

    void skipComment() {
        consume('<');
        consume('!');
        consume('-');
        consume('-');
        skipUntil("-->");
    }

    void skipUntil(const std::string& token) {
        const std::size_t found = source_.find(token, pos_);
        if (found == std::string::npos) {
            fail("unterminated XML section");
        }
        pos_ = found + token.size();
    }

    std::string parseName() {
        if (eof() || !isNameStart(peek())) {
            fail("expected XML name");
        }
        const std::size_t start = pos_++;
        while (!eof() && isNameChar(peek())) {
            ++pos_;
        }
        return source_.substr(start, pos_ - start);
    }

    std::string parseQuotedValue() {
        if (eof() || (peek() != '"' && peek() != '\'')) {
            fail("expected quoted attribute");
        }
        const char quote = source_[pos_++];
        const std::size_t start = pos_;
        while (!eof() && peek() != quote) {
            ++pos_;
        }
        if (eof()) {
            fail("unterminated attribute");
        }
        std::string value = source_.substr(start, pos_ - start);
        ++pos_;
        return decodeEntities(value);
    }

    static bool isNameStart(char ch) {
        return std::isalpha(static_cast<unsigned char>(ch)) || ch == '_' || ch == ':';
    }

    static bool isNameChar(char ch) {
        return isNameStart(ch) || std::isdigit(static_cast<unsigned char>(ch)) || ch == '-' || ch == '.';
    }

    void skipWhitespace() {
        while (!eof() && std::isspace(static_cast<unsigned char>(peek()))) {
            ++pos_;
        }
    }

    bool consumeIf(char expected) {
        if (!eof() && peek() == expected) {
            ++pos_;
            return true;
        }
        return false;
    }

    void consume(char expected) {
        if (eof() || peek() != expected) {
            std::string message = "expected '";
            message.push_back(expected);
            message.push_back('\'');
            fail(message);
        }
        ++pos_;
    }

    bool startsWith(const std::string& token) const {
        return source_.compare(pos_, token.size(), token) == 0;
    }

    char peek() const { return source_[pos_]; }
    bool eof() const { return pos_ >= source_.size(); }

    [[noreturn]] void fail(const std::string& message) const {
        throw std::runtime_error(message + " at byte " + std::to_string(pos_));
    }

    std::string source_;
    std::size_t pos_ = 0;
};

inline UiDocument loadXmlUiDocument(const std::string& path) {
    UiDocument document;
    document.sourcePath = path;
    try {
        XmlParser parser(readTextFile(path));
        document.root = parser.parse();
        document.valid = true;
    } catch (const std::exception& error) {
        document.error = error.what();
        document.valid = false;
    }
    return document;
}

} // namespace visual_editor
