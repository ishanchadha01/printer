
// include macros - ramps, gcodes


// look at marlin stepper and planner

// A+B moves diagonal, -A-B moves diagonal
// A-B or B-A moves one  direction

// is given initial gcode
// modifies gcode based on sensed data


// #define SET_TO_ZERO(a) memset(a, 0, sizeof(a))

// constexpr uint8_t CMD_QUEUE_SIZE = 128;
// constexpr uint8_t CMD_QUEUE[CMD_QUEUE_SIZE];

constexpr size_t THREAD_POOL_CAPACITY = 16;
// constexpr size_t DEFAULT_BLOCK_SIZE = 2048;


// Num Task Bits
template<size_t NumTaskBits>
using TaskEncodedType = std::bitset<NumTaskBits>;

// Use concept to tie together default task
constexpr size_t DEFAULT_NUM_TASK_BITS = 64;
template<size_t N>
concept IsDefaultTask = (N == DEFAULT_NUM_TASK_BITS);

using DefaultTaskDataType = std::variant< 
                        std::array<char, 8>,
                        long int,
                        double>;

enum class DefaultTaskId : uint8_t {
    CHAR_ARR = 0,
    INT = 1,
    DOUBLE = 2
};

template<typename T, typename Variant>
struct is_in_variant : std::false_type {};

template<typename T, typename... Ts>
struct is_in_variant<T, std::variant<Ts>...> : std::disjunction<std::is_same<T, Ts>...> {};

template<typename T>
concept IsDefaultTaskDataType = is_in_variant<T, DefaultTaskDataType>::value;

template<typename Transport, size_t BlockSize, IsDefaultTask NumTaskBits>
struct DataPacket
{

    std::array< std::bitset<NumTaskBits>, BlockSize> data_array; // bitfield and cast to/from value specified
    std::array<uint8_t, BLOCK_SIZE> data_array_types; // which one of variant fields is being used
    size_t data_array_size = 0;
    std::atomic<bool> ready = false;

    constexpr void reset() {
        data_array.fill(static_cast<std::bitset<NumTaskBits>>(0));
        data_array_size = 0;
    }

    template<IsDefaultTaskDataType TaskT>
    void insert(const int& i, const TaskT& data)
    {
        // convert to bitset manually
        using StrippedT = std::decay<TaskT>(data);
        uint64_t raw = 0;
        if constexpr (std::is_same<StrippedT, double>::value)) {
            data_array_types[i] = static_cast<uint8_t>(DefaultTaskId::DOUBLE);
            std::memcpy(&raw, &data, sizeof(uint64_t));
        } else if constexpr (std::is_same<StrippedT, long int>::value) {
            data_array_types[i] = static_cast<uint8_t>(DefaultTaskId::INT);
            std::memcpy(&raw, &data, sizeof(uint64_t));
        } else if constexpr (std::is_same_v<StrippedT, std::array<char, 8>>) {
            data_array_types[i] = static_cast<uint8_t>(DefaultTaskId::CHAR_ARR);
            std::memcpy(&raw, data.data(), sizeof(uint64_t)); // std doesnt guarantee &data == &data[0] ??
        } else {
            static_assert([] {return false;}, "Unsupported type for Default Task!")
        }
        data_array[i] = std::bitset<NumTaskBits>(raw);
    }
};


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
    std::array< std::optional<std::jthread>, THREAD_POOL_CAPACITY > thread_pool;
    std::array<uint8_t, THREAD_POOL_CAPACITY> idle_id_list;
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