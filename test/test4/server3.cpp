#include "../../source/eventLoop.hpp"

#include "../../source/socket.hpp"

void HandleClose(Channel *channel)
{
    std::cout << "close: " << channel->Fd() << std::endl;
    channel->Remove(); // 移除监控
    delete channel;
}

void HandleRead(Channel *channel)
{
    int fd = channel->Fd();
    char buf[1024] = {0};
    int ret = recv(fd, buf, 1023, 0);
    if (ret <= 0)
    {
        return HandleClose(channel); // 关闭释放
    }
    std::cout << buf << std::endl;
    channel->EnableWrite(); // 启动可写事件
}

void HandleWrite(Channel *channel)
{
    int fd = channel->Fd();
    const char *data = "天气还不错!!";
    int ret = send(fd, data, strlen(data), 0);
    if (ret < 0)
    {
        return HandleClose(channel); // 关闭释放
    }
    channel->DisableWrite(); // 关闭写监控
}

void HandleError(Channel *channel)
{
    return HandleClose(channel); // 关闭释放
}

void HandleEvent(EventLoop *loop, Channel *channel, uint64_t timerid)
{
    loop->TimerRefresh(timerid);
}

void Acceptor(EventLoop *loop, Channel *lst_channel)
{
    int fd = lst_channel->Fd();
    int newfd = accept(fd, NULL, NULL);
    if (newfd < 0)
    {
        return;
    }
    uint64_t timerid = rand() % 10000;
    Channel *channel = new Channel(loop, newfd);
    channel->SetReadCallback(std::bind(HandleRead, channel));                  // 为通信套接字设置可读事件的回调函数
    channel->SetWriteCallback(std::bind(HandleWrite, channel));                // 可写事件的回调函数
    channel->SetCloseCallback(std::bind(HandleClose, channel));                // 关闭事件的回调函数
    channel->SetErrorCallback(std::bind(HandleError, channel));                // 关闭事件的回调函数
    channel->SetEventCallback(std::bind(HandleEvent, loop, channel, timerid)); // 关闭事件的回调函数
    loop->TimerAdd(timerid, 10, std::bind(HandleClose, channel));
    channel->EnableRead();
}

int main()
{
    srand(time(NULL));
    // Poller poller;
    EventLoop loop;
    Socket lst_sock;
    lst_sock.CreateServer(8500);
    // 为监听套接字，创建一个Channel进行事件的管理，以及事件的处理
    Channel channel(&loop, lst_sock.Fd());
    // 回调中，获取新连接，为新连接创建Channel并且添加监控
    channel.SetReadCallback(std::bind(Acceptor, &loop, &channel));
    channel.EnableRead(); // 启动可读事件监控
    while (1)
    {
        loop.Start();
    }
    lst_sock.Close();
    return 0;
}