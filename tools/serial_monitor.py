"""
QRSA 采集端智能串口监视与分析工具 (Serial Monitor)
支持：
  1. daemon 模式：持续监听并把数据实时写入 serial.log，并输出在控制台，支持断线自动重连。
  2. read 模式：按需单次捕获 N 秒串口输出并返回（适用于 AI 快速测试）。
  3. tail 模式：快速查看最新的串口日志（配合 daemon 模式，AI 绝不占用/争抢串口）。
  4. send 模式：向串口发送调试指令。
"""

import sys
import os
import time
import argparse
from datetime import datetime

# 自动处理 Windows 控制台编码
if sys.platform == "win32":
    try:
        sys.stdout.reconfigure(encoding="utf-8")
        sys.stderr.reconfigure(encoding="utf-8")
    except Exception:
        pass

try:
    import serial
    import serial.tools.list_ports
except ImportError:
    print("[ERROR] pyserial 未安装，请执行: pip install pyserial")
    sys.exit(1)

DEFAULT_BAUD = 115200
DEFAULT_PORT = "COM5"
LOG_DIR = os.path.dirname(os.path.abspath(__file__))
LOG_FILE = os.path.join(LOG_DIR, "serial.log")


def find_stlink_port():
    """自动查找 ST-Link 或常用串口端口"""
    ports = list(serial.tools.list_ports.comports())
    for p in ports:
        desc = (p.description or "").lower()
        if "stlink" in desc or "st-link" in desc:
            return p.device
    for p in ports:
        desc = (p.description or "").lower()
        if any(chip in desc for chip in ["ch340", "cp210", "ftdi", "usb-serial"]):
            return p.device
    if ports:
        return ports[0].device
    return DEFAULT_PORT


def get_target_port(user_port=None):
    if user_port:
        return user_port
    return find_stlink_port()


def log_to_file(text, file_path=LOG_FILE):
    with open(file_path, "a", encoding="utf-8", errors="replace") as f:
        f.write(text)


def cmd_tail(lines_count=30):
    """读取最新的 N 行串口日志"""
    if not os.path.exists(LOG_FILE):
        print(f"[INFO] 暂无日志文件: {LOG_FILE}")
        return
    with open(LOG_FILE, "r", encoding="utf-8", errors="replace") as f:
        all_lines = f.readlines()
    recent = all_lines[-lines_count:]
    print("".join(recent), end="")


def cmd_read(port, baud, duration=3.0):
    """单次读取 N 秒数据并直接输出到终端"""
    print(f"[MONITOR] 正在连接 {port} @ {baud} bps, 持续监听 {duration} 秒...")
    try:
        ser = serial.Serial(port, baud, timeout=0.1)
    except Exception as e:
        print(f"[ERROR] 无法打开串口 {port}: {e}")
        return

    start_time = time.time()
    buffer = bytearray()
    try:
        while time.time() - start_time < duration:
            data = ser.read(ser.in_waiting or 1)
            if data:
                buffer.extend(data)
                decoded = data.decode("utf-8", errors="replace")
                sys.stdout.write(decoded)
                sys.stdout.flush()
    finally:
        ser.close()
        print(f"\n[MONITOR] 监听完成，共收到 {len(buffer)} 字节。")


def cmd_send(port, baud, message):
    """向串口发送文本并返回回显"""
    print(f"[MONITOR] 向 {port} 发送: {repr(message)}")
    try:
        with serial.Serial(port, baud, timeout=1.0) as ser:
            if not message.endswith("\r\n") and not message.endswith("\n"):
                message += "\r\n"
            ser.write(message.encode("utf-8"))
            time.sleep(0.1)
            resp = ser.read_all().decode("utf-8", errors="replace")
            print(f"[REPLY] 收到响应: {resp}")
    except Exception as e:
        print(f"[ERROR] 发送失败: {e}")


def cmd_daemon(port, baud, clear_log=False):
    """常驻后台服务模式：监听串口，实时写入日志与控制台，自动断线重连"""
    if clear_log and os.path.exists(LOG_FILE):
        try:
            os.remove(LOG_FILE)
        except Exception:
            pass

    print("=" * 60)
    print(f" [QRSA Serial Monitor Daemon]")
    print(f" 目标端口 : {port}")
    print(f" 波 特 率 : {baud}")
    print(f" 日志路径 : {LOG_FILE}")
    print(" 提示: 按 Ctrl+C 可退出监听")
    print("=" * 60)

    start_msg = f"\n[{datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] === 串口监控服务已启动 ({port} @ {baud}) ===\n"
    sys.stdout.write(start_msg)
    log_to_file(start_msg)

    reconnect_interval = 2.0
    while True:
        try:
            with serial.Serial(port, baud, timeout=0.2) as ser:
                conn_msg = f"[{datetime.now().strftime('%H:%M:%S')}] [INFO] 成功打开串口 {port}，正在实时监听...\n"
                sys.stdout.write(conn_msg)
                log_to_file(conn_msg)

                line_buffer = ""
                while True:
                    raw_data = ser.read(ser.in_waiting or 1)
                    if raw_data:
                        text = raw_data.decode("utf-8", errors="replace")
                        line_buffer += text
                        
                        # 按行加时间戳存入文件和输出
                        while "\n" in line_buffer:
                            line, line_buffer = line_buffer.split("\n", 1)
                            clean_line = line.rstrip("\r")
                            now_str = datetime.now().strftime("%H:%M:%S.%f")[:-3]
                            formatted_line = f"[{now_str}] {clean_line}\n"
                            sys.stdout.write(formatted_line)
                            sys.stdout.flush()
                            log_to_file(formatted_line)
                    else:
                        time.sleep(0.01)

        except serial.SerialException as se:
            err_msg = f"[{datetime.now().strftime('%H:%M:%S')}] [WARN] 串口断开或不可用 ({se})，{reconnect_interval}秒后重试...\n"
            sys.stdout.write(err_msg)
            log_to_file(err_msg)
            time.sleep(reconnect_interval)
        except KeyboardInterrupt:
            stop_msg = f"\n[{datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] === 用户终止监控 ===\n"
            sys.stdout.write(stop_msg)
            log_to_file(stop_msg)
            break
        except Exception as e:
            err_msg = f"[{datetime.now().strftime('%H:%M:%S')}] [ERROR] 发生未知异常: {e}\n"
            sys.stdout.write(err_msg)
            log_to_file(err_msg)
            time.sleep(reconnect_interval)


def main():
    parser = argparse.ArgumentParser(description="QRSA 串口调试与监视助手")
    subparsers = parser.add_subparsers(dest="command", help="子命令")

    # daemon
    p_daemon = subparsers.add_parser("daemon", help="启动后台持续监听并记录日志")
    p_daemon.add_argument("--port", type=str, default=None, help="串口号 (如 COM5)")
    p_daemon.add_argument("--baud", type=int, default=DEFAULT_BAUD, help="波特率 (默认 115200)")
    p_daemon.add_argument("--clear", action="store_true", help="启动前清空旧日志")

    # read
    p_read = subparsers.add_parser("read", help="即时单次捕获 N 秒串口数据")
    p_read.add_argument("--port", type=str, default=None, help="串口号")
    p_read.add_argument("--baud", type=int, default=DEFAULT_BAUD, help="波特率")
    p_read.add_argument("--duration", type=float, default=3.0, help="监听秒数")

    # tail
    p_tail = subparsers.add_parser("tail", help="查看最新的串口日志")
    p_tail.add_argument("-n", "--lines", type=int, default=30, help="行数")

    # send
    p_send = subparsers.add_parser("send", help="发送串口指令")
    p_send.add_argument("message", type=str, help="发送的文本")
    p_send.add_argument("--port", type=str, default=None, help="串口号")
    p_send.add_argument("--baud", type=int, default=DEFAULT_BAUD, help="波特率")

    # list
    subparsers.add_parser("list", help="列出系统所有串口端口")

    args = parser.parse_args()

    if args.command == "list" or not args.command:
        ports = list(serial.tools.list_ports.comports())
        print("可用串口列表:")
        for p in ports:
            print(f"  - {p.device}: {p.description}")
        return

    if args.command == "tail":
        cmd_tail(args.lines)
        return

    port = get_target_port(getattr(args, "port", None))

    if args.command == "read":
        cmd_read(port, args.baud, args.duration)
    elif args.command == "send":
        cmd_send(port, args.baud, args.message)
    elif args.command == "daemon":
        cmd_daemon(port, args.baud, args.clear)


if __name__ == "__main__":
    main()
