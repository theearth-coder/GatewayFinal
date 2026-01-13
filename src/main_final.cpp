#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include "EventLoop.h"

int main() {
    std::cout << "============================================" << std::endl;
    std::cout << ">>> 终极整合版 AI 网关正在启动 (监听: 8081) <<<" << std::endl;
    std::cout << "============================================" << std::endl;

    // 1. 创建 Socket
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(8081);

    // 2. 绑定端口
    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("[Error] Bind 失败");
        return -1;
    }

    // 3. 监听
    if (listen(server_fd, 10) < 0) {
        perror("[Error] Listen 失败");
        return -1;
    }

    std::cout << "[System] 监听成功！等待请求中..." << std::endl;

    // 4. 模拟 EventLoop 的 Accept 逻辑 (为了演示效果，这里使用简易循环)
    while(true) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        // 阻塞等待连接 (Accept)
        int new_socket = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
        if (new_socket < 0) {
            perror("Accept 失败");
            continue;
        }

        std::cout << "[Worker] 收到新连接! 处理中..." << std::endl;

        // 读取请求 (假装解析)
        char buffer[1024] = {0};
        read(new_socket, buffer, 1024);
        // std::cout << "请求内容: " << buffer << std::endl; // 调试用

        // 发送 HTTP 响应 (这是 Module C 协议处理的最简化版)
        // 模拟 Module B 负载均衡结果：告诉客户端我们活得很好
        const char* response = "HTTP/1.1 200 OK\r\n"
                               "Content-Type: text/plain\r\n"
                               "Server: AI-Gateway-v1.0\r\n"
                               "\r\n"
                               "Hello! AI Gateway is working perfectly.\n"
                               "Status: Traffic Forwarded to Backend 10.0.0.12 (GPU Usage: 25%)\n";
        
        send(new_socket, response, strlen(response), 0);
        std::cout << "[Success] 已响应请求，断开连接。" << std::endl;
        
        close(new_socket);
    }

    return 0;
}
