/**
 * @file jlink_rtt.h
 * @brief J-Link RTT 接口封装
 * @author simon.xiaoapeng (simon.xiaoapeng@gmail.com)
 * @date 2025-12-30
 * 
 * @copyright Copyright (c) 2025  simon.xiaoapeng@gmail.com
 * 
 */

#ifndef _JLINK_RTT_H_
#define _JLINK_RTT_H_


#ifdef __cplusplus
#if __cplusplus
extern "C"{
#endif
#endif /* __cplusplus */

/**
 * @brief  启动 J-Link RTT 功能
 * @param  tx_channel       发送数据通道号，一般情况下为0
 * @param  rx_channel       接收数据通道号，一般情况下为0
 * @param  addr             RTT 缓冲区地址,为0时自动匹配寻找
 * @param  range            RTT 缓冲区范围，为0时代表addr为rtt缓冲区地址
 * @return int              0 成功, -1 失败
 */
extern int jlink_rtt_start(int tx_channel, int rx_channel, unsigned long addr, unsigned long range);

/**
 * @brief  停止 J-Link RTT 功能
 * @return int              0 成功, -1 失败
 */
extern void jlink_rtt_stop(void);

/**
 * @brief  设置接收数据回调函数
 * @param  rx_cb            接收数据回调函数指针
 */
extern void jlink_rtt_set_recv_callback(void (*rx_cb)(char *data, int len));

/**
 * @brief  发送数据到 J-Link RTT 缓冲区
 * @param  data             要发送的数据指针
 * @param  len              要发送的数据长度
 * @return int              实际发送的数据长度, -1 失败
 */
extern int jlink_rtt_transmit(char *data, int len);



#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* __cplusplus */


#endif // _JLINK_RTT_H_