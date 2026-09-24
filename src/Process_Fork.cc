//Process_Fork.cc.

#include "Process_Fork.h"

#include <atomic>
#include <stdexcept>
#include <thread>


namespace {

// 0 permits fork, 1 permanently disables it, and 2 reserves the process while
// fork() copies its state. An atomic is used because both parent and child must
// be able to release the reservation immediately after fork().
std::atomic<int> python_fork_state{0};

} // namespace

ProcessForkGuard::ProcessForkGuard(){
#if !defined(_WIN32) && !defined(_WIN64)
    for(;;){
        auto expected = 0;
        if(python_fork_state.compare_exchange_weak(expected, 2)){
            reserved_ = true;
            return;
        }
        if(expected == 1){
            throw std::runtime_error("Process forking is unavailable after Python runtime initialization");
        }
        std::this_thread::yield();
    }
#endif
}

ProcessForkGuard::~ProcessForkGuard(){
    release();
}

void ProcessForkGuard::release(){
#if !defined(_WIN32) && !defined(_WIN64)
    if(reserved_){
        python_fork_state.store(0);
        reserved_ = false;
    }
#endif
}

void DisableForkForPythonInitialization(){
#if !defined(_WIN32) && !defined(_WIN64)
    for(;;){
        auto expected = 0;
        if(python_fork_state.compare_exchange_weak(expected, 1)){
            return;
        }
        if(expected == 1){
            return;
        }
        std::this_thread::yield();
    }
#endif
}
