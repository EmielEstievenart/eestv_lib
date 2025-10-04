
#pragma once
#include <atomic>

namespace eestv
{

class SpinlockMutex
{
private:
    std::atomic_flag _flag;

public:
    SpinlockMutex() : _flag {ATOMIC_FLAG_INIT} { }

    void lock()
    {
        while (_flag.test_and_set(std::memory_order_acquire))
        {
            // spin-wait
        }
    }

    void unlock() { _flag.clear(std::memory_order_release); }
};
}