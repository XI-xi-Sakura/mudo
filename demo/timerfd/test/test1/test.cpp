#include <iostream>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <unistd.h>
#include <sys/timerfd.h>
#include <sys/select.h>


//下述例子，是一个定时器的使用示例，是每隔3秒钟触发一次定时器超时，否则就会阻塞在read读取数据这里。
//基于这个例子，则我们可以实现每隔3s，检测一下哪些连接超时了，然后将超时的连接释放掉。
int main()
{
    // 创建一个定时器
    int timerfd = timerfd_create(CLOCK_MONOTONIC, 0);

    if (timerfd < 0) {
        perror("timerfd_create error");
        return -1;
    }
    
    struct itimerspec itm;
    itm.it_value.tv_sec = 3;// 设置第一次超时的时间
    itm.it_value.tv_nsec = 0;
    itm.it_interval.tv_sec = 3; // 第一次超时后，每隔多长时间超时
    itm.it_interval.tv_nsec = 0;

    // 这个定时器描述符将每隔3秒，都会触发一次可读事件，自动启动定时器
    timerfd_settime(timerfd, 0, &itm, NULL);

    time_t start = time(NULL);

    while (1)
    {
        uint64_t tmp;
        // 需要注意的是定时器超时时后，则描述符触发可读事件，必须读取8字节的数据，保存的是自上次启动定时器和read的超时次数
        int ret = read(timerfd, &tmp, sizeof(tmp));
        if (ret < 0)
        {
            perror("read");
            return -1;
        }
        std::cout << tmp << " " << time(NULL) - start << std::endl;
    }
    close(timerfd);
    return 0;
}

