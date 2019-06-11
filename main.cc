#include "./src/logger.hpp"

#include <iostream>
#include <vector>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#define wait(s) Sleep(s)
#elif __linux__ 
#include <unistd.h>
#define wait(s) usleep((s)*1000)
#else
#define wait(s) (void)(s)
#endif

void workerMain(int id, int num = 100) {
    LogLevel::Level level = LogLevel::Level(id%5);
    for (int i = 0; i < num; i++) {
        log(level, "thread id: %d, id: %d, cnt: %d", std::this_thread::get_id(), id, i);
        wait(5);
    }
}

int main() {
    Logger::gInstance = new Logger(LogLevel::Level::Notice);
    Logger::gInstance->setLogFile("server.log"); //log rotate: 1 day, 16 mb
    Logger::gInstance->start();

    std::vector<std::thread*> vec;
    for (int i = 0; i < 50; i++) {
        vec.push_back(new std::thread([=]() {
            workerMain(i);
        }));
    }

    for (auto&& thread : vec) {
        thread->join();
        delete thread;
    }

    wait(3000);
    Logger::gInstance->stop();
    return 0;
}