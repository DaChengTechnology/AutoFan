#!/bin/bash
# Hardware Monitor 卸载脚本

set -e

echo "========================================="
echo "Hardware Monitor 卸载脚本"
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

# 停止服务
echo ""
echo "[1/4] 停止服务..."
if systemctl is-active --quiet hardware-monitor; then
    systemctl stop hardware-monitor
    echo "✓ 服务已停止"
else
    echo "! 服务未运行"
fi

# 禁用服务
echo ""
echo "[2/4] 禁用服务..."
if systemctl is-enabled --quiet hardware-monitor 2>/dev/null; then
    systemctl disable hardware-monitor
    echo "✓ 已禁用开机自启"
else
    echo "! 服务未启用"
fi

# 删除文件
echo ""
echo "[3/4] 删除文件..."
rm -f "$BIN_DIR/hardware_monitor"
rm -f "$SERVICE_DIR/hardware-monitor.service"
systemctl daemon-reload
echo "✓ 程序和服务文件已删除"

# 询问是否删除配置和日志
echo ""
echo "[4/4] 清理配置和日志..."
read -p "是否删除配置文件和日志？(y/N): " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    rm -rf "$CONFIG_DIR"
    rm -rf "$LOG_DIR"
    echo "✓ 配置和日志已删除"
else
    echo "! 保留配置和日志"
    echo "  配置: $CONFIG_DIR"
    echo "  日志: $LOG_DIR"
fi

echo ""
echo "========================================="
echo "卸载完成！"
echo "========================================="
