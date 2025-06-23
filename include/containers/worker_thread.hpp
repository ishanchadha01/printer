// #include "include/"

#include "include/constants.hpp"
#include "include/containers/circular_buffer.hpp"

#include <thread>

using DefaultBuffer = CircularBuffer< 
    std::pair< uint8_t, DefaultDataPacket<DEFAULT_NUM_TASK_BITS, DEFAULT_BLOCK_SIZE>  >, 
    THREAD_POOL_CAPACITY >;

class WorkerThread
{
public:
    WorkerThread(uint8_t id, std::shared_ptr<DefaultBuffer> buffer)
        : _id(id), task_buffer(buffer) {}

    virtual ~WorkerThread() 
    {
        if (this->_thread.joinable()) {
            this->_thread.join();
        }
    }

    virtual void run() = 0;

    void setThread(std::jthread&& thread)
    {
        this->_thread = std::move(thread);
    }

    // delete copy and copy assignment constructors
    WorkerThread& operator=(const WorkerThread& thread) = delete;
    WorkerThread(const WorkerThread& thread) = delete;

private:
    uint8_t _id;
    std::jthread _thread;
    size_t _buffer_size;
    std::shared_ptr<DefaultBuffer> task_buffer;
};