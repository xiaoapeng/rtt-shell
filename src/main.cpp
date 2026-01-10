/**
 * @file main.c
 * @brief rtt-shell main function
 * @author simon.xiaoapeng (simon.xiaoapeng@gmail.com)
 * @date 2025-12-29
 * 
 * @copyright Copyright (c) 2025  simon.xiaoapeng@gmail.com
 * 
 */

#include <string>
#include <thread>
#include <chrono>
#include <iostream>
#include <atomic>

#include "cpp-terminal/key.hpp"
#include "cpp-terminal/terminal.hpp"
#include "cpp-terminal/input.hpp"
#include "cpp-terminal/event.hpp"

#include "cxxopts.hpp"
#include "jlink_lib.h"
#include "jlink_api.h"
#include "jlink_rtt.h"
#include "terminal_display_record.h"

static std::atomic<bool> s_req_stop(false);

static std::string to_lower_locale(const std::string& str, const std::locale& loc = std::locale()) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(),
                   [&loc](char c) { return std::tolower(c, loc); });
    return result;
}
static void terminal_display_record_quit_signal_handler(void){
    s_req_stop.store(true);
    Term::push_event(Term::Event());
}
static std::optional<std::string> key_to_escape(Term::Key key){
    if(key.isExtendedASCII())
        return key.str();
    switch(key){
        case Term::Key::ArrowLeft:
            return "\x1b[D";
        case Term::Key::ArrowRight:
            return "\x1b[C";
        case Term::Key::ArrowUp:
            return "\x1b[A";
        case Term::Key::ArrowDown:
            return "\x1b[B";
        case Term::Key::Home:
            return "\x1b[H";
        case Term::Key::End:
            return "\x1b[F";
        default:
            break;
    }
    return std::nullopt;
}


int main(int argc, char* argv[])
{
    int ret = 0;
    int if_type = 1;
    int rx_channel = 0;
    int tx_channel = 0;
    cxxopts::Options options("rtt-shell", "JLink RTT Shell");
    options.add_options()
        ("h,help", "Print help")
        ("d,device", "JLink device name", cxxopts::value<std::string>()->default_value("MCXN947_M33_0"))
        ("i,if", "JLink interface name (JTAG or SWD or cJTAG)", cxxopts::value<std::string>()->default_value("swd"))
        ("s,speed", "JLink speed", cxxopts::value<int>()->default_value("4000"))
        ("c,channel", "RTT channel number (rx,tx)", cxxopts::value<std::vector<int>>()->default_value("0,0"))
        ("a,addr", "RTT address (0xXXXXXXXX)", cxxopts::value<unsigned long>()->default_value("0"))
        ("r,range", "RTT range (0xXXXXXXXX)", cxxopts::value<unsigned long>()->default_value("0"))
        ("t,time_record", "Log Time record")
        ("l,out_log", "Output log file name", cxxopts::value<std::string>())
        ;

    cxxopts::ParseResult args = options.parse(argc, argv);

    if(args.count("help")){
        std::cout << options.help() << std::endl;
        return 0;
    }
    // if(!args.count("device")){
    //     std::cout << "device is required" << std::endl;
    //     return -1;
    // }

    std::string if_name = to_lower_locale(args["if"].as<std::string>());

    if(if_name == "jtag"){
        if_type = 0;
    }else if(if_name == "swd"){
        if_type = 1;
    }else if(if_name == "cjtag"){
        if_type = 2;
    }else{
        std::cout << "interface name is invalid" << std::endl;
        return -1;
    }
    std::vector<int> channel = args["channel"].as<std::vector<int>>();
    if(channel.size() != 2){
        std::cout << "channel is invalid" << std::endl;
        return -1;
    }

    std::string log_file_path;
    const char *log_file_path_cstr = nullptr;
    if(args.count("out_log")){
        log_file_path = args["out_log"].as<std::string>();
        log_file_path_cstr = log_file_path.c_str();
    }


    if(jlink_lib_init() < 0){
        std::cout << "jlink_lib_init failed" << std::endl;
        return -1;
    }
    ret = JLINK_Open();
    if(ret < 0){
        std::cout << "JLINK_Open failed ret:" << ret << std::endl;
        return -1;
    }

    if(JLINK_ExecCommand(("device=" + args["device"].as<std::string>()).c_str(), NULL, 0) < 0){
        std::cout << "JLINK_ExecCommand failed" << std::endl;
        goto close;
    }
    JLINK_TIF_Select(if_type);
    if(JLINK_SetSpeed(unsigned(args["speed"].as<int>())) < 0){
        std::cout << "JLINK_SetSpeed failed" << std::endl;
        goto close;
    }

    if(JLINK_Connect() < 0){
        std::cout << "JLINK_Connect failed" << std::endl;
        goto close;
    }
    ret = jlink_rtt_start(tx_channel, rx_channel, args["addr"].as<unsigned long>(), args["range"].as<unsigned long>());
    if(ret < 0){
        std::cout << "jlink_rtt_start failed" << std::endl;
        goto close;
    }

    ret = terminal_display_record_start(log_file_path_cstr);
    if(ret < 0){
        std::cout << "terminal_display_record_start failed" << std::endl;
        goto terminal_display_record_start_error;
    }

    Term::terminal.setOptions(Term::Option::Cooked, Term::Option::NoSignalKeys, Term::Option::Cursor);

    jlink_rtt_set_recv_callback(terminal_display_record_write);
    terminal_display_record_quit_signal_set_callback(terminal_display_record_quit_signal_handler);
    
    while(1)
    {
        Term::Event event = Term::read_event();
        switch(event.type()){
            case Term::Event::Type::Key:{
                Term::Key key(event);
                if(auto escape = key_to_escape(key); escape.has_value()){
                    jlink_rtt_transmit(escape->c_str(), int(escape->size()));
                    continue;
                }
            }
            case Term::Event::Type::CopyPaste:{
                std::string key_str(event);
                jlink_rtt_transmit(key_str.c_str(), int(key_str.size()));
                continue;
            }
            default:
                break;
        }
        if(s_req_stop.load())
            break;
    }
    terminal_display_record_stop();
terminal_display_record_start_error:
    jlink_rtt_stop();
close:
    if(JLINK_Close() < 0){
        std::printf("JLINK_Close failed\n");
        return -1;
    }

    jlink_lib_deinit();
    return 0;
}