#pragma once
#include "logger.hpp"
#include <expected>
#include <functional>
#include <mutex>
#include <source_location>
#include <vector>

namespace gs::core {

    // Reuse LogLevel from logger, but add alias for clarity
    using Severity = LogLevel;

    struct ExceptionEvent {
        std::string message;
        Severity severity;
        std::source_location location;
        std::chrono::steady_clock::time_point timestamp;
        std::string type_name; // e.g., "std::runtime_error"
    };

    class ExceptionHandler {
    public:
        using Observer = std::function<void(const ExceptionEvent&)>;

        static ExceptionHandler& get() {
            static ExceptionHandler instance;
            return instance;
        }

        // Wrap any callable with exception handling
        template <typename Func>
        auto wrap(Func&& f, std::source_location loc = std::source_location::current()) {
            return [=, this]<typename... Args>(Args&&... args) -> decltype(auto) {
                try {
                    return f(std::forward<Args>(args)...);
                } catch (const std::filesystem::filesystem_error& e) {
                    handle_exception(e.what(), Severity::Error, "filesystem_error", loc);
                    if constexpr (!std::is_void_v<decltype(f(std::forward<Args>(args)...))>)
                        return {};
                } catch (const std::system_error& e) {
                    handle_exception(e.what(), Severity::Error, "system_error", loc);
                    if constexpr (!std::is_void_v<decltype(f(std::forward<Args>(args)...))>)
                        return {};
                } catch (const std::runtime_error& e) {
                    handle_exception(e.what(), Severity::Error, "runtime_error", loc);
                    if constexpr (!std::is_void_v<decltype(f(std::forward<Args>(args)...))>)
                        return {};
                } catch (const std::exception& e) {
                    handle_exception(e.what(), Severity::Error, "exception", loc);
                    if constexpr (!std::is_void_v<decltype(f(std::forward<Args>(args)...))>)
                        return {};
                } catch (...) {
                    handle_exception("Unknown exception", Severity::Critical, "unknown", loc);
                    if constexpr (!std::is_void_v<decltype(f(std::forward<Args>(args)...))>)
                        return {};
                }
            };
        }

        // For std::expected integration
        template <typename T, typename E>
        void handle_expected(const std::expected<T, E>& result,
                             std::source_location loc = std::source_location::current()) {
            if (!result) {
                if constexpr (std::is_convertible_v<E, std::string>) {
                    report(result.error(), Severity::Warn, loc);
                } else {
                    report("Operation failed", Severity::Warn, loc);
                }
            }
        }

        // Manual error reporting
        void report(std::string_view msg,
                    Severity sev = Severity::Error,
                    std::source_location loc = std::source_location::current()) {
            handle_exception(msg, sev, "manual_report", loc);
        }

        // Observer pattern for GUI
        void add_observer(Observer obs) {
            std::lock_guard lock(observer_mutex_);
            observers_.push_back(std::move(obs));
        }

        void remove_observers() {
            std::lock_guard lock(observer_mutex_);
            observers_.clear();
        }

        // Configuration
        void set_throw_on_critical(bool enabled) {
            throw_on_critical_ = enabled;
        }

    private:
        ExceptionHandler() = default;

        void handle_exception(std::string_view msg,
                              Severity sev,
                              std::string_view type,
                              const std::source_location& loc);

        std::vector<Observer> observers_;
        std::mutex observer_mutex_;
        std::atomic<bool> throw_on_critical_{false};
    };

// Convenience macros that integrate with your logging system
#define GS_SAFE_CALL(func) \
    ::gs::core::ExceptionHandler::get().wrap([&] { return func; })()

#define GS_REPORT_ERROR(msg) \
    ::gs::core::ExceptionHandler::get().report(msg, ::gs::core::LogLevel::Error)

#define GS_REPORT_WARNING(msg) \
    ::gs::core::ExceptionHandler::get().report(msg, ::gs::core::LogLevel::Warn)

#define GS_REPORT_CRITICAL(msg) \
    ::gs::core::ExceptionHandler::get().report(msg, ::gs::core::LogLevel::Critical)

} // namespace gs::core
