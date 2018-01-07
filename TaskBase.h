#pragma once
#include <stdint.h>
#include <memory>
#include <malloc.h>
#include <vector>
#include <cassert>
#include "TimerExcuteQue.h"
#include <limits>

extern TimerExcuteQue* g_pGlobalTimer;

class ActorLogicBase;
struct TaskBase;
struct TaskPullBase;

enum TaskType
{
	task_base = 1,
	task_async,
	task_pull,
};

#define DEFAULT_WAIT_MS 5000
#define TASK_MAX_WAIT_MS (1 << 15)
#define DEFAULT_QUEUE_SIZE 32
#define ACTOR_ID_BIT_NUM 46

struct TaskBase
{
	TaskBase(void)
		: m_taskType(task_base)
		, m_session(0)
		, m_src(0)
		, m_pull(NULL)
	{
	}

	explicit TaskBase(int taskType)
		: m_taskType(taskType)
		, m_session(0)
		, m_src(0)
		, m_pull(NULL)
	{
	}

	virtual ~TaskBase(void);

	TaskBase(const TaskBase&) = delete;
	TaskBase& operator=(const TaskBase&) = delete;

	virtual void Call(ActorLogicBase* logic) = 0;
	bool PushInGroup(ActorLogicBase* logic, int64_t dst, TaskBase* task);
	bool PushInContext(ActorLogicBase* logic, int64_t dstGroup, int64_t dst, TaskBase* task);
	bool AckPull(ActorLogicBase* logic);

	int						m_taskType;
	int64_t					m_src;
	int64_t					m_session;
	TaskPullBase*			m_pull;
};
