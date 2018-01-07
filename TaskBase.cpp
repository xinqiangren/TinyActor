#include "TaskBase.h"
#include "ActorGroup.h"
#include "ActorContext.h"

extern ActorContext* g_context;
extern std::atomic<int64_t> g_sessionId;

bool TaskBase::PushInGroup(ActorLogicBase* logic, int64_t dst, TaskBase* task)
{
	assert(logic->GetId() != dst);
	int64_t session = g_sessionId.fetch_add(1);
	task->m_src = (logic->GetId() | (logic->GetGroupId() << ACTOR_ID_BIT_NUM)); //logic->GetId();
	task->m_session = session;
	return logic->m_group->Req(dst, task, false);
}

bool TaskBase::PushInContext(ActorLogicBase* logic, int64_t dstGroup, int64_t dst, TaskBase* task)
{
	if (logic->GetGroupId() == dstGroup)
	{
		assert(logic->GetId() != dst);
	}

	int64_t session = g_sessionId.fetch_add(1);
	task->m_src = (logic->GetId() | (logic->GetGroupId() << ACTOR_ID_BIT_NUM)); //logic->GetId();
	task->m_session = session;
	return g_context->Req(dstGroup, dst, task, false);
}

bool TaskBase::AckPull(ActorLogicBase* logic)
{
	assert(m_pull);

	int64_t dstGroup = (m_pull->m_src >> ACTOR_ID_BIT_NUM);
	bool ackSucess = true;
	if (logic->m_groupId == dstGroup) {
		ackSucess = logic->m_group->Ack(m_pull);
	}
	else {
		ackSucess = g_context->Ack(dstGroup, m_pull);
	}
	if (ackSucess) {
		m_pull = NULL;
	}
	return ackSucess;
}

TaskBase::~TaskBase(void)
{
	if (m_pull)
	{
		delete m_pull;
		m_pull = NULL;
	}
}