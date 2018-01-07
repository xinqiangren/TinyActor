#pragma once
#include "ActorBase.h"
#include "SpinLock.h"
#include <cassert>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <stdint.h>
#include <unordered_map>
#include <vector>
#include "TaskBase.h"
#include "TaskPull.h"
#include "ActorLogicBase.h"
#include <atomic>
#include <unordered_map>
#include <vector>
#include <initializer_list>
#include "TaskClone.h"

enum e_release_flag_proc
{
	erfp_no_change,
	erfp_set_true,
	erfp_set_false
};
extern std::atomic<int64_t> g_sessionId;

class ActorGroup
{
public:
	explicit ActorGroup(int64_t goupId)
		: m_groupId(goupId)
		, m_listHead(NULL)
		, m_listTail(NULL)
		, m_updatePos(0)
		, m_lastUpdateTick(0)
		, m_inUpdate(false)
	{
		memset(m_blockActorHead, 0, sizeof(m_blockActorHead));
	}

	~ActorGroup();
	ActorGroup(const ActorGroup&) = delete;
	ActorGroup& operator=(const ActorGroup&) = delete;
	
	bool Req(int64_t dst, TaskBase* task, bool newSession);
	bool ReqAll(TaskClone* task, bool newSession);
	bool ReqAll(TaskClone* task, bool newSession, bool(*check_fun)(ActorBase* actor));
	bool Ack(TaskPullBase* pull);

	friend class ActorBase;
	friend class ActorContext;
	friend class ActorLogicBase;
private:
	bool Add(ActorLogicBase* Logic);
	bool ReqAndEnsureActorExist(ActorLogicBase* logic, TaskBase* task, e_release_flag_proc oldActorReleaseFlagProc, e_release_flag_proc newActorReleaseFlagProc);
	bool ReqArrayAndEnsureActorExist(ActorLogicBase* logic, TaskBase** task, int count, e_release_flag_proc oldActorReleaseFlagProc, e_release_flag_proc newActorReleaseFlagProc);

	void SoftErase(int64_t _id);
	void ReqAndSoftErase(int64_t id, TaskBase* task);


	template<typename... T>
	bool ReqMoreOne(int64_t dst, bool newSession, T*... task)
	{
		if (newSession)
		{
			int64_t session = g_sessionId.fetch_add(1, std::memory_order_relaxed);
			std::initializer_list<int>({ (task->m_session = session, 0)... });
		}
	
		m_mapSpinLock.Lock();
		auto it = m_mpActors.find(dst);
		if (it != m_mpActors.end())
		{
			auto actor = it->second;
			bool ret = actor->PushReqList(task...);
			m_mapSpinLock.UnLock();
			return ret;
		}
		else
		{
			m_mapSpinLock.UnLock();
			return false;
		}
	}
	bool ReqArray(int64_t dst, bool newSession, TaskBase**task, int count)
	{
		if (newSession) {
			for (int i = 0; i < count; ++i) {
				int64_t session = g_sessionId.fetch_add(1, std::memory_order_relaxed);
				task[i]->m_session = session;
			}
		}
		m_mapSpinLock.Lock();
		auto it = m_mpActors.find(dst);
		if (it != m_mpActors.end()) {
			auto actor = it->second;
			bool ret = actor->PushReqArray(task, count);
			m_mapSpinLock.UnLock();
			return ret;
		}
		else {
			m_mapSpinLock.UnLock();
			return false;
		}
	}

	bool RunOne(void);
	void InitTick(void);
	inline void PushBackToList(ActorBase* actor)
	{
		m_listSpinlock.Lock();
		assert(actor->m_next == NULL);
		if (m_listTail)
		{
			m_listTail->m_next = actor;
			m_listTail = actor;
		}
		else
		{
			m_listHead = m_listTail = actor;
		}
		m_listSpinlock.UnLock();
	}

	inline ActorBase* PopFromList(void)
	{
		ActorBase* actor;
		m_listSpinlock.Lock();
		actor = m_listHead;
		if (actor)
		{
			m_listHead = actor->m_next;
			if (m_listHead == NULL)
			{
				assert(actor == m_listTail);
				m_listTail = NULL;
			}
			actor->m_next = NULL;
		}
		m_listSpinlock.UnLock();
		return actor;
	}

	inline void EraseFromBlock(ActorBase * actor)
	{
		int64_t _id = actor->m_id;
		std::unordered_map<int64_t, ActorBase*>* pMap;
		m_updateSpinLock.Lock();
		if (pMap = m_blockActorHead[actor->m_blockPos])
		{
			pMap->erase(_id);
		}
		actor->m_blockPos = -1;
		m_updateSpinLock.UnLock();
	}

	inline void UpdateBlockActor(uint64_t curMs)
	{
		if (curMs > m_lastUpdateTick + 2) // 3 ms one time
		{
			if (!m_inUpdate) //此处实时性要求不是非常严格，所以不在锁内访问 m_inUpdate
			{
				m_inUpdate = true;

				int64_t updateNum = 0;
				std::unordered_map<int64_t, ActorBase*>* tmp = NULL;
				int pos;
				std::vector<std::unordered_map<int64_t, ActorBase*>*> fetchmap;
				m_updateSpinLock.Lock();
				curMs = g_pGlobalTimer->GetTickMS();
				if (curMs > m_lastUpdateTick)
				{
					updateNum = curMs - m_lastUpdateTick;
					if (updateNum > TASK_MAX_WAIT_MS)
						updateNum = TASK_MAX_WAIT_MS;

					while (--updateNum >= 0) {
						pos = (m_updatePos & (TASK_MAX_WAIT_MS - 1));
						if (tmp = m_blockActorHead[pos]) {
							fetchmap.push_back(tmp);
							m_blockActorHead[pos] = NULL;
						}
						m_updatePos++;
					}
					m_lastUpdateTick = curMs;
				}
				m_updateSpinLock.UnLock();
				
				int sz = fetchmap.size();
				if (sz > 0) {
					for (int i = 0; i < sz; i++) {
						tmp = fetchmap[i];
						auto it = tmp->begin();
						for (; it != tmp->end(); it++) {
							if (auto p = it->second) {
								BlockToRunOnTimeout(p);
							}
						}
						delete  tmp;
					}
				}
				m_inUpdate = false;
			}
		}	
	}

	

	inline void FreeToRun(ActorBase* actor)
	{
		int exp = actor_free;
		int newval = actor_run;
		
		if (actor->m_actorState.compare_exchange_strong(exp, newval)) {
			PushBackToList(actor);
		}
	}

	inline void BlockToRunOnAckAll(ActorBase* actor)
	{
		int exp = actor_block;
		int newval = actor_run;
	
		if (actor->m_actorState.compare_exchange_strong(exp, newval)) {
			EraseFromBlock(actor);
			PushBackToList(actor);
		}
	}


	inline void BlockToRunOnTimeout(ActorBase* actor)
	{
		int exp = actor_block;
		int newval = actor_run;

		if (actor->m_actorState.compare_exchange_strong(exp, newval)) {
			PushBackToList(actor);
		}
	}


	inline void RunToBlock(ActorBase* actor, int blockMS)
	{
		int64_t _id = actor->m_id;
		std::unordered_map<int64_t, ActorBase*>* pMap;
		int toPos;
		m_updateSpinLock.Lock();
		toPos = ((m_updatePos + blockMS - 1) & (TASK_MAX_WAIT_MS - 1));
		pMap = m_blockActorHead[toPos];
		if (NULL == pMap)
		{
			m_blockActorHead[toPos] = pMap = new std::unordered_map<int64_t, ActorBase*>();
		}
		(*pMap)[_id] = actor;
		actor->m_blockPos = toPos;
		actor->m_actorState.store(actor_block, std::memory_order_relaxed);
		m_updateSpinLock.UnLock();
	}

	inline void RunToFree(ActorBase* actor)
	{
		int exp = actor_run;
		int newval = actor_free;

		actor->m_actorState.compare_exchange_strong(exp, newval);
	}

	SpinLock												 m_mapSpinLock;
	std::unordered_map<int64_t, ActorBase* >				 m_mpActors;

	SpinLock												 m_listSpinlock;
	ActorBase*												 m_listHead;
	ActorBase*												 m_listTail;
	
	SpinLock												 m_updateSpinLock;
	std::unordered_map<int64_t, ActorBase*>*				 m_blockActorHead[TASK_MAX_WAIT_MS];
	int			      										 m_updatePos;
	uint64_t												 m_lastUpdateTick;
	bool													 m_inUpdate;
	int64_t													 m_groupId;
};