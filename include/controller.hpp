
// include macros - ramps, gcodes


// look at marlin stepper and planner

// A+B moves diagonal, -A-B moves diagonal
// A-B or B-A moves one  direction

// is given initial gcode
// modifies gcode based on sensed data


#define SET_TO_ZERO(a) memset(a, 0, sizeof(a))

constexpr uint8_t CMD_QUEUE_SIZE = 128;
constexpr uint8_t CMD_QUEUE[CMD_QUEUE_SIZE];

constexpr size_t THREAD_POOL_CAPACITY = 16;
constexpr size_t BLOCK_SIZE = 2048;

constexpr NUM_TASK_BITS = 64;

using TaskEncodedType = std::bitset<NUM_TASK_BITS>;
using TaskDataType = std::variant< 
                        std::array<char, 64>
                        long int,
                        double> // TODO: decouple from NUM_TASK_BITS
struct TaskBlock
{
    std::array<, BLOCK_SIZE> data; // bitfield and cast to/from value specified
    std::array<TaskDataType, BLOCK_SIZE> data_types; // which one of variant fields is being used
    int size = 0;
    std::atomic<bool> ready = false;
}


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
    constexpr std::array< std::optional<std::jthread>, THREAD_POOL_CAPACITY > thread_pool;
    constexpr std::array<uint8_t, THREAD_POOL_CAPACITY> idle_id_list;
    uint8_t idle_size = THREAD_POOL_CAPACITY;

    constexpr void populate_idle_id_list() {
        for (int i=0; i<THREAD_POOL_CAPACITY; i++) idle_id_list[i] = i;
    }

    // TaskQueue
    // enqueue a ptr to data that needs to be sent over (on stack?)
    // data might need to be chunked and sent over in pieces
    using BlockPtr = uint8_t;
    // make a list of ptrs to mem where blocks can be stored
    std::array< std::pair<uint8_t, BlockPtr>, THREAD_POOL_CAPACITY > circular_task_buffer;

}


const uint

class WorkThread
{
public:
    WorkThread(std::jthread&& worker_thread) {

    }
}