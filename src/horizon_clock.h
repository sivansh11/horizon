#ifndef HORIZON_CLOCK_H
#define HORIZON_CLOCK_H

#include "horizon_core.h"

namespace horizon
{

typedef std::chrono::_V2::system_clock::time_point time;

class HorizonClock
{
public:
    HorizonClock()
    {
        from();
    }

    template <typename T>
    T till()
    {
        time newTime = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<T, std::chrono::seconds::period>(newTime - m_time).count();
    }
    template <typename T>
    T frameTime()
    {
        time newTime = std::chrono::high_resolution_clock::now();
        T timeDuration = std::chrono::duration<T, std::chrono::seconds::period>(newTime - m_time).count();
        m_time = newTime;
        return timeDuration;
    }
    void from()
    {
        m_time = std::chrono::high_resolution_clock::now();
    }

    void sleep(int milliseconds)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
    }

private:
    time m_time;
};

} // namespace horizon


#endif