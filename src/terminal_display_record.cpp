/**
 * @file terminal_display_record.cpp
 * @brief 终端显示，并记录终端日志到文件
 * @author simon.xiaoapeng (simon.xiaoapeng@gmail.com)
 * @date 2026-01-07
 * 
 * @copyright Copyright (c) 2026  simon.xiaoapeng@gmail.com
 * 
 */

#include <iomanip>
#include <iostream>
#include <string>
#include <queue>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <thread>
#include <fstream>
#include <sstream>

enum ehshell_escape_char{
    ESCAPE_CHAR_NUL                 = 0x00,
    ESCAPE_CHAR_CTRL_A              = 0x01,
    ESCAPE_CHAR_CTRL_C_SIGINT       = 0x03,     /* 发送退出信号 */
    ESCAPE_CHAR_CTRL_BACKSPACE_0    = 0x08,     /* 删除前一个字符 */
    ESCAPE_CHAR_CTRL_TAB            = 0x09,     /* 可用于TAB补全 */
    ESCAPE_CHAR_CTRL_J_LF           = 0x0A,     /* 换行 Enter*/
    ESCAPE_CHAR_CTRL_L_CLS          = 0x0C,     /* 清除屏 */
    ESCAPE_CHAR_CTRL_M_CR           = 0x0D,     /* 回到行首 */
    ESCAPE_CHAR_CTRL_U_DEL_LINE     = 0x0E,     /* 删除当前行 */
    ESCAPE_CHAR_CTRL_W_DEL_WORD     = 0x0F,     /* 删除当前单词 */
    ESCAPE_CHAR_CTRL_Z              = 0x1A,     /* 发送退出信号 */
    ESCAPE_CHAR_CTRL_BACKSPACE_1    = 0x7f,     /* 删除前一个字符 */
    ESCAPE_CHAR_CTRL_UTF8_START     = 0x80,     /* UTF-8 多字节字符开始 */
    ESCAPE_CHAR_CTRL_NOSTD_START    = 0xFF,     /* 非标准转义字符 */
    ESCAPE_CHAR_CTRL_RESET,                     /* 重置终端 */
    ESCAPE_CHAR_CTRL_HOME,                      /* 移动光标到行首 */
    ESCAPE_CHAR_CTRL_END,                       /* 移动光标到行尾 */
    ESCAPE_CHAR_CTRL_LEFT,                      /* 移动光标左 */
    ESCAPE_CHAR_CTRL_RIGHT,                     /* 移动光标右 */
    ESCAPE_CHAR_CTRL_UP,                        /* 移动光标上 */
    ESCAPE_CHAR_CTRL_DOWN,                      /* 移动光标下 */
    ESCAPE_CHAR_CTRL_DELETE,                    /* 删除光标后面的字符 */
    ESCAPE_CHAR_CTRL_OTHER ,                    /* 其他转义字符 */
    ESCAPE_CHAR_CTRL_UTF8,                      /* UTF-8 多字节字符 */
};



#define TERMINAL_ESCAPE_MATCH_INCOMPLETE             0xfe
#define TERMINAL_ESCAPE_MATCH_FAIL                   0xff


#define TERMINAL_ESCAPE_MATCH_NONE                   ((uint16_t)0x00)
#define TERMINAL_ESCAPE_MATCH_ESC                    ((uint16_t)0x01)        
#define TERMINAL_ESCAPE_MATCH_ESC_OSC                ((uint16_t)0x02)        // after ESC ]
#define TERMINAL_ESCAPE_MATCH_ESC_DCS                ((uint16_t)0x03)        // after ESC P    
#define TERMINAL_ESCAPE_MATCH_ESC_PM                 ((uint16_t)0x04)        // after ESC ^    
#define TERMINAL_ESCAPE_MATCH_ESC_APC                ((uint16_t)0x05)        // after ESC _    
#define TERMINAL_ESCAPE_MATCH_ESC_SS3                ((uint16_t)0x06)        // after ESC O    
#define TERMINAL_ESCAPE_MATCH_ESC_CSI                ((uint16_t)0x07)        // after ESC [
#define TERMINAL_ESCAPE_MATCH_ESC_STRING_WAIT_ST     ((uint16_t)0x08)        // expecting ESC '\' terminator

#define TERMINAL_ESCAPE_CHAR_PARSE_BUF_SIZE 64

static std::ofstream s_log_file;
static std::mutex s_mtx;
static std::condition_variable s_cv;
static std::queue<std::vector<char>> s_rtt_rx_queue;
static bool s_req_stop = false;
static uint16_t  s_escape_char_match_state;
static char s_escape_char_parse_buf[TERMINAL_ESCAPE_CHAR_PARSE_BUF_SIZE];
static std::string s_escape_char_parse_buf_str;
static std::vector<char> s_linebuf;
static uint32_t          s_linebuf_insert_pos = 0;
static bool              s_is_new_line = true;
static std::string       s_linebuf_current_time_str;
static std::thread *s_thread = nullptr;

extern "C" {
    static void (*s_quit_signal_callback)(void);
}

static inline int is_csi_final(uint8_t c){ return c >= 0x40 && c <= 0x7E; }
static inline int is_middle_byte(uint8_t c){ return c >= 0x20 && c <= 0x2F; }
static inline int is_param_byte(uint8_t c){ return (c >= '0' && c <= '9') || c == ';' || c == '?' || c == '>' || c == '<'; }

static std::string get_current_time_str() {
    using namespace std::chrono;

    // 1. 获取当前时间点
    auto now = system_clock::now();
    
    // 2. 转换为 time_t (秒)
    auto tt = system_clock::to_time_t(now);
    auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;

    // 3. 转换为本地时间结构体
    std::tm bt;
#if defined(_MSC_VER) // Windows 环境安全版本
    localtime_s(&bt, &tt);
#else // Linux/Unix 环境安全版本
    localtime_r(&tt, &bt);
#endif

    // 4. 格式化输出
    std::ostringstream oss;
    oss << std::put_time(&bt, "[%Y-%m-%d %H:%M:%S");
    oss << '.' << std::setfill('0') << std::setw(3) << ms.count() << "]";
    
    return oss.str();
}

/**
 * @brief                   尝试匹配转义字符
 * @param  input            输入字符
 * @return uint32_t 
 */
static enum ehshell_escape_char ehshell_escape_char_parse(const char input){
    if(s_escape_char_match_state != TERMINAL_ESCAPE_MATCH_NONE)
        s_escape_char_parse_buf_str += input;
    switch (s_escape_char_match_state){
        case TERMINAL_ESCAPE_MATCH_NONE:{
            if(input == 0x1B){
                s_escape_char_match_state = TERMINAL_ESCAPE_MATCH_ESC;
                s_escape_char_parse_buf[0] = '\0';
                s_escape_char_parse_buf_str = "\x1B";
                break;
            }
            return (enum ehshell_escape_char)(uint8_t)input;
        }
        case TERMINAL_ESCAPE_MATCH_ESC:{
            if(input == '['){
                s_escape_char_match_state = TERMINAL_ESCAPE_MATCH_ESC_CSI;
                break;
            }else if(input == ']'){
                s_escape_char_match_state = TERMINAL_ESCAPE_MATCH_ESC_OSC;
                break;
            }else if(input == 'P'){
                s_escape_char_match_state = TERMINAL_ESCAPE_MATCH_ESC_DCS;
                break;
            }else if(input == '^'){
                s_escape_char_match_state = TERMINAL_ESCAPE_MATCH_ESC_PM;
                break;
            }else if(input == '_'){
                s_escape_char_match_state = TERMINAL_ESCAPE_MATCH_ESC_APC;
                break;
            }else if(input == 'O'){
                s_escape_char_match_state = TERMINAL_ESCAPE_MATCH_ESC_SS3;
                break;
            }
            /* 双字节转义字符，目前没有我们想用的，直接忽略返回 */
            s_escape_char_match_state = TERMINAL_ESCAPE_MATCH_NONE;
            break;
        }
        case TERMINAL_ESCAPE_MATCH_ESC_OSC:
        case TERMINAL_ESCAPE_MATCH_ESC_DCS:
        case TERMINAL_ESCAPE_MATCH_ESC_PM :
        case TERMINAL_ESCAPE_MATCH_ESC_APC:{
            if(input == 0x07){
                s_escape_char_match_state = TERMINAL_ESCAPE_MATCH_NONE;
                break;
            }else if(input == 0x1B){
                s_escape_char_match_state = TERMINAL_ESCAPE_MATCH_ESC_STRING_WAIT_ST;
                break;
            }
            s_escape_char_parse_buf[0]++;
            if( (uint32_t)s_escape_char_parse_buf[0] >= TERMINAL_ESCAPE_CHAR_PARSE_BUF_SIZE - 1)
                goto reset;
            /* 一个和多个字符序列，直接忽略 */
            break;
        }
        case TERMINAL_ESCAPE_MATCH_ESC_SS3:{
            /* 匹配到任意字符后，直接忽略，正常来说会匹配 0x40..0x7E */
            if(is_csi_final((uint8_t)input)){
                s_escape_char_match_state = TERMINAL_ESCAPE_MATCH_NONE;
                break;
            }
            goto reset;
        }
        case TERMINAL_ESCAPE_MATCH_ESC_CSI:{
            if(s_escape_char_parse_buf[0] < TERMINAL_ESCAPE_CHAR_PARSE_BUF_SIZE -1){
                s_escape_char_parse_buf[s_escape_char_parse_buf[0]+1] = input;
                s_escape_char_parse_buf[0]++;
            }else{
                goto reset;
            }
            if(is_csi_final((uint8_t)input)){
                s_escape_char_match_state = TERMINAL_ESCAPE_MATCH_NONE;
                /* 开始解析存有的字符 */
                if(s_escape_char_parse_buf[0] == 2 && s_escape_char_parse_buf[2] == '~'){
                    switch (s_escape_char_parse_buf[1]) {
                        case '1':
                            return ESCAPE_CHAR_CTRL_HOME;
                        case '3':
                            return ESCAPE_CHAR_CTRL_DELETE;
                        case '4':
                            return ESCAPE_CHAR_CTRL_END;
                        default:
                            return ESCAPE_CHAR_CTRL_OTHER;
                    }
                }else if(s_escape_char_parse_buf[0] == 1){
                    switch (s_escape_char_parse_buf[1]) {
                        case 'A':
                            return ESCAPE_CHAR_CTRL_UP;
                        case 'B':
                            return ESCAPE_CHAR_CTRL_DOWN;
                        case 'C':
                            return ESCAPE_CHAR_CTRL_RIGHT;
                        case 'D':
                            return ESCAPE_CHAR_CTRL_LEFT;
                        case 'F':
                            return ESCAPE_CHAR_CTRL_END;
                        case 'H':
                            return ESCAPE_CHAR_CTRL_HOME;
                        default:
                            return ESCAPE_CHAR_CTRL_OTHER;
                    }
                }

                break;
            }
            if(!is_middle_byte((uint8_t)input) && !is_param_byte((uint8_t)input))
                goto reset;
            break;
        }
        case TERMINAL_ESCAPE_MATCH_ESC_STRING_WAIT_ST:{
            if (input == '\\'){
                s_escape_char_match_state = TERMINAL_ESCAPE_MATCH_NONE;
                break;
            }
            goto reset;
        }
    
    }
    if(s_escape_char_match_state == TERMINAL_ESCAPE_MATCH_NONE)
        return ESCAPE_CHAR_CTRL_OTHER;
    return ESCAPE_CHAR_NUL;
reset:
    s_escape_char_match_state = TERMINAL_ESCAPE_MATCH_NONE;
    return ESCAPE_CHAR_CTRL_RESET;
}



static void terminal_display_record_process_data(std::vector<char>& data)
{
    enum ehshell_escape_char ch;
    bool is_quit_sigint = false;
    
    for(auto c : data){
        ch = ehshell_escape_char_parse(c);
        if(ch <= 0xFF && (std::isprint(ch) || ch >= ESCAPE_CHAR_CTRL_UTF8_START)){
            if(s_is_new_line){
                s_is_new_line = false;
                s_linebuf_current_time_str = get_current_time_str();
                std::cout << s_linebuf_current_time_str << ">>>  ";
            }
            std::cout << char(ch);
            /* 行buf s_linebuf_insert_pos处插入字符 */
            if (s_linebuf_insert_pos < s_linebuf.size()) {
                s_linebuf[s_linebuf_insert_pos] = char(ch);
            }else{
                s_linebuf.insert(s_linebuf.begin() + s_linebuf_insert_pos, char(ch));
            }
            s_linebuf_insert_pos++;
            continue;
        }
        switch (ch) {
            case ESCAPE_CHAR_CTRL_C_SIGINT:
                is_quit_sigint = true;
                continue;
            case ESCAPE_CHAR_CTRL_BACKSPACE_0:
                if(s_linebuf_insert_pos > 0){
                    s_linebuf.erase(s_linebuf.begin() + s_linebuf_insert_pos - 1);
                    s_linebuf_insert_pos--;
                    std::cout << "\b \b";
                }
                continue;
            case ESCAPE_CHAR_CTRL_TAB:
                std::cout << "\t";
                continue;
            case ESCAPE_CHAR_CTRL_J_LF:
                std::cout << "\n";
                s_linebuf.push_back('\0');
                s_log_file << s_linebuf_current_time_str << ">>>  " << (const char*)s_linebuf.data() << "\n" << std::flush;
                s_linebuf_insert_pos = 0;
                s_is_new_line = true;
                s_linebuf.clear();
                continue;
            case ESCAPE_CHAR_CTRL_M_CR:
                std::cout << "\r" << s_linebuf_current_time_str << ">>>  ";
                s_linebuf_insert_pos = 0;
                continue;
            case ESCAPE_CHAR_CTRL_U_DEL_LINE:
                std::cout << "\x0e\r";
                s_linebuf_insert_pos = 0;
                s_linebuf.clear();
                continue;
            case ESCAPE_CHAR_CTRL_LEFT:
                if(s_linebuf_insert_pos > 0){
                    s_linebuf_insert_pos--;
                    std::cout << "\x1B[D";
                }
                continue;
            case ESCAPE_CHAR_CTRL_RIGHT:
                if(s_linebuf_insert_pos < s_linebuf.size()){
                    s_linebuf_insert_pos++;
                    std::cout << "\x1B[C";
                }
                continue;
            case ESCAPE_CHAR_CTRL_OTHER:
                std::cout << s_escape_char_parse_buf_str;
                continue;
            default:
                continue;
        }
    }
    std::cout << std::flush;
    if(is_quit_sigint && s_quit_signal_callback)
        s_quit_signal_callback();
}

static void terminal_display_record_thread(void)
{
    std::vector<char> data;
    while(true){
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
                goto stop;

            s_cv.wait(lck);
        }
    process_data:
        terminal_display_record_process_data(data);
        data.clear();
    }
stop:
    return ;
}



extern "C"{

int terminal_display_record_start(const char* log_file_path)
{
    if(log_file_path){
        /* 检查路径是否存在，不存在则创建，存在就正常打开并追加写入 */
        s_log_file = std::ofstream(log_file_path, std::ios::out | std::ios::app);
        if(!s_log_file.is_open()){
            std::printf("open log file %s failed\n", log_file_path);
            return -1;
        }
    }
    s_rtt_rx_queue = std::queue<std::vector<char>>();
    s_req_stop = false;
    s_escape_char_match_state = TERMINAL_ESCAPE_MATCH_NONE;
    s_escape_char_parse_buf[0] = '\0';
    s_escape_char_parse_buf_str = "";
    s_linebuf.clear();
    s_linebuf_insert_pos = 0;
    s_is_new_line = true;
    s_linebuf_current_time_str = "";
    s_thread = new std::thread(terminal_display_record_thread);

    return 0;
}

void terminal_display_record_stop()
{
    if(s_thread){
        s_req_stop = true;
        s_cv.notify_one();
        s_thread->join();
        delete s_thread;
        s_thread = nullptr;
    }
    if(s_log_file.is_open()){
        s_log_file.close();
    }
}


void terminal_display_record_write(const char* data, size_t size){
    if(!data || size == 0){
        return;
    }
    std::unique_lock<std::mutex> lck(s_mtx);
    s_rtt_rx_queue.push(std::vector<char>(data, data + size));
    s_cv.notify_one();
}


void terminal_display_record_quit_signal_set_callback(void (*callback)(void))
{
    s_quit_signal_callback = callback;
}


}