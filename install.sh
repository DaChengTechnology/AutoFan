#!/bin/bash
# Hardware Monitor 安装脚本

set -e

echo "========================================="
echo "Hardware Monitor 安装脚本"
echo "========================================="

# 检查root权限
if [ "$EUID" -ne 0 ]; then
    echo "错误：请使用root权限运行此脚本"
    echo "使用: sudo $0"
    exit 1
fi

# 安装目录
BIN_DIR="/usr/local/bin"
CONFIG_DIR="/etc/hardware-monitor"
LOG_DIR="/var/log/hardware-monitor"
SERVICE_DIR="/etc/systemd/system"

# 编译程序
echo ""
echo "[1/6] 编译程序..."
if [ -f "hardware_monitor.cpp" ]; then
    g++ -o hardware_monitor hardware_monitor.cpp -std=c++17 -O2
    echo "✓ 编译成功"
else
    echo "✗ 找不到源文件 hardware_monitor.cpp"
    exit 1
fi

# 安装二进制文件
echo ""
echo "[2/6] 安装二进制文件..."
install -m 755 hardware_monitor "$BIN_DIR/hardware_monitor"
echo "✓ 已安装到 $BIN_DIR/hardware_monitor"

# 创建配置目录
echo ""
echo "[3/6] 创建配置目录..."
mkdir -p "$CONFIG_DIR"
if [ -f "hardware-monitor.conf" ]; then
    install -m 644 hardware-monitor.conf "$CONFIG_DIR/hardware-monitor.conf"
    echo "✓ 配置文件已安装到 $CONFIG_DIR/hardware-monitor.conf"
else
    echo "! 配置文件不存在，使用默认配置"
fi

# 创建日志目录
echo ""
echo "[4/6] 创建日志目录..."
mkdir -p "$LOG_DIR"
chmod 755 "$LOG_DIR"
echo "✓ 日志目录: $LOG_DIR"

# 安装systemd服务
echo ""
echo "[5/6] 安装systemd服务..."
if [ -f "hardware-monitor.service" ]; then
    install -m 644 hardware-monitor.service "$SERVICE_DIR/hardware-monitor.service"
    systemctl daemon-reload
    echo "✓ systemd服务已安装"
else
    echo "✗ 找不到服务文件 hardware-monitor.service"
    exit 1
fi

# 设置权限
echo ""
echo "[6/6] 设置权限..."
# 确保程序可以访问IPMI
if command -v ipmitool &> /dev/null; then
    echo "✓ ipmitool 已安装"
else
    echo "! 警告: ipmitool 未安装，IPMI功能可能不可用"
    echo "  安装: apt-get install ipmitool 或 yum install ipmitool"
fi

echo ""
echo "========================================="
echo "安装完成！"
echo "========================================="
echo ""
echo "使用方法:"
echo "  启动服务:   systemctl start hardware-monitor"
echo "  停止服务:   systemctl stop hardware-monitor"
echo "  重启服务:   systemctl restart hardware-monitor"
echo "  查看状态:   systemctl status hardware-monitor"
echo "  查看日志:   journalctl -u hardware-monitor -f"
echo "  开机自启:   systemctl enable hardware-monitor"
echo "  禁用自启:   systemctl disable hardware-monitor"
echo ""
echo "配置文件: $CONFIG_DIR/hardware-monitor.conf"
echo "日志目录: $LOG_DIR"
echo ""
