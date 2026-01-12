// AI实现

#define _CRT_SECURE_NO_WARNINGS

#include <string>
#include <vector>
#include <array>
#include <optional>
#include <filesystem>
#include <algorithm>
#include <cstring>
#include <regex>

namespace fs = std::filesystem;

// 平台特定头文件
#if defined(_WIN32) || defined(_WIN64)
    #include <windows.h>
#elif defined(__APPLE__)
    #include <dlfcn.h>
    #include <mach-o/dyld.h>
#elif defined(__linux__)
    #include <dlfcn.h>
    #include <link.h>
#endif

#define JLINK_SDK_NAME "libjlinkarm"
#define JLINK_SDK_OBJECT "jlinkarm"
#define WINDOWS_32_JLINK_SDK_NAME "JLinkARM"
#define WINDOWS_64_JLINK_SDK_NAME "JLink_x64"

// 获取适当的 Windows SDK 名称
static const char* get_appropriate_windows_sdk_name() {
#if defined(_WIN64) || defined(__x86_64__) || defined(__ppc64__)
    return WINDOWS_64_JLINK_SDK_NAME;
#else
    return WINDOWS_32_JLINK_SDK_NAME;
#endif
}
#if defined(_WIN32) || defined(_WIN64)

static std::optional<std::string> read_registry_string(HKEY hKey, const std::string& subkey, 
                                                const std::string& value_name) {
    HKEY hSubKey = NULL;
    if (RegOpenKeyExA(hKey, subkey.c_str(), 0, KEY_READ, &hSubKey) != ERROR_SUCCESS) {
        return std::nullopt;
    }
    
    char buffer[MAX_PATH] = {0};
    DWORD buffer_size = sizeof(buffer);
    DWORD type = 0;
    
    LONG result = RegQueryValueExA(hSubKey, value_name.c_str(), NULL, &type,
                                   reinterpret_cast<LPBYTE>(buffer), &buffer_size);
    RegCloseKey(hSubKey);
    
    if (result == ERROR_SUCCESS && type == REG_SZ) {
        return std::string(buffer);
    }
    
    return std::nullopt;
}

static bool check_jlink_directory(const fs::path& dir, const std::string& dll_full) {
    if (!fs::exists(dir) || !fs::is_directory(dir)) {
        return false;
    }
    
    for (const auto& entry : fs::directory_iterator(dir)) {
        if (!entry.is_directory()) continue;
        
        std::string dir_name = entry.path().filename().string();
        if (dir_name.find("JLink") == 0) {
            fs::path lib_path = entry.path() / dll_full;
            if (fs::exists(lib_path) && fs::is_regular_file(lib_path)) {
                return true;
            }
        }
    }
    
    return false;
}

static std::vector<std::string> get_jlink_path_from_registry() {
    static const std::vector<std::pair<HKEY, std::string>> registry_locations = {
        {HKEY_LOCAL_MACHINE, "SOFTWARE\\SEGGER\\J-Link"},
        {HKEY_LOCAL_MACHINE, "SOFTWARE\\Wow6432Node\\SEGGER\\J-Link"},
        {HKEY_CURRENT_USER, "SOFTWARE\\SEGGER\\J-Link"}
    };
    
    std::vector<std::string> found_paths;
    
    for (const auto& [hkey, subkey] : registry_locations) {
        auto install_path = read_registry_string(hkey, subkey, "InstallPath");
        if (install_path.has_value() && !install_path->empty()) {
            found_paths.push_back(*install_path);
        }
    }
    
    return found_paths;
}

static std::optional<std::string> find_library_windows() {
    const char* dll = get_appropriate_windows_sdk_name();
    std::string dll_full = std::string(dll) + ".dll";
    
    // 1. 首先尝试从注册表查找
    auto registry_paths = get_jlink_path_from_registry();
    for (const auto& registry_path : registry_paths) {
        fs::path segger_path = registry_path;
        segger_path = segger_path.parent_path(); // 获取SEGGER目录
        
        if (check_jlink_directory(segger_path, dll_full)) {
            for (const auto& entry : fs::directory_iterator(segger_path)) {
                if (!entry.is_directory()) continue;
                
                std::string dir_name = entry.path().filename().string();
                if (dir_name.find("JLink") == 0) {
                    fs::path lib_path = entry.path() / dll_full;
                    if (fs::exists(lib_path) && fs::is_regular_file(lib_path)) {
                        return lib_path.string();
                    }
                }
            }
        }
    }
    
    // 2. 查找常见的安装目录
    static const std::vector<std::string> common_paths = {
        "C:\\Program Files\\SEGGER",
        "C:\\Program Files (x86)\\SEGGER"
    };
    
    for (const auto& path : common_paths) {
        if (check_jlink_directory(fs::path(path), dll_full)) {
            for (const auto& entry : fs::directory_iterator(path)) {
                if (!entry.is_directory()) continue;
                
                std::string dir_name = entry.path().filename().string();
                if (dir_name.find("JLink") == 0) {
                    fs::path lib_path = entry.path() / dll_full;
                    if (fs::exists(lib_path) && fs::is_regular_file(lib_path)) {
                        return lib_path.string();
                    }
                }
            }
        }
    }
    return std::nullopt;
}
#endif

#if defined(__linux__)
static std::optional<std::string> find_library_linux() {
    const char* dll = JLINK_SDK_NAME;
    fs::path root = "/opt/SEGGER";
    
    if (!fs::exists(root) || !fs::is_directory(root)) {
        return std::nullopt;
    }
    
    try {
        bool x86_found = false;
        std::vector<std::string> fnames;
        
        // 收集所有可能的库文件
        for (const auto& entry : fs::recursive_directory_iterator(root)) {
            if (!entry.is_regular_file()) continue;
            
            std::string filename = entry.path().filename().string();
            if (filename.find(dll) == 0) {
                fnames.push_back(entry.path().string());
                if (entry.path().string().find("_x86") != std::string::npos) {
                    x86_found = true;
                }
            }
        }
        
        // 选择正确的库文件
        for (const auto& fpath : fnames) {
            // 检查是否是 64 位系统
            bool is_64bit = (sizeof(void*) == 8);
            
            if (is_64bit) {
                if (fpath.find("_x86") == std::string::npos) {
                    return fpath;
                }
            } else {
                if (x86_found) {
                    if (fpath.find("_x86") != std::string::npos) {
                        return fpath;
                    }
                } else {
                    return fpath;
                }
            }
        }
    } catch (const fs::filesystem_error&) {
        // 忽略文件系统错误
    }
    
    return std::nullopt;
}
#endif

#if defined(__APPLE__)  
// macOS 查找逻辑
static std::optional<std::string> find_library_darwin() {
    const char* dll = JLINK_SDK_NAME;
    fs::path root = "/Applications/SEGGER";
    
    if (!fs::exists(root) || !fs::is_directory(root)) {
        return std::nullopt;
    }
    
    try {
        for (const auto& entry : fs::directory_iterator(root)) {
            if (!entry.is_directory()) continue;
            
            std::string dir_name = entry.path().filename().string();
            if (dir_name.find("JLink") != 0) continue;
            
            fs::path dir_path = entry.path();
            
            // 检查目录中的文件
            std::vector<std::string> files;
            for (const auto& file_entry : fs::directory_iterator(dir_path)) {
                if (file_entry.is_regular_file()) {
                    files.push_back(file_entry.path().filename().string());
                }
            }
            
            // 检查是否有 libjlinkarm.dylib
            fs::path dylib_path = dir_path / (std::string(dll) + ".dylib");
            if (std::find(files.begin(), files.end(), std::string(dll) + ".dylib") != files.end()) {
                return dylib_path.string();
            }
            
            // 查找版本化的库文件
            for (const auto& filename : files) {
                if (filename.find(dll) == 0) {
                    return (dir_path / filename).string();
                }
            }
        }
    } catch (const fs::filesystem_error&) {
        // 忽略文件系统错误
    }
    
    return std::nullopt;
}
#endif
// 尝试通过 ctypes 类似的方法查找（参考 Python 的 load_default）
static std::optional<std::string> find_library_ctypes() {
#if defined(_WIN32) || defined(_WIN64)
    // Windows: 尝试从 PATH 环境变量查找
    const char* dll = get_appropriate_windows_sdk_name();
    std::string dll_full = std::string(dll) + ".dll";
    
    if (const char* path_env = std::getenv("PATH")) {
        std::string path_str(path_env);
        size_t pos = 0;
        
        while ((pos = path_str.find(';')) != std::string::npos) {
            std::string path_item = path_str.substr(0, pos);
            fs::path test_path = fs::path(path_item) / dll_full;
            
            if (fs::exists(test_path) && fs::is_regular_file(test_path)) {
                return test_path.string();
            }
            
            path_str.erase(0, pos + 1);
        }
    }
    
#elif defined(__linux__)
    // Linux: 尝试从 LD_LIBRARY_PATH 查找
    if (const char* ld_path = std::getenv("LD_LIBRARY_PATH")) {
        std::string ld_str(ld_path);
        size_t pos = 0;
        
        while ((pos = ld_str.find(':')) != std::string::npos) {
            std::string path_item = ld_str.substr(0, pos);
            
            // 尝试查找 libjlinkarm.so*
            try {
                for (const auto& entry : fs::directory_iterator(path_item)) {
                    if (entry.is_regular_file()) {
                        std::string filename = entry.path().filename().string();
                        if (filename.find("libjlinkarm") == 0) {
                            return entry.path().string();
                        }
                    }
                }
            } catch (const fs::filesystem_error&) {
                // 忽略
            }
            
            ld_str.erase(0, pos + 1);
        }
    }
    
#elif defined(__APPLE__)
    // macOS: 尝试从 DYLD_LIBRARY_PATH 查找
    if (const char* dyld_path = std::getenv("DYLD_LIBRARY_PATH")) {
        std::string dyld_str(dyld_path);
        size_t pos = 0;
        
        while ((pos = dyld_str.find(':')) != std::string::npos) {
            std::string path_item = dyld_str.substr(0, pos);
            
            // 尝试查找 libjlinkarm.dylib*
            try {
                for (const auto& entry : fs::directory_iterator(path_item)) {
                    if (entry.is_regular_file()) {
                        std::string filename = entry.path().filename().string();
                        if (filename.find("libjlinkarm") == 0) {
                            return entry.path().string();
                        }
                    }
                }
            } catch (const fs::filesystem_error&) {
                // 忽略
            }
            
            dyld_str.erase(0, pos + 1);
        }
    }
#endif
    
    return std::nullopt;
}

// 主查找函数
extern "C" const char *jlink_find_lib_path(void) {
    static char jlink_lib_path[1024] = {0};
    static bool searched = false;
    
    if (searched) {
        return jlink_lib_path[0] ? jlink_lib_path : NULL;
    }
    
    searched = true;
    std::optional<std::string> found_path;
    
    // 1. 先尝试 ctypes 类似的方法
    found_path = find_library_ctypes();
    
    // 2. 如果没找到，进行平台特定的深度搜索
    if (!found_path) {
#if defined(_WIN32) || defined(_WIN64)
        found_path = find_library_windows();
#elif defined(__linux__)
        found_path = find_library_linux();
#elif defined(__APPLE__)
        found_path = find_library_darwin();
#endif
    }
    
    // 3. 如果找到，复制到缓冲区
    if (found_path) {
        const std::string& path = *found_path;
        size_t len = path.length();
        if (len < sizeof(jlink_lib_path)) {
            std::strncpy(jlink_lib_path, path.c_str(), sizeof(jlink_lib_path) - 1);
            jlink_lib_path[sizeof(jlink_lib_path) - 1] = '\0';
            return jlink_lib_path;
        }
    }
    
    // 4. 没找到
    return NULL;
}