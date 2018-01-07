#pragma once
#include "TaskBase.h"
#include <functional>

struct TaskWithCallback  final : public TaskBase
{
	std::function<void(ActorLogicBase*)> m_call;

	TaskWithCallback(std::function<void(ActorLogicBase*)>&& call)
		: m_call(call)
	{
	}
	void Call(ActorLogicBase* logic)
	{
		m_call(logic);
	}
};