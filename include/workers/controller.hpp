#include "include/containers/data_packet.hpp"
#include "include/containers/worker_thread.hpp"
#include "include/containers/circular_buffer.hpp"
// #include "include/containers/state_machine.hpp"

#include "include/workers/path_plan.hpp"

#include "include/constants.hpp"


class Controller
{
public:
    Controller() {};

    void run() {
        switch (static_cast<States>(this->curr_state)) {
            case States::INIT: {
                
            }
            break;

            case States::PLAN: {
                std::shared_ptr<PathPlanner> path_planner = this->request_worker<PathPlanner>();
            }
            break;

            case States::EXECUTE: {

            }
            break;

            // case default: {
                
            // }
            // break;
        }
        this->prev_state = this->curr_state;
    };

    size_t get_thread_pool_size() {
        return this->thread_pool_size;
    };

    // ThreadPool
    // idle threads for large lifetime processes to be executed on
    std::array< std::shared_ptr<WorkerThread>, THREAD_POOL_CAPACITY > thread_pool;
    size_t thread_pool_size = 0;
    std::mutex _thread_pool_mutex;
    std::condition_variable _thread_pool_cv;

    template <ConcreteWorker T, typename... Args>
    std::shared_ptr<T> request_worker(Args&&... extra_args) {
        std::unique_lock<std::mutex> req_worker_lock(_thread_pool_mutex);
        _thread_pool_cv.wait(req_worker_lock, [this] {
            if (this->thread_pool_size == THREAD_POOL_CAPACITY) {
                std::cerr << "Threadpool fully in use. Waiting for available thread...";
            }
            return this->thread_pool_size < THREAD_POOL_CAPACITY;
        });

        const size_t idx = thread_pool_size++;
        auto task_ptr = std::make_shared<T>(idx, this->task_buffer);
        thread_pool[idx] = task_ptr;
        return task_ptr;
    }

    void release_worker(const DefaultDataPacketT& task_data) {
        // signal from thread to release worker from task popped from buffer
        // if id is -1 then packet is for controller, and the first term in packet is reserved keyword JOIN
        std::scoped_lock release_worker_lock(_thread_pool_mutex);
        if (task_data.data_array_types[0] == static_cast<uint8_t>(DefaultTaskId::INT)
            && task_data.data_array[0].to_ulong() == static_cast<uint64_t>(CmdId::ReleaseWorker)
            && task_data.data_array_types[1] == static_cast<uint8_t>(DefaultTaskId::INT)) {
            
            size_t idx = static_cast<size_t>(task_data.data_array[1].to_ulong());
            if (idx < thread_pool_size && thread_pool_size > 1) {
                swap(thread_pool[idx], thread_pool[--thread_pool_size]); // maintain contiguity of array
            }
            // Set old shared ptr to null ptr to release memory
            thread_pool[thread_pool_size + 1] = nullptr;
            _thread_pool_cv.notify_one();
        }
    }


private:

    void task_buffer_dispatcher_loop() {
        while(true) {
            // Check if worker release request, its the only request received right now by controller
            auto packet = task_buffer->pop_if([](const auto& top) {
                return top.first == ControllerTaskIdx;
            });
            if (packet.has_value()) {
                release_worker(packet.value().second);
            }
            
            // timeout, TODO: make configurable
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    }

    // TaskQueue
    // enqueue a shared ptr to data packet that needs to be sent or received
    // int corresponding to who the msg is meant for
    std::shared_ptr<DefaultBuffer> task_buffer;

    // State machine tracking
    int curr_state = 0;
    int prev_state = 0;
};
