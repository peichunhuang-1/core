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
    #include <iostream>
    #include <sys/types.h>
    #include <sys/ipc.h>
    #include <sys/shm.h>
    #include <sys/mman.h>
    #include <semaphore.h>
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
                sem = sem_open("/SIMULATION_SEM", O_CREAT, 0777, 0);
                if(sem == SEM_FAILED) {
                    std::cerr << "sem_open failed!, " << strerror(errno) << std::endl;
                }
            }
            void tick(uint64_t current_time_us) {
                uint64_t* time = ((uint64_t*)buf);
                *time = current_time_us;
                sem_post(sem);
            }
            ~Ticker() {
                const char* shared_memory_name = "SIMULATION_TIME_US";
                if(munmap(buf, sizeof(uint64_t)) != 0) printf("munmap failed!, %s\n", strerror(errno));
                if(shm_unlink(shared_memory_name) != 0) printf("shm_unlink failed!, %s\n", strerror(errno));
                sem_unlink("/SIMULATION_SEM");
            }
        private:
            int fd;
            void *buf;
            sem_t* sem;
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

                sem = sem_open("/SIMULATION_SEM", O_CREAT, 0777, 0);
                if(sem == SEM_FAILED) {
                    std::cerr << "sem_open failed!, " << strerror(errno) << std::endl;
                }
            }
            bool sleep() {
                uint64_t* time = ((uint64_t*)buf);
                while (current >= *time) {
                    sem_wait(sem);
                    sem_post(sem);
                    usleep(100);
                    time = ((uint64_t*)buf);
                }
                current += sleep_us;
                return true;
            }
            ~Rate() {
                shm_unlink("SIMULATION_TIME_US");
                sem_close(sem);
                sem_unlink("/SIMULATION_SEM");
            }
        private:
            uint64_t current = 0;
            int sleep_us;
            int fd;
            void *buf;
            sem_t* sem;
    };

    #endif
}

#endif