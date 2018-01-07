#pragma once
#include "ActorGroup.h"
#include <atomic>
#include <initializer_list>
#include <functional>

#define MAX_GROUP_NUM 64
class ActorContext
{
public:
	ActorContext(int threadnum)
		: m_currentIndex(0)
		, m_gracefullExit(0)
		, m_groupNum(0)
		, m_busyFlagMask(0)
		, m_busyFlag(0)
		, m_waitingThread(0)
		, m_workThreadNum(threadnum)
	{
		memset(m_group, 0, sizeof(m_group));
	}

	~ActorContext(void)
	{
		for (uint32_t i = 0; i < m_groupNum; i++)
		{
			delete m_group[i];
			m_group[i] = NULL;
		}
		m_workThreads.clear();
	}

	void Start(void)
	{
		for (int i = 0; i < m_workThreadNum; i++)
		{
			std::shared_ptr<std::thread>  pt(new std::thread(&ActorContext::Run, this));
			m_workThreads.push_back(pt);
		}
	}

	void WaitExit(void)
	{
		auto size = m_workThreads.size();
		for (size_t i = 0; i < size; i++)
		{
			std::shared_ptr<std::thread> pthread = m_workThreads[i];
			pthread->join();
		}
	}

	void AddGroup(int64_t groupId) // groupId master continuous and from 1
	{
		if (groupId <= 0 && groupId > MAX_GROUP_NUM)
		{
			assert(0);
			return;
		}
		if (m_group[groupId - 1] != 0)
		{
			assert(0);
			return;
		}
		m_group[groupId - 1] = new ActorGroup(groupId);
		m_groupNum++;
		m_busyFlagMask = ((((int64_t)1) << (groupId - 1)) | m_busyFlagMask);
		m_busyFlag = m_busyFlagMask;
	}

	void GracefullExit(void)
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_gracefullExit = 1;
		m_condition.notify_all();
	}

	void WakeWork(void)
	{
		if (m_waitingThread > 0)
		{
			m_condition.notify_one();
		}
	}

	bool AddActor(ActorLogicBase* logic)
	{
		int64_t groupId = logic->GetGroupId();
		if (groupId <= 0 || groupId > m_groupNum)
		{
			assert(0);
			return false;
		}
		return m_group[groupId - 1]->Add(logic);
	}

	// true : actor created with param logic  false : new actor deleted, anyway will send the task
	bool ReqAndEnsureActorExist(ActorLogicBase* newLogicObj, int64_t dstGroup, TaskBase* task,
		e_release_flag_proc oldActorReleaseFlagProc, e_release_flag_proc newActorReleaseFlagProc);

	bool ReqArrayAndEnsureActorExist(ActorLogicBase* newLogicObj, int64_t dstGroup, TaskBase**task, int count,
		e_release_flag_proc oldActorReleaseFlagProc, e_release_flag_proc newActorReleaseFlagProc);
	
	void SoftErase(int64_t groupId, int64_t _id)
	{
		if (groupId <= 0 || groupId > m_groupNum)
		{
			assert(0);
			return;
		}
		return m_group[groupId - 1]->SoftErase(_id);
	}

	void ReqAndSoftErase(int64_t groupId, int64_t _id, TaskBase* task)
	{
		if (groupId <= 0 || groupId > m_groupNum)
		{
			assert(0);
			return;
		}
		return m_group[groupId - 1]->ReqAndSoftErase(_id, task);
	}

	bool Req(int64_t groupId, int64_t dst, TaskBase* task, bool newSession)
	{
		if (groupId <= 0 || groupId > m_groupNum)
		{
			assert(0);
			delete task;
			return false;
		}
		bool ret = m_group[groupId - 1]->Req(dst, task, newSession);
		if (!ret)
			delete task;

		return ret;
	}

	bool TryReq(int64_t groupId, int64_t dst, TaskBase* task, bool newSession)
	{
		if (groupId <= 0 || groupId > m_groupNum)
		{
			assert(0);
			delete task;
			return false;
		}
		bool ret = m_group[groupId - 1]->Req(dst, task, newSession);

		return ret;
	}

	template<typename... T>
	bool ReqMoreOne(int64_t groupId, int64_t dst, bool newSession, T*... task)
	{
		if (groupId <= 0 || groupId > m_groupNum) {
			assert(0);
			return false;
		}

		bool ret = m_group[groupId - 1]->ReqMoreOne(dst, newSession, task...);
		if (!ret)
			std::initializer_list<int>{(delete task, 0)...};

		return ret;
	}
	bool ReqArray(int64_t groupId, int64_t dst, bool newSession, TaskBase** task, int count) {
		if (groupId <= 0 || groupId > m_groupNum) {
			assert(0);
			return false;
		}
		bool ret = m_group[groupId - 1]->ReqArray(dst, newSession, task, count);
		if (!ret) {
			for (int i = 0; i < count; ++i) {
				delete task[i];
			}
		}
		return ret;
	}
	bool Ack(int64_t groupId, TaskPullBase* pull)
	{
		if (groupId <= 0 || groupId > m_groupNum)
		{
			assert(0);
			return false;
		}
		return m_group[groupId - 1]->Ack(pull);
	}
private:
	void Run(void);
	int														 m_gracefullExit;
	std::atomic<uint32_t>									 m_currentIndex;
	int64_t													 m_groupNum;
	uint64_t												 m_busyFlagMask;
	uint64_t												 m_busyFlag;

	std::mutex												 m_mutex;
	std::condition_variable									 m_condition;
	int														 m_waitingThread;
	std::vector<std::shared_ptr<std::thread> >				 m_workThreads;
	int														 m_workThreadNum;
	ActorGroup*												 m_group[MAX_GROUP_NUM];
};

extern ActorContext* g_context;