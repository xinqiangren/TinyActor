#include "ActorGroup.h"
#include <cassert>
#include <thread>

std::atomic<int64_t> g_sessionId(1);
ActorGroup::~ActorGroup()
{
	for (auto pr : m_mpActors)
	{
		delete pr.second;
		pr.second = NULL;
	}

	m_mpActors.clear();
	for (int i = 0; i < TASK_MAX_WAIT_MS; ++i)
	{
		if (m_blockActorHead[i])
		{
			delete m_blockActorHead[i];
			m_blockActorHead[i] = NULL;
		}
	}
}

bool ActorGroup::Add(ActorLogicBase* Logic)
{
	int64_t _id = Logic->GetId();
	ActorBase* actor = new ActorBase(this, Logic);
	m_mapSpinLock.Lock();
	auto it = m_mpActors.find(_id);
	if (it != m_mpActors.end())
	{
		m_mapSpinLock.UnLock();
		delete actor;
		assert(0);
		return false;
	}
	else
	{
		m_mpActors[_id] = actor;
		m_mapSpinLock.UnLock();
		return true;
	}
}

void ActorGroup::SoftErase(int64_t _id)
{
	m_mapSpinLock.Lock();
	auto it = m_mpActors.find(_id);
	if (it != m_mpActors.end())
	{
		auto actor = it->second;
		actor->Release();
	}
	m_mapSpinLock.UnLock();
}

void ActorGroup::ReqAndSoftErase(int64_t id, TaskBase* task)
{
	bool reqAdded = false;
	m_mapSpinLock.Lock();
	auto it = m_mpActors.find(id);
	if (it != m_mpActors.end())
	{
		auto actor = it->second;
		reqAdded = actor->PushReqAndSetRelease(task, true);
	}
	m_mapSpinLock.UnLock();
	if (!reqAdded) {
		delete task;
	}
}

bool ActorGroup::Req(int64_t dst, TaskBase* task, bool newSession)
{
	if (newSession)
		task->m_session = g_sessionId.fetch_add(1, std::memory_order_relaxed);

	m_mapSpinLock.Lock();
	auto it = m_mpActors.find(dst);
	if (it != m_mpActors.end())
	{
		auto actor = it->second;
		bool ret = actor->PushReq(task);
		m_mapSpinLock.UnLock();
		return ret;
	}
	else
	{
		m_mapSpinLock.UnLock();
		return false;
	}
}

bool ActorGroup::ReqAll(TaskClone* task, bool newSession)
{
	if (newSession)
		task->m_session = g_sessionId.fetch_add(1, std::memory_order_relaxed);

	m_mapSpinLock.Lock();
	auto it = m_mpActors.begin();
	for (; it != m_mpActors.end(); it++) {
		auto actor = it->second;
		TaskClone* clone = task->Clone();
		actor->PushReq(clone);
	}
	m_mapSpinLock.UnLock();
	return true;
}

bool ActorGroup::ReqAll(TaskClone* task, bool newSession, bool(*check_fun)(ActorBase* actor))
{
	if (newSession)
		task->m_session = g_sessionId.fetch_add(1, std::memory_order_relaxed);

	m_mapSpinLock.Lock();
	auto it = m_mpActors.begin();
	for (; it != m_mpActors.end(); it++) {
		auto actor = it->second;
		if (check_fun(actor)) {
			TaskClone* clone = task->Clone();
			actor->PushReq(clone);
		}
	}
	m_mapSpinLock.UnLock();
	return true;
}

bool ActorGroup::Ack(TaskPullBase* pull)
{
	int64_t dst = (pull->m_src & ((((int64_t)1) << ACTOR_ID_BIT_NUM) - 1));
	m_mapSpinLock.Lock();
	auto it = m_mpActors.find(dst);
	if (it != m_mpActors.end())
	{
		auto actor = it->second;
		bool ret = actor->PushAck(pull);
		m_mapSpinLock.UnLock();
		return ret;
	}
	else
	{
		m_mapSpinLock.UnLock();
		return false;
	}
}

void ActorGroup::InitTick(void)
{
	m_updateSpinLock.Lock();
	if (0 == m_lastUpdateTick)
		m_lastUpdateTick = g_pGlobalTimer->GetTickMS();
	m_updateSpinLock.UnLock();
}

bool ActorGroup::ReqAndEnsureActorExist(ActorLogicBase* logic, TaskBase* task, 
	e_release_flag_proc oldActorReleaseFlagProc, e_release_flag_proc newActorReleaseFlagProc)
{
	int64_t _id = logic->GetId();
	ActorBase* actor = new ActorBase(this, logic);
	if (newActorReleaseFlagProc != erfp_no_change) {
		actor->m_release = (newActorReleaseFlagProc == erfp_set_true ? true : false);
	}
	m_mapSpinLock.Lock();
	auto it = m_mpActors.find(_id);
	if (it != m_mpActors.end()) {
		if (oldActorReleaseFlagProc != erfp_no_change)
			it->second->PushReqAndSetRelease(task, oldActorReleaseFlagProc == erfp_set_true ? true : false);
		else
			it->second->PushReq(task);

		m_mapSpinLock.UnLock();
		delete actor;
		return false;
	}
	else {
		m_mpActors[_id] = actor;
		actor->PushReq(task);
		m_mapSpinLock.UnLock();
		return true;
	}
}

bool ActorGroup::ReqArrayAndEnsureActorExist(ActorLogicBase* logic, TaskBase** task, int count,
	e_release_flag_proc oldActorReleaseFlagProc, e_release_flag_proc newActorReleaseFlagProc)
{
	int64_t _id = logic->GetId();
	ActorBase* actor = new ActorBase(this, logic);
	if (newActorReleaseFlagProc != erfp_no_change) {
		actor->m_release = (newActorReleaseFlagProc == erfp_set_true ? true : false);
	}
	m_mapSpinLock.Lock();
	auto it = m_mpActors.find(_id);
	if (it != m_mpActors.end()) {
		if (oldActorReleaseFlagProc != erfp_no_change)
			it->second->PushReqArrayAndSetRelease(task, count, oldActorReleaseFlagProc == erfp_set_true ? true : false);
		else
			it->second->PushReqArray(task, count);

		m_mapSpinLock.UnLock();
		delete actor;
		return false;
	}
	else {
		m_mpActors[_id] = actor;
		actor->PushReqArray(task, count);
		m_mapSpinLock.UnLock();
		return true;
	}
}

bool ActorGroup::RunOne()
{
	UpdateBlockActor(g_pGlobalTimer->GetTickMS());
	ActorBase* actor = PopFromList();
	if (NULL == actor)
		return false;

	//0 : maybe need block 1 : have task after PoolOne 2 : no task after PollOne
	int ret = actor->PollOne();
	if (0 == ret) {
		actor->ProcessAckTask();
	}
	else if (1 == ret) {
		PushBackToList(actor);
	}
	else {
		bool bDel = false;
		m_mapSpinLock.Lock();
		int64_t _id = actor->m_id;
		if (actor->CheckNeedDel()) {
			auto it = m_mpActors.find(_id);
			if (it != m_mpActors.end()) {
				m_mpActors.erase(it);
				bDel = true;
			}
		}
		m_mapSpinLock.UnLock();
		if (bDel) {
			delete actor;
		}
	}
	return true;
}