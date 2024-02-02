#ifndef THREAD_WITH_PRIORITY_H
#define THREAD_WITH_PRIORITY_H
#include <thread>
#include <pthread.h>
#include <iostream>
#include <cstring>

class thread : public std::thread
{
  public:
    thread() {}
    static void setScheduling(std::thread &th, int policy, int priority) {
        sch_params.sched_priority = priority;
        if(pthread_setschedparam(th.native_handle(), policy, &sch_params)) {
            std::cerr << "Failed to set Thread scheduling : " << std::strerror(errno) << std::endl;
        }
    }
  private:
    sched_param sch_params;
};

#endif