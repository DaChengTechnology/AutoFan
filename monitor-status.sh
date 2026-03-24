#!/bin/bash
# Hardware Monitor 状态监控脚本

SERVICE_NAME="hardware-monitor"

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo "========================================="
echo "Hardware Monitor 状态监控"
echo "========================================="

# 检查服务状态
echo ""
echo -e "${BLUE}服务状态:${NC}"
if systemctl is-active --quiet $SERVICE_NAME; then
    echo -e "  状态: ${GREEN}运行中${NC}"
    echo -e "  PID:  $(systemctl show --property MainPID $SERVICE_NAME | cut -d= -f2)"
    echo -e "  运行时间: $(systemctl show --property ActiveState $SERVICE_NAME | cut -d= -f2)"
else
    echo -e "  状态: ${RED}未运行${NC}"
fi

# 检查开机自启
echo ""
echo -e "${BLUE}开机自启:${NC}"
if systemctl is-enabled --quiet $SERVICE_NAME 2>/dev/null; then
    echo -e "  状态: ${GREEN}已启用${NC}"
else
    echo -e "  状态: ${YELLOW}未启用${NC}"
fi

# 显示配置
echo ""
echo -e "${BLUE}当前配置:${NC}"
if [ -f "/etc/hardware-monitor/hardware-monitor.conf" ]; then
    grep -v "^#" /etc/hardware-monitor/hardware-monitor.conf | grep -v "^$" | while read line; do
        echo "  $line"
    done
else
    echo -e "  ${YELLOW}配置文件不存在${NC}"
fi

# 显示最近日志
echo ""
echo -e "${BLUE}最近日志 (最后10行):${NC}"
journalctl -u $SERVICE_NAME -n 10 --no-pager 2>/dev/null || echo -e "  ${YELLOW}无日志记录${NC}"

# 显示实时温度
echo ""
echo -e "${BLUE}当前硬件温度:${NC}"
if command -v hardware_monitor &> /dev/null; then
    hardware_monitor -t 2>/dev/null || echo -e "  ${YELLOW}无法读取温度${NC}"
else
    echo -e "  ${YELLOW}hardware_monitor 未安装${NC}"
fi

echo ""
echo "========================================="
echo "管理命令:"
echo "  启动:   systemctl start $SERVICE_NAME"
echo "  停止:   systemctl stop $SERVICE_NAME"
echo "  重启:   systemctl restart $SERVICE_NAME"
echo "  日志:   journalctl -u $SERVICE_NAME -f"
echo "========================================="
