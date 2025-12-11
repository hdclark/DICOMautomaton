
#include "Thread_Pool.h"

#include <functional>
#include <mutex>
#include <list>
#include <chrono>
#include <iostream>

int main(){
    {
        std::mutex m;
        work_queue<std::function<void(void)>> wq(20U);

        for(size_t i = 0U; i < 500U; ++i){
            std::unique_lock<std::mutex> lock(m);
            std::cout << "S" << i << " ";

            wq.submit_task( [&, i](void){
                std::this_thread::sleep_for( std::chrono::milliseconds(20));
                std::unique_lock<std::mutex> lock(m);
                std::cout << "C" << i << ". ";
                return;
            } );
        }

        {
            std::unique_lock<std::mutex> lock(m);
            std::cout << std::endl << "*** All tasks submitted. Waiting for a while to process tasks." << std::endl;
        }
        std::this_thread::sleep_for( std::chrono::milliseconds(200));
        {
            std::unique_lock<std::mutex> lock(m);
            std::cout << std::endl << "*** Time's up. Waiting for outstanding work and terminating queue." << std::endl;
        }

        //wq.clear_tasks();
    }

    {
        work_queue<std::function<void(void)>> wq2;
        wq2.clear_tasks();
    }

    std::cout << std::endl;
    return 0;
}

