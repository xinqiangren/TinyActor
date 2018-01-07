#pragma once
#include <atomic>
#include <thread>

class SpinLock
{
public:
	SpinLock(void)
	{
		m_flag.clear();
	}

    void Lock()
    {
		int poolnum = 0;
		while (m_flag.test_and_set(std::memory_order_acquire))
		{
			if ((++poolnum & 8191) == 0)
			{
				std::this_thread::yield();
			}
		}
    }

	bool TryLock()
	{
		return !m_flag.test_and_set(std::memory_order_acquire);
	}

    void UnLock()
    {
		m_flag.clear(std::memory_order_release);
    }

private:
	std::atomic_flag m_flag;
};