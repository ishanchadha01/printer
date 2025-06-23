#include "include/containers/data_packet.hpp"
#include "include/containers/worker_thread.hpp"
#include "include/containers/circular_buffer.hpp"

#include "include/constants.hpp"


constexpr int8_t ControllerTaskIdx = -1;

template<StateMachine StateMachineType>
class Controller
{
public:
    Controller() {
        state_machine = std::make_unique<StateMachineType>(this->request_worker);
    };
    void run();

private:

    // ThreadPool
    // idle threads for large lifetime processes to be executed on
    std::array< std::shared_ptr<WorkerThread>, THREAD_POOL_CAPACITY > thread_pool;
    size_t thread_pool_size = 0;
    std::mutex _thread_pool_mutex;
    std::condition_variable _thread_pool_cv;

    std::shared_ptr<WorkerThread> request_worker() {
        std::unique_lock<std::mutex> req_worker_lock(_thread_pool_mutex);
        _thread_pool_cv.wait(req_worker_lock, [this] {
            if (this->thread_pool_size == THREAD_POOL_CAPACITY) {
                std::cerr << "Threadpool fully in use. Waiting for available thread...";
            }
            return this->thread_pool_size < THREAD_POOL_CAPACITY;
        });
        return thread_pool[thread_pool_size++];
    }

    void release_worker(const DataPacket& popped_task) {
        // signal from thread to release worker from task popped from buffer
        // if id is -1 then packet is for controller, and the first term in packet is reserved keyword JOIN
        std::scoped_lock(_thread_pool_mutex);
        auto& task_data = popped_task.second;
        if (task_data.data_array_types[0] == static_cast<uint8_t>(DefaultTaskId::INT)
            && task_data.data_array[0].to_ulong() == static_cast<uint64_t>(CmdId::ReleaseWorker)
            && task_data.data_array_types[1] == static_cast<uint8_t>(DefaultTaskId::INT)) {
            
            size_t idx = static_cast<size_t>(task_data.data_array[1].to_ulong());
            if (idx < thread_pool_size && thread_pool_size > 1) {
                swap(thread_pool[idx], thread_pool[--thread_pool_size]); // maintain contiguity of array
            }
            _thread_pool_cv.notify_one();
        }
    }

    void task_buffer_dispatcher_loop() {
        while(true) {
            // Check if worker release request, its the only request received right now by controller
            auto packet = task_buffer.pop_if([](const auto& top) {
                return top.first == ControllerTaskIdx;
            });
            if (packet.has_value()) {
                release_worker(packet);
            }
            
            // timeout, TODO: make configurable
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    }

    // TaskQueue
    // enqueue a shared ptr to data packet that needs to be sent or received
    // int corresponding to who the msg is meant for
    std::shared_ptr<CircularBuffer< std::pair<int, DefaultDataPacket>, THREAD_POOL_CAPACITY >> task_buffer;

    // StateMachine type
    std::unique_ptr<StateMachineType> state_machine; 
};
