#pragma once
#ifndef _WIN32
#ifndef LC_INCLUDE
#define LC_INCLUDE "lc-addrlabels.h"
#endif
#endif

#include "TaskBase.h"
#include "TaskPull.h"
#include "pt.h"
#include "ActorLogicBase.h"

struct TaskAsync : public TaskBase
{
	TaskAsync(void)
		: TaskBase(task_async)
		, m_ackNum(0)
		, m_pullNum(0)
		, m_blockMS(0)
		, m_srcAckSeq(0)
		, m_ackHead(NULL)
	{
		PT_INIT(&m_pt);
	}

	void Call(ActorLogicBase* logic) final {}
	template<typename task_type, typename... task_params_type>
	bool Pull(ActorLogicBase* logic, int64_t dstGroup, int64_t dst, int waitTime, task_params_type... args)
	{
#ifdef __DISABLE_PULL_IN_THE_SAME_GROUP__
		if (logic->GetGroupId() == dstGroup)
		{
			fprintf(stderr, "!!!!! Pull error logic groupid[%lld] dstGroup[%lld] \n", logic->GetGroupId(), dstGroup);
			assert(0);
			return false;
		}
#endif
		task_type* task = task_type::CreateWithPull(args...);
		TaskPullBase* pull = task->m_pull;
		assert(pull);
		pull->m_src = task->m_src = (logic->GetId() | (logic->GetGroupId() << ACTOR_ID_BIT_NUM));
		pull->m_session = task->m_session = m_session;
		pull->m_dstAckSeq = m_srcAckSeq;
		if (_ReqContext(dstGroup, dst, task, false)) {
			if (waitTime == 0)
				waitTime = DEFAULT_WAIT_MS;
			else if (waitTime < 0 || waitTime > TASK_MAX_WAIT_MS)
				waitTime = TASK_MAX_WAIT_MS;

			if (m_blockMS < waitTime)
				m_blockMS = waitTime;

			m_pullNum++;
			return true;
		}
		return false;
	}

	bool _ReqContext(int64_t group, int64_t dst, TaskBase* task, bool newSession);
	virtual int AsyncCall(ActorLogicBase* logic, TaskPullBase* ack, int num, bool timeout) = 0;

	virtual ~TaskAsync(void)
	{
		while (m_ackHead)
		{
			TaskBase* p = m_ackHead;
			m_ackHead = m_ackHead->m_ackNext;
			delete p;
		}
	}


	inline void ResetAckData(void)
	{
		m_ackHead = NULL;
		m_ackNum = 0;
		m_blockMS = 0;
		m_pullNum = 0;
		m_srcAckSeq++;
	}

	inline bool AppendAck(TaskPullBase* task)
	{
		task->m_ackNext = m_ackHead;
		m_ackHead = task;
		if (++m_ackNum >= m_pullNum)
		{
			return true;
		}
		return false;
	}

	int						m_ackNum;
	int						m_pullNum;
	int						m_blockMS;
	int64_t					m_srcAckSeq;
	TaskPullBase*			m_ackHead;
	struct pt				m_pt;
};