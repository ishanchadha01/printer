#pragma once

#include "include/constants.hpp"
#include "include/containers/circular_buffer.hpp"
#include "include/containers/data_packet.hpp"

#include <thread>
#include <stop_token>

using DefaultBuffer = CircularBuffer< 
    std::pair< TaskId, DefaultDataPacket<DEFAULT_NUM_TASK_BITS, DEFAULT_BLOCK_SIZE>  >, 
    THREAD_POOL_CAPACITY >;

using DefaultDataPacketT = DefaultDataPacket<DEFAULT_NUM_TASK_BITS, DEFAULT_BLOCK_SIZE>;

class WorkerThread
{
public:
    WorkerThread(TaskId id, std::shared_ptr<DefaultBuffer> buffer)
        : _id(id), task_buffer(buffer) {}

    virtual ~WorkerThread() = default;

    virtual void run(std::stop_token st) = 0;

    void set_thread(std::jthread&& thread)
    {
        this->_thread = std::move(thread);
    }

    void send_data(DefaultDataPacketT packet, TaskId recipient_id)
    {
        this->task_buffer->emplace({recipient_id, packet});
    }

    // delete copy and copy assignment constructors
    WorkerThread& operator=(const WorkerThread& thread) = delete;
    WorkerThread(const WorkerThread& thread) = delete;


    TaskId get_id()
    {
        return this->_id;
    }

private:
    TaskId _id;
    std::jthread _thread;
    std::shared_ptr<DefaultBuffer> task_buffer;
};

template <class T>
concept ConcreteWorker = std::derived_from<T, WorkerThread> && (!std::is_abstract_v<T>);
