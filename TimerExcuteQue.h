#pragma once
#include <stdint.h>
#include "SpinLock.h"
#include <memory>
#include <functional>
#include <ctime>

#define TIME_NEAR_SHIFT 8
#define TIME_NEAR (1 << TIME_NEAR_SHIFT)
#define TIME_LEVEL_SHIFT 6
#define TIME_LEVEL (1 << TIME_LEVEL_SHIFT)
#define TIME_NEAR_MASK (TIME_NEAR-1)
#define TIME_LEVEL_MASK (TIME_LEVEL-1)

struct TimerNode {
	TimerNode *next;
	uint32_t expire;
	std::function<void(void)> fun;

	explicit TimerNode(const std::function<void(void)>& _fun)
		: next(0)
		, expire(0)
		, fun(_fun)
	{
	}

	TimerNode(void)
		: next(NULL)
		, expire(0)
	{
	}

	void SetExpire(uint32_t _expire)
	{
		expire = _expire;
	}
};

struct TimerList
{
	TimerNode head;
	TimerNode *tail;

	TimerList(void)
		: tail(0)
	{
		Clear();
	}

	void PushBack(TimerNode *node)
	{
		tail->next = node;
		tail = node;
		node->next = 0;
	}

	TimerNode* Clear(void)
	{
		TimerNode* ret = head.next;
		head.next = 0;
		tail = &head;
		return ret;
	}
};

class TimerExcuteQue
{
public:
	TimerExcuteQue(void);
	void Init(void);
	void Update(void);
	void AddTimer(int time, std::function<void(void)> f);
	uint64_t GetTickMS(void) const;
	time_t GetCurTime(void) const;
private:
	void AddTimerNode(TimerNode *node);
	void EventDispatch();
	void ExcuteTimeoutEvent();
	void TimerListUpdate();

	void DispatchTimerList(TimerNode *current);
	void GetSecAndLeft10MS(uint32_t & sec, uint32_t & left) const;
	uint64_t Get10MSNum(void) const;

	TimerList m_nearTimer[TIME_NEAR];
	TimerList m_levelTimer[4][TIME_LEVEL];
	SpinLock  m_lock;
	uint32_t  m_tickUpdated;
	uint32_t  m_tickStart;

	uint64_t  m_tickCount;
	uint64_t  m_timeStamp;
};