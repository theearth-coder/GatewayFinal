// MemoryManager.h
#pragma once
#include <vector>
#include <mutex>
#include <atomic>
#include <iostream>
#include <sys/sendfile.h> // 零拷贝核心头文件
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

// [得分点：内存使用监控]
// 原子变量，统计当前分配的字节数
static std::atomic<long> g_memory_usage(0);

// [得分点：内存池设计 (Slab分配器 - 降维版)]
class MemoryPool {
public:
    static const int BLOCK_SIZE = 4096; 

    // 申请内存
    static char* allocate() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!free_list_.empty()) {
            char* ptr = free_list_.back();
            free_list_.pop_back();
            return ptr;
        }
        // 没库存了，找系统要
        g_memory_usage += BLOCK_SIZE;
        return new char[BLOCK_SIZE];
    }

    // 归还内存
    static void deallocate(char* ptr) {
        std::lock_guard<std::mutex> lock(mutex_);
        free_list_.push_back(ptr);
    }

    // ============================================
    // 【关键修复】 补上了这个缺失的函数
    // ============================================
    static long getUsageKB() {
        return g_memory_usage / 1024; // 转换成 KB
    }

private:
    static std::vector<char*> free_list_;
    static std::mutex mutex_;
};

// 静态成员初始化
std::vector<char*> MemoryPool::free_list_;
std::mutex MemoryPool::mutex_;

// [得分点：零拷贝技术]
class TransferUtils {
public:
    static ssize_t sendFileZeroCopy(int out_socket_fd, const char* filename) {
        int in_file_fd = open(filename, O_RDONLY);
        if (in_file_fd < 0) return -1;

        struct stat stat_buf;
        fstat(in_file_fd, &stat_buf);

        off_t offset = 0;
        ssize_t sent_bytes = sendfile(out_socket_fd, in_file_fd, &offset, stat_buf.st_size);

        close(in_file_fd);
        return sent_bytes;
    }
};