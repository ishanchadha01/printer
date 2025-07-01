#pragma once

#include <iostream>
#include <mutex>
#include <array>
#include <optional>
#include <functional>

template<typename T, size_t BufferSize>
class CircularBuffer
{
public:
    CircularBuffer()
    {
        this->head = 0;
        this->tail = 0;
        this->size = 0;
        this->capacity = BufferSize;
    };

    bool emplace(T&& data)
    {
        std::scoped_lock rw_lock(_rw_mutex);
        buffer_full_cv.wait(rw_lock, [this] {
            return this->size < this->capacity;
        });
        _data[this->tail] = std::move(data);
        this->tail = ((this->tail)+1) % BufferSize;
        size++;
        buffer_empty_cv.notify_one();
        return true;
    }

    bool push(const T& data)
    {
        std::scoped_lock rw_lock(_rw_mutex);
        buffer_full_cv.wait(rw_lock, [this] {
            return this->size < this->capacity;
        });
        _data[this->tail] = data;
        this->tail = ((this->tail)+1) % BufferSize;
        size++;
        buffer_empty_cv.notify_one();
        return true;
    }

    std::optional<T> pop()
    {
        std::scoped_lock rw_lock(_rw_mutex);
        buffer_empty_cv.wait(rw_lock, [this] {
            return this->size > 0;
        });
        auto& old_front = _data[this->head];
        this->head = ((this->head)+1) % BufferSize;
        size--;
        buffer_full_cv.notify_one();
        T result = std::move(old_front);
        return result;
    }

    std::optional<std::reference_wrapper<T>> poll()
    {
        std::scoped_lock rw_lock(_rw_mutex);
        if (this->head == this->tail) {
            std::cerr << "Buffer is empty!\n";
            return std::nullopt;
        }
        return std::ref(_data[this->head]);
    }

    std::optional<T> pop_if(std::function<bool(const T&)> cond_func)
    {
        std::scoped_lock rw_lock(_rw_mutex);
        buffer_empty_cv.wait(rw_lock, [this] {
            return size > 0;
        });
        if (!cond_func(_data[this->head])) return std::nullopt;
        T result = std::move(_data[this->head]);
        this->head = (this->head + 1) % BufferSize;
        size--;
        buffer_full_cv.notify_one();
        return result;
    }
    

private:
    std::array<T, BufferSize> _data;
    size_t head;
    size_t tail;
    size_t size;
    size_t capacity;
    std::mutex _rw_mutex;
    std::condition_variable buffer_full_cv;
    std::condition_variable buffer_empty_cv;
};