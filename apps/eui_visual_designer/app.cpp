#include "../eui_visual_editor/document/ui_document.h"
#include "../eui_visual_editor/document/ui_document_renderer.h"
#include "../eui_visual_editor/document/ui_document_xml.h"
#include "components/mousearea.h"
#include "eui_neo.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace app {

const DslAppConfig& dslAppConfig() {
    static const DslAppConfig config = DslAppConfig{}
        .title("EUI Visual Designer")
        .pageId("eui_visual_designer")
        .clearColor({0.045f, 0.050f, 0.060f, 1.0f})
        .windowSize(1380, 860)
        .fps(90.0);
    return config;
}

namespace {

using visual_editor::UiNode;

struct PaletteItem {
    std::string type;
    std::string label;
};

constexpr float kLeftW = 236.0f;
constexpr float kRightW = 292.0f;
constexpr float kHeaderH = 68.0f;

struct HistoryEntry {
    UiNode root;
    std::string selectedId;
};

UiNode root;
std::string selectedId;
std::string pendingType;
std::string paletteDragType;
std::string statusText = "Ready";
int idCounter = 1;
int documentRevision = 0;
bool initialized = false;
bool paletteDragging = false;
float paletteDragX = 0.0f;
float paletteDragY = 0.0f;
float canvasGlobalX = 0.0f;
float canvasGlobalY = 0.0f;
float canvasGlobalWidth = 0.0f;
float canvasGlobalHeight = 0.0f;
std::vector<HistoryEntry> undoStack;
std::vector<HistoryEntry> redoStack;

const std::vector<PaletteItem> palette = {
    {"rect", "Rect"},
    {"text", "Text"},
    {"button", "Button"},
    {"image", "Image"},
    {"panel", "Panel"},
    {"card", "Card"},
    {"row", "Row"},
    {"column", "Column"},
    {"cardSlider", "CardSlider"},
};

components::theme::ThemeColorTokens theme() {
    return components::theme::dark();
}

eui::Transition motion() {
    auto transition = eui::Transition::make(0.14f, eui::Ease::OutCubic);
    transition.animate(eui::AnimProperty::Color | eui::AnimProperty::TextColor |
                       eui::AnimProperty::Border | eui::AnimProperty::Shadow |
                       eui::AnimProperty::Opacity | eui::AnimProperty::Frame);
    return transition;
}

const std::string& savePath() {
    static const std::string path = [] {
        namespace fs = std::filesystem;
        const char* candidates[] = {
            "apps/eui_visual_designer/designed.ui.xml",
            "../apps/eui_visual_designer/designed.ui.xml",
            "../../apps/eui_visual_designer/designed.ui.xml",
        };
        for (const char* candidate : candidates) {
            std::error_code error;
            const fs::path parent = fs::path(candidate).parent_path();
            if (fs::exists(parent, error)) {
                return fs::absolute(candidate, error).string();
            }
        }
        return fs::absolute("designed.ui.xml").string();
    }();
    return path;
}

bool containsId(const UiNode& node, const std::string& id) {
    if (node.id == id) {
        return true;
    }
    for (const UiNode& child : node.children) {
        if (containsId(child, id)) {
            return true;
        }
    }
    return false;
}

void syncIdCounter(const UiNode& node) {
    const std::size_t dot = node.id.find_last_of('.');
    if (dot != std::string::npos && dot + 1 < node.id.size()) {
        try {
            idCounter = std::max(idCounter, std::stoi(node.id.substr(dot + 1)) + 1);
        } catch (...) {
        }
    }
    for (const UiNode& child : node.children) {
        syncIdCounter(child);
    }
}

std::string nextId(const std::string& type) {
    std::string id;
    do {
        id = type + "." + std::to_string(idCounter++);
    } while (containsId(root, id));
    return id;
}

void setNumber(UiNode& node, const std::string& key, double value) {
    node.numbers[key] = value;
}

void markChanged(const std::string& status) {
    ++documentRevision;
    statusText = status;
}

void pushHistory() {
    undoStack.push_back({root, selectedId});
    if (undoStack.size() > 50) {
        undoStack.erase(undoStack.begin());
    }
    redoStack.clear();
}

void undo() {
    if (undoStack.empty()) {
        statusText = "Nothing to undo";
        return;
    }
    redoStack.push_back({root, selectedId});
    HistoryEntry entry = std::move(undoStack.back());
    undoStack.pop_back();
    root = std::move(entry.root);
    selectedId = std::move(entry.selectedId);
    markChanged("Undo");
}

void redo() {
    if (redoStack.empty()) {
        statusText = "Nothing to redo";
        return;
    }
    undoStack.push_back({root, selectedId});
    HistoryEntry entry = std::move(redoStack.back());
    redoStack.pop_back();
    root = std::move(entry.root);
    selectedId = std::move(entry.selectedId);
    markChanged("Redo");
}

UiNode makeNode(const std::string& type, float x, float y) {
    UiNode node;
    node.type = type;
    node.id = nextId(type);
    setNumber(node, "x", x);
    setNumber(node, "y", y);

    if (type == "rect") {
        setNumber(node, "width", 180.0);
        setNumber(node, "height", 110.0);
        setNumber(node, "radius", 14.0);
        node.colors["color"] = {0.18f, 0.24f, 0.34f, 1.0f};
    } else if (type == "text") {
        setNumber(node, "width", 240.0);
        setNumber(node, "height", 42.0);
        setNumber(node, "fontSize", 24.0);
        setNumber(node, "lineHeight", 30.0);
        setNumber(node, "fontWeight", 760.0);
        node.strings["text"] = "Text";
        node.colors["color"] = {0.94f, 0.96f, 1.0f, 1.0f};
    } else if (type == "button") {
        setNumber(node, "width", 150.0);
        setNumber(node, "height", 48.0);
        setNumber(node, "radius", 12.0);
        setNumber(node, "fontSize", 16.0);
        node.strings["text"] = "Button";
    } else if (type == "image") {
        setNumber(node, "width", 180.0);
        setNumber(node, "height", 120.0);
        setNumber(node, "radius", 12.0);
        node.strings["source"] = "assets/icon.png";
    } else if (type == "panel") {
        setNumber(node, "width", 220.0);
        setNumber(node, "height", 160.0);
        setNumber(node, "radius", 16.0);
    } else if (type == "card") {
        setNumber(node, "width", 260.0);
        setNumber(node, "height", 180.0);
        setNumber(node, "padding", 18.0);
    } else if (type == "row" || type == "column") {
        setNumber(node, "width", 320.0);
        setNumber(node, "height", 160.0);
        setNumber(node, "gap", 12.0);
        setNumber(node, "padding", 14.0);
    } else if (type == "cardSlider") {
        setNumber(node, "width", 760.0);
        setNumber(node, "height", 460.0);
        setNumber(node, "duration", 0.56);
        setNumber(node, "interval", 2.0);
        node.booleans["autoPlay"] = false;
        node.booleans["background"] = true;
        node.booleans["tilt"] = true;
        UiNode itemA;
        itemA.type = "cardSliderItem";
        itemA.id = node.id + ".item.1";
        itemA.strings["source"] = "assets/icon.png";
        itemA.strings["title"] = "EUI";
        itemA.strings["subtitle"] = "Designer";
        itemA.strings["description"] = "A generated CardSlider item.";
        UiNode itemB = itemA;
        itemB.id = node.id + ".item.2";
        itemB.strings["title"] = "Preview";
        itemB.strings["subtitle"] = "XML";
        itemB.strings["description"] = "Saved XML can be loaded by the preview app.";
        node.children.push_back(std::move(itemA));
        node.children.push_back(std::move(itemB));
    }
    return node;
}

void initialize() {
    if (initialized) {
        return;
    }
    std::error_code error;
    if (std::filesystem::exists(savePath(), error)) {
        visual_editor::UiDocument saved = visual_editor::loadXmlUiDocument(savePath());
        if (saved.valid) {
            root = std::move(saved.root);
            syncIdCounter(root);
            if (!root.children.empty()) {
                selectedId = root.children.front().id;
            }
            statusText = "Loaded designed.ui.xml";
            initialized = true;
            return;
        }
        statusText = "Saved XML is invalid; using starter document";
    }
    root.type = "stack";
    root.id = "root";
    root.children.push_back(makeNode("rect", 48.0f, 48.0f));
    root.children.back().id = "hero.bg";
    root.children.back().numbers["width"] = 420.0;
    root.children.back().numbers["height"] = 260.0;
    root.children.back().colors["color"] = {0.08f, 0.10f, 0.13f, 1.0f};

    root.children.push_back(makeNode("text", 76.0f, 78.0f));
    root.children.back().id = "hero.title";
    root.children.back().strings["text"] = "Drag UI";
    root.children.back().numbers["fontSize"] = 34.0;
    root.children.back().numbers["width"] = 300.0;

    root.children.push_back(makeNode("button", 78.0f, 164.0f));
    root.children.back().id = "hero.button";
    root.children.back().strings["text"] = "Save XML";
    selectedId = "hero.title";
    initialized = true;
}

UiNode* findNode(UiNode& node, const std::string& id) {
    if (node.id == id) {
        return &node;
    }
    for (UiNode& child : node.children) {
        if (UiNode* found = findNode(child, id)) {
            return found;
        }
    }
    return nullptr;
}

UiNode* selectedNode() {
    return selectedId.empty() ? nullptr : findNode(root, selectedId);
}

void reassignIds(UiNode& node) {
    node.id = nextId(node.type == "cardSliderItem" ? "item" : node.type);
    for (UiNode& child : node.children) {
        reassignIds(child);
    }
}

bool eraseNode(UiNode& parent, const std::string& id) {
    const auto it = std::find_if(parent.children.begin(), parent.children.end(), [&](const UiNode& child) {
        return child.id == id;
    });
    if (it != parent.children.end()) {
        parent.children.erase(it);
        return true;
    }
    for (UiNode& child : parent.children) {
        if (eraseNode(child, id)) {
            return true;
        }
    }
    return false;
}

void deleteSelected() {
    if (selectedId.empty() || selectedId == root.id) {
        statusText = "Select a node to delete";
        return;
    }
    pushHistory();
    if (eraseNode(root, selectedId)) {
        selectedId.clear();
        markChanged("Node deleted");
    } else {
        undoStack.pop_back();
        statusText = "Node not found";
    }
}

void duplicateSelected() {
    UiNode* source = selectedNode();
    if (!source || source == &root) {
        statusText = "Select a node to duplicate";
        return;
    }
    pushHistory();
    UiNode copy = *source;
    reassignIds(copy);
    copy.numbers["x"] = visual_editor::numberOr(copy, "x", 0.0) + 24.0;
    copy.numbers["y"] = visual_editor::numberOr(copy, "y", 0.0) + 24.0;
    selectedId = copy.id;
    root.children.push_back(std::move(copy));
    markChanged("Node duplicated");
}

std::string xmlEscape(std::string value) {
    auto replaceAll = [](std::string& text, const std::string& from, const std::string& to) {
        std::size_t pos = 0;
        while ((pos = text.find(from, pos)) != std::string::npos) {
            text.replace(pos, from.size(), to);
            pos += to.size();
        }
    };
    replaceAll(value, "&", "&amp;");
    replaceAll(value, "\"", "&quot;");
    replaceAll(value, "<", "&lt;");
    replaceAll(value, ">", "&gt;");
    return value;
}

std::string typeTag(const std::string& type) {
    if (type == "stack") return "Stack";
    if (type == "rect") return "Rect";
    if (type == "text") return "Text";
    if (type == "button") return "Button";
    if (type == "panel") return "Panel";
    if (type == "card") return "Card";
    if (type == "image") return "Image";
    if (type == "row") return "Row";
    if (type == "column") return "Column";
    if (type == "cardSlider") return "CardSlider";
    if (type == "cardSliderItem") return "Item";
    return type;
}

std::string colorHex(const eui::Color& color) {
    auto channel = [](float value) {
        return std::clamp(static_cast<int>(value * 255.0f + 0.5f), 0, 255);
    };
    char buffer[16] = {};
    std::snprintf(buffer, sizeof(buffer), "#%02X%02X%02X%02X",
                  channel(color.r), channel(color.g), channel(color.b), channel(color.a));
    return std::string(buffer);
}

void writeNodeXml(std::ostringstream& out, const UiNode& node, int indent) {
    const std::string pad(static_cast<std::size_t>(indent), ' ');
    out << pad << "<" << typeTag(node.type) << " id=\"" << xmlEscape(node.id) << "\"";
    const char* numberKeys[] = {
        "x", "y", "width", "height", "radius", "fontSize", "lineHeight", "fontWeight",
        "padding", "gap", "duration", "interval", "currentIndex", "cardSpacing"
    };
    for (const char* key : numberKeys) {
        if (const auto it = node.numbers.find(key); it != node.numbers.end()) {
            out << " " << key << "=\"" << it->second << "\"";
        }
    }
    for (const auto& [key, value] : node.strings) {
        out << " " << key << "=\"" << xmlEscape(value) << "\"";
    }
    for (const auto& [key, value] : node.colors) {
        out << " " << (key == "background" ? "backgroundColor" : key) << "=\"" << colorHex(value) << "\"";
    }
    for (const auto& [key, value] : node.booleans) {
        out << " " << (key == "background" ? "backgroundEnabled" : key)
            << "=\"" << (value ? "true" : "false") << "\"";
    }
    if (node.children.empty()) {
        out << " />\n";
        return;
    }
    out << ">\n";
    for (const UiNode& child : node.children) {
        writeNodeXml(out, child, indent + 2);
    }
    out << pad << "</" << typeTag(node.type) << ">\n";
}

void saveXml() {
    std::ostringstream out;
    writeNodeXml(out, root, 0);
    std::ofstream file(savePath());
    if (!file) {
        statusText = "Save failed";
        return;
    }
    file << out.str();
    file.close();
    const visual_editor::UiDocument verification = visual_editor::loadXmlUiDocument(savePath());
    statusText = verification.valid ? "Saved and verified XML" : ("Save validation failed: " + verification.error);
}

void drawHeader(eui::Ui& ui, float width);
void drawPalette(eui::Ui& ui, float height);
void drawPaletteDragGhost(eui::Ui& ui);
void drawCanvas(eui::Ui& ui, float x, float y, float width, float height);
void drawInspector(eui::Ui& ui, float x, float height);
void drawNodeOverlays(eui::Ui& ui, UiNode& node);
void drawNumberEditor(eui::Ui& ui, const std::string& id, UiNode& node, const std::string& key, float x, float y);
void drawTextEditor(eui::Ui& ui, const std::string& id, UiNode& node, const std::string& key, float x, float y);
void drawBoolEditor(eui::Ui& ui, const std::string& id, UiNode& node, const std::string& key, float x, float y);
void drawColorSwatch(eui::Ui& ui, const std::string& id, UiNode& node, float x, float y);

} // namespace

void compose(eui::Ui& ui, const eui::Screen& screen) {
    initialize();
    ui.stack("designer.root")
        .size(screen.width, screen.height)
        .content([&] {
            ui.rect("designer.bg")
                .size(screen.width, screen.height)
                .color({0.045f, 0.050f, 0.060f, 1.0f})
                .build();
            drawHeader(ui, screen.width);
            drawPalette(ui, screen.height);
            drawInspector(ui, std::max(kLeftW, screen.width - kRightW), screen.height);
            drawCanvas(ui, kLeftW, kHeaderH, std::max(1.0f, screen.width - kLeftW - kRightW), std::max(1.0f, screen.height - kHeaderH));
            drawPaletteDragGhost(ui);
        })
        .build();
}

namespace {

void drawHeader(eui::Ui& ui, float width) {
    const auto tokens = theme();
    ui.rect("designer.header.bg")
        .size(width, kHeaderH)
        .color({0.065f, 0.071f, 0.085f, 1.0f})
        .border(1.0f, {1.0f, 1.0f, 1.0f, 0.07f})
        .build();
    ui.text("designer.header.title")
        .position(22.0f, 18.0f)
        .size(270.0f, 30.0f)
        .text("EUI Visual Designer")
        .fontSize(24.0f)
        .lineHeight(30.0f)
        .fontWeight(820)
        .color(tokens.text)
        .build();
    const struct Tool {
        const char* id;
        const char* text;
        float width;
        std::function<void()> action;
    } tools[] = {
        {"undo", "Undo", 68.0f, [] { undo(); }},
        {"redo", "Redo", 68.0f, [] { redo(); }},
        {"duplicate", "Copy", 68.0f, [] { duplicateSelected(); }},
        {"delete", "Delete", 74.0f, [] { deleteSelected(); }},
    };
    float toolX = 292.0f;
    for (const Tool& tool : tools) {
        ui.stack(std::string("designer.header.") + tool.id + ".wrap")
            .position(toolX, 15.0f)
            .size(tool.width, 38.0f)
            .content([&] {
                components::button(ui, std::string("designer.header.") + tool.id)
                    .theme(tokens, false)
                    .size(tool.width, 38.0f)
                    .text(tool.text)
                    .fontSize(13.0f)
                    .radius(9.0f)
                    .onClick(tool.action)
                    .build();
            })
            .build();
        toolX += tool.width + 8.0f;
    }
    ui.text("designer.header.status")
        .position(toolX + 10.0f, 24.0f)
        .size(std::max(1.0f, width - toolX - 190.0f), 22.0f)
        .text(pendingType.empty() ? statusText : ("Click canvas to place " + pendingType))
        .fontSize(14.0f)
        .lineHeight(20.0f)
        .color(components::theme::withAlpha(tokens.text, 0.68f))
        .build();
    ui.stack("designer.header.save.wrap")
        .position(std::max(20.0f, width - 156.0f), 14.0f)
        .size(126.0f, 40.0f)
        .content([&] {
            components::button(ui, "designer.header.save")
                .theme(tokens)
                .size(126.0f, 40.0f)
                .text("Save XML")
                .fontSize(14.0f)
                .onClick([] { saveXml(); })
                .build();
        })
        .build();
}

void drawPalette(eui::Ui& ui, float height) {
    const auto tokens = theme();
    ui.rect("palette.bg")
        .position(0.0f, kHeaderH)
        .size(kLeftW, std::max(1.0f, height - kHeaderH))
        .color({0.055f, 0.061f, 0.074f, 1.0f})
        .border(1.0f, {1.0f, 1.0f, 1.0f, 0.07f})
        .build();
    ui.text("palette.title")
        .position(18.0f, kHeaderH + 18.0f)
        .size(kLeftW - 36.0f, 24.0f)
        .text("Components")
        .fontSize(16.0f)
        .lineHeight(22.0f)
        .fontWeight(800)
        .color(tokens.text)
        .build();

    float y = kHeaderH + 58.0f;
    for (const PaletteItem& item : palette) {
        const bool active = pendingType == item.type || (paletteDragging && paletteDragType == item.type);
        ui.rect("palette.item." + item.type + ".bg")
            .position(16.0f, y)
            .size(kLeftW - 32.0f, 42.0f)
            .states(active ? tokens.primary : tokens.surface,
                    active ? tokens.primary : tokens.surfaceHover,
                    tokens.surfaceActive)
            .radius(10.0f)
            .build();
        ui.text("palette.item." + item.type + ".text")
            .position(30.0f, y + 10.0f)
            .size(kLeftW - 60.0f, 22.0f)
            .text(item.label)
            .fontSize(14.0f)
            .lineHeight(20.0f)
            .fontWeight(680)
            .color(active ? eui::Color{0.04f, 0.06f, 0.08f, 1.0f} : tokens.text)
            .build();
        components::mouseArea(ui, "palette.item." + item.type + ".input")
            .position(16.0f, y)
            .size(kLeftW - 32.0f, 42.0f)
            .zIndex(1000)
            .onTap([type = item.type] {
                pendingType = type;
                statusText = "Click canvas to place " + type;
            })
            .onDragStart([type = item.type](const components::MouseEvent& event) {
                paletteDragging = true;
                paletteDragType = type;
                pendingType.clear();
                paletteDragX = event.globalX;
                paletteDragY = event.globalY;
                statusText = "Drag onto canvas";
            })
            .onDrag([](const components::MouseDragEvent& event) {
                paletteDragX = event.globalX;
                paletteDragY = event.globalY;
            })
            .onDragEnd([](const components::MouseDragEvent& event) {
                const bool inside = event.globalX >= canvasGlobalX && event.globalY >= canvasGlobalY &&
                    event.globalX <= canvasGlobalX + canvasGlobalWidth &&
                    event.globalY <= canvasGlobalY + canvasGlobalHeight;
                if (inside && !paletteDragType.empty()) {
                    pushHistory();
                    UiNode node = makeNode(
                        paletteDragType,
                        std::clamp(event.globalX - canvasGlobalX, 0.0f, canvasGlobalWidth - 40.0f),
                        std::clamp(event.globalY - canvasGlobalY, 0.0f, canvasGlobalHeight - 40.0f));
                    selectedId = node.id;
                    root.children.push_back(std::move(node));
                    markChanged("Component dropped");
                } else {
                    statusText = "Drop cancelled";
                }
                paletteDragging = false;
                paletteDragType.clear();
            })
            .build();
        y += 52.0f;
    }
}

void drawPaletteDragGhost(eui::Ui& ui) {
    if (!paletteDragging || paletteDragType.empty()) {
        return;
    }
    ui.rect("palette.drag.ghost.bg")
        .position(paletteDragX - 62.0f, paletteDragY - 20.0f)
        .size(124.0f, 40.0f)
        .color(theme().primary)
        .radius(8.0f)
        .shadow(14.0f, 0.0f, 6.0f, {0.0f, 0.0f, 0.0f, 0.30f})
        .zIndex(3000)
        .build();
    ui.text("palette.drag.ghost.text")
        .position(paletteDragX - 52.0f, paletteDragY - 10.0f)
        .size(104.0f, 20.0f)
        .text(paletteDragType)
        .fontSize(13.0f)
        .lineHeight(18.0f)
        .fontWeight(760)
        .color({0.04f, 0.06f, 0.08f, 1.0f})
        .horizontalAlign(eui::HorizontalAlign::Center)
        .zIndex(3001)
        .build();
}

void drawCanvas(eui::Ui& ui, float x, float y, float width, float height) {
    canvasGlobalX = x;
    canvasGlobalY = y;
    canvasGlobalWidth = width;
    canvasGlobalHeight = height;
    ui.stack("canvas.shell")
        .position(x, y)
        .size(width, height)
        .clip()
        .content([&] {
            ui.rect("canvas.bg")
                .size(width, height)
                .color({0.030f, 0.034f, 0.043f, 1.0f})
                .build();

            components::mouseArea(ui, "canvas.place")
                .size(width, height)
                .zIndex(10)
                .cursor(pendingType.empty() ? eui::CursorShape::Arrow : eui::CursorShape::Hand)
                .onTap([width, height](const components::MouseEvent& event) {
                    if (pendingType.empty()) {
                        selectedId.clear();
                        return;
                    }
                    pushHistory();
                    UiNode node = makeNode(pendingType, std::clamp(event.x, 0.0f, width - 40.0f), std::clamp(event.y, 0.0f, height - 40.0f));
                    selectedId = node.id;
                    root.children.push_back(std::move(node));
                    pendingType.clear();
                    markChanged("Component placed");
                })
                .build();

            ui.stack("canvas.document")
                .size(width, height)
                .dirtyKey("designer.canvas." + std::to_string(documentRevision))
                .content([&] {
                    visual_editor::renderNode(ui, root, width, height, "root");
                })
                .build();

            for (UiNode& child : root.children) {
                drawNodeOverlays(ui, child);
            }
        })
        .build();
}

void drawNodeOverlays(eui::Ui& ui, UiNode& node) {
    if (node.type == "cardSliderItem") {
        return;
    }
    const float x = visual_editor::floatOr(node, "x", 0.0f);
    const float y = visual_editor::floatOr(node, "y", 0.0f);
    const float w = std::max(24.0f, visual_editor::floatOr(node, "width", 120.0f));
    const float h = std::max(24.0f, visual_editor::floatOr(node, "height", 48.0f));
    const bool selected = node.id == selectedId;

    if (node.type == "row" || node.type == "column") {
        ui.rect("canvas.container.hint." + node.id)
            .position(x, y)
            .size(w, h)
            .color({0.0f, 0.0f, 0.0f, 0.0f})
            .border(1.0f, {0.42f, 0.62f, 0.95f, 0.30f})
            .radius(8.0f)
            .zIndex(880)
            .build();
    }

    components::mouseArea(ui, "canvas.overlay." + node.id)
        .position(x, y)
        .size(w, h)
        .zIndex(900)
        .color({0.0f, 0.0f, 0.0f, 0.0f})
        .cursor(eui::CursorShape::Hand)
        .onTap([id = node.id] {
            selectedId = id;
            pendingType.clear();
            statusText = "Node selected";
        })
        .onDragStart([id = node.id](const components::MouseEvent&) {
            selectedId = id;
            pushHistory();
        })
        .onDrag([id = node.id](const components::MouseDragEvent& event) {
            if (UiNode* node = findNode(root, id)) {
                node->numbers["x"] = visual_editor::numberOr(*node, "x", 0.0) + event.deltaX;
                node->numbers["y"] = visual_editor::numberOr(*node, "y", 0.0) + event.deltaY;
                selectedId = id;
                markChanged("Dragging node");
            }
        })
        .build();

    if (selected) {
        ui.rect("canvas.selection." + node.id)
            .position(x - 2.0f, y - 2.0f)
            .size(w + 4.0f, h + 4.0f)
            .color({0.0f, 0.0f, 0.0f, 0.0f})
            .border(2.0f, theme().primary)
            .radius(6.0f)
            .zIndex(920)
            .build();

        ui.rect("canvas.resize.handle." + node.id + ".bg")
            .position(x + w - 7.0f, y + h - 7.0f)
            .size(14.0f, 14.0f)
            .color(theme().primary)
            .radius(3.0f)
            .zIndex(940)
            .build();
        components::mouseArea(ui, "canvas.resize.handle." + node.id)
            .position(x + w - 12.0f, y + h - 12.0f)
            .size(24.0f, 24.0f)
            .zIndex(950)
            .onDragStart([id = node.id](const components::MouseEvent&) {
                selectedId = id;
                pushHistory();
            })
            .onDrag([id = node.id](const components::MouseDragEvent& event) {
                if (UiNode* node = findNode(root, id)) {
                    node->numbers["width"] = std::max(24.0, visual_editor::numberOr(*node, "width", 120.0) + event.deltaX);
                    node->numbers["height"] = std::max(24.0, visual_editor::numberOr(*node, "height", 48.0) + event.deltaY);
                    markChanged("Resizing node");
                }
            })
            .build();
    }
}

void drawInspector(eui::Ui& ui, float x, float height) {
    const auto tokens = theme();
    ui.rect("inspector.bg")
        .position(x, kHeaderH)
        .size(kRightW, std::max(1.0f, height - kHeaderH))
        .color({0.055f, 0.061f, 0.074f, 1.0f})
        .border(1.0f, {1.0f, 1.0f, 1.0f, 0.07f})
        .build();
    ui.text("inspector.title")
        .position(x + 18.0f, kHeaderH + 18.0f)
        .size(kRightW - 36.0f, 24.0f)
        .text("Inspector")
        .fontSize(16.0f)
        .lineHeight(22.0f)
        .fontWeight(800)
        .color(tokens.text)
        .build();

    UiNode* node = selectedNode();
    if (!node) {
        ui.text("inspector.empty")
            .position(x + 18.0f, kHeaderH + 58.0f)
            .size(kRightW - 36.0f, 58.0f)
            .text("Select a node on the canvas.")
            .fontSize(14.0f)
            .lineHeight(22.0f)
            .wrap(true)
            .color(components::theme::withAlpha(tokens.text, 0.62f))
            .build();
        return;
    }

    ui.text("inspector.node")
        .position(x + 18.0f, kHeaderH + 54.0f)
        .size(kRightW - 36.0f, 24.0f)
        .text(node->id + "  <" + node->type + ">")
        .fontSize(13.0f)
        .lineHeight(18.0f)
        .color(components::theme::withAlpha(tokens.text, 0.70f))
        .build();

    float y = kHeaderH + 92.0f;
    drawNumberEditor(ui, "inspector.x", *node, "x", x + 18.0f, y);
    drawNumberEditor(ui, "inspector.y", *node, "y", x + 150.0f, y);
    y += 66.0f;
    drawNumberEditor(ui, "inspector.w", *node, "width", x + 18.0f, y);
    drawNumberEditor(ui, "inspector.h", *node, "height", x + 150.0f, y);
    y += 66.0f;
    drawNumberEditor(ui, "inspector.radius", *node, "radius", x + 18.0f, y);

    if (node->type == "text" || node->type == "button") {
        y += 66.0f;
        drawTextEditor(ui, "inspector.text", *node, "text", x + 18.0f, y);
    }
    if (node->type == "image") {
        y += 66.0f;
        drawTextEditor(ui, "inspector.source", *node, "source", x + 18.0f, y);
    }
    if (node->type == "row" || node->type == "column") {
        y += 66.0f;
        drawNumberEditor(ui, "inspector.gap", *node, "gap", x + 18.0f, y);
        drawNumberEditor(ui, "inspector.padding", *node, "padding", x + 150.0f, y);
    }
    if (node->type == "cardSlider") {
        y += 66.0f;
        drawNumberEditor(ui, "inspector.duration", *node, "duration", x + 18.0f, y);
        drawNumberEditor(ui, "inspector.interval", *node, "interval", x + 150.0f, y);
        y += 66.0f;
        drawBoolEditor(ui, "inspector.autoplay", *node, "autoPlay", x + 18.0f, y);
        drawBoolEditor(ui, "inspector.tilt", *node, "tilt", x + 150.0f, y);
    }
    if (node->type == "rect" || node->type == "text") {
        y += 80.0f;
        drawColorSwatch(ui, "inspector.color", *node, x + 18.0f, y);
    }
}

void label(eui::Ui& ui, const std::string& id, const std::string& text, float x, float y, float width) {
    ui.text(id)
        .position(x, y)
        .size(width, 18.0f)
        .text(text)
        .fontSize(12.0f)
        .lineHeight(16.0f)
        .color(components::theme::withAlpha(theme().text, 0.58f))
        .build();
}

std::string formatNumber(double value) {
    char buffer[40] = {};
    std::snprintf(buffer, sizeof(buffer), "%.3f", value);
    std::string text(buffer);
    while (!text.empty() && text.back() == '0') {
        text.pop_back();
    }
    if (!text.empty() && text.back() == '.') {
        text.pop_back();
    }
    return text.empty() || text == "-0" ? "0" : text;
}

void drawNumberEditor(eui::Ui& ui, const std::string& id, UiNode& node, const std::string& key, float x, float y) {
    label(ui, id + ".label", key, x, y, 112.0f);
    const std::string valueText = formatNumber(visual_editor::numberOr(node, key, 0.0));
    ui.stack(id + ".wrap")
        .position(x, y + 22.0f)
        .size(112.0f, 36.0f)
        .content([&] {
            components::input(ui, id)
                .theme(theme())
                .size(112.0f, 36.0f)
                .value(valueText)
                .fontSize(14.0f)
                .onFocus([](bool focused) {
                    if (focused) {
                        pushHistory();
                    }
                })
                .onChange([nodeId = node.id, key](const std::string& value) {
                    try {
                        if (UiNode* node = findNode(root, nodeId)) {
                            node->numbers[key] = std::stod(value);
                            markChanged("Property changed");
                        }
                    } catch (...) {
                    }
                })
                .build();
        })
        .build();
}

void drawTextEditor(eui::Ui& ui, const std::string& id, UiNode& node, const std::string& key, float x, float y) {
    label(ui, id + ".label", key, x, y, 244.0f);
    ui.stack(id + ".wrap")
        .position(x, y + 22.0f)
        .size(244.0f, 40.0f)
        .content([&] {
            components::input(ui, id)
                .theme(theme())
                .size(244.0f, 40.0f)
                .value(visual_editor::stringOr(node, key, ""))
                .fontSize(14.0f)
                .onFocus([](bool focused) {
                    if (focused) {
                        pushHistory();
                    }
                })
                .onChange([nodeId = node.id, key](const std::string& value) {
                    if (UiNode* node = findNode(root, nodeId)) {
                        node->strings[key] = value;
                        markChanged("Text changed");
                    }
                })
                .build();
        })
        .build();
}

void drawBoolEditor(eui::Ui& ui, const std::string& id, UiNode& node, const std::string& key, float x, float y) {
    ui.stack(id + ".wrap")
        .position(x, y + 22.0f)
        .size(112.0f, 36.0f)
        .content([&] {
            components::toggleSwitch(ui, id)
                .theme(theme())
                .size(112.0f, 36.0f)
                .checked(visual_editor::boolOr(node, key, false))
                .text(key)
                .onChange([nodeId = node.id, key](bool value) {
                    if (UiNode* node = findNode(root, nodeId)) {
                        pushHistory();
                        node->booleans[key] = value;
                        markChanged("Option changed");
                    }
                })
                .build();
        })
        .build();
}

void drawColorSwatch(eui::Ui& ui, const std::string& id, UiNode& node, float x, float y) {
    label(ui, id + ".label", "color", x, y, 244.0f);
    const eui::Color colors[] = {
        {0.18f, 0.24f, 0.34f, 1.0f},
        {0.86f, 0.24f, 0.40f, 1.0f},
        {0.18f, 0.64f, 0.82f, 1.0f},
        {0.95f, 0.68f, 0.18f, 1.0f},
        {0.34f, 0.76f, 0.45f, 1.0f},
    };
    for (int i = 0; i < 5; ++i) {
        ui.rect(id + "." + std::to_string(i))
            .position(x + i * 42.0f, y + 24.0f)
            .size(30.0f, 30.0f)
            .color(colors[i])
            .radius(8.0f)
            .border(1.0f, {1.0f, 1.0f, 1.0f, 0.20f})
            .onClick([nodeId = node.id, color = colors[i]] {
                if (UiNode* node = findNode(root, nodeId)) {
                    pushHistory();
                    node->colors["color"] = color;
                    markChanged("Color changed");
                }
            })
            .build();
    }
}

} // namespace

} // namespace app
