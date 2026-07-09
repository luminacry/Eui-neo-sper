#pragma once

#include "ui_document.h"

#include "eui_neo.h"

#include <algorithm>
#include <string>
#include <vector>

namespace visual_editor {

inline components::theme::ThemeColorTokens previewTheme() {
    return components::theme::dark();
}

inline void renderNode(eui::Ui& ui, const UiNode& node, float parentWidth, float parentHeight, const std::string& fallbackId);

inline void renderChildren(eui::Ui& ui, const UiNode& node, float width, float height) {
    for (std::size_t i = 0; i < node.children.size(); ++i) {
        renderNode(ui, node.children[i], width, height, nodeId(node, "node") + "." + std::to_string(i));
    }
}

inline void renderStack(eui::Ui& ui, const UiNode& node, float parentWidth, float parentHeight, const std::string& id) {
    const float x = floatOr(node, "x", 0.0f);
    const float y = floatOr(node, "y", 0.0f);
    const float width = floatOr(node, "width", parentWidth);
    const float height = floatOr(node, "height", parentHeight);
    ui.stack(id)
        .position(x, y)
        .size(std::max(1.0f, width), std::max(1.0f, height))
        .content([&] {
            renderChildren(ui, node, width, height);
        })
        .build();
}

inline void renderRow(eui::Ui& ui, const UiNode& node, float parentWidth, float parentHeight, const std::string& id) {
    const float x = floatOr(node, "x", 0.0f);
    const float y = floatOr(node, "y", 0.0f);
    const float width = floatOr(node, "width", parentWidth);
    const float height = floatOr(node, "height", parentHeight);
    ui.row(id)
        .position(x, y)
        .size(std::max(1.0f, width), std::max(1.0f, height))
        .gap(floatOr(node, "gap", 0.0f))
        .content([&] {
            renderChildren(ui, node, width, height);
        })
        .build();
}

inline void renderColumn(eui::Ui& ui, const UiNode& node, float parentWidth, float parentHeight, const std::string& id) {
    const float x = floatOr(node, "x", 0.0f);
    const float y = floatOr(node, "y", 0.0f);
    const float width = floatOr(node, "width", parentWidth);
    const float height = floatOr(node, "height", parentHeight);
    ui.column(id)
        .position(x, y)
        .size(std::max(1.0f, width), std::max(1.0f, height))
        .gap(floatOr(node, "gap", 0.0f))
        .padding(floatOr(node, "padding", 0.0f))
        .content([&] {
            renderChildren(ui, node, width, height);
        })
        .build();
}

inline void renderRect(eui::Ui& ui, const UiNode& node, const std::string& id) {
    ui.rect(id)
        .position(floatOr(node, "x", 0.0f), floatOr(node, "y", 0.0f))
        .size(std::max(1.0f, floatOr(node, "width", 120.0f)), std::max(1.0f, floatOr(node, "height", 80.0f)))
        .color(colorOr(node, "color", {0.18f, 0.20f, 0.25f, 1.0f}))
        .radius(floatOr(node, "radius", 8.0f))
        .opacity(floatOr(node, "opacity", 1.0f))
        .build();
}

inline void renderText(eui::Ui& ui, const UiNode& node, const std::string& id) {
    const float fontSize = floatOr(node, "fontSize", 18.0f);
    ui.text(id)
        .position(floatOr(node, "x", 0.0f), floatOr(node, "y", 0.0f))
        .size(std::max(1.0f, floatOr(node, "width", 240.0f)), std::max(1.0f, floatOr(node, "height", fontSize * 1.6f)))
        .text(stringOr(node, "text", "Text"))
        .fontSize(fontSize)
        .lineHeight(floatOr(node, "lineHeight", fontSize * 1.25f))
        .fontWeight(static_cast<int>(floatOr(node, "fontWeight", 500.0f)))
        .wrap(boolOr(node, "wrap", false))
        .color(colorOr(node, "color", {0.92f, 0.94f, 0.98f, 1.0f}))
        .build();
}

inline void renderImage(eui::Ui& ui, const UiNode& node, const std::string& id) {
    ui.image(id)
        .position(floatOr(node, "x", 0.0f), floatOr(node, "y", 0.0f))
        .size(std::max(1.0f, floatOr(node, "width", 180.0f)), std::max(1.0f, floatOr(node, "height", 120.0f)))
        .source(stringOr(node, "source", "assets/icon.png"))
        .cover()
        .radius(floatOr(node, "radius", 8.0f))
        .build();
}

inline void renderButton(eui::Ui& ui, const UiNode& node, const std::string& id) {
    ui.stack(id + ".wrap")
        .position(floatOr(node, "x", 0.0f), floatOr(node, "y", 0.0f))
        .size(std::max(1.0f, floatOr(node, "width", 140.0f)), std::max(1.0f, floatOr(node, "height", 46.0f)))
        .content([&] {
            components::button(ui, id)
                .size(std::max(1.0f, floatOr(node, "width", 140.0f)), std::max(1.0f, floatOr(node, "height", 46.0f)))
                .text(stringOr(node, "text", "Button"))
                .fontSize(floatOr(node, "fontSize", 16.0f))
                .radius(floatOr(node, "radius", 12.0f))
                .theme(previewTheme(), boolOr(node, "primary", true))
                .onClick([] {})
                .build();
        })
        .build();
}

inline void renderCard(eui::Ui& ui, const UiNode& node, float parentWidth, float parentHeight, const std::string& id) {
    const float width = std::max(1.0f, floatOr(node, "width", 320.0f));
    const float height = std::max(1.0f, floatOr(node, "height", 220.0f));
    components::card(ui, id)
        .position(floatOr(node, "x", 0.0f), floatOr(node, "y", 0.0f))
        .size(width, height)
        .padding(floatOr(node, "padding", 18.0f))
        .theme(previewTheme())
        .content([&] {
            renderChildren(ui, node, parentWidth, parentHeight);
        })
        .build();
}

inline void renderPanel(eui::Ui& ui, const UiNode& node, const std::string& id) {
    components::panel(ui, id, previewTheme())
        .position(floatOr(node, "x", 0.0f), floatOr(node, "y", 0.0f))
        .size(std::max(1.0f, floatOr(node, "width", 320.0f)), std::max(1.0f, floatOr(node, "height", 180.0f)))
        .radius(floatOr(node, "radius", 14.0f))
        .build();
}

inline void renderCardSlider(eui::Ui& ui, const UiNode& node, const std::string& id) {
    std::vector<components::CardSliderItem> items;
    for (const UiNode& child : node.children) {
        items.push_back({
            stringOr(child, "source", "assets/icon.png"),
            stringOr(child, "title", "Card"),
            stringOr(child, "subtitle", "Subtitle"),
            stringOr(child, "description", "Card slider item")
        });
    }
    if (items.empty()) {
        items.push_back({"assets/icon.png", "EUI", "Preview", "Add children to this cardSlider node."});
    }

    ui.stack(id + ".wrap")
        .position(floatOr(node, "x", 0.0f), floatOr(node, "y", 0.0f))
        .size(std::max(1.0f, floatOr(node, "width", 760.0f)), std::max(1.0f, floatOr(node, "height", 460.0f)))
        .content([&] {
            components::cardSlider(ui, id)
                .size(std::max(1.0f, floatOr(node, "width", 760.0f)), std::max(1.0f, floatOr(node, "height", 460.0f)))
                .items(items)
                .currentIndex(static_cast<int>(floatOr(node, "currentIndex", 0.0f)))
                .duration(floatOr(node, "duration", 0.56f))
                .interval(floatOr(node, "interval", 2.0f))
                .cardSpacing(floatOr(node, "cardSpacing", 0.0f))
                .autoPlay(boolOr(node, "autoPlay", false))
                .background(boolOr(node, "background", true))
                .tilt(boolOr(node, "tilt", true))
                .theme(previewTheme())
                .build();
        })
        .build();
}

inline void renderUnknown(eui::Ui& ui, const UiNode& node, const std::string& id) {
    ui.rect(id + ".bg")
        .position(floatOr(node, "x", 0.0f), floatOr(node, "y", 0.0f))
        .size(std::max(1.0f, floatOr(node, "width", 260.0f)), std::max(1.0f, floatOr(node, "height", 56.0f)))
        .color({0.38f, 0.13f, 0.15f, 1.0f})
        .radius(8.0f)
        .build();
    ui.text(id + ".text")
        .position(floatOr(node, "x", 0.0f) + 12.0f, floatOr(node, "y", 0.0f) + 12.0f)
        .size(std::max(1.0f, floatOr(node, "width", 260.0f) - 24.0f), 28.0f)
        .text("Unknown node: " + node.type)
        .fontSize(14.0f)
        .lineHeight(18.0f)
        .color({1.0f, 0.82f, 0.82f, 1.0f})
        .build();
}

inline void renderNode(eui::Ui& ui, const UiNode& node, float parentWidth, float parentHeight, const std::string& fallbackId) {
    const std::string id = nodeId(node, fallbackId);
    if (node.type == "stack") {
        renderStack(ui, node, parentWidth, parentHeight, id);
    } else if (node.type == "row") {
        renderRow(ui, node, parentWidth, parentHeight, id);
    } else if (node.type == "column") {
        renderColumn(ui, node, parentWidth, parentHeight, id);
    } else if (node.type == "rect") {
        renderRect(ui, node, id);
    } else if (node.type == "text") {
        renderText(ui, node, id);
    } else if (node.type == "image") {
        renderImage(ui, node, id);
    } else if (node.type == "button") {
        renderButton(ui, node, id);
    } else if (node.type == "card") {
        renderCard(ui, node, parentWidth, parentHeight, id);
    } else if (node.type == "panel") {
        renderPanel(ui, node, id);
    } else if (node.type == "cardSlider" || node.type == "workshop::cardSlider") {
        renderCardSlider(ui, node, id);
    } else {
        renderUnknown(ui, node, id);
    }
}

} // namespace visual_editor
