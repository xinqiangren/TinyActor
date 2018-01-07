#include "TimerExcuteQue.h"
#include <ctime>
#include <stdio.h>
#include <chrono>

TimerExcuteQue::TimerExcuteQue(void)
	: m_tickUpdated(0)
	, m_tickStart(0)
	, m_tickCount(0)
	, m_timeStamp(0)
{
}

void TimerExcuteQue::Init(void)
{
	uint32_t current = 0;
	GetSecAndLeft10MS(m_tickStart, current);
	m_tickCount = current;
	m_timeStamp = Get10MSNum();
}

void TimerExcuteQue::Update(void)
{
	uint64_t currentStamp = Get10MSNum();
	if (currentStamp < m_timeStamp)
	{
		fprintf(stderr, "current timestamp less than last time stamp last[%lld]  current[%lld]\n", m_timeStamp, currentStamp);
		m_timeStamp = currentStamp;
	}
	else if (currentStamp != m_timeStamp)
	{
		uint32_t diff = (uint32_t)(currentStamp - m_timeStamp);
		m_timeStamp = currentStamp;
		m_tickCount += diff;
		for (uint32_t i = 0; i<diff; i++) 
		{
			EventDispatch();
		}
	}
}

void TimerExcuteQue::EventDispatch()
{
	m_lock.Lock();
	ExcuteTimeoutEvent();
	TimerListUpdate();
	ExcuteTimeoutEvent();
	m_lock.UnLock();
}

void TimerExcuteQue::ExcuteTimeoutEvent()
{
	int idx = m_tickUpdated & TIME_NEAR_MASK;
	while (m_nearTimer[idx].head.next)
	{
		TimerNode *current = m_nearTimer[idx].Clear();
		m_lock.UnLock();
		DispatchTimerList(current);
		m_lock.Lock();
	}
}

void TimerExcuteQue::TimerListUpdate()
{
	int mask = TIME_NEAR;
	uint32_t ct = ++ m_tickUpdated;
	if (ct == 0) 
	{
		TimerNode* current = m_levelTimer[3][0].Clear();
		while (current)
		{
			TimerNode *temp = current->next;
			AddTimerNode(current);
			current = temp;
		}
	}
	else 
	{
		uint32_t time = ct >> TIME_NEAR_SHIFT;
		int i = 0;

		while ((ct & (mask - 1)) == 0) 
		{
			int idx = time & TIME_LEVEL_MASK;
			if (idx != 0) {
				
				TimerNode* current = m_levelTimer[i][idx].Clear();
				while (current)
				{
					TimerNode *temp = current->next;
					AddTimerNode(current);
					current = temp;
				}
				break;
			}
			mask <<= TIME_LEVEL_SHIFT;
			time >>= TIME_LEVEL_SHIFT;
			++i;
		}
	}
}

void TimerExcuteQue::DispatchTimerList(TimerNode *current)
{
	while (current)
	{
		current->fun();
		TimerNode * temp = current;
		current = current->next;
		delete temp;
	}
}


void TimerExcuteQue::GetSecAndLeft10MS(uint32_t & sec, uint32_t & left) const
{
	int64_t milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
	sec = (uint32_t)(milliseconds / 1000);
	left = (uint32_t)((milliseconds % 1000) / 10);
}

uint64_t TimerExcuteQue::Get10MSNum(void) const
{
	int64_t milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
	return (milliseconds / 10);
}

void TimerExcuteQue::AddTimer(int time, std::function<void(void)> f)
{
	TimerNode *node = new TimerNode(f);
	m_lock.Lock();
	node->SetExpire(time + m_tickUpdated);
	AddTimerNode(node);
	m_lock.UnLock();
}

uint64_t TimerExcuteQue::GetTickMS(void) const
{
	return m_tickCount * 10;
}
time_t TimerExcuteQue::GetCurTime(void) const
{
	return m_tickStart + m_tickCount / 100;
}

void TimerExcuteQue::AddTimerNode(TimerNode *node)
{
	uint32_t time = node->expire;
	if ((time | TIME_NEAR_MASK) == (m_tickUpdated | TIME_NEAR_MASK))
	{
		m_nearTimer[time&TIME_NEAR_MASK].PushBack(node);
	}
	else
	{
		uint32_t mask = TIME_NEAR << TIME_LEVEL_SHIFT;
		int i = 0;
		for (; i<3; i++)
		{
			if ((time | (mask - 1)) == (m_tickUpdated | (mask - 1))) {
				break;
			}
			mask <<= TIME_LEVEL_SHIFT;
		}
		int idx = (time >> (TIME_NEAR_SHIFT + i*TIME_LEVEL_SHIFT)) & TIME_LEVEL_MASK;
		m_levelTimer[i][idx].PushBack(node);
	}
}
