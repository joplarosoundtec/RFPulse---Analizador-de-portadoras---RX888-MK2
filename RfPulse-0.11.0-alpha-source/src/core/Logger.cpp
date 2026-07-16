#include "Logger.h"

#include <chrono>
#include <cstdio>
#include <ctime>
#include <windows.h>

namespace rfpulse::core {

namespace {

const char* levelName(LogLevel level)
{
    switch (level) {
        case LogLevel::Debug:
            return "DEBUG";
        case LogLevel::Info:
            return "INFO";
        case LogLevel::Warning:
            return "WARNING";
        case LogLevel::Error:
            return "ERROR";
    }
    return "?";
}

} // namespace

Logger::Logger(std::string filePath)
    : filePath_(std::move(filePath))
{
    writerThread_ = std::thread([this]() { writerThreadMain(); });
}

Logger::~Logger()
{
    running_.store(false, std::memory_order_release);
    cv_.notify_all();
    if (writerThread_.joinable()) {
        writerThread_.join();
    }
}

void Logger::log(LogLevel level, const std::string& message)
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.emplace_back(level, message);
    }
    cv_.notify_one();
}

void Logger::writerThreadMain()
{
    FILE* file = nullptr;
    fopen_s(&file, filePath_.c_str(), "a");

    while (true) {
        std::deque<std::pair<LogLevel, std::string>> pending;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [this]() { return !queue_.empty() || !running_.load(std::memory_order_acquire); });
            pending.swap(queue_);
        }
        if (pending.empty() && !running_.load(std::memory_order_acquire)) {
            break;
        }

        for (const auto& [level, message] : pending) {
            const auto now = std::chrono::system_clock::now();
            const auto timeT = std::chrono::system_clock::to_time_t(now);
            std::tm tmBuf{};
            localtime_s(&tmBuf, &timeT);
            char timestamp[32];
            std::strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &tmBuf);

            if (file != nullptr) {
                fprintf(file, "[%s] [%s] %s\n", timestamp, levelName(level), message.c_str());
            }
            OutputDebugStringA(("[" + std::string(levelName(level)) + "] " + message + "\n").c_str());
        }
        if (file != nullptr) {
            fflush(file);
        }
    }

    if (file != nullptr) {
        fclose(file);
    }
}

} // namespace rfpulse::core
