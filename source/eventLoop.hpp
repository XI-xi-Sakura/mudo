#pragma once

#include "log.hpp"

#include <unistd.h>
#include <string.h>
#include <vector>
#include <unordered_map>

#include <sys/epoll.h>
#include <functional>

class Poller; // 前向声明
class EventLoop;
class Channel
{
private:
    int _fd;
    EventLoop *_loop;

    uint32_t _events;  // 当前需要监控的事件
    uint32_t _revents; // 当前连接触发的事件

    using EventCallback = std::function<void()>;
    EventCallback _read_callback;  // 可读事件被触发的回调函数
    EventCallback _write_callback; // 可写事件被触发的回调函数
    EventCallback _error_callback; // 错误事件被触发的回调函数
    EventCallback _close_callback; // 连接断开事件被触发的回调函数
    EventCallback _event_callback; // 任意事件被触发的回调函数
public:
    Channel(EventLoop *loop, int fd) : _fd(fd), _events(0), _revents(0), _loop(loop) {}
    int Fd() { return _fd; }
    uint32_t Events() { return _events; }                   // 获取想要监控的事件
    void SetREvents(uint32_t events) { _revents = events; } // 设置实际就绪的事件
    void SetReadCallback(const EventCallback &cb) { _read_callback = cb; }
    void SetWriteCallback(const EventCallback &cb) { _write_callback = cb; }
    void SetErrorCallback(const EventCallback &cb) { _error_callback = cb; }
    void SetCloseCallback(const EventCallback &cb) { _close_callback = cb; }
    void SetEventCallback(const EventCallback &cb) { _event_callback = cb; }
    // 当前是否监控了可读
    bool ReadAble() { return (_events & EPOLLIN); }
    // 当前是否监控了可写
    bool WriteAble() { return (_events & EPOLLOUT); }
    // 启动读事件监控
    void EnableRead()
    {
        _events |= EPOLLIN;
        Update();
    }
    // 启动写事件监控
    void EnableWrite()
    {
        _events |= EPOLLOUT;
        Update();
    }
    // 关闭读事件监控
    void DisableRead()
    {
        _events &= ~EPOLLIN;
        Update();
    }
    // 关闭写事件监控
    void DisableWrite()
    {
        _events &= ~EPOLLOUT;
        Update();
    }
    // 关闭所有事件监控
    void DisableAll()
    {
        _events = 0;
        Update();
    }
    // 移除监控
    void Remove();
    void Update();
    // 事件处理，一旦连接触发了事件，就调用这个函数，自己触发了什么事件如何处理自己决定
    void HandleEvent()
    {
        // EPOLLRDHUP     用于检测 TCP 连接的对端关闭连接的情况，特别是半关闭（half-close）状态。
        // EPOLLPRI       用于监控文件描述符上的紧急数据（带外数据）
        if ((_revents & EPOLLIN) || (_revents & EPOLLRDHUP) || (_revents & EPOLLPRI))
        {
            /*不管任何事件，都调用的回调函数*/
            if (_read_callback)
                _read_callback();
        }
        /*有可能会释放连接的操作事件，一次只处理一个*/
        if (_revents & EPOLLOUT)
        {
            if (_write_callback)
                _write_callback();
        }
        else if (_revents & EPOLLERR)
        {
            if (_error_callback)
                _error_callback(); // 一旦出错，就会释放连接，因此要放到前边调用任意回调
        }
        // 用于表示文件描述符对应的设备或流发生了挂起（hang up）事件，在网络编程里，一般意味着连接被关闭或者异常断开。
        else if (_revents & EPOLLHUP)
        {
            if (_close_callback)
                _close_callback();
        }
        if (_event_callback)
            _event_callback();
    }
};
#define MAX_EPOLLEVENTS 1024
class Poller
{
private:
    int _epfd;
    struct epoll_event _evs[MAX_EPOLLEVENTS];
    std::unordered_map<int, Channel *> _channels;

private:
    // 对epoll的直接操作
    void Update(Channel *channel, int op)
    {
        // int epoll_ctl(int epfd, int op,  int fd,  struct epoll_event *ev);
        int fd = channel->Fd();
        struct epoll_event ev;
        ev.data.fd = fd;
        ev.events = channel->Events();
        int ret = epoll_ctl(_epfd, op, fd, &ev);
        if (ret < 0)
        {
            ERR_LOG("EPOLLCTL FAILED!");
        }
        return;
    }
    // 判断一个Channel是否已经添加了事件监控
    bool HasChannel(Channel *channel)
    {
        auto it = _channels.find(channel->Fd());
        if (it == _channels.end())
        {
            return false;
        }
        return true;
    }

public:
    Poller()
    {
        _epfd = epoll_create(MAX_EPOLLEVENTS);
        if (_epfd < 0)
        {
            ERR_LOG("EPOLL CREATE FAILED!!");
            abort(); // 退出程序
        }
    }
    // 添加或修改监控事件
    void UpdateEvent(Channel *channel)
    {
        bool ret = HasChannel(channel);
        if (ret == false)
        {
            // 不存在则添加
            _channels.insert(std::make_pair(channel->Fd(), channel));
            return Update(channel, EPOLL_CTL_ADD);
        }
        return Update(channel, EPOLL_CTL_MOD);
    }
    // 移除监控
    void RemoveEvent(Channel *channel)
    {
        auto it = _channels.find(channel->Fd());
        if (it != _channels.end())
        {
            _channels.erase(it);
        }
        Update(channel, EPOLL_CTL_DEL);
    }
    // 开始监控，返回活跃连接
    void Poll(std::vector<Channel *> *active)
    {
        // int epoll_wait(int epfd, struct epoll_event *evs, int maxevents, int timeout)
        int nfds = epoll_wait(_epfd, _evs, MAX_EPOLLEVENTS, -1);
        if (nfds < 0)
        {
            if (errno == EINTR)
            {
                return;
            }
            ERR_LOG("EPOLL WAIT ERROR:%s\n", strerror(errno));
            abort(); // 退出程序
        }
        for (int i = 0; i < nfds; i++)
        {
            auto it = _channels.find(_evs[i].data.fd);
            assert(it != _channels.end());
            it->second->SetREvents(_evs[i].events); // 设置实际就绪的事件
            active->push_back(it->second);
        }
        return;
    }
};

// 在 Channel 类定义时，Poller 类仅进行了前向声明，编译器仅知晓 Poller 是一个类，并不清楚其具体定义。
// 当在 Channel 类内部定义 Remove() 和 Update() 函数时，
// 代码里调用了 Poller 类的成员函数 RemoveEvent() 和 UpdateEvent()，
// 由于此时 Poller 类定义不完整，编译器无法解析这些调用，就会报错。

// 把函数定义放到 Poller 类完整定义之后，编译器就能看到 Poller 类的完整结构，从而正确解析这些函数调用。
// 之前遇到的 invalid use of incomplete type ‘class Poller’ 错误，就可通过这种方式解决。

// 所以说，poller以及channel就不放在两个头文件了，放在一起就可以了

#include <sys/timerfd.h>

using TaskFunc = std::function<void()>;
using ReleaseFunc = std::function<void()>;
class TimerTask
{
private:
    uint64_t _id;         // 定时器任务对象ID
    uint32_t _timeout;    // 定时任务的超时时间
    bool _canceled;       // false-表示没有被取消， true-表示被取消
    TaskFunc _task_cb;    // 定时器对象要执行的定时任务
    ReleaseFunc _release; // 用于删除TimerWheel中保存的定时器对象信息
public:
    // uint64_t _id--定时器任务对象ID; uint32_t _timeout--定时任务的超时时间
    TimerTask(uint64_t id, uint32_t delay, const TaskFunc &cb) : _id(id),
                                                                 _timeout(delay),
                                                                 _task_cb(cb),
                                                                 _canceled(false) {}
    ~TimerTask()
    {
        if (_canceled == false)
            _task_cb();
        _release();
    }
    void Cancel() { _canceled = true; }
    void SetRelease(const ReleaseFunc &cb) { _release = cb; }
    uint32_t DelayTime() { return _timeout; }
};

class TimerWheel
{
private:
    using WeakTask = std::weak_ptr<TimerTask>;
    using PtrTask = std::shared_ptr<TimerTask>;

    int _tick;     // 当前的秒针，走到哪里释放哪里，释放哪里，就相当于执行哪里的任务
    int _capacity; // 表盘最大数量---其实就是最大延迟时间
    std::vector<std::vector<PtrTask>> _wheel;
    std::unordered_map<uint64_t, WeakTask> _timers;

    EventLoop *_loop;
    std::unique_ptr<Channel> _timer_channel;

    int _timerfd; // 定时器描述符--可读事件回调就是读取计数器，执行定时任务

private:
    void RemoveTimer(uint64_t id)
    {
        auto it = _timers.find(id);
        if (it != _timers.end())
        {
            _timers.erase(it);
        }
    }
    static int CreateTimerfd()
    {
        int timerfd = timerfd_create(CLOCK_MONOTONIC, 0);
        if (timerfd < 0)
        {
            ERR_LOG("TIMERFD CREATE FAILED!");
            abort();
        }

        // int timerfd_settime(int fd, int flags, struct itimerspec *new, struct itimerspec *old);
        struct itimerspec itime;
        itime.it_value.tv_sec = 1;
        itime.it_value.tv_nsec = 0; // 第一次超时时间为1s后
        itime.it_interval.tv_sec = 1;
        itime.it_interval.tv_nsec = 0; // 第一次超时后，每次超时的间隔时

        timerfd_settime(timerfd, 0, &itime, NULL);
        return timerfd;
    }

    // 读取到数据times---从上一次read之后超时的次数
    int ReadTimefd()
    {
        uint64_t times;
        // 有可能因为其他描述符的事件处理花费事件比较长，然后在处理定时器描述符事件的时候，有可能就已经超时了很多次
        // read读取到的数据times就是从上一次read之后超时的次数
        int ret = read(_timerfd, &times, 8);
        if (ret < 0)
        {
            ERR_LOG("READ TIMEFD FAILED!");
            abort();
        }
        return times;
    }
    // 这个函数应该每秒钟被执行一次，相当于秒针向后走了一步
    void RunTimerTask()
    {
        _tick = (_tick + 1) % _capacity;
        _wheel[_tick].clear(); // 清空指定位置的数组，就会把数组中保存的所有管理定时器对象的shared_ptr释放掉
    }

    // 根据实际超时的次数，执行对应的超时任务
    void OnTime()
    {
        int times = ReadTimefd();
        for (int i = 0; i < times; i++)
        {
            RunTimerTask();
        }
    }
    void TimerAddInLoop(uint64_t id, uint32_t delay, const TaskFunc &cb)
    {
        PtrTask pt(new TimerTask(id, delay, cb));
        pt->SetRelease(std::bind(&TimerWheel::RemoveTimer, this, id));
        int pos = (_tick + delay) % _capacity;
        _wheel[pos].push_back(pt);
        _timers[id] = WeakTask(pt);
    }
    void TimerRefreshInLoop(uint64_t id)
    {
        // 通过保存的定时器对象的weak_ptr构造一个shared_ptr出来，添加到轮子中
        auto it = _timers.find(id);
        if (it == _timers.end())
        {
            return; // 没找着定时任务，没法刷新，没法延迟
        }
        PtrTask pt = it->second.lock(); // lock获取weak_ptr管理的对象对应的shared_ptr
        int delay = pt->DelayTime();
        int pos = (_tick + delay) % _capacity;
        _wheel[pos].push_back(pt);
    }
    void TimerCancelInLoop(uint64_t id)
    {
        auto it = _timers.find(id);
        if (it == _timers.end())
        {
            return; // 没找着定时任务，没法刷新，没法延迟
        }
        PtrTask pt = it->second.lock();
        if (pt)
            pt->Cancel();
    }

public:
    TimerWheel(EventLoop *loop) : _capacity(60), _tick(0), _wheel(_capacity), _loop(loop),
                                  _timerfd(CreateTimerfd()), _timer_channel(new Channel(_loop, _timerfd))
    {
        _timer_channel->SetReadCallback(std::bind(&TimerWheel::OnTime, this));
        _timer_channel->EnableRead(); // 启动读事件监控
    }
    /*定时器中有个_timers成员，定时器信息的操作有可能在多线程中进行，因此需要考虑线程安全问题*/
    /*如果不想加锁，那就把对定期的所有操作，都放到一个线程中进行*/
    void TimerAdd(uint64_t id, uint32_t delay, const TaskFunc &cb);
    // 刷新/延迟定时任务
    void TimerRefresh(uint64_t id);
    void TimerCancel(uint64_t id);
    /*这个接口存在线程安全问题--这个接口实际上不能被外界使用者调用，只能在模块内，在对应的EventLoop线程内执行*/
    bool HasTimer(uint64_t id)
    {
        auto it = _timers.find(id);
        if (it == _timers.end())
        {
            return false;
        }
        return true;
    }
};

#include <thread>
#include <mutex>
#include <sys/eventfd.h>
class EventLoop
{
private:
    using Functor = std::function<void()>;
    std::thread::id _thread_id; // 线程ID
    int _event_fd;              // eventfd唤醒IO事件监控有可能导致的阻塞
    std::unique_ptr<Channel> _event_channel;
    Poller _poller;              // 进行所有描述符的事件监控
    std::vector<Functor> _tasks; // 任务池
    std::mutex _mutex;           // 实现任务池操作的线程安全
    TimerWheel _timer_wheel;     // 定时器模块
public:
    // 执行任务池中的所有任务
    void RunAllTask()
    {
        std::vector<Functor> functor;
        {
            std::unique_lock<std::mutex> _lock(_mutex);
            _tasks.swap(functor);
        }
        for (auto &f : functor)
        {
            f();
        }
        return;
    }
    static int CreateEventFd()
    {
        int efd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
        if (efd < 0)
        {
            ERR_LOG("CREATE EVENTFD FAILED!!");
            abort(); // 让程序异常退出
        }
        return efd;
    }
    void ReadEventfd()
    {
        uint64_t res = 0;
        int ret = read(_event_fd, &res, sizeof(res));
        if (ret < 0)
        {
            // EINTR -- 被信号打断；   EAGAIN -- 表示无数据可读
            if (errno == EINTR || errno == EAGAIN)
            {
                return;
            }
            ERR_LOG("READ EVENTFD FAILED!");
            abort();
        }
        return;
    }
    void WeakUpEventFd()
    {
        uint64_t val = 1;
        int ret = write(_event_fd, &val, sizeof(val));
        if (ret < 0)
        {
            if (errno == EINTR)
            {
                return;
            }
            ERR_LOG("READ EVENTFD FAILED!");
            abort();
        }
        return;
    }

public:
    EventLoop() : _thread_id(std::this_thread::get_id()),
                  _event_fd(CreateEventFd()),
                  _event_channel(new Channel(this, _event_fd)),
                  _timer_wheel(this)
    {
        // 给eventfd添加可读事件回调函数，读取eventfd事件通知次数
        _event_channel->SetReadCallback(std::bind(&EventLoop::ReadEventfd, this));
        // 启动eventfd的读事件监控
        _event_channel->EnableRead();
    }
    // 三步走--事件监控-》就绪事件处理-》执行任务
    void Start()
    {
        while (1)
        {
            // 1. 事件监控，
            std::vector<Channel *> actives;
            _poller.Poll(&actives);
            // 2. 事件处理。
            for (auto &channel : actives)
            {
                channel->HandleEvent();
            }
            // 3. 执行任务
            RunAllTask();
        }
    }
    // 用于判断当前线程是否是EventLoop对应的线程；
    bool IsInLoop()
    {
        return (_thread_id == std::this_thread::get_id());
    }
    void AssertInLoop()
    {
        assert(_thread_id == std::this_thread::get_id());
    }
    // 判断将要执行的任务是否处于当前线程中，如果是则执行，不是则压入队列。
    void RunInLoop(const Functor &cb)
    {
        if (IsInLoop())
        {
            return cb();
        }
        return QueueInLoop(cb);
    }
    // 将操作压入任务池
    void QueueInLoop(const Functor &cb)
    {
        {
            std::unique_lock<std::mutex> _lock(_mutex);
            _tasks.push_back(cb);
        }
        // 唤醒有可能因为没有事件就绪，而导致的epoll阻塞；
        // 其实就是给eventfd写入一个数据，eventfd就会触发可读事件
        WeakUpEventFd();
    }
    // 添加/修改描述符的事件监控
    void UpdateEvent(Channel *channel) { return _poller.UpdateEvent(channel); }
    // 移除描述符的监控
    void RemoveEvent(Channel *channel) { return _poller.RemoveEvent(channel); }
    void TimerAdd(uint64_t id, uint32_t delay, const TaskFunc &cb)
    {
        return _timer_wheel.TimerAdd(id, delay, cb);
    }
    void TimerRefresh(uint64_t id) { return _timer_wheel.TimerRefresh(id); }
    void TimerCancel(uint64_t id) { return _timer_wheel.TimerCancel(id); }
    bool HasTimer(uint64_t id) { return _timer_wheel.HasTimer(id); }
};

void Channel::Remove() { return _loop->RemoveEvent(this); }
void Channel::Update() { return _loop->UpdateEvent(this); }

void TimerWheel::TimerAdd(uint64_t id, uint32_t delay, const TaskFunc &cb)
{
    _loop->RunInLoop(std::bind(&TimerWheel::TimerAddInLoop, this, id, delay, cb));
}
// 刷新/延迟定时任务
void TimerWheel::TimerRefresh(uint64_t id)
{
    _loop->RunInLoop(std::bind(&TimerWheel::TimerRefreshInLoop, this, id));
}
void TimerWheel::TimerCancel(uint64_t id)
{
    _loop->RunInLoop(std::bind(&TimerWheel::TimerCancelInLoop, this, id));
}