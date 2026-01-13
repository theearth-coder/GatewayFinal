// EventLoop.h
#pragma once
#include "Poller.h"
#include <vector>
#include <iostream>
#include <unistd.h>
#include <cstring>
#include <queue>        // [Task 1] 引入队列
#include <functional>   // [Task 1] 引入回调函数
#include <mutex>        // [Task 1] 线程锁

// [Task 1] 定义一个任务结构体
struct Task {
    int priority; // 优先级：1=高(VIP), 0=普通
    int fd;       // 连接描述符
    std::string data; // 读到的数据

    // 优先级队列的排序规则：priority 小的排后面 (我们希望 priority 大的排前面)
    bool operator<(const Task& other) const {
        return priority < other.priority;
    }
};

class EventLoop {
public:
    EventLoop() {
        poller_ = Poller::newDefaultPoller(); 
    }

    ~EventLoop() { 
        delete poller_; 
    }

    // 核心工作循环
    void loop() {
        while (!quit_) {
            std::vector<Channel*> activeChannels;
            poller_->poll(5000, &activeChannels);

            // --- 阶段 1: 接收 IO 事件并封装成任务 ---
            for (auto channel : activeChannels) {
                char buf[4096] = {0};
                ssize_t n = ::read(channel->fd, buf, sizeof(buf) - 1);

                if (n > 0) {
                    std::string request(buf);
                    
                    // [Task 1] 核心逻辑：判断是否为 VIP
                    int prio = 0; // 默认普通
                    if (request.find("X-Priority: High") != std::string::npos) {
                        prio = 1; // 标记为 VIP
                        std::cout << "[Priority] 检测到 VIP 请求 (FD=" << channel->fd << ") -> 插队!" << std::endl;
                    }

                    // 将任务加入优先级队列
                    {
                        std::lock_guard<std::mutex> lock(mutex_);
                        taskQueue_.push({prio, channel->fd, request});
                    }

                } else if (n == 0) {
                    close(channel->fd);
                    poller_->removeChannel(channel);
                    delete channel;
                }
            }

            // --- 阶段 2: 执行任务 (VIP 优先) ---
            processPendingTasks();
        }
    }

    void addConnection(int fd) {
        Channel* channel = new Channel();
        channel->fd = fd;
        channel->events = 1; 
        poller_->updateChannel(channel);
    }

private:
    // [Task 1] 处理积压的任务
    void processPendingTasks() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        while (!taskQueue_.empty()) {
            // 取出优先级最高的任务 (top)
            Task task = taskQueue_.top();
            taskQueue_.pop();

            // 真正的业务处理逻辑
            std::cout << "[Worker] 执行任务 FD=" << task.fd 
                      << " | 级别: " << (task.priority == 1 ? "★ VIP ★" : "普通") 
                      << " | 内容: " << task.data.substr(0, 10) << "..." << std::endl;
        }
    }

private:
    Poller* poller_;
    bool quit_ = false;
    
    // [Task 1] 优先级队列 (自动排序)
    std::priority_queue<Task> taskQueue_;
    std::mutex mutex_;
};