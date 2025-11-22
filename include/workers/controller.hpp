#include "include/containers/data_packet.hpp"
#include "include/containers/worker_thread.hpp"
#include "include/containers/circular_buffer.hpp"

#include "include/workers/path_plan.hpp"
#include "include/workers/frontend.hpp"

#include "include/constants.hpp"

#include <chrono>
#include <stop_token>
#include <optional>
#include <utility>
#include <condition_variable>
#include <mutex>
#include <iostream>
#include <filesystem>
#include <cstdlib>
#include <cstring>
#include <csignal>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>


class Controller
{
public:
    Controller() :
            task_buffer(std::make_shared<DefaultBuffer>()),
            dispatcher_thread([this](std::stop_token st) {
                task_buffer_dispatcher_loop(st);
            }),
            ipc_thread([this](std::stop_token st) {
                ipc_listener_loop(st);
            })
    {}

    ~Controller() = default;

    States state() const {
        std::scoped_lock lock(state_mutex);
        return static_cast<States>(curr_state);
    }

    void run() {
        set_state(States::INIT);
        std::cout << "Controller entering INIT state\n";
        while (true) {
            auto current = state();
            if (current == States::SHUTDOWN) {
                break;
            }

            switch (current) {
                case States::INIT: {
                    // bring up long-lived services as workers
                    start_frontend_workers();
                    set_state(States::PLAN);
                    std::cout << "Controller -> PLAN state\n";
                } break;

                case States::PLAN: {
                    auto path_planner = this->request_worker<PathPlanner>();
                    (void)path_planner;
                    set_state(States::EXECUTE);
                    std::cout << "Controller -> EXECUTE state\n";
                } break;

                case States::EXECUTE: {
                    // idle until a shutdown command arrives
                    std::unique_lock<std::mutex> lk(state_mutex);
                    state_cv.wait(lk, [this]() {
                        return curr_state == static_cast<int>(States::SHUTDOWN);
                    });
                } break;

                case States::EXECUTE_ERROR:
                case States::PLAN_ERROR:
                case States::INIT_ERROR: {
                    set_state(States::SHUTDOWN);
                } break;

                case States::SHUTDOWN: {
                    // loop will exit next iteration
                } break;
            }
        }
        dispatcher_thread.request_stop();
    }

    void request_shutdown() {
        set_state(States::SHUTDOWN);
        dispatcher_thread.request_stop();
        ipc_thread.request_stop();
        close_ipc_socket();
        stop_frontend_process("ui");
        stop_frontend_process("visualization-server");
        clear_thread_pool();
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
        auto task_ptr = std::make_shared<T>(idx, this->task_buffer, std::forward<Args>(extra_args)...);
        std::jthread task_thread([worker = task_ptr](std::stop_token st) {
            worker->run(st);
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
            release_worker_unlocked(idx);
        }
    }


private:

    void release_worker_unlocked(size_t idx) {
        if (idx < thread_pool_size && thread_pool_size > 0) {
            std::size_t last = thread_pool_size-1;
            if (idx != last) {
                std::swap(thread_pool[idx], thread_pool[last]); // maintain contiguity of array
            }
            thread_pool[last].reset(); // release memory
            --thread_pool_size;
            _thread_pool_cv.notify_one();
        }
    }

    void clear_thread_pool() {
        std::scoped_lock lock(_thread_pool_mutex);
        clear_thread_pool_unlocked();
    }

    void clear_thread_pool_unlocked() {
        for (size_t i = 0; i < thread_pool_size; ++i) {
            thread_pool[i].reset();
        }
        thread_pool_size = 0;
        _thread_pool_cv.notify_all();
    }

    void start_frontend_workers() {
        const char* env = std::getenv("AUTO_START_FRONTENDS");
        const bool auto_start = (env == nullptr) || (std::string(env) != "0");
        if (!auto_start) {
            std::cout << "AUTO_START_FRONTENDS=0, skipping UI/viz launch\n";
        }
        auto [ui_cmd, ui_cwd] = frontend_command_defaults("client", "npm start");
        auto [viz_cmd, viz_cwd] = frontend_command_defaults("visualization", "python3 server.py --module-path ../build --port 8000");

        if (!ui_started_) {
            request_worker<UiWorker>(auto_start ? ui_cmd : "", ui_cwd);
            ui_started_ = true;
            std::cout << "UI worker started";
            if (auto_start) std::cout << " (launching `" << ui_cmd << "` in " << ui_cwd << ")";
            std::cout << "\n";
        }
        if (!viz_started_) {
            request_worker<VisualizationServerWorker>(auto_start ? viz_cmd : "", viz_cwd);
            viz_started_ = true;
            std::cout << "Visualization server worker started";
            if (auto_start) std::cout << " (launching `" << viz_cmd << "` in " << viz_cwd << ")";
            std::cout << "\n";
        }
    }

    void handle_controller_packet(const DefaultDataPacketT& packet) {
        if (packet.data_array_types[0] != static_cast<uint8_t>(DefaultTaskId::INT)) return;

        auto cmd_val = static_cast<std::int64_t>(packet.data_array[0].to_ullong());
        auto cmd = static_cast<CmdId>(cmd_val);

        switch (cmd) {
            case CmdId::ReleaseWorker: {
                release_worker(packet);
            } break;
            case CmdId::ShutdownController: {
                std::optional<size_t> target_idx;
                if (packet.data_array_types[1] == static_cast<uint8_t>(DefaultTaskId::INT)) {
                    target_idx = static_cast<size_t>(packet.data_array[1].to_ulong());
                }

                {
                    std::scoped_lock lock(_thread_pool_mutex);
                    if (target_idx.has_value()) {
                        release_worker_unlocked(*target_idx);
                    } else {
                        clear_thread_pool_unlocked();
                    }
                }

                request_shutdown();
            } break;
        }
    }

    void task_buffer_dispatcher_loop(std::stop_token st) {
        while(!st.stop_requested()) {
            // check for controller-directed packets (release worker, shutdown, etc.)
            // non-blocking try_pop_if
            auto packet = task_buffer->try_pop_if([](const auto& top) {
                return top.first == ControllerTaskIdx;
            });

            if (packet.has_value()) {
                handle_controller_packet(packet->second);
                if (state() == States::SHUTDOWN) {
                    break;
                }
            }
            
            // timeout, TODO: make configurable
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    void ipc_listener_loop(std::stop_token st) {
        int server_fd = setup_ipc_socket();
        if (server_fd < 0) {
            std::cerr << "IPC listener failed to bind\n";
            return;
        }
        {
            std::scoped_lock lock(ipc_mutex);
            ipc_socket_fd = server_fd;
        }

        while (!st.stop_requested()) {
            fd_set readfds;
            FD_ZERO(&readfds);
            FD_SET(server_fd, &readfds);
            timeval tv{1, 0};
            int ready = select(server_fd + 1, &readfds, nullptr, nullptr, &tv);
            if (ready <= 0) {
                continue;
            }

            sockaddr_in client_addr{};
            socklen_t addrlen = sizeof(client_addr);
            int client_fd = accept(server_fd, reinterpret_cast<sockaddr*>(&client_addr), &addrlen);
            if (client_fd < 0) {
                continue;
            }

            char buffer[256];
            ssize_t bytes = read(client_fd, buffer, sizeof(buffer) - 1);
            if (bytes > 0) {
                buffer[bytes] = '\0';
                std::string cmd(buffer);
                if (cmd.find("shutdown") != std::string::npos) {
                    const char* resp = "ok\n";
                    write(client_fd, resp, std::strlen(resp));
                    request_shutdown();
                } else {
                    const char* resp = "unknown\n";
                    write(client_fd, resp, std::strlen(resp));
                }
            }
            close(client_fd);
        }

        close_ipc_socket();
    }

    int setup_ipc_socket() {
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0) {
            return -1;
        }

        int opt = 1;
        setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port = htons(CONTROLLER_IPC_PORT);
        if (bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
            close(fd);
            return -1;
        }
        if (listen(fd, 4) < 0) {
            close(fd);
            return -1;
        }
        return fd;
    }

    void close_ipc_socket() {
        std::scoped_lock lock(ipc_mutex);
        if (ipc_socket_fd >= 0) {
            close(ipc_socket_fd);
            ipc_socket_fd = -1;
        }
    }

    void stop_frontend_process(const std::string& name) {
        std::filesystem::path pid_path = "/tmp/" + name + ".pid";
        if (!std::filesystem::exists(pid_path)) return;
        std::ifstream pid_file(pid_path);
        int pid = -1;
        pid_file >> pid;
        if (pid > 0) {
            kill(static_cast<pid_t>(pid), SIGTERM);
        }
        std::filesystem::remove(pid_path);
    }

    // TaskQueue
    // enqueue a shared ptr to data packet that needs to be sent or received
    // int corresponding to who the msg is meant for
    std::shared_ptr<DefaultBuffer> task_buffer;
    std::jthread dispatcher_thread;
    std::jthread ipc_thread;

    // State machine tracking
    mutable std::mutex state_mutex;
    std::condition_variable state_cv;
    int curr_state = static_cast<int>(States::INIT);

    bool ui_started_ = false;
    bool viz_started_ = false;
    int ipc_socket_fd = -1;
    std::mutex ipc_mutex;

    void set_state(States state) {
        {
            std::scoped_lock lock(state_mutex);
            curr_state = static_cast<int>(state);
        }
        state_cv.notify_all();
    }

    std::pair<std::string, std::string> frontend_command_defaults(const std::string& subdir, const std::string& command) {
        std::filesystem::path cwd = std::filesystem::current_path();
        if (!std::filesystem::exists(cwd / subdir)) {
            cwd = cwd.parent_path();
        }
        auto target = cwd / subdir;
        return {command, target.string()};
    }
};
