#include "exception_visualizer.hpp"
#include <filesystem>
#include <format>

ExceptionVisualizer::ExceptionVisualizer() {
    ExceptionHandler::instance().add_observer([this](const auto& event) {
        if (enabled_)
            on_exception(event);
    });
}

ExceptionVisualizer::~ExceptionVisualizer() = default;

void ExceptionVisualizer::on_exception(const ExceptionEvent& event) {
    if (event.severity == Severity::Critical) {
        modal_error_ = event;
        ImGui::OpenPopup("##CriticalError");
        return;
    }

    std::lock_guard lock(toast_mutex_);

    // Extract just filename from path
    std::filesystem::path path(event.location.file_name());
    auto location = std::format("{}:{}",
                                path.filename().string(),
                                event.location.line());

    toasts_.push_back({event.message,
                       location,
                       TOAST_LIFETIME,
                       severity_to_color(event.severity),
                       event.severity});

    if (toasts_.size() > MAX_TOASTS) {
        toasts_.pop_front();
    }
}

void ExceptionVisualizer::render(float dt) {
    render_toasts(dt);
    render_modal();
}

void ExceptionVisualizer::render_toasts(float dt) {
    std::lock_guard lock(toast_mutex_);

    const auto* viewport = ImGui::GetMainViewport();
    float y_offset = 20.0f;

    for (auto it = toasts_.begin(); it != toasts_.end();) {
        it->lifetime -= dt;

        if (it->lifetime <= 0) {
            it = toasts_.erase(it);
            continue;
        }

        // Position and alpha
        float alpha = std::min(it->lifetime, 1.0f);
        ImGui::SetNextWindowPos({viewport->Pos.x + viewport->Size.x - TOAST_WIDTH - 20,
                                 viewport->Pos.y + y_offset});
        ImGui::SetNextWindowSize({TOAST_WIDTH, 0});
        ImGui::SetNextWindowBgAlpha(alpha * 0.95f);

        // Styling
        ImGui::PushStyleColor(ImGuiCol_WindowBg,
                              IM_COL32((it->color >> 0) & 0xFF,
                                       (it->color >> 8) & 0xFF,
                                       (it->color >> 16) & 0xFF,
                                       200));
        ImGui::PushStyleColor(ImGuiCol_Border, it->color);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 4.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 2.0f);

        char window_id[32];
        snprintf(window_id, sizeof(window_id), "##Toast%td",
                 std::distance(toasts_.begin(), it));

        if (ImGui::Begin(window_id, nullptr,
                         ImGuiWindowFlags_NoDecoration |
                             ImGuiWindowFlags_AlwaysAutoResize |
                             ImGuiWindowFlags_NoSavedSettings |
                             ImGuiWindowFlags_NoFocusOnAppearing |
                             ImGuiWindowFlags_NoNav |
                             ImGuiWindowFlags_NoDocking)) {

            // Icon and severity
            const char* icon = it->severity == Severity::Error ? "⚠" : it->severity == Severity::Warning ? "⚡"
                                                                                                         : "ℹ";
            ImGui::Text("%s %s", icon, it->text.c_str());

            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(180, 180, 180, 255));
            ImGui::Text("  %s", it->location.c_str());
            ImGui::PopStyleColor();
        }
        ImGui::End();

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(2);

        y_offset += 80;
        ++it;
    }
}

void ExceptionVisualizer::render_modal() {
    if (ImGui::BeginPopupModal("##CriticalError", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("⚠ Critical Error");
        ImGui::Separator();

        if (modal_error_) {
            ImGui::TextWrapped("%s", modal_error_->message.c_str());

            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(150, 150, 150, 255));
            ImGui::Text("Location: %s:%d",
                        std::filesystem::path(modal_error_->location.file_name()).filename().c_str(),
                        modal_error_->location.line());
            ImGui::Text("Function: %s", modal_error_->location.function_name());
            ImGui::PopStyleColor();
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Button("View Logs", ImVec2(100, 0))) {
            // Open log viewer
            ImGui::CloseCurrentPopup();
            modal_error_.reset();
        }

        ImGui::SameLine();
        if (ImGui::Button("Continue", ImVec2(100, 0))) {
            ImGui::CloseCurrentPopup();
            modal_error_.reset();
        }

        ImGui::EndPopup();
    }
}

ImU32 ExceptionVisualizer::severity_to_color(Severity sev) const {
    switch (sev) {
    case Severity::Critical:
    case Severity::Error:
        return IM_COL32(220, 50, 50, 255);
    case Severity::Warning:
        return IM_COL32(220, 150, 50, 255);
    case Severity::Info:
        return IM_COL32(50, 150, 220, 255);
    }
    return IM_COL32(200, 200, 200, 255);
}
