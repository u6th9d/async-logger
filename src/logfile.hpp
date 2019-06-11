#ifndef __LOGFILE_HPP__
#define __LOGFILE_HPP__

#include <cstdio>
#include <cstring>
#include <ctime>

#include <string>

class LogFile {
public:
    LogFile(const char* filename, int rotateDay = 1, int rotateSize = 16) :
        mFilename(filename), mTotalBytes(0), mRotateCnt(0), mRotateSecs(rotateDay * 24 * 3600), mRotateBytes(rotateSize * 1024 * 1024) {
        std::remove(filename);
        this->mFile = std::fopen(filename, "a");
        time_t now = std::time(nullptr);
        struct tm m;
        localtime_r(&now, &m);
        m.tm_hour = m.tm_min = m.tm_sec = 0;
        this->mLastRotate = std::mktime(&m);
    }

    ~LogFile() {
        std::fclose(this->mFile);
    }

    void rotate(time_t now, int id) {
        char name[1024] = {0};
        char date[128] = {0};
        struct tm m;
        localtime_r(&now, &m);
        strftime(date, 128, "%Y%m%d", &m);
        sprintf(name, "%s.%s.%d", this->mFilename.c_str(), date, id);
        std::fclose(this->mFile);
        std::rename(this->mFilename.c_str(), name);
        this->mFile = std::fopen(this->mFilename.c_str(), "a");
    }

    void checkRotate() {
        time_t now = std::time(nullptr);
        if (now > this->mLastRotate + this->mRotateSecs) {
            this->rotate(now - 24 * 3600, this->mRotateCnt++);
            this->mTotalBytes = 0;
            this->mRotateCnt = 0;
            struct tm m;
            localtime_r(&now, &m);
            m.tm_hour = m.tm_min = m.tm_sec = 0;
            this->mLastRotate = std::mktime(&m);
        } else if (this->mTotalBytes >= this->mRotateBytes) {
            this->rotate(now, this->mRotateCnt++);
            this->mTotalBytes = 0;
        }
    }

    void write(const char* str, int length) {
        this->checkRotate();
        if (this->mFile) {
            if (std::fwrite(str, length, 1, this->mFile) == 1) {
                this->mTotalBytes += length;
                std::fflush(this->mFile);
            }
        }
    }
private:
    FILE* mFile;
    std::string mFilename;
    time_t mLastRotate;
    size_t mTotalBytes;
    int mRotateCnt;
    unsigned long mRotateSecs;
    unsigned long mRotateBytes;
};

#endif