// Poller.h
#pragma once
#include <vector>
#include <map>

// 前置声明：Channel 是对 socket 的封装，包含 fd 和感兴趣的事件（读/写）
struct Channel {
    int fd;
    int events;      // 你希望监听的事件 (如 EPOLLIN, EPOLLOUT)
    int revents;     // 实际发生的事件
};

// 抽象基类
class Poller {
public:
    virtual ~Poller() = default;

    // 核心接口 1：等待事件
    // timeoutMs: 超时毫秒数
    // activeChannels: 传出参数，把有动静的 Channel 放进去
    virtual void poll(int timeoutMs, std::vector<Channel*>* activeChannels) = 0;

    // 核心接口 2：更新事件 (添加或修改监听)
    virtual void updateChannel(Channel* channel) = 0;

    // 核心接口 3：移除事件 (不再监听)
    virtual void removeChannel(Channel* channel) = 0;

    // 静态工厂方法：根据配置生产具体的 Poller
    static Poller* newDefaultPoller(); 
};