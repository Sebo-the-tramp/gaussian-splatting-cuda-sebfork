#pragma once
#include "core/exception_handler.hpp"
#include <deque>
#include <filesystem>
#include <imgui.h>
#include <mutex>
#include <optional>

namespace gs::visualizer {

    class ExceptionVisualizer {
    public:
        ExceptionVisualizer();
        ~ExceptionVisualizer();

        // Non-copyable, moveable
        ExceptionVisualizer(const ExceptionVisualizer&) = delete;
        ExceptionVisualizer& operator=(const ExceptionVisualizer&) = delete;
        ExceptionVisualizer(ExceptionVisualizer&&) = default;
        ExceptionVisualizer& operator=(ExceptionVisualizer&&) = default;

        void render(float dt);
        void set_enabled(bool enabled) { enabled_ = enabled; }
        bool is_enabled() const { return enabled_; }

        // Configuration
        void set_max_toasts(size_t max) { max_toasts_ = max; }
        void set_toast_lifetime(float seconds) { toast_lifetime_ = seconds; }
        void set_position_corner(int corner) { position_corner_ = corner; } // 0=TR, 1=TL, 2=BR, 3=BL

        // Clear all notifications
        void clear() {
            std::lock_guard lock(mutex_);
            toasts_.clear();
            modal_error_.reset();
        }

        // Get statistics
        size_t get_error_count() const { return error_count_; }
        size_t get_warning_count() const { return warning_count_; }

    private:
        struct Toast {
            std::string message;
            std::string location;
            std::string type;
            float lifetime;
            float initial_lifetime;
            ImU32 color;
            core::Severity severity;
        };

        void on_exception(const core::ExceptionEvent& event);
        void render_toasts(float dt);
        void render_modal();
        void render_statistics_window();

        ImU32 severity_to_color(core::Severity sev) const;
        const char* severity_to_icon(core::Severity sev) const;

        // Thread-safe toast queue
        std::deque<Toast> toasts_;
        mutable std::mutex mutex_;

        // Modal for critical errors
        std::optional<core::ExceptionEvent> modal_error_;

        // Configuration
        std::atomic<bool> enabled_{true};
        std::atomic<size_t> max_toasts_{5};
        std::atomic<float> toast_lifetime_{4.0f};
        std::atomic<int> position_corner_{0}; // Top-right by default

        // Statistics
        std::atomic<size_t> error_count_{0};
        std::atomic<size_t> warning_count_{0};
        std::atomic<size_t> info_count_{0};

        // UI state
        bool show_statistics_{false};
        static constexpr float TOAST_WIDTH = 320.0f;
        static constexpr float TOAST_SPACING = 10.0f;
        static constexpr float TOAST_MARGIN = 20.0f;
    };

} // namespace gs::visualizer
