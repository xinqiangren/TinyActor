#include "TestBase.h"
#include "TaskWithCallback.h"

//每个actor保存了一个逻辑对象的指针，每个逻辑对象都有一个所在actor group里的唯一的id
class Pepole : public ActorLogicBase
{
public:
	Pepole(int64_t id)
		: ActorLogicBase(TEST_GROUP_ID, id)
	{
	}
	void SayHi(void)
	{
		cout << "hi! " << m_peerName << endl;
	}
	void SayBye(void)
	{
		cout << "bye! " << m_peerName << endl;
	}
	void RememberPeerName(const std::string& name)
	{
		m_peerName = name;
	}
private:
	string m_peerName;
};

//每个任务都会分配给一个工作线程执行,调用call. 同一个actor的任务保证按进入顺序串行执行,但可能分配给不同的工作线程
struct SayHiTask : public TaskBase
{
	SayHiTask(const string& name)
		: m_myName(name)
	{
	}
	void Call(ActorLogicBase* logic)
	{
		Pepole* pepole = static_cast<Pepole*>(logic);
		pepole->RememberPeerName(m_myName);
		pepole->SayHi();
	}
	string m_myName;
};

int main(void)
{
	_TEST_INIT_
	int64_t logicObjId = 1L; 
	g_context->AddActor(new Pepole(logicObjId)); //添加一个actor
	g_context->Req(TEST_GROUP_ID, logicObjId, new SayHiTask("dengqunli"), true); //投递一个任务给工作线程执行
	//可以直接传递一个回调任务,更简单!
	g_context->Req(
		TEST_GROUP_ID, 
		logicObjId, 
		new TaskWithCallback([](ActorLogicBase* logic){
			Pepole* pepole = static_cast<Pepole*>(logic);
			pepole->SayBye();
		}), 
		true);

	g_context->SoftErase(TEST_GROUP_ID, logicObjId); //如果actor没有新的任务可以删除，没有任务的actor不占用cpu,只占用内存
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