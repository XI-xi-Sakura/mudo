#pragma once

#include "HttpContect.hpp"

#define DEFALT_TIMEOUT 10

class HttpServer
{
private:
    using Handler = std::function<void(const HttpRequest &, HttpResponse *)>;
    using Handlers = std::vector<std::pair<std::regex, Handler>>;
    Handlers _get_route;
    Handlers _post_route;
    Handlers _put_route;
    Handlers _delete_route;
    std::string _basedir; // 静态资源根目录
    TcpServer _server;

private:
    void ErrorHandler(const HttpRequest &req, HttpResponse *rsp)
    {
        // 1. 组织一个错误展示页面
        std::string body;
        body += "<html>";
        body += "<head>";
        body += "<meta http-equiv='Content-Type' content='text/html;charset=utf-8'>";
        body += "</head>";
        body += "<body>";
        body += "<h1>";
        body += std::to_string(rsp->_statu);
        body += " ";
        body += Util::StatuDesc(rsp->_statu);
        body += "</h1>";
        body += "</body>";
        body += "</html>";
        // 2. 将页面数据，当作响应正文，放入rsp中
        rsp->SetContent(body, "text/html");
    }
    // 将HttpResponse中的要素按照http协议格式进行组织，发送
    void WriteReponse(const PtrConnection &conn, const HttpRequest &req, HttpResponse &rsp)
    {
        // 1. 先完善头部字段
        if (req.Close() == true)
        {
            rsp.SetHeader("Connection", "close");
        }
        else
        {
            rsp.SetHeader("Connection", "keep-alive");
        }
        if (rsp._body.empty() == false && rsp.HasHeader("Content-Length") == false)
        {
            rsp.SetHeader("Content-Length", std::to_string(rsp._body.size()));
        }
        if (rsp._body.empty() == false && rsp.HasHeader("Content-Type") == false)
        {
            rsp.SetHeader("Content-Type", "application/octet-stream");
        }
        if (rsp._redirect_flag == true)
        {
            rsp.SetHeader("Location", rsp._redirect_url);
        }
        // 2. 将rsp中的要素，按照http协议格式进行组织
        std::stringstream rsp_str;
        rsp_str << req._version << " " << std::to_string(rsp._statu) << " " << Util::StatuDesc(rsp._statu) << "\r\n";
        for (auto &head : rsp._headers)
        {
            rsp_str << head.first << ": " << head.second << "\r\n";
        }
        rsp_str << "\r\n";
        rsp_str << rsp._body;
        // 3. 发送数据
        conn->Send(rsp_str.str().c_str(), rsp_str.str().size());
    }
    bool IsFileHandler(const HttpRequest &req)
    {
        // 1. 必须设置了静态资源根目录
        if (_basedir.empty())
        {
            return false;
        }
        // 2. 请求方法，必须是GET / HEAD请求方法
        if (req._method != "GET" && req._method != "HEAD")
        {
            return false;
        }
        // 3. 请求的资源路径必须是一个合法路径
        if (Util::ValidPath(req._path) == false)
        {
            return false;
        }
        // 4. 请求的资源必须存在,且是一个普通文件
        //    有一种请求比较特殊 -- 目录：/, /image/， 这种情况给后边默认追加一个 index.html
        // index.html    /image/a.png
        // 不要忘了前缀的相对根目录,也就是将请求路径转换为实际存在的路径  /image/a.png  ->   ./wwwroot/image/a.png
        std::string req_path = _basedir + req._path; // 为了避免直接修改请求的资源路径，因此定义一个临时对象
        if (req._path.back() == '/')
        {
            req_path += "index.html";
        }
        if (Util::IsRegular(req_path) == false)
        {
            return false;
        }
        return true;
    }
    // 静态资源的请求处理 --- 将静态资源文件的数据读取出来，放到rsp的_body中, 并设置mime
    void FileHandler(const HttpRequest &req, HttpResponse *rsp)
    {
        std::string req_path = _basedir + req._path;
        if (req._path.back() == '/')
        {
            req_path += "index.html";
        }
        bool ret = Util::ReadFile(req_path, &rsp->_body);
        if (ret == false)
        {
            return;
        }
        std::string mime = Util::ExtMime(req_path);
        rsp->SetHeader("Content-Type", mime);
        return;
    }
    // 功能性请求的分类处理
    void Dispatcher(HttpRequest &req, HttpResponse *rsp, Handlers &handlers)
    {
        // 在对应请求方法的路由表中，查找是否含有对应资源请求的处理函数，有则调用，没有则发挥404
        // 思想：路由表存储的时键值对 -- 正则表达式 & 处理函数
        // 使用正则表达式，对请求的资源路径进行正则匹配，匹配成功就使用对应函数进行处理
        //   /numbers/(\d+)       /numbers/12345
        for (auto &handler : handlers)
        {
            const std::regex &re = handler.first;
            const Handler &functor = handler.second;
            bool ret = std::regex_match(req._path, req._matches, re);
            if (ret == false)
            {
                continue;
            }
            return functor(req, rsp); // 传入请求信息，和空的rsp，执行处理函数
        }
        rsp->_statu = 404;
    }
    void Route(HttpRequest &req, HttpResponse *rsp)
    {
        // 1. 对请求进行分辨，是一个静态资源请求，还是一个功能性请求
        //    静态资源请求，则进行静态资源的处理
        //    功能性请求，则需要通过几个请求路由表来确定是否有处理函数
        //    既不是静态资源请求，也没有设置对应的功能性请求处理函数，就返回405
        if (IsFileHandler(req) == true)
        {
            // 是一个静态资源请求, 则进行静态资源请求的处理
            return FileHandler(req, rsp);
        }
        if (req._method == "GET" || req._method == "HEAD")
        {
            return Dispatcher(req, rsp, _get_route);
        }
        else if (req._method == "POST")
        {
            return Dispatcher(req, rsp, _post_route);
        }
        else if (req._method == "PUT")
        {
            return Dispatcher(req, rsp, _put_route);
        }
        else if (req._method == "DELETE")
        {
            return Dispatcher(req, rsp, _delete_route);
        }
        rsp->_statu = 405; // Method Not Allowed
        return;
    }
    // 设置上下文
    void OnConnected(const PtrConnection &conn)
    {
        conn->SetContext(HttpContext());
        DBG_LOG("NEW CONNECTION %p", conn.get());
    }
    // 缓冲区数据解析+处理
    void OnMessage(const PtrConnection &conn, Buffer *buffer)
    {
        while (buffer->ReadAbleSize() > 0)
        {
            // 1. 获取上下文
            HttpContext *context = conn->GetContext()->get<HttpContext>();
            // 2. 通过上下文对缓冲区数据进行解析，得到HttpRequest对象
            //   1. 如果缓冲区的数据解析出错，就直接回复出错响应
            //   2. 如果解析正常，且请求已经获取完毕，才开始去进行处理
            context->RecvHttpRequest(buffer);
            HttpRequest &req = context->Request();
            HttpResponse rsp(context->RespStatu());
            if (context->RespStatu() >= 400)
            {
                // 进行错误响应，关闭连接
                ErrorHandler(req, &rsp);      // 填充一个错误显示页面数据到rsp中
                WriteReponse(conn, req, rsp); // 组织响应发送给客户端
                context->ReSet();
                buffer->MoveReadOffset(buffer->ReadAbleSize()); // 出错了就把缓冲区数据清空
                conn->Shutdown();                               // 关闭连接
                return;
            }
            if (context->RecvStatu() != RECV_HTTP_OVER)
            {
                // 当前请求还没有接收完整,则退出，等新数据到来再重新继续处理
                return;
            }
            // 3. 请求路由 + 业务处理
            Route(req, &rsp);
            // 4. 对HttpResponse进行组织发送
            WriteReponse(conn, req, rsp);
            // 5. 重置上下文
            context->ReSet();
            // 6. 根据长短连接判断是否关闭连接或者继续处理
            if (rsp.Close() == true)
                conn->Shutdown(); // 短链接则直接关闭
        }
        return;
    }

public:
    HttpServer(int port, int timeout = DEFALT_TIMEOUT) : _server(port)
    {
        _server.EnableInactiveRelease(timeout);
        _server.SetConnectedCallback(std::bind(&HttpServer::OnConnected, this, std::placeholders::_1));
        _server.SetMessageCallback(std::bind(&HttpServer::OnMessage, this, std::placeholders::_1, std::placeholders::_2));
    }
    void SetBaseDir(const std::string &path)
    {
        assert(Util::IsDirectory(path) == true);
        _basedir = path;
    }
    /*设置/添加，请求（请求的正则表达）与处理函数的映射关系*/
    void Get(const std::string &pattern, const Handler &handler)
    {
        _get_route.push_back(std::make_pair(std::regex(pattern), handler));
    }
    void Post(const std::string &pattern, const Handler &handler)
    {
        _post_route.push_back(std::make_pair(std::regex(pattern), handler));
    }
    void Put(const std::string &pattern, const Handler &handler)
    {
        _put_route.push_back(std::make_pair(std::regex(pattern), handler));
    }
    void Delete(const std::string &pattern, const Handler &handler)
    {
        _delete_route.push_back(std::make_pair(std::regex(pattern), handler));
    }
    void SetThreadCount(int count)
    {
        _server.SetThreadCount(count);
    }
    void Listen()
    {
        _server.Start();
    }
};