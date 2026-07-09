#include "document/ui_document_json.h"
#include "document/ui_document_xml.h"
#include "document/ui_document_renderer.h"
#include "preview/file_watcher.h"
#include "eui_neo.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>

namespace app {

const DslAppConfig& dslAppConfig() {
    static const DslAppConfig config = DslAppConfig{}
        .title("EUI Visual Editor")
        .pageId("eui_visual_editor")
        .clearColor({0.045f, 0.050f, 0.060f, 1.0f})
        .windowSize(1180, 760)
        .fps(60.0);
    return config;
}

namespace {

const std::string& documentPath() {
    static const std::string path = [] {
        namespace fs = std::filesystem;
        const char* candidates[] = {
            "apps/eui_visual_designer/designed.ui.xml",
            "../apps/eui_visual_designer/designed.ui.xml",
            "../../apps/eui_visual_designer/designed.ui.xml",
            "/home/user/EUI/apps/eui_visual_designer/designed.ui.xml",
            "apps/eui_visual_editor/examples/basic.ui.xml",
            "../apps/eui_visual_editor/examples/basic.ui.xml",
            "../../apps/eui_visual_editor/examples/basic.ui.xml",
            "/home/user/EUI/apps/eui_visual_editor/examples/basic.ui.xml",
            "apps/eui_visual_editor/examples/basic.ui.json",
            "../apps/eui_visual_editor/examples/basic.ui.json",
            "../../apps/eui_visual_editor/examples/basic.ui.json",
            "/home/user/EUI/apps/eui_visual_editor/examples/basic.ui.json",
        };
        for (const char* candidate : candidates) {
            std::error_code error;
            if (fs::exists(candidate, error)) {
                return fs::absolute(candidate, error).string();
            }
        }
        return std::string(candidates[0]);
    }();
    return path;
}

visual_editor::UiDocument document;
std::unique_ptr<visual_editor::FileWatcher> watcher;
std::string statusText = "Loading document";
std::string lastError;
int reloadCount = 0;
bool initialized = false;
float watchAccumulator = 0.0f;
float reloadPulseSeconds = 0.0f;

bool hasSuffix(const std::string& value, const std::string& suffix) {
    return value.size() >= suffix.size() &&
           value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
}

components::theme::ThemeColorTokens theme() {
    return components::theme::dark();
}

eui::Transition motion() {
    auto transition = eui::Transition::make(0.16f, eui::Ease::OutCubic);
    transition.animate(eui::AnimProperty::Color | eui::AnimProperty::TextColor |
                       eui::AnimProperty::Border | eui::AnimProperty::Shadow |
                       eui::AnimProperty::Opacity);
    return transition;
}

std::string reloadLabel() {
    char buffer[80] = {};
    std::snprintf(buffer, sizeof(buffer), "reloads: %d", reloadCount);
    return std::string(buffer);
}

std::string documentDirtyKey() {
    return document.sourcePath + "#" + std::to_string(reloadCount);
}

void reloadDocument() {
    document = hasSuffix(documentPath(), ".xml")
        ? visual_editor::loadXmlUiDocument(documentPath())
        : visual_editor::loadUiDocument(documentPath());
    ++reloadCount;
    reloadPulseSeconds = 0.80f;
    if (document.valid) {
        statusText = "Preview reloaded";
        lastError.clear();
    } else {
        statusText = "Document parse failed";
        lastError = document.error;
    }
}

void ensureInitialized() {
    if (initialized) {
        return;
    }
    watcher = std::make_unique<visual_editor::FileWatcher>(documentPath());
    watcher->poll();
    reloadDocument();
    initialized = true;
}

void pollDocument(float deltaSeconds) {
    ensureInitialized();
    reloadPulseSeconds = std::max(0.0f, reloadPulseSeconds - deltaSeconds);
    watchAccumulator += std::max(0.0f, deltaSeconds);
    if (watchAccumulator < 0.18f) {
        return;
    }
    watchAccumulator = 0.0f;
    if (watcher && watcher->poll()) {
        reloadDocument();
    } else if (document.valid && reloadPulseSeconds <= 0.0f) {
        statusText = hasSuffix(documentPath(), ".xml") ? "Watching XML" : "Watching JSON";
    }
}

void drawHeader(eui::Ui& ui, float width);
void drawStatusPill(eui::Ui& ui, float x, float y);
void drawPreviewTicker(eui::Ui& ui);
void drawPreview(eui::Ui& ui, float x, float y, float width, float height);
void drawError(eui::Ui& ui, float x, float y, float width, float height);

} // namespace

void compose(eui::Ui& ui, const eui::Screen& screen) {
    ensureInitialized();

    ui.stack("root")
        .size(screen.width, screen.height)
        .content([&] {
            drawPreviewTicker(ui);

            ui.rect("root.bg")
                .size(screen.width, screen.height)
                .color({0.045f, 0.050f, 0.060f, 1.0f})
                .build();

            drawHeader(ui, screen.width);

            const float margin = 24.0f;
            const float previewX = margin;
            const float previewY = 104.0f;
            const float previewW = std::max(1.0f, screen.width - margin * 2.0f);
            const float previewH = std::max(1.0f, screen.height - previewY - margin);
            drawPreview(ui, previewX, previewY, previewW, previewH);
        })
        .build();
}

namespace {

void drawHeader(eui::Ui& ui, float width) {
    const auto tokens = theme();
    ui.rect("header.bg")
        .size(width, 82.0f)
        .color({0.065f, 0.071f, 0.085f, 1.0f})
        .border(1.0f, {1.0f, 1.0f, 1.0f, 0.07f})
        .build();

    ui.text("header.title")
        .position(24.0f, 18.0f)
        .size(320.0f, 30.0f)
        .text("EUI Visual Preview")
        .fontSize(24.0f)
        .lineHeight(30.0f)
        .fontWeight(820)
        .color(tokens.text)
        .build();

    ui.text("header.path")
        .position(24.0f, 50.0f)
        .size(std::max(1.0f, width - 48.0f), 22.0f)
        .text(documentPath())
        .fontSize(13.0f)
        .lineHeight(18.0f)
        .color(components::theme::withAlpha(tokens.text, 0.58f))
        .build();

    drawStatusPill(ui, std::max(24.0f, width - 396.0f), 23.0f);
}

void drawStatusPill(eui::Ui& ui, float x, float y) {
    const bool ok = document.valid;
    const float pulse = std::clamp(reloadPulseSeconds / 0.80f, 0.0f, 1.0f);
    const eui::Color bg = ok ? eui::Color{0.10f, 0.25f, 0.18f, 1.0f}
                             : eui::Color{0.34f, 0.12f, 0.14f, 1.0f};
    const eui::Color fg = ok ? eui::Color{0.58f, 0.96f, 0.72f, 1.0f}
                             : eui::Color{1.0f, 0.70f, 0.72f, 1.0f};

    ui.rect("header.status.bg")
        .position(x, y)
        .size(174.0f, 36.0f)
        .color(bg)
        .radius(18.0f)
        .transition(motion())
        .animate(eui::AnimProperty::Color)
        .build();

    if (pulse > 0.0f) {
        ui.rect("header.status.pulse")
            .position(x + 4.0f, y + 4.0f)
            .size(28.0f + 138.0f * pulse, 28.0f)
            .color(ok ? eui::Color{0.48f, 1.0f, 0.66f, 0.16f * pulse}
                      : eui::Color{1.0f, 0.42f, 0.44f, 0.18f * pulse})
            .radius(14.0f)
            .build();
    }

    ui.text("header.status.text")
        .position(x + 14.0f, y + 8.0f)
        .size(146.0f, 20.0f)
        .text(statusText)
        .fontSize(13.0f)
        .lineHeight(18.0f)
        .fontWeight(700)
        .color(fg)
        .transition(motion())
        .animate(eui::AnimProperty::TextColor)
        .build();

    ui.text("header.reloads")
        .position(x + 190.0f, y + 8.0f)
        .size(138.0f, 20.0f)
        .text(reloadLabel())
        .fontSize(13.0f)
        .lineHeight(18.0f)
        .color({0.70f, 0.74f, 0.82f, 1.0f})
        .build();
}

void drawPreviewTicker(eui::Ui& ui) {
    ui.stack("preview.watch.ticker")
        .size(1.0f, 1.0f)
        .onFrame([](float deltaSeconds) {
            pollDocument(deltaSeconds);
        })
        .build();
}

void drawPreview(eui::Ui& ui, float x, float y, float width, float height) {
    ui.stack("preview.shell")
        .position(x, y)
        .size(width, height)
        .clip()
        .content([&] {
            ui.rect("preview.bg")
                .size(width, height)
                .color({0.030f, 0.034f, 0.043f, 1.0f})
                .radius(14.0f)
                .border(1.0f, {1.0f, 1.0f, 1.0f, 0.10f})
                .shadow(20.0f, 0.0f, 10.0f, {0.0f, 0.0f, 0.0f, 0.24f})
                .build();

            if (!document.valid) {
                drawError(ui, 24.0f, 24.0f, std::max(1.0f, width - 48.0f), std::max(1.0f, height - 48.0f));
                return;
            }

            ui.stack("preview.document")
                .position(0.0f, 0.0f)
                .size(width, height)
                .dirtyKey(documentDirtyKey())
                .content([&] {
                    visual_editor::renderNode(ui, document.root, width, height, "document.root");
                })
                .build();
        })
        .build();
}

void drawError(eui::Ui& ui, float x, float y, float width, float height) {
    ui.rect("preview.error.bg")
        .position(x, y)
        .size(width, std::min(height, 168.0f))
        .color({0.20f, 0.07f, 0.08f, 1.0f})
        .radius(12.0f)
        .border(1.0f, {1.0f, 0.42f, 0.44f, 0.38f})
        .build();

    ui.text("preview.error.title")
        .position(x + 20.0f, y + 18.0f)
        .size(std::max(1.0f, width - 40.0f), 28.0f)
        .text("JSON document error")
        .fontSize(20.0f)
        .lineHeight(26.0f)
        .fontWeight(800)
        .color({1.0f, 0.82f, 0.82f, 1.0f})
        .build();

    ui.text("preview.error.body")
        .position(x + 20.0f, y + 58.0f)
        .size(std::max(1.0f, width - 40.0f), 82.0f)
        .text(lastError.empty() ? "Unknown parse error." : lastError)
        .fontSize(15.0f)
        .lineHeight(22.0f)
        .wrap(true)
        .color({1.0f, 0.70f, 0.72f, 1.0f})
        .build();
}

} // namespace

} // namespace app
