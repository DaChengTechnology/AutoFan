# Hardware Monitor - 硬件温度监控和风扇控制系统

一个专业的Linux硬件监控和IPMI风扇控制程序，支持systemd服务管理。

## 功能特性

- ✅ **多硬件支持**: CPU、GPU（NVIDIA/AMD/摩尔线程）、内存温度监控
- ✅ **智能风扇控制**: 基于温度的自动风扇转速调节
- ✅ **IPMI集成**: 通过IPMI控制服务器风扇
- ✅ **systemd服务**: 支持systemd管理，开机自启
- ✅ **配置文件**: 灵活的配置文件支持
- ✅ **实时监控**: 可配置的监控周期
- ✅ **日志记录**: systemd journal日志集成

## 快速开始

### 1. 安装

```bash
# 编译并安装
sudo ./install.sh
```

### 2. 启动服务

```bash
# 启动服务
sudo systemctl start hardware-monitor

# 查看状态
sudo systemctl status hardware-monitor

# 查看日志
sudo journalctl -u hardware-monitor -f
```

### 3. 配置

编辑配置文件 `/etc/hardware-monitor/hardware-monitor.conf`:

```bash
# 监控间隔（秒）
INTERVAL=5

# 风扇控制策略 (auto/manual/none)
STRATEGY=auto

# CPU温度阈值
CPU_TEMP_HIGH=80
CPU_TEMP_MEDIUM=70
CPU_TEMP_LOW=60
CPU_TEMP_MIN=50

# 风扇转速配置（百分比）
FAN_SPEED_MAX=100
FAN_SPEED_HIGH=80
FAN_SPEED_MEDIUM=60
FAN_SPEED_LOW=45
FAN_SPEED_MIN=30
```

## 使用方法

### 命令行使用

```bash
# 显示温度
hardware_monitor -t

# 显示风扇转速
hardware_monitor -f

# 设置风扇转速为50%
hardware_monitor -s 50

# 设置风扇模式
hardware_monitor -m auto     # 自动模式
hardware_monitor -m manual   # 手动模式
hardware_monitor -m full     # 全速模式

# 实时监控（每3秒）
hardware_monitor -r 3

# 实时监控并自动控制风扇
hardware_monitor -r 5 --strategy auto

# 服务模式（systemd使用）
hardware_monitor --service
```

### systemd管理

```bash
# 启动服务
systemctl start hardware-monitor

# 停止服务
systemctl stop hardware-monitor

# 重启服务
systemctl restart hardware-monitor

# 查看状态
systemctl status hardware-monitor

# 查看日志
journalctl -u hardware-monitor -f

# 开机自启
systemctl enable hardware-monitor

# 禁用自启
systemctl disable hardware-monitor
```

### 状态监控

```bash
# 使用状态监控脚本
./monitor-status.sh
```

## 配置说明

### 温度阈值

| 配置项 | 默认值 | 说明 |
|--------|--------|------|
| CPU_TEMP_HIGH | 80 | CPU高温阈值（°C） |
| CPU_TEMP_MEDIUM | 70 | CPU中温阈值（°C） |
| CPU_TEMP_LOW | 60 | CPU低温阈值（°C） |
| CPU_TEMP_MIN | 50 | CPU最低阈值（°C） |
| GPU_TEMP_HIGH | 70 | GPU高温阈值（°C） |
| MEM_TEMP_HIGH | 60 | 内存高温阈值（°C） |

### 风扇转速

| 配置项 | 默认值 | 说明 |
|--------|--------|------|
| FAN_SPEED_MAX | 100 | 最大转速（%） |
| FAN_SPEED_HIGH | 80 | 高转速（%） |
| FAN_SPEED_MEDIUM | 60 | 中转速（%） |
| FAN_SPEED_LOW | 45 | 低转速（%） |
| FAN_SPEED_MIN | 30 | 最小转速（%） |

### 风扇控制策略

- **auto**: 根据温度自动调节风扇转速
- **manual**: 只监控温度和风扇转速，不自动控制
- **none**: 只显示温度信息

## 系统要求

- Linux系统（支持sysfs）
- GCC 7+（支持C++17）
- ipmitool（IPMI功能）
- systemd（服务管理）

## 支持的硬件

### CPU
- AMD (k10temp, k8temp)
- Intel (coretemp)

### GPU
- NVIDIA (nvidia-smi)
- AMD (amdgpu, radeon)
- 摩尔线程 (mthreads)

### 其他
- IPMI传感器（内存、风扇等）

### 已测试硬件
超云 R2216 A12 机架式服务器
运行在 Ubuntu 24.04 搭配 nvdia Tesla P40、MTT S80

欢迎添加支持设备列表

## 文件结构

```
/usr/local/bin/hardware_monitor          # 主程序
/etc/hardware-monitor/hardware-monitor.conf  # 配置文件
/etc/systemd/system/hardware-monitor.service # systemd服务
/var/log/hardware-monitor/               # 日志目录
```

## 卸载

```bash
sudo ./uninstall.sh
```

## 故障排查

### 1. 无法读取温度

- 检查是否有权限访问 `/sys/class/hwmon`
- 确认内核支持相应的硬件传感器

### 2. IPMI不工作

- 安装ipmitool: `apt-get install ipmitool`
- 加载IPMI内核模块: `modprobe ipmi_devintf`
- 检查IPMI权限

### 3. 服务无法启动

- 查看日志: `journalctl -u hardware-monitor -xe`
- 检查配置文件语法
- 确认程序有执行权限

## 许可证

MIT License

## 作者

Hardware Monitor Project
