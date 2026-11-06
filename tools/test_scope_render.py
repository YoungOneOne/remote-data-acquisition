"""Raster regression: CH2 must not fill over the grid or the earlier CH1 trace."""
import os
os.environ.setdefault('QT_QPA_PLATFORM', 'offscreen')
import runpy
from pathlib import Path
import numpy as np
from PyQt5.QtWidgets import QApplication
root = Path(__file__).resolve().parents[1]
app = QApplication.instance() or QApplication([])
ns = runpy.run_path(str(root/'Display_Simulator/display_app.py'), run_name='render_test')
canvas = ns['ScopeCanvas']()
x = np.arange(500)
frame = dict(mode='binary', timebase=3, count=500, timestamp=0,
             ch1=np.full(500, .76), ch2=np.where(x % 100 < 50, 3.3, 0.),
             dig0=np.zeros(500, dtype=np.uint8), dig1=np.ones(500, dtype=np.uint8),
             dig2=(x % 100 < 50).astype(np.uint8))
canvas.set_frame(frame)
def raster():
    image = canvas.grab().toImage().convertToFormat(4)  # QImage.Format_RGB32
    buf = image.bits(); buf.setsize(image.byteCount())
    return np.frombuffer(buf, np.uint8).reshape(image.height(), image.width(), 4).copy()
for width, height in [(1000, 650), (1400, 850), (1900, 1100)]:
    canvas.resize(width, height)
    canvas.show_ch2 = False
    baseline = raster()
    canvas.show_ch2 = True
    combined = raster()
    analog_h = int((height - 36 - 28) * .64)
    ch1_y = int(36 + analog_h - .76 / 3.3 * analog_h)
    def yellow(a):
        row = a[ch1_y-2:ch1_y+3, 66:width-38]
        return ((row[:, :, 2] > 200) & (row[:, :, 1] > 160) & (row[:, :, 0] < 100)).any(axis=0).sum()
    assert yellow(combined) > yellow(baseline) * .95, (width, 'CH1 obscured')
    grid_y = int(36 + analog_h / 8)
    def grid(a):
        row = a[grid_y-1:grid_y+2, 66:width-38]
        return ((row[:, :, 0] > 40) & (row[:, :, 0] < 150) &
                (row[:, :, 1] > 30) & (row[:, :, 1] < 140) &
                (row[:, :, 2] > 25) & (row[:, :, 2] < 120)).any(axis=0).sum()
    assert grid(combined) > grid(baseline) * .90, (width, 'grid obscured')
    print(f'PASS {width}x{height}: CH1 and grid remain visible beneath CH2')
canvas.resize(1400, 850)
(root/'tmp').mkdir(exist_ok=True)
assert canvas.grab().save(str(root/'tmp/grid-waveform-fixed.png'))
