#pragma once

#include "include/containers/worker_thread.hpp"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <utility>
#include <thread>
#include <fstream>

// Lightweight workers representing long-lived external services (UI and viz API).
// They can launch external commands (e.g., React dev server or Flask API) if configured,
// otherwise they idle until a stop is requested/release packet is delivered.
class FrontendWorker : public WorkerThread {
public:
    FrontendWorker(uint8_t id, std::shared_ptr<DefaultBuffer> buffer, std::string name, std::string command = {}, std::string workdir = {})
        : WorkerThread(id, std::move(buffer)), name_(std::move(name)), command_(std::move(command)), workdir_(std::move(workdir)) {}

    void run(std::stop_token st) override {
        const std::string log_path = "/tmp/" + name_ + ".log";

        // Always write a line so we know the worker thread actually ran.
        {
            std::ofstream log(log_path, std::ios::app);
            log << "[frontend:" << name_ << "] "
                << (command_.empty() ? "no command configured, idling" : "launching: " + command_);
            if (!workdir_.empty()) log << " (cwd=" << workdir_ << ")";
            log << std::endl;
        }

        if (!command_.empty()) {
            std::string cmd = command_;
            if (!workdir_.empty()) {
                cmd = "cd " + workdir_ + " && " + cmd;
            }
            const std::string pid_path = "/tmp/" + name_ + ".pid";
            // Run detached via nohup and capture the child PID for later shutdown.
            std::string launch = cmd +
                " >/dev/null 2>&1 < /dev/null & echo $! > " + pid_path;
            std::string full = "nohup sh -c \"" + launch + "\" >> " + log_path + " 2>&1 < /dev/null &";
            std::system(full.c_str());
        }

        using namespace std::chrono_literals;
        while (!st.stop_requested()) {
            std::this_thread::sleep_for(25ms);
        }
    }

    const std::string& name() const { return name_; }

private:
    std::string name_;
    std::string command_;
    std::string workdir_;
};

class VisualizationServerWorker : public FrontendWorker {
public:
    VisualizationServerWorker(uint8_t id, std::shared_ptr<DefaultBuffer> buffer, std::string command = {}, std::string workdir = {})
        : FrontendWorker(id, std::move(buffer), "visualization-server", std::move(command), std::move(workdir)) {}
};

class UiWorker : public FrontendWorker {
public:
    UiWorker(uint8_t id, std::shared_ptr<DefaultBuffer> buffer, std::string command = {}, std::string workdir = {})
        : FrontendWorker(id, std::move(buffer), "ui", std::move(command), std::move(workdir)) {}
};
