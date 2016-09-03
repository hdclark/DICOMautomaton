//Thread_Pool.h.

#pragma once

#include <thread>

#include <asio.hpp>
#include <boost/thread.hpp> // For class thread_group.
#include <boost/bind.hpp>

//#include <boost/asio/io_service.hpp>
//#include <boost/thread/thread.hpp>


class asio_thread_pool {
  private:

    asio::io_service _io_service;
    boost::thread_group _thread_pool;
    typedef std::unique_ptr<asio::io_service::work> work_t;
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
    ~asio_thread_pool(void){
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


