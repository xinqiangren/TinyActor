#include "ActorContext.h"
#include <stdio.h>

void ActorContext::Run()
{
	ActorGroup* pGroup;
	if (m_groupNum <= 0){
		assert(0);
		return;
	}
	for (uint32_t i = 0; i < m_groupNum; i++){
		m_group[i]->InitTick();
	}
	while (!m_gracefullExit) {
		uint32_t pos = m_currentIndex.fetch_add(1, std::memory_order_relaxed) % m_groupNum;
		pGroup = m_group[pos];
		if (pGroup->RunOne()) {
			m_busyFlag |= (((uint64_t)1) << pos);
		}
		else {
			m_busyFlag &= ~(((uint64_t)1) << pos);
		}
		if ((m_busyFlag & m_busyFlagMask) == 0) {
			m_busyFlag = m_busyFlagMask;
			std::unique_lock<std::mutex> lock(m_mutex);
			if (m_gracefullExit)
				return;
			
			m_waitingThread++;
			m_condition.wait(lock);
			m_waitingThread--;
		}
	}	
}

bool ActorContext::ReqAndEnsureActorExist(ActorLogicBase* newLogicObj, int64_t dstGroup, TaskBase* task, 
	e_release_flag_proc oldActorReleaseFlagProc, e_release_flag_proc newActorReleaseFlagProc) {
	if (dstGroup <= 0 || dstGroup > m_groupNum) {
		fprintf(stderr, "ReqAndEnsureActorExist dstGroup <= 0 || dstGroup > m_groupNum!!!!!!!\n dstGroup = %lld", dstGroup);
		assert(0);
		newLogicObj->DelRef();
		delete task;
		return false;
	}

	ActorGroup* group = m_group[dstGroup - 1];
	return group->ReqAndEnsureActorExist(newLogicObj, task, oldActorReleaseFlagProc, newActorReleaseFlagProc);
}

bool ActorContext::ReqArrayAndEnsureActorExist(ActorLogicBase* newLogicObj, int64_t dstGroup, TaskBase** task, int count,
	e_release_flag_proc oldActorReleaseFlagProc, e_release_flag_proc newActorReleaseFlagProc) {
	if (dstGroup <= 0 || dstGroup > m_groupNum) {
		fprintf(stderr, "ReqAndEnsureActorExist dstGroup <= 0 || dstGroup > m_groupNum!!!!!!!\n dstGroup = %lld", dstGroup);
		assert(0);
		newLogicObj->DelRef();
		delete task;
		return false;
	}

	ActorGroup* group = m_group[dstGroup - 1];
	return group->ReqArrayAndEnsureActorExist(newLogicObj, task, count, oldActorReleaseFlagProc, newActorReleaseFlagProc);
}