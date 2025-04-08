# MuduoCpp11  
Notes & Code for Muduo using C++11 standard  

---

## 核心模块说明  

### TcpServer  
- 维护 `unordered_map` 记录每个 `TcpConnection` 类  
- 初始化时传入 `InetAddress`（记录监听IP和端口）和 `EventLoop`（主线程事件循环）  
- 初始化流程：  
  1. 开启 `Acceptor` 和线程池 `EventLoopThreadPool`  
  2. 设置 `Acceptor` 的新连接回调函数  
  3. 配置线程池数量、`connectionCallback_`、`messageCallback_`、`ThreadInitCallback`  
  4. 调用 `start()` 开启 `Acceptor::listen` 和线程池  

#### 新连接处理  
1. `Acceptor` 通过 `::accept` 获取客户端 `sockaddr_in` 和 `connfd`  
2. 调用 `TcpServer::newConnection()`：  
   - 使用 `::getsockname` 和 `::getpeername` 获取地址信息  
   - 创建 `TcpConnection` 并记录到 `unordered_map`  
   - 从线程池获取 `EventLoop` 绑定到 `TcpConnection` 的 `Channel`  
   - 调用 `TcpConnection::connectEstablished` 启用读事件监听  

---

### TcpConnection  
- 封装连接的 `Channel` 和 `Socket` 类  
- 初始化流程：  
  1. 在 `Acceptor` 的读回调中创建  
  2. 绑定 `Channel` 的事件回调（如 `handleRead`）  
  3. 通过 `enableReading()` 将 `Channel` 注册到 `EventLoop`  

#### 数据管理  
- 使用 `Buffer` 类：  
  - `inputBuffer`：处理读操作  
  - `outputBuffer`：处理写操作（应对内核发送速度慢）  

#### 连接管理  
- `shutdown()` 和 `handleWrite()`：关闭写端，处理剩余数据  
- `connectDestroyed()`：从 `epoll` 注销并关闭连接  

---

### EventLoopThreadPool  
- 维护 `EventLoopThread` 和 `EventLoop` 的集合（`vector`）  
- `start()` 函数：  
  - 启动线程池  
  - 调用 `EventLoop::startLoop()` 开启子线程事件循环  

---

### EventLoopThread  
- 成员：  
  - `Thread` 线程对象  
  - `threadFunc`（绑定到线程）  
  - `ThreadInitCallback`（初始化时执行）  
- `startLoop()`：  
  - 异步启动线程  
  - 在子线程中创建 `EventLoop` 并执行 `loop()`  

---

### EventLoop  
- 核心成员：  
  - `ChannelList`：活跃事件列表  
  - `pendingFunctors`：待执行回调队列  
  - `EpollPoller`：事件监听器  
  - `wakeupChannel`：用于唤醒事件循环  
- 工作流程：  
  1. 执行 `epoll_wait` 监听事件  
  2. 处理活跃 `Channel` 的回调  
  3. 执行其他线程提交的 `pendingFunctors`  

#### 跨线程调度  
- `runInLoop(callback)`：  
  - 若在非绑定线程调用，将回调加入队列并触发 `wakeup()`  
  - `wakeup()` 通过写 `wakeupFd` 唤醒 `epoll_wait`  

---

### Channel  
- 参数：`EventLoop*` 和文件描述符 `fd`  
- 核心功能：  
  - 注册/更新事件监听（`enableReading()` 等）  
  - 处理事件（`handleEvent()` 根据 `revents_` 触发回调）  

---

### EpollPoller  
#### 系统调用封装  
- `epoll_create1`：创建 `epoll` 实例  
- `epoll_ctl`：管理监听事件（`ADD/MOD/DEL`）  
- `epoll_wait`：获取就绪事件  

#### 数据结构  
- 维护 `map<int, Channel*>` 记录已注册的 `fd`  
- `fillActiveChannels`：将内核事件转换为 `Channel` 列表  

#### 内核机制  
```cpp
// epoll_event 结构（用户态）
struct epoll_event {
    uint32_t events;
    epoll_data_t data; // 包含 Channel* 指针
};

// epitem 结构（内核态）
struct epitem {
    struct rb_node rbn;      // 红黑树节点
    struct list_head rdllink; // 就绪链表节点
    struct eventpoll *ep;    
    struct file *fp;         // 关联的 fd
    uint32_t events;          // 监听的事件
};