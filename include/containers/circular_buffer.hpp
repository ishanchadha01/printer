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
    };

    bool emplace(T&& data)
    {
        std::scoped_lock rw_lock(_rw_mutex);
        if ((((this->tail)+1) % BufferSize) == (this->head)) {
            std::cerr << "Buffer is full!\n";
            return false;
        }
        _data[this->tail] = std::move(data);
        this->tail = ((this->tail)+1) % BufferSize;
        return true;
    }

    bool push(const T& data)
    {
        std::scoped_lock rw_lock(_rw_mutex);
        if ((((this->tail)+1) % BufferSize) == (this->head)) {
            std::cerr << "Buffer is full!\n";
            return false;
        }
        _data[this->tail] = data;
        this->tail = ((this->tail)+1) % BufferSize;
        return true;
    }

    std::optional<T> pop()
    {
        std::scoped_lock rw_lock(_rw_mutex);
        if (this->head == this->tail) {
            std::cerr << "Buffer is empty!\n";
            return std::nullopt;
        }
        auto& old_front = _data[this->head];
        this->head = ((this->head)+1) % BufferSize;
        return std::move(old_front);
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
        if (this->head == this->tail) {
            std::cerr << "Buffer is empty!\n";
            return std::nullopt;
        }
        if (!cond_func(_data[this->head])) return std::nullopt;

        auto& old_front = _data[this->head];
        this->head = ((this->head)+1) % BufferSize;
        return std::move(old_front);
    }
    

private:
    std::array<T, BufferSize> _data;
    size_t head;
    size_t tail;
    std::mutex _rw_mutex;
};