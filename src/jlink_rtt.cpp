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
#include <thread>
#include <queue>
#include <vector>
#include <chrono>
#include "jlink_api.h"

#define RTT_FIND_BUFFER_MAX_RETRY_COUNT 100
#define RTT_FIND_BUFFER_DELAY_MS 100

static int s_rtt_up_buffer_num = 0;
static int s_rtt_down_buffer_num = 0;
static int s_rtt_tx_channel = 0;
static int s_rtt_rx_channel = 0;
static std::mutex s_mtx;
static std::condition_variable s_cv;
static std::queue<std::vector<char>> s_rtt_rx_queue;
static char s_rtt_rx_buf[1024];
static bool s_req_stop = false;
static std::thread *s_rtt_thread = nullptr;
extern "C" void (*s_rx_cb)(char *data, int len) = nullptr;
static void rtt_thread(void){
    enum rtt_read_state{
        RTT_RECV_IDLE = 0,
        RTT_RECV_TRY_READ = 1,
    };
    rtt_read_state read_state = RTT_RECV_TRY_READ;
    while(true){
        std::vector<char> data;
        while(true){
            std::unique_lock<std::mutex> lck(s_mtx);
            if(!s_rtt_rx_queue.empty()){
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
        if(s_rtt_tx_channel < 0)
            continue;
        JLINK_RTTERMINAL_Write(s_rtt_tx_channel, data.data(), (int)data.size());
        continue;
    process_read:
        int len = JLINK_RTTERMINAL_Read(s_rtt_rx_channel, s_rtt_rx_buf, sizeof(s_rtt_rx_buf));
        if(len > 0){
            if(s_rx_cb)
                s_rx_cb(s_rtt_rx_buf, len);
        }else{
            read_state = RTT_RECV_IDLE;
        }
        continue;
    }
quit:
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
    if(addr && range){
        std::snprintf(cmd, sizeof(cmd), "SetRTTSearchRanges %#lx %#lx", addr, range);
        set_search_addr = true;
    }else if(addr){
        std::snprintf(cmd, sizeof(cmd), "SetRTTAddr %#lx", addr);
        set_search_addr = true;
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

    direction = RTT_DIRECTION_DOWN;
    for(int i = 0; i < RTT_FIND_BUFFER_MAX_RETRY_COUNT; i++){
        s_rtt_down_buffer_num = JLINK_RTTERMINAL_Control(RTT_CMD_GET_NUM_BUF, &direction);
        if(s_rtt_down_buffer_num >= 0)
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(RTT_FIND_BUFFER_DELAY_MS));
    }

    if(s_rtt_down_buffer_num < 0){
        std::printf("JLINK_RTTERMINAL_Control RTT_CMD_GET_NUM_BUF failed, ret = %d\n", s_rtt_down_buffer_num);
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


    if(tx_channel > s_rtt_down_buffer_num){
        s_rtt_tx_channel = -1;
    }else{
        s_rtt_tx_channel = tx_channel;
    }

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

void jlink_rtt_set_recv_callback(void (*rx_cb)(char *data, int len)){
    s_rx_cb = rx_cb;
}

int jlink_rtt_transmit(char *data, int len){
    std::unique_lock<std::mutex> lck(s_mtx);
    s_rtt_rx_queue.push(std::vector<char>(data, data + len));
    s_cv.notify_one();
    return len;
}

}