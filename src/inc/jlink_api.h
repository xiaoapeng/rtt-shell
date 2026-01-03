/**
 * @file jlink_rtt.h
 * @brief J-Link RTT 接口
 * @author simon.xiaoapeng (simon.xiaoapeng@gmail.com)
 * @date 2025-12-29
 * 
 * @copyright Copyright (c) 2025  simon.xiaoapeng@gmail.com
 * 
 */

#ifndef _JLINK_API_H_
#define _JLINK_API_H_


#ifdef __cplusplus
#if __cplusplus
extern "C"{
#endif
#endif /* __cplusplus */

#include <stdint.h>

extern int JLINK_EMU_SelectByUSBSN(unsigned usbsn);
extern int JLINK_Open(void);
extern int JLINK_Close(void);
extern int JLINK_GetSN(unsigned *sn);
extern int JLINK_SetSpeed(unsigned speed);
extern int JLINK_TIF_Select(int tif);
extern int JLINK_Connect(void);
extern int JLINK_ExecCommand(const char *in, char *out, int size);
extern void JLINK_EMU_GetProductName(char *out, int size);

#define RTT_DIRECTION_UP            0
#define RTT_DIRECTION_DOWN          1

struct rtt_desc {
    uint32_t index;
    uint32_t direction;
    char name[32];
    uint32_t size;
    uint32_t flags;
};

enum rtt_cmd{
    RTT_CMD_START = 0,
    RTT_CMD_STOP = 1,
    RTT_CMD_GET_DESC = 2,
    RTT_CMD_GET_NUM_BUF = 3,
    RTT_CMD_GET_STAT = 4
};

extern int JLINK_RTTERMINAL_Control(enum rtt_cmd cmd, void *data);
extern int JLINK_RTTERMINAL_Read(int channel, char *data, int len);
extern int JLINK_RTTERMINAL_Write(int channel, const char *data, int len);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* __cplusplus */


#endif // _JLINK_API_H_