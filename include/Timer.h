#ifndef TIMER_H
#define TIMER_H
#include <thread>
#include <condition_variable>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
namespace core {
    #ifndef SIMULATION
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
    #else

    class Ticker {
        public:
            Ticker() {
                const char* shared_memory_name = "SIMULATION_TIME_US";
                size_t bufSize = sizeof(uint64_t);
                fd = shm_open(
                shared_memory_name,
                O_CREAT | O_RDWR,
                0777);
                if(fd < 0) printf("shm_open failed!, %s\n", strerror(errno));
                int r = ftruncate(fd, bufSize);
                if(r < 0) printf("ftruncate failed!, %s\n", strerror(errno));
                buf = mmap(
                NULL,
                sizeof(uint64_t),
                PROT_WRITE,
                MAP_SHARED,
                fd,
                0);
                if(buf == MAP_FAILED) printf("mmap failed!, %s\n", strerror(errno));
            }
            void tick(uint64_t current_time_us) {
                uint64_t* time = ((uint64_t*)buf);
                *time = current_time_us;
            }
            ~Ticker() {
                const char* shared_memory_name = "SIMULATION_TIME_US";
                if(munmap(buf, sizeof(uint64_t)) != 0) printf("munmap failed!, %s\n", strerror(errno));
                if(shm_unlink(shared_memory_name) != 0) printf("shm_unlink failed!, %s\n", strerror(errno));
            }
        private:
            int fd;
            void *buf;
    };
    class Rate {
        public:
            Rate(float freq) {
                sleep_us = 1e6 / freq;
                const char* shared_memory_name = "SIMULATION_TIME_US";
                size_t bufSize = sizeof(uint64_t);
                fd = shm_open(
                shared_memory_name,
                O_RDWR,
                0777);
                if(fd < 0) printf("shm_open failed!, %s\n", strerror(errno));
                buf = mmap(
                NULL,
                sizeof(uint64_t),
                PROT_READ,
                MAP_SHARED,
                fd,
                0);
                if(buf == MAP_FAILED) printf("mmap failed!, %s\n", strerror(errno));
            }
            bool sleep() {
                uint64_t* time = ((uint64_t*)buf);
                while (current > *time) {
                    usleep(1000);
                    time = ((uint64_t*)buf);
                }
                current += sleep_us;
                return true;
            }
        private:
            uint64_t current = 0;
            int sleep_us;
            int fd;
            void *buf;
    };

    #endif
}

#endif