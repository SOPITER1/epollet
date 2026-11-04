# 基于 epoll ET 的多客户端聊天服务器
## 功能
- 支持多客户端同时连接，消息广播
- 基于 epoll ET + 非阻塞 IO 的单线程事件循环
- 每连接独立缓冲区，处理 TCP 粘包/半包
- 支持 `/list` 命令，单发在线用户列表
- 客户端异常断开时从 epoll 摘除并回收资源

## 编译运行
gcc -Wall -Wextra -o server server.c
./server

## 测试
nc 127.0.0.1 8080

## 技术点
- ET 模式下 recv 循环读到 EAGAIN
- memchr 定位 `\n`，memmove 保留半包数据
- 双向链表管理连接，O(1) 删除
- 应用层命令路由：`/list` 单发，普通消息广播

## 已知限制
- send 暂未处理 EAGAIN 和 EPOLLOUT，发送缓冲区满时可能丢数据
- 暂未实现优雅退出信号处理
