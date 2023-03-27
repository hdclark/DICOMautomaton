//Thread_Pool.h.

#pragma once

#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <algorithm>
#include <list>


// Multi-threaded work queue for offloading processing tasks.
//
// Note that if there is a single thread, then work is processed sequentially in FIFO order.

template<class T>
class work_queue {

  private:
    std::list<T> queue;
    std::mutex queue_mutex;

    std::condition_variable new_task_notifier;
    std::atomic<bool> should_quit = false;
    std::list<std::thread> worker_threads;

    std::condition_variable end_task_notifier;

  public:

    work_queue(unsigned int n_workers = std::thread::hardware_concurrency()){
        std::unique_lock<std::mutex> lock(this->queue_mutex);

        auto l_n_workers = (n_workers == 0U) ? std::thread::hardware_concurrency()
                                             : n_workers;
        l_n_workers = (l_n_workers == 0U) ? 2U : l_n_workers;

        for(unsigned int i = 0; i < l_n_workers; ++i){
            this->worker_threads.emplace_back(
                [this](){
                    // Continually check the queue and wait on the condition variable.
                    bool l_should_quit = false;
                    while(!l_should_quit){

                        std::unique_lock<std::mutex> lock(this->queue_mutex);
                        while( !(l_should_quit = this->should_quit.load())
                               && this->queue.empty() ){

                            // Waiting releases the lock, which allows for work to be submitted.
                            //
                            // Note: spurious notifications are OK, since the queue will be empty and the worker will return to
                            // waiting on the condition variable.
                            this->new_task_notifier.wait(lock);
                        }

                        // Assume ownership of only the first item in the queue (FIFO).
                        std::list<T> l_queue;
                        if(!this->queue.empty()) l_queue.splice( std::end(l_queue), this->queue, std::begin(this->queue) );
                        
                        //// Assume ownership of all available items in the queue.
                        //std::list<T> l_queue;
                        //l_queue.swap( this->queue );

                        // Perform the work in FIFO order.
                        lock.unlock();
                        for(const auto &user_f : l_queue){
                            try{
                                user_f();
                            }catch(const std::exception &){};

                            lock.lock();
                            this->end_task_notifier.notify_all();
                            lock.unlock();
                        }
                    }
                }
            );
        }
    }

    void submit_task(T f){
        std::lock_guard<std::mutex> lock(this->queue_mutex);
        this->queue.push_back(std::move(f));

        // Note: it's not strictly necessary to lock the mutex before notifying, but it's possible it could lead to a data
        // race or use-after-free. If nothing else, locking suppresses warnings of a 'possible' data race in thread sanitizers.
        // See discussion and some links at
        // https://stackoverflow.com/questions/17101922/do-i-have-to-acquire-lock-before-calling-condition-variable-notify-one
        // Also note that this can potentially lead to a performance downgrade; in practice, most pthread
        // implementations will detect and mitigate the issue.
        this->new_task_notifier.notify_one();
        return;
    }

    std::list<T> clear_tasks(){
        std::list<T> out;
        std::lock_guard<std::mutex> lock(this->queue_mutex);
        out.swap( this->queue );
        return out;
    }

    ~work_queue(){
        // We can either cancel outstanding tasks or wait for all queued tasks to be completed.
        // Since there is a mechanism to clear queued tasks that have not been acquired by worker threads,
        // it is least-surprising to wait for all queued tasks to be completed before destructing.

        // To wait for tasks to be processed, we will poll until either the task queue is empty, or the signal to quit
        // is received. Note that tasks may still be being processed when this function returns, however it will be
        // safe to call the destructor (which will safely wait for tasks by joining the threads).
        //
        // TODO: could we instead use a separate condition variable (and mutex?) to receive a signal when tasks are
        // actually completed? This would avoid (minimize?) polling and also allow tasks to be fully completed
        // before returning.
        bool l_should_quit = false;
        while(!l_should_quit){

            std::unique_lock<std::mutex> lock(this->queue_mutex);
            while( !(l_should_quit = this->should_quit.load())
                   && !this->queue.empty() ){

                // Waiting releases the lock while waiting, which still allows for outstanding work to be completed.
                //
                // We also periodically wake up in case there is a signalling race. For longer-running tasks, this will
                // hopefully be an insignificant amount of extra processing.
                this->end_task_notifier.wait_for(lock, std::chrono::milliseconds(2000) );
            }

            this->should_quit.store(true);
            this->new_task_notifier.notify_all(); // notify threads to wake up and 'notice' they need to terminate.
            lock.unlock();
            for(auto &wt : this->worker_threads) wt.join();
            break;
        }
    }
};


