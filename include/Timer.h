#ifndef TIMER_H
#define TIMER_H
#include <thread>
#include <condition_variable>
namespace core {
    class Rate {
    public:
        Rate(float freq) {
            sleep_us = 1e6 / freq;
        }
        bool sleep() {
            bool ret = true;
            if (desired < std::chrono::steady_clock::now()) {
                desired = std::chrono::steady_clock::now() + std::chrono::microseconds(sleep_us);;
                ret = false;
            }
            std::this_thread::sleep_until(desired);
            desired += std::chrono::microseconds(sleep_us);
            return ret;
        }
    private:
        int sleep_us;
        std::chrono::time_point<std::chrono::steady_clock> desired;
    };
}

#endif