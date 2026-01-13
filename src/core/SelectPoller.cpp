// SelectPoller.cpp
#include "Poller.h"
#include <sys/select.h>

class SelectPoller : public Poller {
private:
    fd_set read_fds_;  // select 专用的位图
    int max_fd_;       // select 需要知道最大的 fd 是多少

public:
    SelectPoller() : max_fd_(-1) {
        FD_ZERO(&read_fds_);
    }

    void poll(int timeoutMs, std::vector<Channel*>* activeChannels) override {
        // 必须拷贝一份，因为 select 会修改传入的 fd_set
        fd_set temp_fds = read_fds_;
        struct timeval timeout;
        timeout.tv_sec = timeoutMs / 1000;
        timeout.tv_usec = (timeoutMs % 1000) * 1000;

        // 调用系统的 select 函数
        int num = ::select(max_fd_ + 1, &temp_fds, nullptr, nullptr, &timeout);

        if (num > 0) {
            // 笨办法：select 不告诉我们谁活了，我们得遍历所有可能的 fd
            // (这里简化逻辑，实际代码需要配合 ChannelMap 遍历)
            // fillActiveChannels(num, &temp_fds, activeChannels);
        }
    }

    void updateChannel(Channel* channel) override {
        FD_SET(channel->fd, &read_fds_); // 把 fd 加入监听位图
        if (channel->fd > max_fd_) max_fd_ = channel->fd;
    }

    void removeChannel(Channel* channel) override {
        FD_CLR(channel->fd, &read_fds_); // 把 fd 移出位图
    }
};