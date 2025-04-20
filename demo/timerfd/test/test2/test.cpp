#include <iostream>
#include <list>
#include <vector>
#include <unordered_set>
#include <functional>
#include <memory>
#include <unordered_map>
#include <cassert>
#include <unistd.h>
#include <thread>

// 定时任务类
using OnTimerCallback = std::function<void()>;
using ReleaseCallback = std::function<void()>;

class Timer
{
private:
    int _timeout;       // 当前定时器任务的延迟时间
    bool _canceled;     // false-任务正常执行; true-任务被取消
    uint64_t _timer_id; // 定时器ID
    OnTimerCallback _timer_callback;

    ReleaseCallback _release_callback; // 实际释放任务类对象实际析构时要执行的操作，主要用于移除TimerQueue中保存的weak_ptr

public:
    Timer(uint64_t timer_id, int timeout) : _timer_id(timer_id),
                                            _timeout(timeout), _canceled(false) {}
    ~Timer()
    {
        // 先从timerqueue保存的weak_ptr<timer>数组中，将weak_ptr移除，避免下边的定时任务回调中有对定时器任务的操作
        if (_release_callback)
            _release_callback();
        if (_timer_callback && !_canceled)
            _timer_callback(); // 定时器被取消，则析构时不再执行任务
    }
    uint64_t id() { return _timer_id; }
    // 获取定时器任务的初始延迟时间
    int delay() { return _timeout; }      
    void canceled() { _canceled = true; } // 取消定时器任务
    void set_on_time_callback(const OnTimerCallback &cb) { _timer_callback = cb; }
    void set_release_callback(const ReleaseCallback &cb)
    {
        _release_callback = cb;
    }
};

#define MAX_TIMEOUT 60
class TimerQueue
{
private:
    using WeakTimer = std::weak_ptr<Timer>;
    using PtrTimer = std::shared_ptr<Timer>;

    // 时间轮每个节点都是一个vector，这样同一个时刻可以添加多个定时器任务
    using Bucket = std::vector<PtrTimer>;
    // 为了提高BucketList随机访问的效率，这里使用了数组实现单层时间轮
    using BucketList = std::vector<Bucket>;

    // 滴答秒针；每次执行一次定时器时间到，就会向后走一步，走到哪里就清空BucketList的哪个数组
    int _tick;

    // 时间轮的容量，这个容量其实就是最大的计时时长
    int _capacity;

    // 实现单层时间轮的数组，每秒钟tick向后走一步，走到哪里，就将哪里的vector进行clear，
    // 则其PtrTimer的shared_ptr计数器位0，将将会真正被释放，执行Timer的析构函数，完成定时任务的执行
    BucketList _conns;

    // 保存所有定时任务对象的weak_ptr，这样才能在不影响shared_ptr计数器的同时，获取shared_ptr
    std::unordered_map<uint64_t, WeakTimer> _timers;

public:
    TimerQueue() : _tick(0),
                   _capacity(MAX_TIMEOUT),
                   _conns(MAX_TIMEOUT) {}
    // 判断任务是否存在，这个接口是一个非线程安全接口，只能内部使用，不能外部其他线程调用，
    // 目前只在enable_inactive_release_in_loop中使用了
    bool has_timer(uint64_t id)
    {
        auto it = _timers.find(id);
        if (it != _timers.end())
        {
            return true;
        }
        return false;
    }
    // 添加一个定时任务
    void timer_add(const OnTimerCallback &cb, int delay, uint64_t id)
    {
        if (delay > _capacity || delay <= 0)
            return;
        PtrTimer timer(new Timer(id, delay));
        // 设置定时任务对象要执行的定时任务--会在对象被析构时执行
        timer->set_on_time_callback(cb);
        // 因为当前类成员_timers中保存了一份定时任务对象的weak_ptr，因此希望在析构的同时进行移除
        timer->set_release_callback(std::bind(&TimerQueue::remove_weaktimer_from_timerqueue, this, id));
        // 根据定时任务对象的shared_ptr获取其weak_ptr保存起来，便于二次刷新任务，
        // 也就是任务需要延迟执行的时候，重新继续添加一个定时任务进去，可以使用相同的shared_ptr计数
        _timers[id] = WeakTimer(timer);
        // tick是当前的指针位置
        _conns[(_tick + delay) % _capacity].push_back(timer);
    }
    // 根据id刷新定时任务
    void timer_refresh(uint64_t id)
    {
        auto it = _timers.find(id);
        // 不可能存在这种情况呀，添加的定时任务找不着，其实也不能因为一个定时任务找不到而让程序退出，
        // 其实就行了，这里主要是调试用断言
        assert(it != _timers.end());
        int delay = it->second.lock()->delay();
        _conns[(_tick + delay) % _capacity].push_back(PtrTimer(it->second.lock()));
    }
    void timer_cancel(uint64_t id)
    {
        auto it = _timers.find(id);
        assert(it != _timers.end());
        PtrTimer pt = it->second.lock();
        if (pt)
            pt->canceled();
    }
    // 设置给Timer，最终定时任务执行完毕后从timequeue移除timer信息的回调函数
    void remove_weaktimer_from_timerqueue(uint64_t id)
    {
        auto it = _timers.find(id);
        if (it != _timers.end())
        {
            _timers.erase(it);
        }
    }
    void run_once_task()
    {
        // 每滴答一次被执行_tick++就会向后走一步，走到哪里，释放哪里的定时器，也就是执行哪里的定时任务
        _tick = (_tick + 1) % _capacity;
        _conns[_tick].clear();
    }
};

class TimerTest
{
private:
    int _data;

public:
    TimerTest(int data) : _data(data) { std::cout << "test 构造!\n"; }
    ~TimerTest() { std::cout << "test 析构!\n"; }
};
void deleteTimerTest(TimerTest *t)
{
    delete t;
}

int main()
{
    // 创建一个固定5s的定时任务队列，每个添加的任务将在被添加5s后执行
    TimerQueue tq;
    TimerTest *t = new TimerTest(10);
    // 随便设置了一个定时器ID/添加了一个定时器，作为任务执行函数
    int id = 3;
    tq.timer_add(std::bind(deleteTimerTest, t), 5, id);
    // 这里其实就是添加了一个5秒后执行t的定时任务，任务是销毁t指针指向的空间
    // 按理论说add是添加5s后执行的定时任务，因为循环内总是在刷新任务，也就是二次添加任务，
    // 因此，它的计数总是>0，不会被释放，之后最后一个任务对象shared_ptr被释放才会真正析构
    for (int i = 0; i < 5; ++i)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        tq.timer_refresh(id);
        std::cout << "刷新了一下" << id << "号定时任务!\n";
        tq.run_once_task();
        std::cout << "tick调用一次，模拟定时器\n";
    }
    std::cout << "刷新定时任务停止，5s后释放任务将被执行\n";
    while (1)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        std::cout << "-----------------------\n";
        tq.run_once_task();
        if (tq.has_timer(id) == false)
        {
            std::cout << "定时任务已经被执行完毕!\n";
            break;
        }
    }
    return 0;
}