#pragma once
#include "ActorContext.h"
#include "TimerExcuteQue.h"
#include <cstdint>
#include <iostream>
#include <string>
using namespace std;

TimerExcuteQue* g_pGlobalTimer = NULL; //定时器,处理定时事件,actor框架本身也依赖它唤醒空闲线程,处理任务超时
bool g_stopTimer = false; //  true: 停止运行
ActorContext* g_context = NULL; //actor环境
#define TEST_GROUP_ID 1 //组id,工作线程在多个组之间轮叫调度,从而保证在组与组的actor数量不同的情况下cpu占用的公平性，避免饿死
						//样例中只使用了一个actor组，实际中可以自行配置 

void RunTimer(void) //定时器线程函数
{
	while (!g_stopTimer)
	{
		g_pGlobalTimer->Update();
		std::this_thread::sleep_for(std::chrono::milliseconds(3));
		if (g_context)
		{
			g_context->WakeWork();
		}
	}
}

//样例中启动3个工作线程,实际中可以自行配置
#define _TEST_INIT_ \
	g_pGlobalTimer = new TimerExcuteQue; \
	g_pGlobalTimer->Init(); \
	std::thread timerThread(RunTimer); \
	g_context = new ActorContext(3); \
	g_context->AddGroup(TEST_GROUP_ID);  \
	g_context->Start();
