#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <future>
#include <atomic>
#include <array>
#include <vector>
#include <cstring>

#include "include/constants.hpp"
#include "include/containers/data_packet.hpp"
#include "include/containers/circular_buffer.hpp"
#include "include/workers/controller.hpp"
#include "include/workers/path_plan.hpp"
#include "include/workers/frontend.hpp"

class ControllerTest : public testing::Test {
protected:
    Controller controller_;
};

// helper to build a "ReleaseWorker" packet
static DefaultDataPacketT make_release_worker_packet(std::size_t worker_id) {
    DefaultDataPacketT packet;
    packet.reset();

    // index 0: command id
    packet.insert(0, static_cast<std::int64_t>(CmdId::ReleaseWorker));
    // index 1: worker index
    packet.insert(1, static_cast<std::int64_t>(worker_id));

    return packet;
}

static bool wait_for_pool_size(Controller& controller, size_t expected, std::chrono::milliseconds timeout) {
    auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (controller.get_thread_pool_size() == expected) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return controller.get_thread_pool_size() == expected;
}

static bool wait_for_state(Controller& controller, States target, std::chrono::milliseconds timeout) {
    auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (controller.state() == target) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return controller.state() == target;
}

// test 1 - request worker, release worker, check pool
TEST_F(ControllerTest, RequestWorker) {
    // no worker objects in pool
    EXPECT_EQ(controller_.get_thread_pool_size(), 0u);

    // create worker
    std::shared_ptr<PathPlanner> worker1 = controller_.request_worker<PathPlanner>();
    ASSERT_TRUE(worker1 != nullptr);
    auto worker_id = worker1->get_id();
    EXPECT_EQ(controller_.get_thread_pool_size(), 1u);

    // build release packet and send to Controller
    auto release_data = make_release_worker_packet(static_cast<std::size_t>(worker_id));
    worker1->send_data(std::move(release_data), ControllerTaskIdx);

    // release worker
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (std::chrono::steady_clock::now() < deadline) {
        if (controller_.get_thread_pool_size() == 0u) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    EXPECT_EQ(controller_.get_thread_pool_size(), 0u);
}

// test 2 - max out worker pool
TEST_F(ControllerTest, RequestWorkerBlocksUntilPoolSlotFrees) {
    std::vector<std::shared_ptr<PathPlanner>> workers;
    for (size_t i = 0; i < THREAD_POOL_CAPACITY; ++i) {
        workers.push_back(controller_.request_worker<PathPlanner>());
    }
    ASSERT_EQ(controller_.get_thread_pool_size(), THREAD_POOL_CAPACITY);

    std::atomic<bool> started{false};
    std::atomic<bool> finished{false};
    auto future = std::async(std::launch::async, [&] {
        started = true;
        auto w = controller_.request_worker<PathPlanner>();
        finished = true;
        return w;
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_TRUE(started.load());
    EXPECT_FALSE(finished.load());
    EXPECT_EQ(future.wait_for(std::chrono::milliseconds(0)), std::future_status::timeout);

    auto release_data = make_release_worker_packet(static_cast<std::size_t>(workers.front()->get_id()));
    workers.front()->send_data(std::move(release_data), ControllerTaskIdx);

    ASSERT_EQ(future.wait_for(std::chrono::milliseconds(500)), std::future_status::ready);
    auto new_worker = future.get();
    ASSERT_NE(new_worker, nullptr);
    EXPECT_TRUE(finished.load());
    EXPECT_EQ(controller_.get_thread_pool_size(), THREAD_POOL_CAPACITY);
}

// test 3 - max out task queue
TEST(ControllerBufferTest, TaskQueueBlocksWhenFull) {
    CircularBuffer<int, 2> buffer;
    ASSERT_TRUE(buffer.emplace(1));
    ASSERT_TRUE(buffer.emplace(2));

    std::atomic<bool> started{false};
    std::atomic<bool> completed{false};
    auto producer = std::async(std::launch::async, [&] {
        started = true;
        buffer.emplace(3);
        completed = true;
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_TRUE(started.load());
    EXPECT_FALSE(completed.load());
    EXPECT_EQ(producer.wait_for(std::chrono::milliseconds(0)), std::future_status::timeout);

    auto first = buffer.pop();
    ASSERT_TRUE(first.has_value());
    EXPECT_EQ(*first, 1);

    ASSERT_EQ(producer.wait_for(std::chrono::milliseconds(200)), std::future_status::ready);
    EXPECT_TRUE(completed.load());

    auto second = buffer.pop();
    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(*second, 2);

    auto third = buffer.pop();
    ASSERT_TRUE(third.has_value());
    EXPECT_EQ(*third, 3);
}

// pool contents stay continuous when released worker in the middle of pool
TEST_F(ControllerTest, ReleaseWorkerMaintainsContiguousPool) {
    auto w0 = controller_.request_worker<PathPlanner>();
    auto w1 = controller_.request_worker<PathPlanner>();
    auto w2 = controller_.request_worker<PathPlanner>();
    ASSERT_EQ(controller_.get_thread_pool_size(), 3u);

    DefaultDataPacketT release;
    release.reset();
    release.insert(0, static_cast<std::int64_t>(CmdId::ReleaseWorker));
    release.insert(1, static_cast<std::int64_t>(w1->get_id()));
    controller_.release_worker(release);

    EXPECT_EQ(controller_.get_thread_pool_size(), 2u);
    ASSERT_NE(controller_.thread_pool[0], nullptr);
    ASSERT_NE(controller_.thread_pool[1], nullptr);
    EXPECT_NE(controller_.thread_pool[0], controller_.thread_pool[1]);
}

// data packet basics
TEST(DefaultDataPacketTest, InsertPopulatesTypesAndBits) {
    DefaultDataPacketT packet;
    packet.reset();

    std::array<char, 8> chars = {'o', 'k', '\0', '\0', '\0', '\0', '\0', '\0'};
    packet.insert(0, static_cast<std::int64_t>(42));
    packet.insert(1, 3.5);
    packet.insert(2, chars);

    EXPECT_EQ(packet.data_array_size, 3u);
    EXPECT_EQ(packet.data_array_types[0], static_cast<uint8_t>(DefaultTaskId::INT));
    EXPECT_EQ(packet.data_array_types[1], static_cast<uint8_t>(DefaultTaskId::DOUBLE));
    EXPECT_EQ(packet.data_array_types[2], static_cast<uint8_t>(DefaultTaskId::CHAR_ARR));

    std::int64_t int_val = static_cast<std::int64_t>(packet.data_array[0].to_ullong());
    EXPECT_EQ(int_val, 42);

    uint64_t raw_double = packet.data_array[1].to_ullong();
    double double_val = 0.0;
    std::memcpy(&double_val, &raw_double, sizeof(double_val));
    EXPECT_DOUBLE_EQ(double_val, 3.5);

    uint64_t raw_chars = packet.data_array[2].to_ullong();
    std::array<char, 8> restored{};
    std::memcpy(restored.data(), &raw_chars, restored.size());
    EXPECT_EQ(restored[0], 'o');
    EXPECT_EQ(restored[1], 'k');
}

// worker that sends shutdown to controller to release itself + stop the controller loop
TEST_F(ControllerTest, ShutdownCommandReleasesUiAndStopsController) {
    auto ui = controller_.request_worker<UiWorker>();
    auto viz = controller_.request_worker<VisualizationServerWorker>();
    ASSERT_EQ(controller_.get_thread_pool_size(), 2u);

    DefaultDataPacketT shutdown_packet;
    shutdown_packet.reset();
    shutdown_packet.insert(0, static_cast<std::int64_t>(CmdId::ShutdownController));
    // ask controller to release the UI worker
    shutdown_packet.insert(1, static_cast<std::int64_t>(ui->get_id()));

    ui->send_data(std::move(shutdown_packet), ControllerTaskIdx);

    ASSERT_TRUE(wait_for_pool_size(controller_, 0u, std::chrono::seconds(2)));
    ASSERT_TRUE(wait_for_state(controller_, States::SHUTDOWN, std::chrono::seconds(2)));
}
