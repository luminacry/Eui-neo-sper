#pragma once

#include "core/dsl.h"

#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace visual_editor {

using Color = core::Color;

struct UiNode {
    std::string type = "stack";
    std::string id;
    std::map<std::string, std::string> strings;
    std::map<std::string, double> numbers;
    std::map<std::string, bool> booleans;
    std::map<std::string, Color> colors;
    std::vector<UiNode> children;
};

struct UiDocument {
    UiNode root;
    std::string sourcePath;
    std::string error;
    bool valid = false;
};

inline std::string nodeId(const UiNode& node, const std::string& fallback) {
    return node.id.empty() ? fallback : node.id;
}

inline double numberOr(const UiNode& node, const std::string& key, double fallback) {
    const auto it = node.numbers.find(key);
    return it == node.numbers.end() ? fallback : it->second;
}

inline float floatOr(const UiNode& node, const std::string& key, float fallback) {
    return static_cast<float>(numberOr(node, key, fallback));
}

inline bool boolOr(const UiNode& node, const std::string& key, bool fallback) {
    const auto it = node.booleans.find(key);
    return it == node.booleans.end() ? fallback : it->second;
}

inline std::string stringOr(const UiNode& node, const std::string& key, std::string fallback = {}) {
    const auto it = node.strings.find(key);
    return it == node.strings.end() ? std::move(fallback) : it->second;
}

inline Color colorOr(const UiNode& node, const std::string& key, const Color& fallback) {
    const auto it = node.colors.find(key);
    return it == node.colors.end() ? fallback : it->second;
}

inline std::string readTextFile(const std::string& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("failed to open " + path);
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

} // namespace visual_editor
