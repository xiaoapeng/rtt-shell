/**
 * @file jlink_rtt.cpp
 * @brief J-Link RTT 接口封装实现
 * @author simon.xiaoapeng (simon.xiaoapeng@gmail.com)
 * @date 2025-12-31
 * 
 * @copyright Copyright (c) 2025  simon.xiaoapeng@gmail.com
 * 
 */

#include <cstdio>
#include <queue>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <thread>
#include <atomic>

#include "jlink_api.h"

#define RTT_FIND_BUFFER_MAX_RETRY_COUNT 100
#define RTT_FIND_BUFFER_DOWN_MAX_RETRY_COUNT 10
#define RTT_FIND_BUFFER_DELAY_MS 100

// Ctrl+C 超时检测相关参数
#define CTRL_C_TIMEOUT_MS 200        // Ctrl+C 超时时间
#define CTRL_C_ISOLATION_MS 50       // Ctrl+C 前后隔离时间
#define CTRL_C_CHAR 0x03             // Ctrl+C 的ASCII码

static int s_rtt_up_buffer_num = 0;
static int s_rtt_down_buffer_num = 0;
static int s_rtt_tx_channel = -1;
static int s_rtt_rx_channel = 0;
static std::mutex s_mtx;
static std::condition_variable s_cv;
static std::queue<std::vector<char>> s_rtt_rx_queue;
static char s_rtt_rx_buf[1024];
static bool s_req_stop = false;
static std::thread *s_rtt_thread = nullptr;

// Ctrl+C 超时检测相关变量
static std::atomic<bool> s_ctrl_c_pending{false};
static std::atomic<std::chrono::steady_clock::time_point> s_ctrl_c_sent_time{};
static std::atomic<std::chrono::steady_clock::time_point> s_last_data_time{};
static std::atomic<bool> s_ctrl_c_timeout_active{false};

extern "C" { 
    static void (*s_rx_cb)(const char *data, size_t len) = nullptr;
    static void (*s_err_cb)(void) = nullptr;
}

// 检查是否是单独的Ctrl+C信号
static bool is_isolated_ctrl_c(const std::vector<char>& data) {
    if (data.size() != 1 || data[0] != CTRL_C_CHAR) {
        return false;
    }
    
    auto now = std::chrono::steady_clock::now();
    auto last_data = s_last_data_time.load();
    
    // 检查前后50ms内是否有其他数据
    if (last_data != std::chrono::steady_clock::time_point{} &&
        std::chrono::duration_cast<std::chrono::milliseconds>(now - last_data).count() < CTRL_C_ISOLATION_MS) {
        return false;
    }
    
    return true;
}

// 超时检测线程函数
static void timeout_thread(void) {
    while (!s_req_stop) {
        if (s_ctrl_c_pending.load() && s_ctrl_c_timeout_active.load()) {
            auto now = std::chrono::steady_clock::now();
            auto sent_time = s_ctrl_c_sent_time.load();
            
            if (sent_time != std::chrono::steady_clock::time_point{} &&
                std::chrono::duration_cast<std::chrono::milliseconds>(now - sent_time).count() >= CTRL_C_TIMEOUT_MS) {
                
                // 超时发生，调用错误回调
                if (s_err_cb) {
                    s_err_cb();
                }
                
                // 重置状态
                s_ctrl_c_pending.store(false);
                s_ctrl_c_timeout_active.store(false);
                s_ctrl_c_sent_time.store(std::chrono::steady_clock::time_point{});
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

static void rtt_thread(void){
    enum rtt_read_state{
        RTT_RECV_IDLE = 0,
        RTT_RECV_TRY_READ = 1,
    };
    rtt_read_state read_state = RTT_RECV_TRY_READ;
    std::vector<char> data;
    
    // 启动超时检测线程
    std::thread timeout_detector(timeout_thread);
    
    while(true){
        while(true){
            std::unique_lock<std::mutex> lck(s_mtx);
            if(!s_rtt_rx_queue.empty() || !data.empty()){
                /* 合并queue里面的多个数据包到data */
                while(!s_rtt_rx_queue.empty()){
                    data.insert(data.end(), s_rtt_rx_queue.front().begin(), s_rtt_rx_queue.front().end());
                    s_rtt_rx_queue.pop();
                }
                goto process_data;
            }
            if(s_req_stop)
                goto quit;

            if(read_state == RTT_RECV_TRY_READ)
                goto process_read;
            
            s_cv.wait_for(lck,std::chrono::microseconds(100));
            read_state = RTT_RECV_TRY_READ;
        }
    process_data:
    {
        // 检查是否是单独的Ctrl+C信号
        if (is_isolated_ctrl_c(data)) {
            s_ctrl_c_pending.store(true);
            s_ctrl_c_sent_time.store(std::chrono::steady_clock::now());
            s_ctrl_c_timeout_active.store(true);
        }
        
        // 更新最后数据时间
        s_last_data_time.store(std::chrono::steady_clock::now());
        
        int ret;
        ret = JLINK_RTTERMINAL_Write(s_rtt_tx_channel, data.data(), (int)data.size());
        if(ret >= 0){
            data.erase(data.begin(), data.begin() + ret);
        }else{
            std::printf("JLINK_RTTERMINAL_Write, tx_channel = %d, data.size() = %d ret = %d\n",
                 s_rtt_tx_channel, (int)data.size(), ret);
            if(s_err_cb)
                s_err_cb();
        }
        continue;
    }
    process_read:
        int len = JLINK_RTTERMINAL_Read(s_rtt_rx_channel, s_rtt_rx_buf, sizeof(s_rtt_rx_buf));
        if(len > 0){
            // 收到下位机回复，重置Ctrl+C超时状态
            if (s_ctrl_c_pending.load()) {
                s_ctrl_c_pending.store(false);
                s_ctrl_c_timeout_active.store(false);
                s_ctrl_c_sent_time.store(std::chrono::steady_clock::time_point{});
            }
            
            if(s_rx_cb)
                s_rx_cb(s_rtt_rx_buf, size_t(len));
        }else if(len == 0){
            read_state = RTT_RECV_IDLE;
        }else{
            std::printf("JLINK_RTTERMINAL_Read, rx_channel = %d, len = %d\n", s_rtt_rx_channel, len);
            if(s_err_cb)
                s_err_cb();
        }
        continue;
    }
quit:
    s_req_stop = true;
    timeout_detector.join();
    return ;
}


extern "C"{

int jlink_rtt_start(int tx_channel, int rx_channel, unsigned long addr, unsigned long range){
    char cmd[128];
    bool set_search_addr = false;
    int ret = 0;
    int direction;
    s_rtt_up_buffer_num = -1;
    s_rtt_down_buffer_num = -1;
    
    // 初始化Ctrl+C检测状态
    s_ctrl_c_pending.store(false);
    s_ctrl_c_timeout_active.store(false);
    s_ctrl_c_sent_time.store(std::chrono::steady_clock::time_point{});
    s_last_data_time.store(std::chrono::steady_clock::time_point{});
    
    if(addr && range){
        std::snprintf(cmd, sizeof(cmd), "SetRTTSearchRanges %#lx %#lx", addr, range);
        set_search_addr = true;
    }else if(addr){
        std::snprintf(cmd, sizeof(cmd), "SetRTTAddr %#lx", addr);
        set_search_addr = true;
    }

    if(rx_channel < 0){
        std::printf("rx_channel %d is invalid\n", rx_channel);
        return -1;
    }

    if(set_search_addr){
        ret = JLINK_ExecCommand(cmd, NULL, 0);
        if(ret < 0){
            std::printf("SetRTTSearchRanges or SetRTTAddr failed, ret = %d\n", ret);
            return -1;
        }
    }

    ret = JLINK_RTTERMINAL_Control(RTT_CMD_START, NULL);
    if(ret < 0){
        std::printf("JLINK_RTTERMINAL_Control RTT_CMD_START failed, ret = %d\n", ret);
        return -1;
    }
    direction = RTT_DIRECTION_UP;
    for(int i = 0; i < RTT_FIND_BUFFER_MAX_RETRY_COUNT; i++){
        s_rtt_up_buffer_num = JLINK_RTTERMINAL_Control(RTT_CMD_GET_NUM_BUF, &direction);
        if(s_rtt_up_buffer_num >= 0)
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(RTT_FIND_BUFFER_DELAY_MS));
    }

    if(s_rtt_up_buffer_num < 0){
        std::printf("JLINK_RTTERMINAL_Control RTT_CMD_GET_NUM_BUF failed, ret = %d\n", s_rtt_up_buffer_num);
        JLINK_RTTERMINAL_Control(RTT_CMD_STOP, NULL);
        return -1;
    }
    
    /* 检查发送和接收通道号是否超出范围 */
    if(rx_channel > s_rtt_up_buffer_num){
        std::printf("rx_channel %d is out of range %d\n", rx_channel, s_rtt_up_buffer_num);
        JLINK_RTTERMINAL_Control(RTT_CMD_STOP, NULL);
        return -1;
    }
    s_rtt_rx_channel = rx_channel;

    if(tx_channel >= 0){
        direction = RTT_DIRECTION_DOWN;
        for(int i = 0; i < RTT_FIND_BUFFER_DOWN_MAX_RETRY_COUNT; i++){
            s_rtt_down_buffer_num = JLINK_RTTERMINAL_Control(RTT_CMD_GET_NUM_BUF, &direction);
            if(s_rtt_down_buffer_num >= 0)
                break;
            std::this_thread::sleep_for(std::chrono::milliseconds(RTT_FIND_BUFFER_DELAY_MS));
        }

        if(s_rtt_down_buffer_num < 0){
            std::printf("No found RTT down buffer, ret = %d\n", s_rtt_down_buffer_num);
            JLINK_RTTERMINAL_Control(RTT_CMD_STOP, NULL);
            return -1;
        }
        
        if(tx_channel > s_rtt_down_buffer_num){
            std::printf("tx_channel %d is out of range %d\n", tx_channel, s_rtt_down_buffer_num);
            JLINK_RTTERMINAL_Control(RTT_CMD_STOP, NULL);
            return -1;
        }
    }
    s_rtt_tx_channel = tx_channel;

    s_req_stop = false;
    s_rtt_rx_queue = std::queue<std::vector<char>>();
    // 启动接收线程
    s_rtt_thread = new std::thread(rtt_thread);
    return 0;
}

void jlink_rtt_stop(void){
    s_req_stop = true;
    s_rtt_thread->join();
    delete s_rtt_thread;
    s_rtt_thread = nullptr;
    JLINK_RTTERMINAL_Control(RTT_CMD_STOP, NULL);
}

void jlink_rtt_set_recv_callback(void (*rx_cb)(const char *data, size_t len)){
    s_rx_cb = rx_cb;
}

void jlink_rtt_set_error_callback(void (*err_cb)(void)){
    s_err_cb = err_cb;
}


int jlink_rtt_transmit(const char *data, int len){
    std::unique_lock<std::mutex> lck(s_mtx);
    if(s_rtt_tx_channel < 0)
        return -1;
    s_rtt_rx_queue.push(std::vector<char>(data, data + len));
    s_cv.notify_one();
    return len;
}

}