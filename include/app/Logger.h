/**
 * @file Logger.h
 * @brief 轻量级日志系统 - 按小时生成日志文件，纯C++单头文件实现
 * 
 * 使用方法：
 *     #include "app/Logger.h"
 *     
 *     int main()
 *     {
 *         Logger::init("logs");  // 指定日志目录
 *         // 或 Logger::init();  // 默认保存到程序所在目录
 *         
 *         LOG("程序启动");
 *         LOG("用户登录: %s", username);
 *         LOG("变量值: x=%d, y=%d", x, y);
 *         
 *         return 0;
 *     }
 * 
 * 日志文件：
 *     logs/2026-02-04_16.txt
 *     logs/2026-02-04_17.txt
 * 
 * 日志格式：
 *     [16:30:25.123] [main.cpp:42] 程序启动
 */

#ifndef LOGGER_H
#define LOGGER_H

#include <string>
#include <fstream>
#include <sstream>
#include <mutex>
#include <ctime>
#include <cstdarg>
#include <cstdio>
#include <chrono>
#include <filesystem>
#include <iomanip>

#ifdef _WIN32
#include <Windows.h>
#include <direct.h>
#define MKDIR(dir) _mkdir(dir)
#else
#include <sys/stat.h>
#define MKDIR(dir) mkdir(dir, 0755)
#endif

class Logger
{
public:
    /**
     * @brief 初始化日志系统
     * @param logDir 日志目录，默认为程序所在目录
     */
    static void init(const std::string& logDir = "")
    {
        auto& inst = instance();
        std::lock_guard<std::mutex> lock(inst.m_mutex);
        
        if (logDir.empty()) {
            inst.m_logDir = getModulePath();
        } else {
            inst.m_logDir = logDir;
            MKDIR(logDir.c_str());
        }
        inst.m_initialized = true;
        cleanupExpiredLogs(inst.m_logDir);
    }

    /**
     * @brief 记录日志
     */
    static void log(const char* file, int line, const char* fmt, ...)
    {
        auto& inst = instance();
        
        if (!inst.m_initialized) {
            return;
        }

        // 格式化用户消息
        char buffer[4096];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buffer, sizeof(buffer), fmt, args);
        va_end(args);

        // 获取当前小时
        std::string currentHour = getCurrentHour();

        std::lock_guard<std::mutex> lock(inst.m_mutex);

        // 检查是否需要切换日志文件（新的整点小时）
        if (currentHour != inst.m_currentHour) {
            if (inst.m_file.is_open()) {
                inst.m_file.close();
            }
            inst.m_currentHour = currentHour;
            cleanupExpiredLogs(inst.m_logDir);

            std::string filename = inst.m_logDir + "/" + currentHour + ".txt";
            inst.m_file.open(filename, std::ios::out | std::ios::app);
        }

        // 写入日志
        if (inst.m_file.is_open()) {
            std::string message = formatMessage(file, line, buffer);
            inst.m_file << message << std::endl;
            inst.m_file.flush();
        }
    }

    /**
     * @brief 关闭日志
     */
    static void close()
    {
        auto& inst = instance();
        std::lock_guard<std::mutex> lock(inst.m_mutex);
        if (inst.m_file.is_open()) {
            inst.m_file.close();
        }
    }

private:
    std::string m_logDir;
    std::string m_currentHour;
    std::ofstream m_file;
    std::mutex m_mutex;
    bool m_initialized = false;

    Logger() = default;
    ~Logger() { close(); }
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    static Logger& instance()
    {
        static Logger logger;
        return logger;
    }

    static std::string getModulePath()
    {
#ifdef _WIN32
        char path[MAX_PATH] = { 0 };
        GetModuleFileNameA(nullptr, path, MAX_PATH);
        std::string fullPath(path);
        size_t pos = fullPath.find_last_of("\\/");
        return (pos != std::string::npos) ? fullPath.substr(0, pos) : fullPath;
#else
        return ".";
#endif
    }

    static std::string getCurrentHour()
    {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::tm localTime;
#ifdef _WIN32
        localtime_s(&localTime, &time);
#else
        localtime_r(&time, &localTime);
#endif
        std::ostringstream oss;
        oss << std::put_time(&localTime, "%Y-%m-%d_%H");
        return oss.str();
    }

    static bool isDigits(const std::string& value, std::size_t offset, std::size_t count)
    {
        if (offset + count > value.size()) {
            return false;
        }
        for (std::size_t i = offset; i < offset + count; ++i) {
            if (value[i] < '0' || value[i] > '9') {
                return false;
            }
        }
        return true;
    }

    static bool isManagedLogFileName(const std::string& name)
    {
        // 兼容清理旧的 yyyy-MM-dd.txt，以及新的 yyyy-MM-dd_HH.txt。
        const bool commonDate =
            isDigits(name, 0, 4) && name.size() >= 10 && name[4] == '-' &&
            isDigits(name, 5, 2) && name[7] == '-' && isDigits(name, 8, 2);
        if (!commonDate) {
            return false;
        }
        if (name.size() == 14) {
            return name.compare(10, 4, ".txt") == 0;
        }
        if (name.size() == 17) {
            return name[10] == '_' && isDigits(name, 11, 2) &&
                   name.compare(13, 4, ".txt") == 0;
        }
        return false;
    }

    static void cleanupExpiredLogs(const std::string& logDir)
    {
        namespace fs = std::filesystem;
        constexpr auto retention = std::chrono::hours(7 * 24);
        const auto cutoff = fs::file_time_type::clock::now() - retention;

        std::error_code ec;
        const fs::path directory(logDir);
        fs::directory_iterator it(directory, ec);
        const fs::directory_iterator end;
        while (!ec && it != end) {
            const fs::directory_entry entry = *it;
            it.increment(ec);

            const std::string name = entry.path().filename().string();
            if (!isManagedLogFileName(name)) {
                continue;
            }

            std::error_code itemError;
            if (!entry.is_regular_file(itemError) || itemError) {
                continue;
            }
            const auto modified = entry.last_write_time(itemError);
            if (!itemError && modified < cutoff) {
                fs::remove(entry.path(), itemError);
            }
        }
    }

    static std::string getCurrentTime()
    {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;
        
        std::tm localTime;
#ifdef _WIN32
        localtime_s(&localTime, &time);
#else
        localtime_r(&time, &localTime);
#endif
        std::ostringstream oss;
        oss << std::put_time(&localTime, "%H:%M:%S")
            << '.' << std::setfill('0') << std::setw(3) << ms.count();
        return oss.str();
    }

    static std::string extractFileName(const char* path)
    {
        std::string fullPath(path);
        size_t pos = fullPath.find_last_of("/\\");
        return (pos != std::string::npos) ? fullPath.substr(pos + 1) : fullPath;
    }

    static std::string formatMessage(const char* file, int line, const char* msg)
    {
        std::ostringstream oss;
        oss << "[" << getCurrentTime() << "] "
            << "[" << extractFileName(file) << ":" << line << "] "
            << msg;
        return oss.str();
    }
};

// 便捷宏
#define LOG(fmt, ...) Logger::log(__FILE__, __LINE__, fmt, ##__VA_ARGS__)

#endif // LOGGER_H
