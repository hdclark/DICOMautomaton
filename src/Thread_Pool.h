//Thread_Pool.h.

#pragma once

#include <mutex>
#include <shared_mutex>
#include <condition_variable>
#include <thread>
#include <atomic>

#include <asio.hpp>
#include <boost/thread.hpp> // For class thread_group.
#include <boost/bind/bind.hpp>

//#include <boost/asio/io_service.hpp>
//#include <boost/thread/thread.hpp>


class asio_thread_pool {
  private:

    asio::io_service _io_service;
    boost::thread_group _thread_pool;
    using work_t = std::unique_ptr<asio::io_service::work>;
    work_t _work;

  public:

    //Constructor and destructor.
    asio_thread_pool(const size_t num_threads = 0) : _io_service(), 
                                                     _work(new work_t::element_type(_io_service)) {
        auto n = (num_threads == 0) ? std::thread::hardware_concurrency()
                                    : num_threads;
        if(n == 0) n = 2;
        for(size_t i = 0 ; i < n; ++i){
            this->_thread_pool.create_thread(
                boost::bind(&asio::io_service::run, &this->_io_service)
            );
        }
    }
    ~asio_thread_pool(){
        this->_work.reset();
        this->_thread_pool.join_all();
        this->_io_service.stop();
    }

    //Work submission routine.
    template<class T>
    void submit_task(T atask){
        this->_io_service.post(atask);
    }
}; 



// Single-threaded work queue for sequential FIFO offloading processing.
template<class T>
class work_queue {

  private:
    std::list<T> queue;
    std::mutex queue_mutex;

    std::condition_variable notifier;
    std::atomic<bool> should_quit = false;
    std::thread worker_thread;

  public:

    work_queue() : worker_thread(
        [this](){
            // Continually check the queue and wait on the condition variable.
            bool l_should_quit = false;
            while(!l_should_quit){

                std::unique_lock<std::mutex> lock(this->queue_mutex);
                while( !(l_should_quit = this->should_quit.load())
                       && this->queue.empty() ){

                    // Release the lock while waiting, which allows for work to be submitted.
                    //
                    // Note: spurious notifications are OK, since the queue will be empty and the worker will return to
                    // waiting on the condition variable.
                    this->notifier.wait(lock);
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
                }
            }
        }
    ){}

    void submit_task(T f){
        {
            std::lock_guard<std::mutex> lock(this->queue_mutex);
            this->queue.push_back(std::move(f));
        }
        this->notifier.notify_one();
        return;
    }

    std::list<T> clear_tasks(){
        std::list<T> out;
        std::lock_guard<std::mutex> lock(this->queue_mutex);
        out.swap( this->queue );
        return std::move(out);
    }

    ~work_queue(){
        this->should_quit.store(true);
        this->notifier.notify_one();
        this->worker_thread.join();
    }
};


