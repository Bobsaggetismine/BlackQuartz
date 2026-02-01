#pragma once

#include <chrono>
#include <cstdio>
#include <format>
#include <fstream>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <print>

#if defined(PLATFORM_WINDOWS)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#elif defined(PLATFORM_LINUX)
#include <unistd.h>
#endif

namespace bq {

    enum class LogLevel : int { trace = 0, debug, info, warn, critical, off };

    class Logger {
    public:
        // Runtime level filter (optional but handy)
        static void setLevel(LogLevel lvl) { level() = lvl; }
        static LogLevel getLevel() { return level(); }

        static void logToFile(const std::string& file, bool clear_file = false) {
            std::scoped_lock lk(mu());

            if (ofs().is_open())
                ofs().close();

            std::ios::openmode mode = std::ios::out;
            mode |= (clear_file ? std::ios::trunc : std::ios::app);

            ofs().open(file, mode);
            file_enabled() = ofs().is_open();
        }
        static void stopConsoleLogging() {
            console_enabled() = false;
        }
        static void stopFileLogging() {
            std::scoped_lock lk(mu());
            if (ofs().is_open()) ofs().close();
            file_enabled() = false;
        }

        template <class... Args>
        static void Info(std::format_string<Args...> fmt, Args&&... args) {
            Log(LogLevel::info, "INFO", color::info, fmt, std::forward<Args>(args)...);
        }

        template <class... Args>
        static void Warn(std::format_string<Args...> fmt, Args&&... args) {
            Log(LogLevel::warn, "WARN", color::warn, fmt, std::forward<Args>(args)...);
        }

        template <class... Args>
        static void Critical(std::format_string<Args...> fmt, Args&&... args) {
            Log(LogLevel::critical, "CRITICAL", color::critical, fmt, std::forward<Args>(args)...);
        }

        template <class... Args>
        static void Debug(std::format_string<Args...> fmt, Args&&... args) {
#if !defined(NDEBUG)
            Log(LogLevel::debug, "DEBUG", color::debug, fmt, std::forward<Args>(args)...);
#else
            (void)fmt; (void)sizeof...(args);
#endif
        }

        template <class... Args>
        static void Trace(std::format_string<Args...> fmt, Args&&... args) {
#if !defined(NDEBUG)
            Log(LogLevel::trace, "TRACE", color::trace, fmt, std::forward<Args>(args)...);
#else
            (void)fmt; (void)sizeof...(args);
#endif
        }

    private:
        struct color {
#if defined(PLATFORM_WINDOWS)
            static constexpr int trace = 8;
            static constexpr int debug = 11;
            static constexpr int info = 10;
            static constexpr int warn = 14;
            static constexpr int critical = 12;
            static constexpr int normal = 7;
#else
            static constexpr int trace = 90;
            static constexpr int debug = 36;
            static constexpr int info = 32;
            static constexpr int warn = 33;
            static constexpr int critical = 31;
            static constexpr int normal = 0;
#endif
        };

        static inline std::mutex& mu() {
            static std::mutex m;
            return m;
        }
        static inline std::ofstream& ofs() {
            static std::ofstream f;
            return f;
        }
        static inline bool& console_enabled() {
            static bool c = true;
            return c;
        }
        static inline bool& file_enabled() {
            static bool b = false;
            return b;
        }
        static inline LogLevel& level() {
            static LogLevel lvl = LogLevel::info;
            return lvl;
        }

        static std::string timestamp_now() {
            using namespace std::chrono;
            auto now = system_clock::now();
            auto t = system_clock::to_time_t(now);

            std::tm tm{};
#if defined(PLATFORM_WINDOWS)
            localtime_s(&tm, &t);
#else
            localtime_r(&t, &tm);
#endif

            auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
            return std::format("{:02}:{:02}:{:02}.{:03}",
                tm.tm_hour, tm.tm_min, tm.tm_sec, (int)ms.count());
        }

        static bool stderr_is_tty() {
#if defined(PLATFORM_LINUX)
            return ::isatty(::fileno(stderr));
#else
            return true;
#endif
        }

        static void set_console_color(int c) {
#if defined(PLATFORM_WINDOWS)
            HANDLE h = GetStdHandle(STD_ERROR_HANDLE);
            SetConsoleTextAttribute(h, (WORD)c);
#elif defined(PLATFORM_LINUX)
            if (!stderr_is_tty()) return;
            std::fputs(std::format("\033[{}m", c).c_str(), stderr);
#else
            (void)c;
#endif
        }

        static void reset_console_color() {
#if defined(PLATFORM_WINDOWS)
            set_console_color(color::normal);
#elif defined(PLATFORM_LINUX)
            if (!stderr_is_tty()) return;
            std::fputs("\033[0m", stderr);
#endif
        }

        template <class... Args>
        static void Log(LogLevel msg_level,
            std::string_view tag,
            int col,
            std::format_string<Args...> fmt,
            Args&&... args)
        {
            if (level() == LogLevel::off || msg_level < level())
                return;

            std::scoped_lock lk(mu());

            const auto ts = timestamp_now();
            const auto tid = std::hash<std::thread::id>{}(std::this_thread::get_id());
            const std::string body = std::format(fmt, std::forward<Args>(args)...);
            if (console_enabled()) {
                set_console_color(col);
                std::println(stderr, "[{}][{}][{:x}] {}", ts, tag, tid, body);
                reset_console_color();
            }

            // File
            if (file_enabled() && ofs().is_open()) {
                ofs() << '[' << ts << "][" << tag << "][" << std::hex << tid << std::dec << "] "
                    << body << '\n';
                ofs().flush();
            }
        }
    };

}
