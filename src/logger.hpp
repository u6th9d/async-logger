#ifndef __LOGGER_HPP__
#define __LOGGER_HPP__

#include <cstdarg>
#include <ctime>
#include <cstdio>

#include "logfile.hpp"

#include <vector>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <chrono>
#include <iostream>

struct LogLevel {
    enum Level {
        Debug,
        Info,
        Notice,
        Warning,
        Error
    };
    const static char* Prefix[];
};

const char* LogLevel::Prefix[] = { "D", "I", "N", "W", "E" };


struct LogUnit {
    LogUnit() : mLength(0) {
    }

    ~LogUnit() {
        this->mLength = 0;
    }

    void format(LogLevel::Level level, const char* filename, int line, const char* fmt, va_list ap) {
        long long us = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        time_t now = us / 1000000;
        struct tm m;
        localtime_r(&now, &m);
        char* p = this->mBuffer;
        int len = kMaxLength;
        int n = strftime(p, len, "%Y-%m-%d %H:%M:%S", &m);
        p += n;
        this->mLength = n;
        len -= n;
        n = snprintf(p, len, ".%06lld %s %s:%d ", us % 1000000, LogLevel::Prefix[level], filename, line);
        p += n;
        this->mLength += n;
        len -= n;
        n = vsnprintf(p, len, fmt, ap);
        this->mLength += n;
        if (this->mLength >= kMaxLength) {
            this->mLength = kMaxLength - 1;
        }
        this->mBuffer[this->mLength++] = '\n';
    }

    int length() {
        return this->mLength;
    }

    const char* str() const {
        return this->mBuffer;
    }

private:
    const static int kMaxLength = 2048;
    int mLength;
    char mBuffer[kMaxLength];
};


class Logger {
public:
    Logger(LogLevel::Level level, int maxCnt = 50):
        mThread(nullptr), mLogFile(nullptr), mLevel(level), mLogUnitCnt(0), mStopASAP(false), mTask(maxCnt), mFree(maxCnt) {
        this->mTask.resize(0);
        this->mFree.resize(0);
    }

    ~Logger() {
        if (this->mThread) {
            delete this->mThread;
        }
        if (this->mLogFile) {
            delete this->mLogFile;
        }
    }

    void start() {
        this->mThread = new std::thread([=]() { this->loggerMain(); });
        this->mThread->detach();
    }

    void stop() {
        this->mStopASAP = true;
        std::unique_lock<std::mutex> guard(this->mLockTask);
        this->mCondTask.notify_one();
    }

    void setLogFile(const char* filename) {
        if (!this->mLogFile) {
            this->mLogFile = new LogFile(filename);
        }
    }

    void loggerMain() {
        std::vector<LogUnit*> task(this->mTask.capacity());
        task.resize(0);
        while (!this->mStopASAP) {
            do {
                std::unique_lock<std::mutex> guard(this->mLockTask);
                while (this->mTask.empty() && !this->mStopASAP) {
                    this->mCondTask.wait(guard);
                }
                task.swap(this->mTask);
            } while (false);
            for (auto unit : task) {
                this->mLogFile->write(unit->str(), unit->length());
                std::unique_lock<std::mutex> guard(this->mLockFree);
                this->mFree.push_back(unit);
                this->mCondFree.notify_one();
            }
            task.resize(0);
        }
    }

    void push_unit(LogUnit* unit) {
        std::unique_lock<std::mutex> guard(this->mLockTask);
        this->mTask.push_back(unit);
        this->mCondTask.notify_one();
    }

    LogUnit* pop_unit(LogLevel::Level level) {
        if (level < this->mLevel) {
            return nullptr;
        }
        LogUnit* unit = nullptr;
        std::unique_lock<std::mutex> guard(this->mLockFree);
        while (!this->mStopASAP) {
            if (this->mFree.size() > 0) {
                unit = this->mFree.back();
                this->mFree.pop_back();
                break;
            } else if (this->mLogUnitCnt < this->mFree.capacity()) {
                unit = new LogUnit();
                this->mLogUnitCnt++;
                break;
            } else {
                while (this->mFree.empty() && !this->mStopASAP) {
                    this->mCondFree.wait(guard);
                }
            }
        }
        return unit;
    }

    static Logger* gInstance;
private:
    std::thread* mThread;
    LogFile* mLogFile;
    LogLevel::Level mLevel;
    int mLogUnitCnt;
    bool mStopASAP;
    std::mutex mLockTask;
    std::condition_variable mCondTask;
    std::vector<LogUnit*> mTask;
    std::mutex mLockFree;
    std::condition_variable mCondFree;
    std::vector<LogUnit*> mFree;
};

Logger* Logger::gInstance = nullptr;


/* logger interface */
void logRaw(LogLevel::Level level, const char* filename, int line, const char* fmt, ...) {
    LogUnit* unit = Logger::gInstance->pop_unit(level);
    if (unit != nullptr) {
        va_list ap;
        va_start(ap, fmt);
        unit->format(level, filename, line, fmt, ap);
        va_end(ap);
        Logger::gInstance->push_unit(unit);
    }
    // write to syslog / udp...
}

#define logDebug(fmt, ...)   logRaw(LogLevel::Level::Debug,   __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define logInfo(fmt, ...)    logRaw(LogLevel::Level::Info,    __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define logNotice(fmt, ...)  logRaw(LogLevel::Level::Notice,  __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define logWarning(fmt, ...) logRaw(LogLevel::Level::Warning, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define logError(fmt, ...)   logRaw(LogLevel::Level::Error,   __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#define log(level, fmt, ...) logRaw(level,                    __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#endif
