#include "ActorBase.h"
#include "TaskAsync.h"
#include "ActorGroup.h"
#include "TimerExcuteQue.h"
#include <stdio.h>

extern  TimerExcuteQue* g_pGlobalTimer;

void ActorBase::_PushReq(TaskBase* msg)
{
	m_queue[m_tail] = msg;
	if (++m_tail >= m_cap) {
		m_tail = 0;
	}
	if (m_head == m_tail) {
		ExpandQueue();
	}

	m_group->FreeToRun(this);
}

bool ActorBase::PushReq(TaskBase* msg)
{
	m_spinlock.Lock();
	_PushReq(msg);
	m_spinlock.UnLock();
	return true;
}

bool ActorBase::PushReqAndSetRelease(TaskBase* msg, bool release)
{
	m_spinlock.Lock(); 
	m_release = release;
	_PushReq(msg);
	m_spinlock.UnLock();
	return true;
}

bool ActorBase::PushReqArrayAndSetRelease(TaskBase** msg, int count, bool release)
{
	m_spinlock.Lock();
	m_release = release;
	for (int i = 0; i < count; ++i) {
		_PushReq(msg[i]);
	}
	m_spinlock.UnLock();
	return true;
}

bool ActorBase::PushAck(TaskPullBase* msg)
{
	m_spinlock.Lock();
	if (m_tail != m_head) {
		TaskBase* head = m_queue[m_head];
		assert(head->m_taskType == task_async);
		TaskAsync* asyncTask = static_cast<TaskAsync*>(head);
		if (asyncTask->m_session == msg->m_session) {
			if (asyncTask->m_srcAckSeq == msg->m_dstAckSeq) {
				if (asyncTask->AppendAck(msg)) {
					m_group->BlockToRunOnAckAll(this);
				}
				m_spinlock.UnLock();
				return true;
			}
		}
	}
	m_spinlock.UnLock();
	return false;
}

//0 : maybe need block 1 : have task after PoolOne 2 : no task after PollOne
int ActorBase::PollOne(void)
{
	int ret = 2;
	TaskPullBase* ackList = NULL;
	int ackNum = 0;
	int pullnum = 0;
	
	TaskAsync* taskAsync = NULL;
	m_spinlock.Lock();
	if (m_head != m_tail) {
		TaskBase* message = m_queue[m_head];
		if (message->m_taskType == task_async) {
			taskAsync = static_cast<TaskAsync*>(message);
			ackList = taskAsync->m_ackHead;
			ackNum = taskAsync->m_ackNum;
			pullnum = taskAsync->m_pullNum;
			taskAsync->ResetAckData();
			m_spinlock.UnLock();

			if (pullnum > ackNum) {
				taskAsync->AsyncCall(m_logic, ackList, ackNum, true);
			}
			else if (ackList) {
				taskAsync->AsyncCall(m_logic, ackList, ackNum, false);
			}
			else {
				taskAsync->AsyncCall(m_logic, NULL, 0, false);
			}
			while (ackList) {
				TaskPullBase* p = ackList;
				ackList = ackList->m_ackNext;
				delete p;
			}
		}
		else {
			m_spinlock.UnLock();
			message->Call(m_logic);
		}
		

		m_spinlock.Lock();
		if (taskAsync) {
			if (taskAsync->m_blockMS <= 0) {
				delete message;
				m_queue[m_head] = NULL;
				if (++m_head >= m_cap)
					m_head = 0;

				if (m_head != m_tail) {
					ret = 1;
				}
			}
			else {
				if (taskAsync->m_ackNum < taskAsync->m_pullNum)
					ret = 0;
				else
					ret = 1;
			}
		}
		else {
			delete message;
			m_queue[m_head] = NULL;
			if (++m_head >= m_cap)
				m_head = 0;

			if (m_head != m_tail) {
				ret = 1;
			}
		}
	}
	m_spinlock.UnLock();
	return ret;
}

bool ActorBase::CheckNeedDel(void)
{
	m_spinlock.Lock();
	if (m_head == m_tail) {
		if (m_release) {
			m_spinlock.UnLock();
			return true;
		}
		else {
			m_group->RunToFree(this);
		}
	}
	else {
		m_group->PushBackToList(this);
	}
	m_spinlock.UnLock();
	return false;
}

void ActorBase::ProcessAckTask(void)
{
	TaskAsync* message = static_cast<TaskAsync*>(m_queue[m_head]);
	m_spinlock.Lock();
	if (message->m_ackNum >= message->m_pullNum) {
		m_group->PushBackToList(this);
	}
	else {
		m_group->RunToBlock(this, message->m_blockMS);
	}
	m_spinlock.UnLock();
}

void ActorBase::Release(void)
{
	m_spinlock.Lock();
	m_release = true;
	m_group->FreeToRun(this);
	m_spinlock.UnLock();
}
