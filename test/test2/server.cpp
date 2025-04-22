#include "../../source/common/socket.hpp"
#include <iostream>

int main()
{
    Socket lst_sock;
    lst_sock.CreateServer(8580);
    while (1)
    {
        int newfd = lst_sock.Accept();
        if (newfd < 0)
        {
            continue;
        }

        Socket cli_sock(newfd);
        char buf[1024] = {0};
        int ret = cli_sock.Recv(buf, 1023);
        DBG_LOG("[%s]", buf);

        if (ret < 0)
        {
            cli_sock.Close();
            continue;
        }

        ret = cli_sock.Send(buf, ret);

        cli_sock.Close();
    }
    lst_sock.Close();
    return 0;
}