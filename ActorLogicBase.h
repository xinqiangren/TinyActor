#pragma once
#include <stdint.h>
#include "TaskBase.h"
#include "TaskPull.h"
#include <atomic>

class ActorGroup;
class ActorLogicBase
{
public:
	ActorLogicBase(int64_t groupId, int64_t _id)
		: m_groupId(groupId)
		, m_id(_id)
		, m_group(NULL)
	{
		m_refCount.store(1, std::memory_order_relaxed);
	}

	ActorLogicBase(const ActorLogicBase& ) = delete;
	ActorLogicBase& operator=(const ActorLogicBase& ) = delete;

	inline bool DelRef(void) {
		int priRef = m_refCount.fetch_sub(1, std::memory_order_relaxed);
		if (priRef == 1) {
			delete this;
			return true;
		}
		return false;
	}

	inline void IncRef(void) {
		m_refCount.fetch_add(1, std::memory_order_relaxed);
	}

	inline int64_t GetId(void) { return m_id; }
	inline int64_t GetGroupId(void) { return m_groupId; }

	friend class ActorBase;
	inline void SetGroup(ActorGroup* context) { m_group = context; }
	
	std::atomic<int>	m_refCount;
	int64_t				m_groupId;
	int64_t				m_id;		
	ActorGroup*			m_group;

protected:
	virtual ~ActorLogicBase(void) {}
};