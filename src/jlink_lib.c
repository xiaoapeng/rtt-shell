/**
 * @file jlink_lib.c
 * @brief jlink library 加载
 * @author simon.xiaoapeng (simon.xiaoapeng@gmail.com)
 * @date 2025-12-30
 * 
 * @copyright Copyright (c) 2025  simon.xiaoapeng@gmail.com
 * 
 */

#include <stdio.h>

#ifdef _WIN32
    #include <windows.h>
    #define DYNLIB_HANDLE HMODULE
    #define DYNLIB_OPEN(path) LoadLibrary(path)
    #define DYNLIB_GET(handle, name) GetProcAddress(handle, name)
    #define DYNLIB_CLOSE(handle) FreeLibrary(handle)
#else
    #include <dlfcn.h>
    #define DYNLIB_HANDLE void*
    #define DYNLIB_OPEN(path) dlopen(path, RTLD_LAZY)
    #define DYNLIB_GET(handle, name) dlsym(handle, name)
    #define DYNLIB_CLOSE(handle) dlclose(handle)
#endif

static int (*jlink_emu_select_by_usbsn)(unsigned usbsn);
static int (*jlink_open)(void);
static int (*jlink_close)(void);
static int (*jlink_get_sn)(unsigned *sn);
static int (*jlink_set_speed)(unsigned speed);
static int (*jlink_tif_select)(int tif);
static int (*jlink_connect)(void);
static int (*jlink_exec_command)(const char *in, char *out, int size);
static void (*jlink_emu_get_product_name)(char *out, int size);
static int (*jlink_rtterminal_control)(int cmd, void *data);
static int (*jlink_rtterminal_read)(int channel, char *data, int len);
static int (*jlink_rtterminal_write)(int channel, const char *data, int len);

static DYNLIB_HANDLE jlink_lib_handle = NULL;

int JLINK_EMU_SelectByUSBSN(unsigned usbsn){
    if(jlink_emu_select_by_usbsn){
        return jlink_emu_select_by_usbsn(usbsn);
    }
    return -1;
}

int JLINK_Open(void){
    if(jlink_open){
        return jlink_open();
    }
    return -1;
}

int JLINK_Close(void){
    if(jlink_close){
        return jlink_close();
    }
    return -1;
}

int JLINK_GetSN(unsigned *sn){
    if(jlink_get_sn){
        return jlink_get_sn(sn);
    }
    return -1;
}

int JLINK_SetSpeed(unsigned speed){
    if(jlink_set_speed){
        return jlink_set_speed(speed);
    }
    return -1;
}
int JLINK_TIF_Select(int tif){
    if(jlink_tif_select){
        return jlink_tif_select(tif);
    }
    return -1;
}

int JLINK_Connect(void){
    if(jlink_connect){
        return jlink_connect();
    }
    return -1;
}

int JLINK_ExecCommand(const char *in, char *out, int size){
    if(jlink_exec_command){
        return jlink_exec_command(in, out, size);
    }
    return -1;
}

void JLINK_EMU_GetProductName(char *out, int size){
    if(jlink_emu_get_product_name){
        jlink_emu_get_product_name(out, size);
    }
}

int JLINK_RTTERMINAL_Control(int cmd, void *data){
    if(jlink_rtterminal_control){
        return jlink_rtterminal_control(cmd, data);
    }
    return -1;
}



int JLINK_RTTERMINAL_Read(int channel, char *data, int len){
    if(jlink_rtterminal_read){
        return jlink_rtterminal_read(channel, data, len);
    }
    return -1;
}
int JLINK_RTTERMINAL_Write(int channel, const char *data, int len){
    if(jlink_rtterminal_write){
        return jlink_rtterminal_write(channel, data, len);
    }
    return -1;
}


extern const char *jlink_find_lib_path(void);

int jlink_lib_init(void){
    const char *lib_path = jlink_find_lib_path();
    if(!lib_path){
        /* 提醒客户 jlink 库未找到 */
        printf("jlink library not found.\n");
        return -1;
    }
    jlink_lib_handle = DYNLIB_OPEN(lib_path);
    if(!jlink_lib_handle){
        return -1;
    }

    jlink_emu_select_by_usbsn = DYNLIB_GET(jlink_lib_handle, "JLINK_EMU_SelectByUSBSN");
    jlink_open = DYNLIB_GET(jlink_lib_handle, "JLINK_Open");
    jlink_close = DYNLIB_GET(jlink_lib_handle, "JLINK_Close");
    jlink_get_sn = DYNLIB_GET(jlink_lib_handle, "JLINK_GetSN");
    jlink_set_speed = DYNLIB_GET(jlink_lib_handle, "JLINK_SetSpeed");
    jlink_tif_select = DYNLIB_GET(jlink_lib_handle, "JLINK_TIF_Select");
    jlink_connect = DYNLIB_GET(jlink_lib_handle, "JLINK_Connect");
    jlink_exec_command = DYNLIB_GET(jlink_lib_handle, "JLINK_ExecCommand");
    jlink_emu_get_product_name = DYNLIB_GET(jlink_lib_handle, "JLINK_EMU_GetProductName");
    jlink_rtterminal_control = DYNLIB_GET(jlink_lib_handle, "JLINK_RTTERMINAL_Control");
    jlink_rtterminal_read = DYNLIB_GET(jlink_lib_handle, "JLINK_RTTERMINAL_Read");
    jlink_rtterminal_write = DYNLIB_GET(jlink_lib_handle, "JLINK_RTTERMINAL_Write");

    if( !jlink_emu_select_by_usbsn || !jlink_open || 
        !jlink_close || !jlink_get_sn || !jlink_set_speed || !jlink_tif_select || 
        !jlink_connect || !jlink_exec_command || !jlink_emu_get_product_name || 
        !jlink_rtterminal_control || !jlink_rtterminal_read || !jlink_rtterminal_write){
        return -1;
    }

    return 0;
}

void jlink_lib_deinit(void){
    if(jlink_lib_handle){
        DYNLIB_CLOSE(jlink_lib_handle);
        jlink_lib_handle = NULL;
    }
}


