/**
 * 硬件温度监控和IPMI风扇控制程序
 * 
 * 功能：
 * 1. 读取CPU温度（通过sysfs hwmon接口）
 * 2. 读取内存温度（通过IPMI或sysfs）
 * 3. 读取显卡温度（通过sysfs或nvidia-smi）
 * 4. 通过IPMI设置风扇转速
 * 
 * 编译：g++ -o hardware_monitor hardware_monitor.cpp -std=c++17
 * 运行需要root权限或ipmi权限
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <filesystem>
#include <algorithm>
#include <cstdlib>
#include <regex>
#include <map>
#include <chrono>
#include <thread>
#include <csignal>
#include <atomic>

namespace fs = std::filesystem;

// 全局变量用于信号处理
std::atomic<bool> running(true);

// 配置结构体
struct Config {
    int interval = 5;
    std::string strategy = "auto";
    
    // 温度阈值
    int cpuTempHigh = 80;
    int cpuTempMedium = 70;
    int cpuTempLow = 60;
    int cpuTempMin = 50;
    int gpuTempHigh = 70;
    int memTempHigh = 60;
    
    // 风扇转速
    int fanSpeedMax = 100;
    int fanSpeedHigh = 80;
    int fanSpeedMedium = 60;
    int fanSpeedLow = 45;
    int fanSpeedMin = 30;
    
    // 其他设置
    int logLevel = 3;
    bool enableIPMI = true;
};

// 全局配置
Config g_config;

// 温度结构体
struct TemperatureInfo {
    std::string name;
    double celsius;
    bool valid;
};

// IPMI风扇模式
enum class FanMode {
    Automatic,
    Manual,
    FullSpeed
};

/**
 * 从文件读取字符串
 */
std::string readFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return "";
    }
    std::string content;
    std::getline(file, content);
    return content;
}

/**
 * 从文件读取整数
 */
int readFileInt(const std::string& path) {
    std::string content = readFile(path);
    if (content.empty()) {
        return -1;
    }
    try {
        return std::stoi(content);
    } catch (...) {
        return -1;
    }
}

/**
 * 加载配置文件
 */
bool loadConfig(const std::string& configPath) {
    std::ifstream file(configPath);
    if (!file.is_open()) {
        return false;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        // 跳过注释和空行
        if (line.empty() || line[0] == '#') {
            continue;
        }
        
        // 解析 KEY=VALUE 格式
        size_t pos = line.find('=');
        if (pos == std::string::npos) {
            continue;
        }
        
        std::string key = line.substr(0, pos);
        std::string value = line.substr(pos + 1);
        
        // 去除首尾空格
        key.erase(0, key.find_first_not_of(" \t"));
        key.erase(key.find_last_not_of(" \t") + 1);
        value.erase(0, value.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t") + 1);
        
        // 应用配置
        if (key == "INTERVAL") {
            g_config.interval = std::stoi(value);
        } else if (key == "STRATEGY") {
            g_config.strategy = value;
        } else if (key == "CPU_TEMP_HIGH") {
            g_config.cpuTempHigh = std::stoi(value);
        } else if (key == "CPU_TEMP_MEDIUM") {
            g_config.cpuTempMedium = std::stoi(value);
        } else if (key == "CPU_TEMP_LOW") {
            g_config.cpuTempLow = std::stoi(value);
        } else if (key == "CPU_TEMP_MIN") {
            g_config.cpuTempMin = std::stoi(value);
        } else if (key == "GPU_TEMP_HIGH") {
            g_config.gpuTempHigh = std::stoi(value);
        } else if (key == "MEM_TEMP_HIGH") {
            g_config.memTempHigh = std::stoi(value);
        } else if (key == "FAN_SPEED_MAX") {
            g_config.fanSpeedMax = std::stoi(value);
        } else if (key == "FAN_SPEED_HIGH") {
            g_config.fanSpeedHigh = std::stoi(value);
        } else if (key == "FAN_SPEED_MEDIUM") {
            g_config.fanSpeedMedium = std::stoi(value);
        } else if (key == "FAN_SPEED_LOW") {
            g_config.fanSpeedLow = std::stoi(value);
        } else if (key == "FAN_SPEED_MIN") {
            g_config.fanSpeedMin = std::stoi(value);
        } else if (key == "LOG_LEVEL") {
            g_config.logLevel = std::stoi(value);
        } else if (key == "ENABLE_IPMI") {
            g_config.enableIPMI = (value == "true" || value == "1");
        }
    }
    
    return true;
}

/**
 * 执行shell命令并获取输出
 */
std::string executeCommand(const std::string& cmd) {
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        return "";
    }
    
    char buffer[256];
    std::string result;
    while (fgets(buffer, sizeof(buffer), pipe)) {
        result += buffer;
    }
    pclose(pipe);
    return result;
}

/**
 * 读取CPU温度（通过sysfs hwmon）
 * 返回所有CPU核心温度的平均值
 */
TemperatureInfo getCPUTemperature() {
    TemperatureInfo info{"CPU", 0.0, false};
    std::vector<double> temps;
    
    // 遍历所有hwmon设备
    for (const auto& entry : fs::directory_iterator("/sys/class/hwmon")) {
        std::string hwmonPath = entry.path().string();
        std::string name = readFile(hwmonPath + "/name");
        
        // 查找CPU温度传感器（k10temp, coretemp, k8temp等）
        if (name.find("k10temp") != std::string::npos ||
            name.find("coretemp") != std::string::npos ||
            name.find("k8temp") != std::string::npos ||
            name.find("cpu") != std::string::npos) {
            
            // 读取所有temp*_input文件
            for (const auto& sensor : fs::directory_iterator(hwmonPath)) {
                std::string filename = sensor.path().filename().string();
                if (filename.find("temp") != std::string::npos &&
                    filename.find("_input") != std::string::npos) {
                    int tempMilliC = readFileInt(sensor.path().string());
                    if (tempMilliC > 0) {
                        temps.push_back(tempMilliC / 1000.0);
                    }
                }
            }
        }
    }
    
    if (!temps.empty()) {
        // 计算平均温度
        double sum = 0;
        for (double t : temps) {
            sum += t;
        }
        info.celsius = sum / temps.size();
        info.valid = true;
    }
    
    return info;
}

/**
 * 读取内存温度（通过IPMI）
 */
TemperatureInfo getMemoryTemperature() {
    TemperatureInfo info{"Memory", 0.0, false};
    
    // 尝试通过IPMI读取内存温度
    std::string output = executeCommand("ipmitool sdr list 2>/dev/null | grep -i 'dimm\\|memory'");
    
    if (!output.empty()) {
        std::vector<double> temps;
        std::regex tempRegex("([\\d.]+) degrees C");
        std::smatch match;
        
        std::istringstream iss(output);
        std::string line;
        while (std::getline(iss, line)) {
            if (std::regex_search(line, match, tempRegex)) {
                try {
                    double temp = std::stod(match[1].str());
                    if (temp > 0 && temp < 100) {  // 合理范围检查
                        temps.push_back(temp);
                    }
                } catch (...) {}
            }
        }
        
        if (!temps.empty()) {
            double sum = 0;
            for (double t : temps) {
                sum += t;
            }
            info.celsius = sum / temps.size();
            info.valid = true;
        }
    }
    
    // 如果IPMI失败，尝试从hwmon读取
    if (!info.valid) {
        for (const auto& entry : fs::directory_iterator("/sys/class/hwmon")) {
            std::string hwmonPath = entry.path().string();
            std::string name = readFile(hwmonPath + "/name");
            
            if (name.find("dimm") != std::string::npos ||
                name.find("memory") != std::string::npos) {
                
                for (const auto& sensor : fs::directory_iterator(hwmonPath)) {
                    std::string filename = sensor.path().filename().string();
                    if (filename.find("temp") != std::string::npos &&
                        filename.find("_input") != std::string::npos) {
                        int tempMilliC = readFileInt(sensor.path().string());
                        if (tempMilliC > 0) {
                            info.celsius = tempMilliC / 1000.0;
                            info.valid = true;
                            break;
                        }
                    }
                }
            }
        }
    }
    
    return info;
}

/**
 * 读取显卡温度
 * 支持NVIDIA、AMD和摩尔线程(Moore Threads)显卡
 */
TemperatureInfo getGPUTemperature() {
    TemperatureInfo info{"GPU", 0.0, false};
    
    // 方法1: 尝试nvidia-smi（NVIDIA显卡）
    std::string nvidiaOutput = executeCommand("nvidia-smi --query-gpu=temperature.gpu --format=csv,noheader,nounits 2>/dev/null");
    if (!nvidiaOutput.empty()) {
        try {
            info.celsius = std::stod(nvidiaOutput);
            info.valid = true;
            return info;
        } catch (...) {}
    }
    
    // 方法2: 通过sysfs hwmon读取（AMD/摩尔线程/其他显卡）
    for (const auto& entry : fs::directory_iterator("/sys/class/hwmon")) {
        std::string hwmonPath = entry.path().string();
        std::string name = readFile(hwmonPath + "/name");
        
        if (name.find("amdgpu") != std::string::npos ||
            name.find("radeon") != std::string::npos ||
            name.find("gpu") != std::string::npos ||
            name.find("nvidia") != std::string::npos ||
            name.find("mthreads") != std::string::npos ||  // 摩尔线程
            name.find("moore") != std::string::npos) {     // 摩尔线程
            
            for (const auto& sensor : fs::directory_iterator(hwmonPath)) {
                std::string filename = sensor.path().filename().string();
                if (filename.find("temp") != std::string::npos &&
                    filename.find("_input") != std::string::npos) {
                    int tempMilliC = readFileInt(sensor.path().string());
                    if (tempMilliC > 0) {
                        info.celsius = tempMilliC / 1000.0;
                        info.valid = true;
                        return info;
                    }
                }
            }
        }
    }
    
    // 方法3: 尝试读取显卡的PCI设备信息（包括摩尔线程）
    // 摩尔线程的vendor ID是0x1ed0
    for (const auto& entry : fs::directory_iterator("/sys/class/drm")) {
        std::string drmPath = entry.path().string();
        if (drmPath.find("card") != std::string::npos && 
            drmPath.find("-") == std::string::npos) {  // 只匹配cardX，不匹配cardX-YY
            
            std::string devicePath = drmPath + "/device";
            std::string vendorFile = devicePath + "/vendor";
            std::string vendor = readFile(vendorFile);
            
            // 检查是否是摩尔线程显卡 (vendor: 0x1ed0)
            bool isMooreThreads = (vendor.find("1ed0") != std::string::npos);
            
            std::string tempPath = devicePath + "/hwmon";
            if (fs::exists(tempPath)) {
                for (const auto& hwmon : fs::directory_iterator(tempPath)) {
                    std::string tempFile = hwmon.path().string() + "/temp1_input";
                    int tempMilliC = readFileInt(tempFile);
                    if (tempMilliC > 0) {
                        info.celsius = tempMilliC / 1000.0;
                        info.valid = true;
                        if (isMooreThreads) {
                            info.name = "Moore Threads GPU";
                        }
                        return info;
                    }
                }
            }
        }
    }
    
    // 方法4: 尝试使用mthreads-smi工具（摩尔线程的监控工具，如果存在）
    std::string mthreadsOutput = executeCommand("mthreads-smi --query-gpu=temperature.gpu --format=csv,noheader,nounits 2>/dev/null");
    if (!mthreadsOutput.empty()) {
        try {
            info.celsius = std::stod(mthreadsOutput);
            info.name = "Moore Threads GPU";
            info.valid = true;
            return info;
        } catch (...) {}
    }
    
    return info;
}

/**
 * 通过IPMI设置风扇模式
 */
bool setIPMIFanMode(FanMode mode) {
    std::string cmd;
    
    switch (mode) {
        case FanMode::Automatic:
            // 设置为自动模式
            cmd = "ipmitool raw 0x30 0x45 0x01 0x00 2>/dev/null";
            break;
        case FanMode::Manual:
            // 设置为手动模式
            cmd = "ipmitool raw 0x30 0x45 0x01 0x01 2>/dev/null";
            break;
        case FanMode::FullSpeed:
            // 设置为全速模式
            cmd = "ipmitool raw 0x30 0x45 0x01 0x02 2>/dev/null";
            break;
    }
    
    std::string result = executeCommand(cmd);
    return true;  // IPMI命令通常没有输出表示成功
}

/**
 * 通过IPMI设置风扇转速（百分比）
 * speed: 0-100 的百分比
 * fanId: 风扇ID（可选，某些服务器支持单独控制每个风扇）
 */
bool setIPMIFanSpeed(int speed, int fanId = 0) {
    if (speed < 0 || speed > 100) {
        std::cerr << "错误：风扇转速必须在0-100之间" << std::endl;
        return false;
    }
    
    // 设置风扇转速（0-100映射到0x00-0x64）
    char hexSpeed[4];
    snprintf(hexSpeed, sizeof(hexSpeed), "%02x", speed);
    std::string cmd = "ipmitool raw 0x30 0x70 0x66 0x01 0x00 0x" + 
                      std::string(hexSpeed) + " 2>/dev/null";
    std::string result = executeCommand(cmd);
    
    // 方法2: 使用ipmitool sensor命令（某些服务器）
    // 这需要根据具体服务器的风扇传感器名称调整
    // std::string cmd2 = "ipmitool sensor set \"Fan 1\" " + std::to_string(speed);
    
    return true;
}

/**
 * 获取当前风扇转速
 */
std::map<std::string, int> getFanSpeeds() {
    std::map<std::string, int> fans;
    
    // 通过IPMI获取风扇转速
    std::string output = executeCommand("ipmitool sdr list 2>/dev/null | grep -i fan");
    
    if (!output.empty()) {
        std::istringstream iss(output);
        std::string line;
        std::regex fanRegex("^(.+?)\\s+\\|\\s+(\\d+)\\s+RPM");
        std::smatch match;
        
        while (std::getline(iss, line)) {
            if (std::regex_search(line, match, fanRegex)) {
                std::string fanName = match[1].str();
                int rpm = std::stoi(match[2].str());
                
                // 去除首尾空格
                fanName.erase(0, fanName.find_first_not_of(" \t"));
                fanName.erase(fanName.find_last_not_of(" \t") + 1);
                
                fans[fanName] = rpm;
            }
        }
    }
    
    return fans;
}

/**
 * 打印所有温度信息
 */
void printAllTemperatures() {
    std::cout << "\n========== 硬件温度监控 ==========" << std::endl;
    
    // CPU温度
    auto cpuTemp = getCPUTemperature();
    if (cpuTemp.valid) {
        std::cout << "CPU温度: " << cpuTemp.celsius << "°C" << std::endl;
    } else {
        std::cout << "CPU温度: 无法读取" << std::endl;
    }
    
    // 内存温度
    auto memTemp = getMemoryTemperature();
    if (memTemp.valid) {
        std::cout << "内存温度: " << memTemp.celsius << "°C" << std::endl;
    } else {
        std::cout << "内存温度: 无法读取" << std::endl;
    }
    
    // 显卡温度
    auto gpuTemp = getGPUTemperature();
    if (gpuTemp.valid) {
        std::cout << "显卡温度: " << gpuTemp.celsius << "°C" << std::endl;
    } else {
        std::cout << "显卡温度: 无法读取" << std::endl;
    }
    
    std::cout << "==================================\n" << std::endl;
}

/**
 * 打印所有风扇信息
 */
void printAllFanSpeeds() {
    std::cout << "\n========== 风扇转速监控 ==========" << std::endl;
    
    auto fans = getFanSpeeds();
    if (fans.empty()) {
        std::cout << "无法读取风扇转速（可能需要root权限或IPMI支持）" << std::endl;
    } else {
        for (const auto& [name, rpm] : fans) {
            std::cout << name << ": " << rpm << " RPM" << std::endl;
        }
    }
    
    std::cout << "==================================\n" << std::endl;
}

/**
 * 根据温度自动调节风扇转速
 * 返回计算出的风扇转速
 */
int autoFanControl(bool verbose = true) {
    auto cpuTemp = getCPUTemperature();
    auto gpuTemp = getGPUTemperature();
    auto memTemp = getMemoryTemperature();
    
    if (!cpuTemp.valid) {
        if (verbose) std::cout << "无法读取CPU温度，跳过自动风扇控制" << std::endl;
        return -1;
    }
    
    int fanSpeed = g_config.fanSpeedMin;
    
    // 根据CPU温度调整风扇转速（使用配置的阈值）
    if (cpuTemp.celsius >= g_config.cpuTempHigh) {
        fanSpeed = g_config.fanSpeedMax;
    } else if (cpuTemp.celsius >= g_config.cpuTempMedium) {
        fanSpeed = g_config.fanSpeedHigh;
    } else if (cpuTemp.celsius >= g_config.cpuTempLow) {
        fanSpeed = g_config.fanSpeedMedium;
    } else if (cpuTemp.celsius >= g_config.cpuTempMin) {
        fanSpeed = g_config.fanSpeedLow;
    } else {
        fanSpeed = g_config.fanSpeedMin;
    }
    
    // 如果有GPU温度，也考虑进去
    if (gpuTemp.valid && gpuTemp.celsius >= g_config.gpuTempHigh) {
        fanSpeed = std::max(fanSpeed, g_config.fanSpeedHigh);
    }
    
    // 如果有内存温度，也考虑进去
    if (memTemp.valid && memTemp.celsius >= g_config.memTempHigh) {
        fanSpeed = std::max(fanSpeed, g_config.fanSpeedMedium);
    }
    
    if (verbose) {
        std::cout << "CPU: " << cpuTemp.celsius << "°C";
        if (gpuTemp.valid) {
            std::cout << ", GPU: " << gpuTemp.celsius << "°C";
        }
        if (memTemp.valid) {
            std::cout << ", MEM: " << memTemp.celsius << "°C";
        }
        std::cout << " -> 风扇: " << fanSpeed << "%" << std::endl;
    }
    
    if (g_config.enableIPMI) {
        setIPMIFanSpeed(fanSpeed);
    }
    return fanSpeed;
}

/**
 * 信号处理函数
 */
void signalHandler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        std::cout << "\n收到退出信号，正在停止监控..." << std::endl;
        running = false;
    }
}

/**
 * 实时监控模式
 * interval: 读取周期（秒）
 * strategy: 风扇控制策略 ("auto", "manual", "none")
 */
void realtimeMonitor(int interval, const std::string& strategy = "auto") {
    // 注册信号处理
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    
    std::cout << "\n========== 实时监控模式 ==========" << std::endl;
    std::cout << "读取周期: " << interval << " 秒" << std::endl;
    std::cout << "风扇策略: " << strategy << std::endl;
    std::cout << "按 Ctrl+C 退出\n" << std::endl;
    
    // 设置手动模式（如果需要控制风扇）
    if (strategy == "auto" || strategy == "manual") {
        setIPMIFanMode(FanMode::Manual);
    }
    
    int iteration = 0;
    auto startTime = std::chrono::steady_clock::now();
    
    while (running) {
        iteration++;
        auto currentTime = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            currentTime - startTime).count();
        
        // 打印时间戳
        auto now = std::chrono::system_clock::now();
        auto timeT = std::chrono::system_clock::to_time_t(now);
        char timeStr[20];
        strftime(timeStr, sizeof(timeStr), "%H:%M:%S", localtime(&timeT));
        
        std::cout << "[" << timeStr << "] ";
        
        // 读取温度
        auto cpuTemp = getCPUTemperature();
        auto gpuTemp = getGPUTemperature();
        auto memTemp = getMemoryTemperature();
        
        // 显示温度
        std::cout << "CPU: ";
        if (cpuTemp.valid) {
            std::cout << cpuTemp.celsius << "°C";
        } else {
            std::cout << "N/A";
        }
        
        std::cout << " | GPU: ";
        if (gpuTemp.valid) {
            std::cout << gpuTemp.celsius << "°C";
        } else {
            std::cout << "N/A";
        }
        
        std::cout << " | MEM: ";
        if (memTemp.valid) {
            std::cout << memTemp.celsius << "°C";
        } else {
            std::cout << "N/A";
        }
        
        // 根据策略控制风扇
        if (strategy == "auto") {
            int fanSpeed = autoFanControl(false);
            if (fanSpeed > 0) {
                std::cout << " | 风扇: " << fanSpeed << "%";
            }
        } else if (strategy == "manual") {
            // 手动模式只显示当前风扇转速
            auto fans = getFanSpeeds();
            if (!fans.empty()) {
                std::cout << " | 风扇: ";
                bool first = true;
                for (const auto& [name, rpm] : fans) {
                    if (!first) std::cout << ", ";
                    std::cout << rpm << "RPM";
                    first = false;
                }
            }
        }
        
        std::cout << std::endl;
        
        // 等待下一个周期
        for (int i = 0; i < interval && running; ++i) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
    
    std::cout << "\n监控已停止，共运行 " << iteration << " 个周期" << std::endl;
    
    // 恢复自动模式
    if (strategy == "auto" || strategy == "manual") {
        std::cout << "恢复风扇自动模式..." << std::endl;
        setIPMIFanMode(FanMode::Automatic);
    }
}

/**
 * 显示帮助信息
 */
void printHelp() {
    std::cout << "\n硬件温度监控和IPMI风扇控制程序\n" << std::endl;
    std::cout << "用法: hardware_monitor [选项]\n" << std::endl;
    std::cout << "选项:" << std::endl;
    std::cout << "  -h, --help              显示帮助信息" << std::endl;
    std::cout << "  -t, --temp              显示所有硬件温度" << std::endl;
    std::cout << "  -f, --fan               显示所有风扇转速" << std::endl;
    std::cout << "  -s <speed>              设置风扇转速 (0-100)" << std::endl;
    std::cout << "  -m <mode>               设置风扇模式 (auto/manual/full)" << std::endl;
    std::cout << "  -a, --auto              自动根据温度调节风扇" << std::endl;
    std::cout << "  -r, --realtime [间隔]   实时监控模式（默认5秒）" << std::endl;
    std::cout << "      --strategy <策略>   风扇控制策略 (auto/manual/none)" << std::endl;
    std::cout << "  --all                   显示所有信息\n" << std::endl;
    std::cout << "示例:" << std::endl;
    std::cout << "  hardware_monitor -t                  # 显示温度" << std::endl;
    std::cout << "  hardware_monitor -s 50               # 设置风扇50%" << std::endl;
    std::cout << "  hardware_monitor -m manual           # 设置手动模式" << std::endl;
    std::cout << "  hardware_monitor -a                  # 自动风扇控制" << std::endl;
    std::cout << "  hardware_monitor -r 3                # 每3秒监控一次" << std::endl;
    std::cout << "  hardware_monitor -r 5 --strategy auto  # 实时监控并自动控制风扇\n" << std::endl;
}

int main(int argc, char* argv[]) {
    // 尝试加载配置文件
    const char* configFileEnv = std::getenv("CONFIG_FILE");
    std::string configPath = configFileEnv ? configFileEnv : "/etc/hardware-monitor/hardware-monitor.conf";

    if (fs::exists(configPath)) {
        if (loadConfig(configPath)) {
            std::cerr << "已加载配置文件: " << configPath << std::endl;
        }
    }

    // 早期启动:立即设置风扇为手动模式并应用初始转速
    // 这确保在系统启动早期就控制风扇,避免过热
    if (g_config.enableIPMI) {
        std::cerr << "早期启动:设置风扇为手动模式..." << std::endl;
        setIPMIFanMode(FanMode::Manual);

        // 设置初始风扇转速(使用配置的最小转速或中等转速)
        int initialSpeed = g_config.fanSpeedMedium;  // 使用中等转速作为启动转速
        std::cerr << "早期启动:设置初始风扇转速为 " << initialSpeed << "%" << std::endl;
        setIPMIFanSpeed(initialSpeed);
    }

    if (argc == 1) {
        // 默认显示所有信息
        printAllTemperatures();
        printAllFanSpeeds();
        return 0;
    }
    
    std::string arg1 = argv[1];
    
    if (arg1 == "-h" || arg1 == "--help") {
        printHelp();
    }
    else if (arg1 == "--service") {
        // systemd服务模式
        std::cerr << "启动服务模式..." << std::endl;
        std::cerr << "配置: 间隔=" << g_config.interval << "秒, 策略=" << g_config.strategy << std::endl;
        realtimeMonitor(g_config.interval, g_config.strategy);
    }
    else if (arg1 == "-t" || arg1 == "--temp") {
        printAllTemperatures();
    }
    else if (arg1 == "-f" || arg1 == "--fan") {
        printAllFanSpeeds();
    }
    else if (arg1 == "-s") {
        if (argc < 3) {
            std::cerr << "错误：请指定风扇转速 (0-100)" << std::endl;
            return 1;
        }
        int speed = std::stoi(argv[2]);
        if (setIPMIFanSpeed(speed)) {
            std::cout << "风扇转速已设置为 " << speed << "%" << std::endl;
        }
    }
    else if (arg1 == "-m") {
        if (argc < 3) {
            std::cerr << "错误：请指定风扇模式 (auto/manual/full)" << std::endl;
            return 1;
        }
        std::string mode = argv[2];
        if (mode == "auto") {
            setIPMIFanMode(FanMode::Automatic);
            std::cout << "风扇模式已设置为自动" << std::endl;
        } else if (mode == "manual") {
            setIPMIFanMode(FanMode::Manual);
            std::cout << "风扇模式已设置为手动" << std::endl;
        } else if (mode == "full") {
            setIPMIFanMode(FanMode::FullSpeed);
            std::cout << "风扇模式已设置为全速" << std::endl;
        } else {
            std::cerr << "错误：无效的模式，请使用 auto/manual/full" << std::endl;
            return 1;
        }
    }
    else if (arg1 == "-a" || arg1 == "--auto") {
        autoFanControl(true);
    }
    else if (arg1 == "-r" || arg1 == "--realtime") {
        // 实时监控模式
        int interval = 5;  // 默认5秒
        std::string strategy = "auto";  // 默认自动控制
        
        // 解析参数
        for (int i = 2; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--strategy" && i + 1 < argc) {
                strategy = argv[++i];
                if (strategy != "auto" && strategy != "manual" && strategy != "none") {
                    std::cerr << "错误：无效的策略，请使用 auto/manual/none" << std::endl;
                    return 1;
                }
            } else if (arg.find("--") == std::string::npos) {
                // 数字参数，作为间隔
                try {
                    interval = std::stoi(arg);
                    if (interval < 1) {
                        std::cerr << "错误：间隔必须大于等于1秒" << std::endl;
                        return 1;
                    }
                } catch (...) {
                    std::cerr << "错误：无效的间隔值" << std::endl;
                    return 1;
                }
            }
        }
        
        realtimeMonitor(interval, strategy);
    }
    else if (arg1 == "--all") {
        printAllTemperatures();
        printAllFanSpeeds();
    }
    else {
        std::cerr << "错误：未知选项 '" << arg1 << "'" << std::endl;
        printHelp();
        return 1;
    }
    
    return 0;
}
