/**
 * @file main.c
 * @brief rtt-shell main function
 * @author simon.xiaoapeng (simon.xiaoapeng@gmail.com)
 * @date 2025-12-29
 * 
 * @copyright Copyright (c) 2025  simon.xiaoapeng@gmail.com
 * 
 */

#include <cstdio>
#include <thread>
#include <chrono>

#include "jlink_lib.h"
#include "jlink_api.h"
#include "jlink_rtt.h"

void rtt_recv_cb(char *data, int len){
    std::printf("%.*s", len, data);
}

int main(void)
{
    int ret = 0;
    std::printf("rtt-shell\n");
    if(jlink_lib_init() < 0){
        std::printf("jlink_lib_init failed\n");
        return -1;
    }
    if(JLINK_Open() < 0){
        std::printf("JLINK_Open failed\n");
        return -1;
    }

    if(JLINK_ExecCommand("device=MCXN947_M33_0", NULL, 0) < 0){
        std::printf("JLINK_ExecCommand failed\n");
        goto close;
    }
    JLINK_TIF_Select(1);
    if(JLINK_SetSpeed(4000) < 0){
        std::printf("JLINK_SetSpeed failed\n");
        goto close;
    }

    if(JLINK_Connect() < 0){
        std::printf("JLINK_Connect failed\n");
        goto close;
    }
    ret = jlink_rtt_start(0, 0, 0, 0);
    if(ret < 0){
        std::printf("jlink_rtt_start failed\n");
        goto close;
    }

    jlink_rtt_set_recv_callback(rtt_recv_cb);

    for(int i = 0; i < 10; i++){
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    jlink_rtt_stop();

close:
    if(JLINK_Close() < 0){
        std::printf("JLINK_Close failed\n");
        return -1;
    }

    jlink_lib_deinit();
    return 0;
}
