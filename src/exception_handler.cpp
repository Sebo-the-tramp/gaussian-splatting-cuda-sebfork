#include "exception_visualizer.hpp"
#include <algorithm>
#include <format>

namespace gs::visualizer {

    ExceptionVisualizer::ExceptionVisualizer() {
        // Register as observer
        core::ExceptionHandler::get().add_observer(
            [this](const auto& event) {
                if (enabled_) {
                    on_exception(event);
                }
            });

        LOG_DEBUG("Exception visualizer initialized");
    }

    ExceptionVisualizer::~ExceptionVisualizer() {
        // Could unregister here if ExceptionHandler supported it
        LOG_DEBUG("Exception visualizer destroyed");
    }

    void ExceptionVisualizer::on_exception(const core::ExceptionEvent& event) {
        // Update statistics
        switch (event.severity) {
        case core::LogLevel::Error:
        case core::LogLevel::Critical:
            error_count_++;
            break;
        case core::LogLevel::Warn:
            warning_count_++;
            break;
        case core::LogLevel::Info:
        case core::LogLevel::Debug:
        case core::LogLevel::Trace:
            info_count_++;
            break;
        default:
            break;
        }

        // Handle critical errors with modal
        if (event.severity == core::LogLevel::Critical) {
            modal_error_ = event;
            ImGui::OpenPopup("##CriticalError");
            return;
        }

        // Add to toast queue
        std::lock_guard lock(mutex_);

        // Extract filename from path
        std::filesystem::path path(event.location.file_name());
        auto location = std::format("{}:{}",
                                    path.filename().string(),
                                    event.location.line());

        toasts_.push_back({event.message,
                           location,
                           event.type_name,
                           toast_lifetime_,
                           toast_lifetime_,
                           severity_to_color(event.severity),
                           event.severity});

        // Limit toast count
        while (toasts_.size() > max_toasts_) {
            toasts_.pop_front();
        }
    }

    void ExceptionVisualizer::render(float dt) {
        render_toasts(dt);
        render_modal();

        if (show_statistics_) {
            render_statistics_window();
        }
    }

    void ExceptionVisualizer::render_toasts(float dt) {
        std::lock_guard lock(mutex_);

        if (toasts_.empty())
            return;

        const auto* viewport = ImGui::GetMainViewport();

        // Calculate starting position based on corner
        float x_pos, y_pos;
        bool stack_downward = true;

        switch (position_corner_) {
        case 0: // Top-right (default)
            x_pos = viewport->Pos.x + viewport->Size.x - TOAST_WIDTH - TOAST_MARGIN;
            y_pos = viewport->Pos.y + TOAST_MARGIN;
            stack_downward = true;
            break;
        case 1: // Top-left
            x_pos = viewport->Pos.x + TOAST_MARGIN;
            y_pos = viewport->Pos.y + TOAST_MARGIN;
            stack_downward = true;
            break;
        case 2: // Bottom-right
            x_pos = viewport->Pos.x + viewport->Size.x - TOAST_WIDTH - TOAST_MARGIN;
            y_pos = viewport->Pos.y + viewport->Size.y - TOAST_MARGIN;
            stack_downward = false;
            break;
        case 3: // Bottom-left
            x_pos = viewport->Pos.x + TOAST_MARGIN;
            y_pos = viewport->Pos.y + viewport->Size.y - TOAST_MARGIN;
            stack_downward = false;
            break;
        default:
            x_pos = viewport->Pos.x + viewport->Size.x - TOAST_WIDTH - TOAST_MARGIN;
            y_pos = viewport->Pos.y + TOAST_MARGIN;
            stack_downward = true;
        }

        float current_y = y_pos;

        for (auto it = toasts_.begin(); it != toasts_.end();) {
            it->lifetime -= dt;

            if (it->lifetime <= 0) {
                it = toasts_.erase(it);
                continue;
            }

            // Calculate alpha for fade effect
            float alpha = 1.0f;
            if (it->lifetime < 0.5f) {
                alpha = it->lifetime / 0.5f;
            } else if (it->lifetime > it->initial_lifetime - 0.3f) {
                alpha = (it->initial_lifetime - it->lifetime) / 0.3f;
            }

            // Position window
            ImGui::SetNextWindowPos({x_pos, current_y});
            ImGui::SetNextWindowSize({TOAST_WIDTH, 0});
            ImGui::SetNextWindowBgAlpha(alpha * 0.95f);

            // Style based on severity
            ImGui::PushStyleColor(ImGuiCol_WindowBg,
                                  IM_COL32(30, 30, 33, static_cast<int>(255 * alpha * 0.95f)));
            ImGui::PushStyleColor(ImGuiCol_Border, it->color);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 4.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 2.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 8));

            char window_id[64];
            snprintf(window_id, sizeof(window_id), "##Toast_%p", static_cast<void*>(&*it));

            if (ImGui::Begin(window_id, nullptr,
                             ImGuiWindowFlags_NoDecoration |
                                 ImGuiWindowFlags_AlwaysAutoResize |
                                 ImGuiWindowFlags_NoSavedSettings |
                                 ImGuiWindowFlags_NoFocusOnAppearing |
                                 ImGuiWindowFlags_NoNav |
                                 ImGuiWindowFlags_NoDocking |
                                 ImGuiWindowFlags_NoMove)) {

                // Header with icon and type
                ImGui::TextColored(ImColor(it->color), "%s %s",
                                   severity_to_icon(it->severity),
                                   it->type.c_str());

                // Message
                ImGui::TextWrapped("%s", it->message.c_str());

                // Location in smaller, dimmed text
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(150, 150, 150, 255));
                ImGui::Text("  %s", it->location.c_str());
                ImGui::PopStyleColor();

                // Get window height for stacking
                float window_height = ImGui::GetWindowHeight();
                if (stack_downward) {
                    current_y += window_height + TOAST_SPACING;
                } else {
                    current_y -= window_height + TOAST_SPACING;
                }
            }
            ImGui::End();

            ImGui::PopStyleVar(3);
            ImGui::PopStyleColor(2);

            ++it;
        }
    }

    void ExceptionVisualizer::render_modal() {
        if (ImGui::BeginPopupModal("##CriticalError", nullptr,
                                   ImGuiWindowFlags_AlwaysAutoResize)) {

            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 100, 100, 255));
            ImGui::Text("⚠ CRITICAL ERROR");
            ImGui::PopStyleColor();

            ImGui::Separator();
            ImGui::Spacing();

            if (modal_error_) {
                // Error message
                ImGui::TextWrapped("%s", modal_error_->message.c_str());

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                // Details in monospace font
                ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]); // Assuming monospace is first

                ImGui::Text("Type:     %s", modal_error_->type_name.c_str());
                ImGui::Text("Location: %s:%d",
                            std::filesystem::path(modal_error_->location.file_name()).filename().c_str(),
                            modal_error_->location.line());
                ImGui::Text("Function: %s", modal_error_->location.function_name());

                ImGui::PopFont();
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // Action buttons
            if (ImGui::Button("Copy Details", ImVec2(120, 0))) {
                if (modal_error_) {
                    auto details = std::format(
                        "Critical Error: {}\nType: {}\nLocation: {}:{}\nFunction: {}",
                        modal_error_->message,
                        modal_error_->type_name,
                        modal_error_->location.file_name(),
                        modal_error_->location.line(),
                        modal_error_->location.function_name());
                    ImGui::SetClipboardText(details.c_str());
                }
            }

            ImGui::SameLine();

            if (ImGui::Button("View Logs", ImVec2(120, 0))) {
                // Could open log viewer here
                LOG_INFO("User requested to view logs from critical error dialog");
                ImGui::CloseCurrentPopup();
                modal_error_.reset();
            }

            ImGui::SameLine();

            if (ImGui::Button("Continue", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
                modal_error_.reset();
            }

            ImGui::EndPopup();
        }
    }

    void ExceptionVisualizer::render_statistics_window() {
        if (ImGui::Begin("Exception Statistics", &show_statistics_)) {
            ImGui::Text("Total Errors:   %zu", error_count_.load());
            ImGui::Text("Total Warnings: %zu", warning_count_.load());
            ImGui::Text("Total Info:     %zu", info_count_.load());

            ImGui::Separator();

            ImGui::Text("Active Toasts: %zu", toasts_.size());

            if (ImGui::Button("Clear All")) {
                clear();
            }

            ImGui::End();
        }
    }

    ImU32 ExceptionVisualizer::severity_to_color(core::Severity sev) const {
        switch (sev) {
        case core::LogLevel::Critical:
            return IM_COL32(255, 50, 50, 255);
        case core::LogLevel::Error:
            return IM_COL32(220, 80, 80, 255);
        case core::LogLevel::Warn:
            return IM_COL32(220, 180, 50, 255);
        case core::LogLevel::Info:
            return IM_COL32(100, 180, 220, 255);
        case core::LogLevel::Debug:
            return IM_COL32(150, 150, 220, 255);
        case core::LogLevel::Trace:
            return IM_COL32(180, 180, 180, 255);
        default:
            return IM_COL32(200, 200, 200, 255);
        }
    }

    const char* ExceptionVisualizer::severity_to_icon(core::Severity sev) const {
        switch (sev) {
        case core::LogLevel::Critical:
            return "💀";
        case core::LogLevel::Error:
            return "❌";
        case core::LogLevel::Warn:
            return "⚠️";
        case core::LogLevel::Info:
            return "ℹ️";
        case core::LogLevel::Debug:
            return "🔧";
        case core::LogLevel::Trace:
            return "📝";
        default:
            return "•";
        }
    }

} // namespace gs::visualizer
