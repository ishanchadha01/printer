
// include macros - ramps, gcodes


// look at marlin stepper and planner

// A+B moves diagonal, -A-B moves diagonal
// A-B or B-A moves one  direction

// is given initial gcode
// modifies gcode based on sensed data


// #define SET_TO_ZERO(a) memset(a, 0, sizeof(a))

// constexpr uint8_t CMD_QUEUE_SIZE = 128;
// constexpr uint8_t CMD_QUEUE[CMD_QUEUE_SIZE];

#include "include/containers/data_packet.hpp"
#include "include/containers/worker_thread.hpp"

constexpr size_t THREAD_POOL_CAPACITY = 16;


class Controller
{
public:
    Controller() {
        populate_idle_id_list();
    };
    void run();

private:

    // ThreadPool
    // idle threads for large lifetime processes to be executed on
    std::array< WorkerThread, THREAD_POOL_CAPACITY > thread_pool;
    std::array<uint8_t, THREAD_POOL_CAPACITY> idle_id_list;
    uint8_t idle_size = THREAD_POOL_CAPACITY;

    constexpr void populate_idle_id_list() { 
        for (int i=0; i<THREAD_POOL_CAPACITY; i++) idle_id_list[i] = i;
    }

    // TaskQueue
    // enqueue a shared ptr to data packet that needs to be sent or received
    std::array< std::pair<bool, DefaultDataPacket>, THREAD_POOL_CAPACITY > circular_task_buffer;
    bu

}

const uint

class WorkThread
{
public:
    WorkThread(std::jthread&& worker_thread) {

    }
}