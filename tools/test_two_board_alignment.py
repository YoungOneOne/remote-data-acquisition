"""Build the real acquisition packer, then decode its bytes with the real display parser."""
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
subprocess.run([sys.executable, 'tools/test_acquisition_refactor.py'], cwd=root, check=True)
subprocess.run(['cmd', '/c', 'tools\\run_new_display_protocol_test.bat',
                'tmp\\acquisition-refactor-test\\after\\frames.bin'], cwd=root, check=True)
subprocess.run(['cmd', '/c', 'tools\\run_new_display_ui_test.bat'], cwd=root, check=True)
print('PASS: two-board packet compatibility, control states and mixed display and three-tab UI')
