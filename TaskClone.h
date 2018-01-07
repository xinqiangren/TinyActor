#pragma once
#include "TaskBase.h"
struct TaskClone : public TaskBase
{
	virtual TaskClone* Clone(void) = 0;
};