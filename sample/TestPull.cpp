#include "TestBase.h"
#include "TaskAsync.h"

//每个actor保存了一个逻辑对象的指针，每个逻辑对象都有一个所在actor group里的唯一的id
class Friend : public ActorLogicBase
{
public:
	Friend(int64_t id, const string& myname)
		: ActorLogicBase(TEST_GROUP_ID, id)
		, m_myName(myname)
	{
	}
	void SayHiTo(const string& name)
	{
		cout << "hi! " << name << "!" << " my name is " << m_myName << "." << endl;
	}
	const string& GetName(void)
	{
		return m_myName;
	}
private:
	string m_myName;
};

//每个任务都会分配给一个工作线程执行,调用call. 同一个actor的任务保证按进入顺序串行执行,但可能分配给不同的工作线程
struct SayHiTask : public TaskBase
{
	SayHiTask(const string& name)
	: m_name(name)
	{
	}
	void Call(ActorLogicBase* logic)
	{
		Friend* pepole = static_cast<Friend*>(logic);
		pepole->SayHiTo(m_name);
	}
	string m_name;
};

struct GetNameTask : public TaskBase
{
	static GetNameTask* CreateWithPull(void)
	{
		return new GetNameTask();
	}
	GetNameTask(void) { m_pull = new TaskPullObj<string>(); }
	void Call(ActorLogicBase* logic)
	{
		Friend* pepole = static_cast<Friend*>(logic);
		TaskPullObj<string>* p = static_cast<TaskPullObj<string>*>(m_pull);
		p->SetReturnObjPtr(new string(pepole->GetName()));
		AckPull(logic);
	}
};

struct IntroduceFriend : public TaskAsync
{
	IntroduceFriend(int64_t beIntroducedPepole, int64_t introduceToPepole)
	{
		m_beIntroducedPepole = beIntroducedPepole;
		m_introduceToPepole = introduceToPepole;
	}
	int AsyncCall(ActorLogicBase* logic, TaskPullBase* ack, int num, bool timeout)
	{
		Friend* introducer = static_cast<Friend*>(logic);
		PT_BEGIN(&m_pt);
		{
			Pull<GetNameTask>(logic, TEST_GROUP_ID, m_beIntroducedPepole, -1);
		}
		PT_YIELD(&m_pt);
		{
			if (timeout) {
				cout << "fetch name timeout!" << endl;
				PT_EXIT(&m_pt);
			}
			TaskPullObj<string>* pullName = static_cast<TaskPullObj<string>*>(ack);
			auto nameptr = pullName->FetchObjPtr();
			g_context->Req(TEST_GROUP_ID, m_introduceToPepole, new SayHiTask(*nameptr), true);
			delete nameptr;
		}
		PT_END(&m_pt);
	}
	int64_t m_beIntroducedPepole;
	int64_t m_introduceToPepole;
};

//dengqunli获取hanmeimei的名字,然后将名字告诉lilei, 然后通知lilei对hanmeimei说hi, 整个过程由IntroduceFriend调度
//注意: 1) TaskAsync使用的是stackless协程，pull之后的代码不能访问pull之前的栈空间(即栈变量),如需保存状态，可以存入对象中
//		2) 同一group的actor习惯上都是同一个类的对象,逻辑上的调用关系通常是相互的，
//		所以不鼓励他们之间相互pull消息（如果两个actor同时相互pull容易出现死锁),
//      如需屏蔽同一group的actor之间相互pull,可以定义宏 __DISABLE_PULL_IN_THE_SAME_GROUP__
//		任何时候都要保证actor之间的pull调用是单向的,对这些actor进行逻辑上的分层是一个推荐的做法
//		本样例为了方便没做这个限制
int main(void)
{
	_TEST_INIT_
	g_context->AddActor(new Friend(1L, "dengqunli"));
	g_context->AddActor(new Friend(2L, "lilei")); 
	g_context->AddActor(new Friend(3L, "hanmeimei")); 
	g_context->Req(TEST_GROUP_ID, 1L, new IntroduceFriend(3L, 2L), true); //投递一个任务给工作线程执行

	//框架保证该actor的所有任务处理完才会删除actor
	std::this_thread::sleep_for(std::chrono::milliseconds(100)); //保证任务执行完才执行下面的退出
	g_context->GracefullExit(); //优雅退出工作线程
	g_context->WaitExit(); //等待工作线程退出
	g_stopTimer = true; //退出定时器
	timerThread.join(); //等待定时器退出
	cout << "input any string exit!" << endl;
	string exitstring; cin >> exitstring;
	return 0;
}