#include "../../source/common/socket.hpp"

int main()
{
    Socket cli_sock;
    cli_sock.CreateClient(8580, "127.0.0.1");
    std::string req = "nihao";
    

    while (1)
    {
        assert(cli_sock.Send(req.c_str(), req.size()) != -1);
        char buf[1024] = {0};
        cli_sock.Recv(buf, 1023);
        DBG_LOG("[%s]", buf);
        sleep(3);
    }

    cli_sock.Close();
    return 0;
}