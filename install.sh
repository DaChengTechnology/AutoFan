#!/bin/bash
# Hardware Monitor 安装脚本

set -e

echo "========================================="
echo "Hardware Monitor 安装脚本"
echo "========================================="

# 检查是否为root用户
if [ "$EUID" -ne 0 ]; then
    echo "请使用root权限运行此脚本"
    echo "使用: sudo bash install.sh"
    exit 1
fi

# 1. 编译程序
echo ""
echo "[1/5] 编译 hardware_monitor..."
if [ -f "hardware_monitor.cpp" ]; then
    g++ -o hardware_monitor hardware_monitor.cpp -std=c++17
    echo "✓ 编译成功"
else
    echo "✗ 错误:找不到 hardware_monitor.cpp"
    exit 1
fi

# 2. 安装可执行文件
echo ""
echo "[2/5] 安装可执行文件到 /usr/local/bin/..."
cp hardware_monitor /usr/local/bin/
chmod +x /usr/local/bin/hardware_monitor
echo "✓ 安装成功"

# 3. 创建配置目录和文件
echo ""
echo "[3/5] 创建配置文件..."
mkdir -p /etc/hardware-monitor
if [ -f "hardware-monitor.conf" ]; then
    cp hardware-monitor.conf /etc/hardware-monitor/
    echo "✓ 配置文件已安装到 /etc/hardware-monitor/hardware-monitor.conf"
else
    echo "! 警告:找不到配置文件模板,使用默认配置"
fi

# 4. 安装systemd服务
echo ""
echo "[4/5] 安装 systemd 服务..."
if [ -f "hardware-monitor.service" ]; then
    cp hardware-monitor.service /etc/systemd/system/
    systemctl daemon-reload
    echo "✓ systemd 服务已安装"
else
    echo "✗ 错误:找不到 hardware-monitor.service"
    exit 1
fi

# 5. 启用并启动服务
echo ""
echo "[5/5] 启用并启动服务..."
echo ""
echo "是否现在启动服务? (y/n)"
read -r answer
if [ "$answer" = "y" ] || [ "$answer" = "Y" ]; then
    systemctl enable hardware-monitor.service
    systemctl start hardware-monitor.service
    echo ""
    echo "✓ 服务已启动"
    echo ""
    echo "查看服务状态:"
    systemctl status hardware-monitor.service --no-pager
else
    echo ""
    echo "服务未启动。你可以稍后使用以下命令启动:"
    echo "  sudo systemctl enable hardware-monitor.service"
    echo "  sudo systemctl start hardware-monitor.service"
fi

echo ""
echo "========================================="
echo "安装完成!"
echo "========================================="
echo ""
echo "使用说明:"
echo "  查看状态: systemctl status hardware-monitor"
echo "  查看日志: journalctl -u hardware-monitor -f"
echo "  停止服务: systemctl stop hardware-monitor"
echo "  重启服务: systemctl restart hardware-monitor"
echo ""
echo "手动使用:"
echo "  查看温度: hardware_monitor -t"
echo "  查看风扇: hardware_monitor -f"
echo "  设置转速: hardware_monitor -s 50"
echo "  实时监控: hardware_monitor -r 5"
echo ""
