#include <iostream>
#include <cstdio>
#include <string>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

int main()
{
    // 创建TCP套接字
    int sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sockfd < 0)
    {
        perror("socket error");
        return -1;
    }

    // 设置服务器地址信息
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;        // IPv4地址族
    addr.sin_port = htons(8085);       // 监听端口8085
    addr.sin_addr.s_addr = inet_addr("0.0.0.0"); // 监听所有网络接口
    socklen_t len = sizeof(struct sockaddr_in);
    
    // 绑定套接字与地址
    int ret = bind(sockfd, (struct sockaddr *)&addr, len);
    if (ret < 0)
    {
        perror("bind error");
        return -1;
    }

    // 开始监听连接（最大等待队列为5）
    ret = listen(sockfd, 5);
    if (ret < 0)
    {
        perror("listen error");
        return -1;
    }

    // 主循环处理连接
    while (1)
    {
        // 接受新连接（不关心客户端地址信息）
        int newfd = accept(sockfd, NULL, 0);
        if (newfd < 0)
        {
            perror("accept error");
            return -1;  
        }

        // 接收客户端请求（最多1024字节）
        char buf[1024] = {0};
        int ret = recv(newfd, buf, 1024, 0);
        if (ret < 0)
        {
            perror("recv error");
            close(newfd);
            continue;  // 跳过本次连接的后续处理
        }
        else if (ret == 0)
        {
            perror("peer shutdown!");
            close(newfd);
            continue;
        }

        // 构建HTTP响应（此处未解析请求直接响应）
        std::string body = "<html><body><h1>Hello World</h1></body></html>";
        std::string rsp = "HTTP/1.1 200 OK\r\n";
        rsp += "Content-Length: " + std::to_string(body.size()) + "\r\n"; // 正文长度
        rsp += "Content-Type: text/html\r\n";  // MIME类型
        rsp += "\r\n";  // 空行分隔头与正文
        rsp += body;

        // 发送HTTP响应
        ret = send(newfd, rsp.c_str(), rsp.size(), 0);
        if (ret < 0)
        {
            perror("send error!");
            close(newfd);
            continue;
        }
        close(newfd);  // 关闭当前连接
    }
    close(sockfd);  // 关闭监听套接字（实际不会执行到这里）
    return 0;
}
