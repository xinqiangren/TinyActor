#pragma once
#include "TaskBase.h"
#include <type_traits>

struct TaskPullBase : public TaskBase
{
	TaskPullBase(void)
		: TaskBase(task_pull)
		, m_dstAckSeq(0)
		, m_ackNext(NULL)
	{
	}
	void Call(ActorLogicBase* logic) {}
	int64_t					m_dstAckSeq;
	TaskPullBase*			m_ackNext;
};



template<typename T>
struct TaskPullPod final : public TaskPullBase
{
	explicit TaskPullPod(void)
	{
		static_assert(std::is_pod<T>::value, "TaskPullPod template param must be pod type");
	}

	void SetReturnVal(const T& t)
	{
		m_returnVal = t;
	}
	T GetReturnVal(void)
	{
		return m_returnVal;
	}

	void Call(ActorLogicBase* logic) {}
	T	m_returnVal;
};


template<typename T>
struct TaskPullObj final : public TaskPullBase
{
	TaskPullObj(void) 
	: m_returnObj(NULL)
	{ 
		static_assert(std::is_class<T>::value, "TaskPullPod template param must be class type"); 
	}
	~TaskPullObj(void)
	{
		if (m_returnObj)
		{
			delete m_returnObj;
			m_returnObj = NULL;
		}
	}

	void SetReturnObjPtr(T* ptr)
	{
		m_returnObj = ptr;
	}

	void Call(ActorLogicBase* logic) {}

	T* FetchObjPtr(void)
	{
		T* ret = m_returnObj;
		m_returnObj = NULL;
		return ret;
	}

protected:
	T*  m_returnObj;
};