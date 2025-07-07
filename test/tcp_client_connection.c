//=====================================================================
//
// tcp_client_connection.c -
//
// Created by wwq on 2025/07/03
// Last Modified: 2025/07/03 09:53:40
// 管理 TCP 连接（如 MMS 客户端与服务器的通信）
// 检测连接状态（心跳检测、超时判断）
// 断线自动重连（指数退避策略）
// 资源清理（关闭无效连接，释放内存）
// 事件通知（如连接成功、断开、错误等） “通用函数
//=====================================================================
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <time.h>

#define MAX_RETRIES 5
#define INITIAL_RETRY_DELAY 1000 // 1秒
#define HEARTBEAT_INTERVAL 3000  // 3秒心跳
#define CONNECTION_TIMEOUT 5000  // 5秒超时

typedef enum
{
    CS_DISCONNECTED,
    CS_CONNECTING,
    CS_CONNECTED,
    CS_SHUTDOWN
} ConnectionState;

typedef struct
{
    int sockfd;
    char server_ip[16];
    int server_port;
    ConnectionState state;
    pthread_mutex_t lock;
    pthread_t thread_id;
    volatile int running;
    int retry_count;
    int retry_delay;
} TCPClient;

// 回调函数类型定义
typedef void (*ConnectionCallback)(TCPClient *client, ConnectionState state);
typedef void (*DataReceivedCallback)(TCPClient *client, const char *data, size_t len);

// 全局回调函数指针
static ConnectionCallback connection_callback = NULL;
static DataReceivedCallback data_received_callback = NULL;

// 设置回调函数
void set_connection_callback(ConnectionCallback cb)
{
    connection_callback = cb;
}

void set_data_received_callback(DataReceivedCallback cb)
{
    data_received_callback = cb;
}

// 初始化TCP客户端
TCPClient *tcp_client_create(const char *ip, int port)
{
    TCPClient *client = (TCPClient *)malloc(sizeof(TCPClient));
    if (!client)
        return NULL;

    memset(client, 0, sizeof(TCPClient));
    strncpy(client->server_ip, ip, sizeof(client->server_ip) - 1);
    client->server_port = port;
    client->state = CS_DISCONNECTED;
    client->running = 1;
    client->retry_count = 0;
    client->retry_delay = INITIAL_RETRY_DELAY;
    client->sockfd = -1;
    pthread_mutex_init(&client->lock, NULL);

    return client;
}

// 清理资源
void tcp_client_destroy(TCPClient *client)
{
    if (!client)
        return;

    client->running = 0;
    if (client->thread_id)
    {
        pthread_join(client->thread_id, NULL);
    }

    if (client->sockfd != -1)
    {
        close(client->sockfd);
        client->sockfd = -1;
    }

    pthread_mutex_destroy(&client->lock);
    free(client);
}

// 尝试建立TCP连接
static int try_connect(TCPClient *client)
{
    struct sockaddr_in server_addr;
    struct timeval timeout;

    // 创建socket
    client->sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (client->sockfd < 0)
    {
        perror("socket creation failed");
        return -1;
    }

    // 设置超时
    timeout.tv_sec = CONNECTION_TIMEOUT / 1000;
    timeout.tv_usec = (CONNECTION_TIMEOUT % 1000) * 1000;
    setsockopt(client->sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(client->sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

    // 配置服务器地址
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(client->server_port);
    if (inet_pton(AF_INET, client->server_ip, &server_addr.sin_addr) <= 0)
    {
        perror("invalid address");
        close(client->sockfd);
        client->sockfd = -1;
        return -1;
    }

    // 尝试连接
    if (connect(client->sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("connection failed");
        close(client->sockfd);
        client->sockfd = -1;
        return -1;
    }

    return 0;
}

// 发送心跳包
static int send_heartbeat(TCPClient *client)
{
    const char *heartbeat_msg = "HEARTBEAT";
    ssize_t sent = send(client->sockfd, heartbeat_msg, strlen(heartbeat_msg), 0);
    return (sent > 0) ? 0 : -1;
}

// 检测连接是否活跃
static int check_connection_active(TCPClient *client)
{
    if (client->sockfd == -1)
        return 0;

    // 尝试发送心跳包
    if (send_heartbeat(client) != 0)
    {
        return 0;
    }

    // 可以添加读取响应的逻辑
    return 1;
}

// 连接管理线程
static void *connection_handling_thread(void *arg)
{
    TCPClient *client = (TCPClient *)arg;
    time_t last_heartbeat = 0;

    while (client->running)
    {
        pthread_mutex_lock(&client->lock);

        switch (client->state)
        {
            case CS_DISCONNECTED:
                // 尝试重新连接
                client->state = CS_CONNECTING;
                if (connection_callback)
                {
                    connection_callback(client, client->state);
                }

                if (try_connect(client) == 0)
                {
                    client->state = CS_CONNECTED;
                    client->retry_count = 0;
                    client->retry_delay = INITIAL_RETRY_DELAY;
                    if (connection_callback)
                    {
                        connection_callback(client, client->state);
                    }
                }
                else
                {
                    client->state = CS_DISCONNECTED;
                    if (connection_callback)
                    {
                        connection_callback(client, client->state);
                    }

                    // 指数退避
                    client->retry_count++;
                    if (client->retry_count >= MAX_RETRIES)
                    {
                        printf("Max retries reached, giving up.\n");
                        client->running = 0;
                    }
                    else
                    {
                        client->retry_delay *= 2;
                        usleep(client->retry_delay * 1000);
                    }
                }
                break;

            case CS_CONNECTED:
                // 检查连接是否仍然活跃
                if (!check_connection_active(client))
                {
                    printf("Connection lost\n");
                    close(client->sockfd);
                    client->sockfd = -1;
                    client->state = CS_DISCONNECTED;
                    if (connection_callback)
                    {
                        connection_callback(client, client->state);
                    }
                }
                else
                {
                    // 定期发送心跳
                    time_t now = time(NULL);
                    if (now - last_heartbeat >= HEARTBEAT_INTERVAL / 1000)
                    {
                        send_heartbeat(client);
                        last_heartbeat = now;
                    }
                }
                break;

            case CS_SHUTDOWN:
                client->running = 0;
                break;

            default:
                break;
        }

        pthread_mutex_unlock(&client->lock);
        usleep(100000); // 100ms间隔检查
    }

    // 清理
    if (client->sockfd != -1)
    {
        close(client->sockfd);
        client->sockfd = -1;
    }

    return NULL;
}

// 启动连接管理线程
void tcp_client_start(TCPClient *client)
{
    if (!client)
        return;

    if (pthread_create(&client->thread_id, NULL, connection_handling_thread, client) != 0)
    {
        perror("failed to create connection thread");
        return;
    }
}

// 发送数据
int tcp_client_send(TCPClient *client, const char *data, size_t len)
{
    if (!client || client->state != CS_CONNECTED)
        return -1;

    pthread_mutex_lock(&client->lock);
    ssize_t sent = send(client->sockfd, data, len, 0);
    pthread_mutex_unlock(&client->lock);

    return (sent == (ssize_t)len) ? 0 : -1;
}

// 示例回调函数
void on_connection_change(TCPClient *client, ConnectionState state)
{
    const char *state_str[] = { "DISCONNECTED", "CONNECTING", "CONNECTED", "SHUTDOWN" };
    printf("Connection state changed to: %s\n", state_str[state]);
}

void on_data_received(TCPClient *client, const char *data, size_t len)
{
    printf("Received %zu bytes: %.*s\n", len, (int)len, data);
}

// 主函数示例
int main()
{
    // 创建TCP客户端
    TCPClient *client = tcp_client_create("127.0.0.1", 8080);
    if (!client)
    {
        fprintf(stderr, "Failed to create TCP client\n");
        return 1;
    }

    // 设置回调
    set_connection_callback(on_connection_change);
    set_data_received_callback(on_data_received);

    // 启动连接管理线程
    tcp_client_start(client);

    // 主线程可以做其他工作
    while (1)
    {
        sleep(1);
        // 可以在这里发送数据
        // tcp_client_send(client, "Hello", 5);
    }

    // 清理
    tcp_client_destroy(client);
    return 0;
}
