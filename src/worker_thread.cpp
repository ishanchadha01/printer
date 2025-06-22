#include "include/workers/worker_thread.hpp"

WorkerThread::WorkerThread() {}

WorkerThread::~WorkerThread()
{
    if (this->_thread.joinable()) {
        this->_thread.join();
    }
}

void setThread(std::jthread&& thread)
{
    this->_thread = std::move(thread);
}