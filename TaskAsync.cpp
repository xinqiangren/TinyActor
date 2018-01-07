#include "TaskAsync.h"
#include "ActorGroup.h"
#include "ActorContext.h"

extern  ActorContext* g_context;
bool  TaskAsync::_ReqContext(int64_t group, int64_t dst, TaskBase* task, bool newSession)
{
	return g_context->Req(group, dst, task, false);
}
