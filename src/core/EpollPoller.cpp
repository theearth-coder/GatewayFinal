// EpollPoller.cpp
#include "Poller.h"
#include <sys/epoll.h>
#include <unistd.h>
#include <cstring> // for memset

class EpollPoller : public Poller {
private:
    int epollfd_;
    std::vector<struct epoll_event> events_; // 存放内核返回的事件

public:
    EpollPoller() : epollfd_(::epoll_create1(EPOLL_CLOEXEC)), events_(1024) {}
    
    ~EpollPoller() { ::close(epollfd_); }

    void poll(int timeoutMs, std::vector<Channel*>* activeChannels) override {
        // 核心：直接获取活跃的事件
        int numEvents = ::epoll_wait(epollfd_, events_.data(), static_cast<int>(events_.size()), timeoutMs);
        
        if (numEvents > 0) {
            // 把 epoll_event 转换回我们的 Channel
            for (int i = 0; i < numEvents; ++i) {
                Channel* channel = static_cast<Channel*>(events_[i].data.ptr);
                channel->revents = events_[i].events;
                activeChannels->push_back(channel);
            }
            // 自动扩容：如果事件太多装不下，下次把容器弄大点
            if (numEvents == events_.size()) {
                events_.resize(events_.size() * 2);
            }
        }
    }

    void updateChannel(Channel* channel) override {
        struct epoll_event event;
        memset(&event, 0, sizeof(event));
        event.events = channel->events;
        event.data.ptr = channel; // 关键：把 Channel 指针存进去
        
        // 实际上这里要判断是 EPOLL_CTL_ADD 还是 MOD
        ::epoll_ctl(epollfd_, EPOLL_CTL_ADD, channel->fd, &event);
    }

    void removeChannel(Channel* channel) override {
        ::epoll_ctl(epollfd_, EPOLL_CTL_DEL, channel->fd, nullptr);
    }
};