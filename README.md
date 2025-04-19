# mudo库


## 目标定位

**One Thread One Loop主从Reactor模型高并发服务器**

该项目实现的是主从Reactor模型服务器，也就是主Reactor线程仅仅监控监听描述符，获取新建连接，保证获取新连接的高效性，提高服务器的并发性能。

主Reactor获取到新连接后分发给子Reactor进行通信事件监控。而子Reactor线程监控各自的描述符的读写事件进行数据读写以及业务处理。

One Thread One Loop的思想就是把所有的操作都放到一个线程中进行，一个线程对应一个事件处理的循环。

当前实现中，因为并不确定组件使用者的使用意向，因此并不提供业务层工作线程池的实现，只实现主从Reactor，而Worker工作线程池，可由组件库的使用者的需要自行决定是否使用和实现。 

## 功能模块划分：

我们要实现的是一个带有协议支持的Reactor模型高性能服务器，因此将整个项目的实现划分为两个大的模块：
- SERVER模块：实现Reactor模型的TCP服务器；
- 协议模块：对当前的Reactor模型服务器提供应用层协议支持。

### SERVER模块：
SERVER模块就是对所有的连接以及线程进行管理，让它们各司其职，在合适的时候做合适的事，最终完成高性能服务器组件的实现。

而具体的管理也分为三个方面：
- 监听连接管理：对监听连接进行管理。
- 通信连接管理：对通信连接进行管理。
- 超时连接管理：对超时连接进行管理。
基于以上的管理思想，将这个模块进行细致的划分又可以划分为以下多个子模块：

**Buffer模块**：

Buffer模块是一个缓冲区模块，用于实现通信中用户态的接收缓冲区和发送缓冲区功能。

**Socket模块**：

Socket模块是对套接字操作封装的一个模块，主要实现的socket的各项操作。

**Channel模块**：

Channel模块是对一个描述符需要进行的IO事件管理的模块，实现对描述符可读，可写，错误...事件的管理操作，以及Poller模块对描述符进行IO事件监控就绪后，根据不同的事件，回调不同的处理函数功能。

**Connection模块**

Connection模块是对Buffer模块，Socket模块，Channel模块的一个整体封装，实现了**对一个通信套接字的整体的管理**，每一个进行数据通信的套接字（也就是accept获取到的新连接）都会使用Connection进行管理。
- Connection模块内部包含有四个由组件使用者传入的回调函数：连接建立完成回调，事件回调，新数据回调，关闭回调。
- Connection模块内部包含有两个组件使用者提供的接口：数据发送接口，连接关闭接口
- Connection模块内部包含有两个用户态缓冲区：用户态接收缓冲区，用户态发送缓冲区
- Connection模块内部包含有一个Socket对象：完成描述符面向系统的IO操作
- Connection模块内部包含有一个Channel对象：完成描述符IO事件就绪的处理

具体处理流程如下：
1. 实现向Channel提供可读，可写，错误等不同事件的IO事件回调函数，然后将Channel和对应的描述符添加到Poller事件监控中。
2. 当描述符在Poller模块中就绪了IO可读事件，则调用描述符对应Channel中保存的读事件处理函数，进行数据读取，将socket接收缓冲区全部读取到Connection管理的用户态接收缓冲区中。然后调用由组件使用者传入的新数据到来回调函数进行处理。
3. 组件使用者进行的数据业务处理完毕后，通过Connection向使用者提供的数据发送接口，将数据写入Connection的发送缓冲区中。
4. 启动描述符在Poll模块中的IO写事件监控，就绪后，调用Channel中保存的写事件处理函数，将发送缓冲区中的数据通过Socket进行面向系统的实际数据发送。

**Acceptor模块**：

Acceptor模块是对Socket模块，Channel模块的一个整体封装，实现了**对一个监听套接字的整体的管理**。
- Acceptor模块内部包含有一个Socket对象：实现监听套接字的操作
- Acceptor模块内部包含有一个Channel对象：实现监听套接字IO事件就绪的处理

具体处理流程如下：
1. 实现向Channel提供可读事件的IO事件处理回调函数，函数的功能其实也就是获取新连接
2. 为新连接构建一个Connection对象出来。

**TimerQueue模块**：

TimerQueue模块是实现固定时间定时任务的模块，可以理解就是要给定时任务管理器，向定时任务管理器中添加一个任务，任务将在固定时间后被执行，同时也可以通过刷新定时任务来延迟任务的执行。这个模块**主要是对Connection对象的生命周期管理，对非活跃连接进行超时后的释放功能**。

- TimerQueue模块内部包含有一个timerfd：linux系统提供的定时器。
- TimerQueue模块内部包含有一个Channel对象：实现对timerfd的IO时间就绪回调处理

**Poller模块**：

Poller模块是对epoll进行封装的一个模块，主要实现epoll的IO事件添加，修改，移除，获取活跃连接功能。

**EventLoop模块**：

EventLoop模块可以理解就是我们上边所说的Reactor模块，它**是对Poller模块，TimerQueue模块，Socket模块的一个整体封装，进行所有描述符的事件监控**。
EventLoop模块必然是一个对象对应一个线程的模块，线程内部的目的就是运行EventLoop的启动函数。
EventLoop模块为了保证整个服务器的线程安全问题，因此要求使用者对于Connection的所有操作一定要在其对应的EventLoop线程内完成，不能在其他线程中进行（比如组件使用者使用Connection发送数据，以及关闭连接这种操作）。
EventLoop模块保证自己内部所监控的所有描述符，都要是活跃连接，非活跃连接就要及时释放避免资源浪费。
- EventLoop模块内部包含有一个eventfd：eventfd其实就是linux内核提供的一个事件fd，专门用于事件通知。
- EventLoop模块内部包含有一个Poller对象：用于进行描述符的IO事件监控。
- EventLoop模块内部包含有一个TimerQueue对象：用于进行定时任务的管理。
- EventLoop模块内部包含有一个PendingTask队列：组件使用者将对Connection进行的所有操作，都加入到任务队列中，由EventLoop模块进行管理，并在EventLoop对应的线程中进行执行。
- 每一个Connection对象都会绑定到一个EventLoop上，这样能保证对这个连接的所有操作都是在一个线程中完成的。

具体操作流程：
1. 通过Poller模块对当前模块管理内的所有描述符进行IO事件监控，有描述符事件就绪后，通过描述符对应的Channel进行事件处理。
2. 所有就绪的描述符IO事件处理完毕后，对任务队列中的所有操作顺序进行执行。
3. 由于epoll的事件监控，有可能会因为没有事件到来而持续阻塞，导致任务队列中的任务不能及时得到执行，因此创建了eventfd，添加到Poller的事件监控中，用于实现每次向任务队列添加任务的时候，通过向eventfd写入数据来唤醒epoll的阻塞。

**TcpServer模块**：

这个模块是一个整体Tcp服务器模块的封装，内部封装了Acceptor模块，EventLoopThreadPool模块。
- TcpServer中包含有一个EventLoop对象：以备在超轻量使用场景中不需要EventLoop线程池，只需要在主线程中完成所有操作的情况。
- TcpServer模块内部包含有一个EventLoopThreadPool对象：其实就是EventLoop线程池，也就是子Reactor线程池
- TcpServer模块内部包含有一个Acceptor对象：一个TcpServer服务器，必然对应有一个监听套接字，能够完成获取客户端新连接，并处理的任务。
- TcpServer模块内部包含有一个std::shared_ptr<Connection>的hash表：保存了所有的新建连接对应的Connection，注意，所有的Connection使用shared_ptr进行管理，这样能够保证在hash表中删除了Connection信息后，在shared_ptr计数器为0的情况下完成对Connection资源的释放操作。

具体操作流程如下：
1. 在实例化TcpServer对象过程中，完成BaseLoop的设置，Acceptor对象的实例化，以及EventLoop线程池的实例化，以及std::shared_ptr<Connection>的hash表的实例化。
2. 为Acceptor对象设置回调函数：获取到新连接后，为新连接构建Connection对象，设置Connection的各项回调，并使用shared_ptr进行管理，并添加到hash表中进行管理，并为Connection选择一个EventLoop线程，为Connection添加一个定时销毁任务，为Connection添加事件监控，
3. 启动BaseLoop。


### HTTP协议模块
HTTP协议模块用于对高并发服务器模块进行协议支持，基于提供的协议支持能够更方便的完成指定协议服务器的搭建。
而HTTP协议支持模块的实现，可以细分为以下几个模块：

Util模块：
这个模块是一个工具模块，主要提供HTTP协议模块所用到的一些工具函数，比如url编解码，文件读写…等。

HttpRequest模块：
这个模块是HTTP请求数据模块，用于保存HTTP请求数据被解析后的各项请求元素信息。

HttpResponse模块：
这个模块是HTTP响应数据模块，用于业务处理后设置并保存HTTP响应数据的的各项元素信息，最终会被按照HTTP协议响应格式组织成为响应信息发送给客户端。

HttpContext模块：
这个模块是一个HTTP请求接收的上下文模块，主要是为了防止在一次接收的数据中，不是一个完整的HTTP请求，则解析过程并未完成，无法进行完整的请求处理，需要在下次接收到新数据后继续根据上下文进行解析，最终得到一个HttpRequest请求信息对象，因此在请求数据的接收以及解析部分需要一个上下文来进行控制接收和处理节奏。

HttpServer模块：
这个模块是最终给组件使用者提供的HTTP服务器模块了，用于以简单的接口实现HTTP服务器的搭建。
- HttpServer模块内部包含有一个TcpServer对象：TcpServer对象实现服务器的搭建
- HttpServer模块内部包含有两个提供给TcpServer对象的接口：连接建立成功设置上下文接口，数据处理接口。
- HttpServer模块内部包含有一个hash - map表存储请求与处理函数的映射表：组件使用者向HttpServer设置哪些请求应该使用哪些函数进行处理，等TcpServer收到对应的请求就会使用对应的函数进行处理。
