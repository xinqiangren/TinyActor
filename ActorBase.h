#pragma once
#include <memory>
#include <unordered_map>
#include <stdint.h>
#include <malloc.h>
#include "TaskBase.h"
#include "TaskPull.h"
#include "SpinLock.h"
#include "ActorLogicBase.h"
#include <string.h>



enum actor_state
{
	actor_free,
	actor_run,
	actor_block
};

class ActorGroup;
class ActorBase
{
public:
	friend class ActorGroup;
	ActorBase(const ActorBase&) = delete;
	ActorBase& operator=(const ActorBase&) = delete;

private:
	ActorBase(ActorGroup* group, ActorLogicBase* logic)
		: m_group(group)
		, m_logic(logic)
		, m_queue(NULL)
		, m_release(false)
		, m_head(0)
		, m_tail(0)
		, m_cap(DEFAULT_QUEUE_SIZE)
		, m_next(NULL)
		, m_actorState(actor_free)
		, m_blockPos(-1)
		, m_srcAckSeq(0)
	{
		m_id = logic->GetId();
		m_queue = (TaskBase**)malloc(sizeof(TaskBase*)* m_cap); // new ActorMessageBase[DEFAULT_QUEUE_SIZE];
		memset(m_queue, 0, sizeof(TaskBase*)* m_cap);
		logic->SetGroup(group);
	}

	void _PushReq(TaskBase* msg);
	bool PushReq(TaskBase* msg);

	template<typename... T>
	bool PushReqList(T*... task)
	{
		m_spinlock.Lock();
		std::initializer_list<int>({ (_PushReq(task), 0)... });
		m_spinlock.UnLock();
		return true;
	}
	bool PushReqArray(TaskBase** task, int count)
	{
		m_spinlock.Lock();
		for (int i = 0; i < count; ++i) {
			_PushReq(task[i]);
		}
		m_spinlock.UnLock();
		return true;
	}

	bool PushReqAndSetRelease(TaskBase* msg, bool release);
	bool PushReqArrayAndSetRelease(TaskBase** msg, int count, bool release);
	bool PushAck(TaskPullBase* msg);
	int  PollOne(void); //0 : need block 1 : have task after PoolOne 2 : no task after PollOne
	bool CheckNeedDel(void);
	void ProcessAckTask(void);
	void Release(void);
	inline void UnRelease(void)
	{
		m_spinlock.Lock();
		m_release = false;
		m_spinlock.UnLock();
	}

	virtual ~ActorBase(void)
	{
	//	assert(m_blockPos == -1);
		m_group = NULL;
		if (m_queue)
		{
			if (m_head < m_tail)
			{
				for (int i = m_head; i < m_tail; i++)
				{
					TaskBase* p = m_queue[i];
					m_queue[i] = NULL;
					delete p;
				}
			}
			else if (m_head > m_tail)
			{
				for (int i = m_head; i < m_cap; i++)
				{
					TaskBase* p = m_queue[i];
					m_queue[i] = NULL;
					delete p;
				}
				for (int i = 0; i < m_tail; i++)
				{
					TaskBase* p = m_queue[i];
					m_queue[i] = NULL;
					delete p;
				}
			}
			
			free(m_queue);
			m_queue = NULL;
		}
		
		if (m_logic) {
			m_logic->DelRef();
			m_logic = NULL;
		}

		m_next = NULL;
		m_group = NULL;
	}

	inline void ExpandQueue(void)
	{
		TaskBase** new_queue = (TaskBase**)malloc(sizeof(TaskBase*)* m_cap * 2);
		memset(new_queue, 0, sizeof(TaskBase*)* m_cap * 2);
		for (int i = 0; i<m_cap; i++) 
		{
			new_queue[i] = m_queue[(m_head + i) % m_cap];
		}
		m_head = 0;
		m_tail = m_cap;
		m_cap *= 2;
		
		free(m_queue);
		m_queue = new_queue;
	}

	bool								   m_release;
	int									   m_head;
	int									   m_tail;
	int									   m_cap;
	int									   m_blockPos;
	int64_t								   m_id;
	ActorGroup*							   m_group;
	
	ActorLogicBase*						   m_logic;
	TaskBase**							   m_queue;
	SpinLock							   m_spinlock;
	
	ActorBase*							   m_next;
	std::atomic<int>					   m_actorState;
	int64_t								   m_srcAckSeq;
};
