/**
 * @file terminal_display_record.h
 * @brief 终端显示，并记录终端日志到文件
 * @author simon.xiaoapeng (simon.xiaoapeng@gmail.com)
 * @date 2026-01-07
 * 
 * @copyright Copyright (c) 2026  simon.xiaoapeng@gmail.com
 * 
 */
#ifndef _TERMINAL_DISPLAY_RECORD_H_
#define _TERMINAL_DISPLAY_RECORD_H_


#ifdef __cplusplus
#if __cplusplus
extern "C"{
#endif
#endif /* __cplusplus */

/**
 * @brief 启动终端显示记录功能
 * 
 * @param log_file_path 日志文件路径
 * @return int 0 成功 -1 失败
 */
extern int terminal_display_record_start(const char* log_file_path);

/**
 * @brief 停止终端显示记录功能
 */
extern void terminal_display_record_stop(void);

/**
 * @brief 写入数据到终端显示记录程序
 * @param  data             数据指针
 * @param  size             数据大小
 */
extern void terminal_display_record_write(const char* data, size_t size);

/**
 * @brief 设置终端显示记录功能的退出信号回调函数
 * 
 * @param callback 退出信号回调函数指针
 */
extern void terminal_display_record_quit_signal_set_callback(void (*callback)(void));

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* __cplusplus */


#endif // _TERMINAL_DISPLAY_RECORD_H_