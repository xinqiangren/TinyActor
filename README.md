本项目是基于c++11实现的简单实用的多线的跨平台多线程actor框架,支持push和pull两种基本接口,目前不支持分布式actor之间的交互
push: 发送回调任务给其他actor,不等待回复
pull: 发送回调任务给其他actor,等待回复（通过协程异步等待）,回复后仍然回到回调函数继续执行

基本类简介:
ActorContext: 提供actor多线程运行环境，提供对外接口

ActorGroup: 对actor进行分组，同一类型actor放在同一group.工作线程对group进行轮叫调度,保证各group之间cpu占用的公平性,
	避免某一类型actor过多而饿死其他类型的actor，ActorGroup内的actor通过队列链接在一起

ActorBase: actor框架核心类，actor内有一个task队列,轮到该actor执行逻辑时,工作线程从队列中取出任务调用任务的回调函数

ActorLogicBase: 业务逻辑接口,客户端代码通过扩展该接口实现业务逻辑。ActorBase保存该类对象指针，任务回调调用时该指针会作为输入参数

TaskBase: 任务类的基本类，提供call函数接口供工作线程调用

TaskAsync: 继承自ActorBase,提供AyncCall函数接口替代call函数，在该函数内可以使用上述pull接口

TaskPullBase: pull接口被调用时作为信使在同actor之间承载和拉取数据(其有两个子类：TaskPullPod和TaskPullObj)

TimerExcuteQue: 当定时器用。框架内部也使用它来唤醒空闲线程，处理任务超时

实例代码:
见sample文件夹

编译与安装:
过于简单，略

本框架的核心数据结构(两级队列)参考了云风skynet开源框架
本框架使用蝇级stackless协程库 protothreads, 作者 Adam Dunkels 

联系：email xinqiangren11@126.com






