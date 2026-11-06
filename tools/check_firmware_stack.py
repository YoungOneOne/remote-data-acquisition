"""Check the actual Keil link report for the 500-point stack regression."""
import re
from pathlib import Path
root = Path(__file__).resolve().parents[1]
startup = (root/'Acquisition/MDK-ARM/startup_stm32f746xx.s').read_text()
html = (root/'Acquisition/MDK-ARM/Acquisition/Acquisition.htm').read_text(errors='replace')
linkmap = (root/'Acquisition/MDK-ARM/Acquisition/Acquisition.map').read_text(errors='replace')
reserved = int(re.search(r'Stack_Size\s+EQU\s+(0x[0-9a-fA-F]+)', startup)[1], 16)
main = int(re.search(r'>main</STRONG>[^\n]+Stack size (\d+) bytes', html)[1])
known = int(re.search(r'Maximum Stack Usage\s*=\s*(\d+) bytes', html)[1])
assert reserved >= 4096, reserved
assert main < 256, f'main frame {main} bytes; keep waveform buffer static'
assert known < reserved, (known, reserved)
assert re.search(r'frame_points\s+0x[0-9a-f]+\s+Data\s+1500\s+main\.o\(\.bss\)', linkmap)
print(f'PASS: frame_points=1500 bytes in .bss, main={main}, known call chain={known}, reserved MSP={reserved}')
print('Indirect calls and interrupt nesting still require hardware validation.')
