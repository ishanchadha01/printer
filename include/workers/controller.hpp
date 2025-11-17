#include "include/containers/data_packet.hpp"
#include "include/containers/worker_thread.hpp"
#include "include/containers/circular_buffer.hpp"

#include "include/workers/path_plan.hpp"

#include "include/constants.hpp"

#include <chrono>


class Controller
{
public:
    Controller()
        : task_buffer(std::make_shared<DefaultBuffer>()) ,
          dispatcher_thread(&Controller::task_buffer_dispatcher_loop, this)
    {}

    ~Controller() = default;

    void run() {
        while (static_cast<States>(curr_state) != States::SHUTDOWN) {
            switch (static_cast<States>(curr_state)) {
                case States::INIT: {
                    curr_state = static_cast<int>(States::PLAN);
                } break;

                case States::PLAN: {
                    auto path_planner = this->request_worker<PathPlanner>();
                    curr_state = static_cast<int>(States::EXECUTE);
                } break;

                case States::EXECUTE: {
                    curr_state = static_cast<int>(States::SHUTDOWN);
                } break;

                case States::EXECUTE_ERROR:
                case States::PLAN_ERROR:
                case States::INIT_ERROR: {
                    curr_state = static_cast<int>(States::SHUTDOWN);
                } break;

                case States::SHUTDOWN: {
                    // loop will exit next iteration
                } break;
            }

            prev_state = curr_state;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    void request_shutdown() {
        curr_state = static_cast<int>(States::SHUTDOWN);
    }

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
        while (this->thread_pool_size == THREAD_POOL_CAPACITY) {
            std::cerr << "Threadpool fully in use. Waiting for available thread...\n";
            _thread_pool_cv.wait(req_worker_lock);
        }

        const size_t idx = thread_pool_size++;
        auto task_ptr = std::make_shared<T>(idx, this->task_buffer);
        std::jthread task_thread([worker = task_ptr]() {
            worker->run();
        });
        task_ptr->set_thread(std::move(task_thread));
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
            if (idx < thread_pool_size && thread_pool_size > 0) {
                std::size_t last = thread_pool_size-1;
                if (idx != last) {
                    swap(thread_pool[idx], thread_pool[last]); // maintain contiguity of array
                }
                thread_pool[last].reset(); // release memory
                --thread_pool_size;
                _thread_pool_cv.notify_one();
            }
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
    std::jthread dispatcher_thread;

    // State machine tracking
    int curr_state = static_cast<int>(States::INIT);
    int prev_state = static_cast<int>(States::INIT);
};
