#pragma once
#include "../common/log.hpp"
#include <sys/epoll.h>
#include <functional>

class Poller;
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
        else if (_revents & EPOLLHUP) // 用于表示文件描述符对应的设备或流发生了挂起（hang up）事件，在网络编程里，一般意味着连接被关闭或者异常断开。
        {
            if (_close_callback)
                _close_callback();
        }
        if (_event_callback)
            _event_callback();
    }
};