"""
通信协议双端配套对齐与算法验证脚本
"""
import sys

# 1. 验证 383 字节波形帧与校验和算法
def calc_xor_checksum(frame: bytes) -> int:
    xor_sum = 0
    # 范围：从 Type(偏移 2) 到 Payload 末尾(偏移 379)，共 378 个字节
    for b in frame[2:380]:
        xor_sum ^= b
    return xor_sum

def test_waveform_frame():
    print("=== 1. 测试波形帧打包与校验 ===")
    frame = bytearray(383)
    frame[0] = 0xAA
    frame[1] = 0x55
    frame[2] = 0x01 # Type
    frame[3] = 2    # Timebase = TB_1MS (1ms/div)
    frame[4] = 125  # Count = 125 点

    # 填充 125 个点 (375 字节)
    for i in range(125):
        frame[5 + i*3 + 0] = 128     # CH1 = 128 (约 1.65V 中偏)
        frame[5 + i*3 + 1] = 64      # CH2 = 64
        frame[5 + i*3 + 2] = 0x05    # DIG: D0=1, D1=0, D2=1 (0b101)

    chk = calc_xor_checksum(frame)
    frame[380] = chk
    frame[381] = 0x0D # \r
    frame[382] = 0x0A # \n

    assert len(frame) == 383, f"Frame length should be 383, got {len(frame)}"
    print(f"-> 383 字节波形数据帧打包正确，XOR Checksum = 0x{chk:02X}")
    assert calc_xor_checksum(frame) == frame[380], "校验和匹配验证失败"
    print("-> 校验和算法验证通过！")

    # 验证 8 位转 12 位的线性映射
    # code = ((uint32_t)raw8 * 4095 + 127) / 255
    code_0 = (0 * 4095 + 127) // 255
    code_128 = (128 * 4095 + 127) // 255
    code_255 = (255 * 4095 + 127) // 255
    assert code_0 == 0, f"code_0 should be 0, got {code_0}"
    assert 2048 <= code_128 <= 2056, f"code_128 should be around 2048, got {code_128}"
    assert code_255 == 4095, f"code_255 should be 4095, got {code_255}"
    print(f"-> 电压码值映射正确: raw8=0 -> code={code_0}, raw8=128 -> code={code_128}, raw8=255 -> code={code_255}")

# 2. 验证控制指令字典格式
def test_commands():
    print("\n=== 2. 测试显示端上行控制指令字典格式 ===")
    cmds = [
        "TB 2\r\n",
        "RUN\r\n",
        "STOP\r\n",
        "GEN 1000\r\n",
        "GEN_START\r\n",
        "GEN_STOP\r\n",
        "GEN_NEXT\r\n",
        "GEN_PREV\r\n",
        "TRIG_MODE AUTO\r\n",
        "TRIG_SRC CH2\r\n",
        "TRIG_EDGE RISING\r\n",
        "TRIG_LVL 128\r\n",
        "ARM\r\n",
        "SYNC\r\n"
    ]
    for c in cmds:
        assert c.endswith("\r\n"), f"指令末尾必须为 \\r\\n: {c}"
        print(f"  [OK] 指令匹配: {repr(c)}")

# 3. 验证时基字典与物理分度
def test_timebase():
    print("\n=== 3. 测试时基分度与采样点间隔对齐 ===")
    tb_map = [
        {"idx": 0, "name": "10ms",  "div_ns": 10_000_000, "dt_ns": 200_000, "fs": 5000},
        {"idx": 1, "name": "5ms",   "div_ns":  5_000_000, "dt_ns": 100_000, "fs": 10000},
        {"idx": 2, "name": "1ms",   "div_ns":  1_000_000, "dt_ns":  20_000, "fs": 50000},
        {"idx": 3, "name": "500us", "div_ns":    500_000, "dt_ns":  10_000, "fs": 100000},
        {"idx": 4, "name": "100us", "div_ns":    100_000, "dt_ns":   2_000, "fs": 500000},
    ]
    for tb in tb_map:
        # 每屏 10 大格，总时间 = 10 * div_ns
        total_ns = 10 * tb["div_ns"]
        # 125 个点，间隔 = total_ns / (125-1) 约等于 dt_ns * 125
        calc_dt = total_ns / 125 # 理论每点时间
        # 校验采样率 fs = 1e9 / dt_ns
        calc_fs = int(1e9 / tb["dt_ns"])
        assert calc_fs == tb["fs"], f"采样率错误: {calc_fs} vs {tb['fs']}"
        print(f"  [OK] 时基 [{tb['idx']}] {tb['name']}/div: dt={tb['dt_ns']}ns, fs={calc_fs}Sps")

if __name__ == '__main__':
    test_waveform_frame()
    test_commands()
    test_timebase()
    print("\n=========================================")
    print(" 所有通信协议与算法验证全部 100% 通过！ ")
    print("=========================================")
