// DefaultPoller.cpp
#include "Poller.h"
#include "EpollPoller.cpp" // 注意：实际工程中应该在 CMake 中管理文件，不要 include .cpp
#include "SelectPoller.cpp"
#include "IOUringPoller.cpp"
#include <cstdlib> // getenv

Poller* Poller::newDefaultPoller() {
    // 读取环境变量来决定用哪个模型
    if (::getenv("USE_IO_URING")) {
        return new IOUringPoller();
    }
    if (::getenv("USE_SELECT")) {
        return new SelectPoller();
    }
    
    // 默认使用 Epoll (生产级)
    return new EpollPoller();
}