"""Exercise UI timebase metadata, trigger ACK barriers and single rearm."""
import os
os.environ.setdefault('QT_QPA_PLATFORM', 'offscreen')
import runpy, time
from pathlib import Path
import numpy as np
from PyQt5.QtWidgets import QApplication
app=QApplication.instance() or QApplication([])
root=Path(__file__).resolve().parents[1]
n=runpy.run_path(str(root/'Display_Simulator/display_app.py'),run_name='trigger_test')
w=n['DisplayMainWindow']()
x=np.arange(500)
def frame(tb=2):
 return dict(mode='binary',timebase=tb,count=500,timestamp=0,ch1=np.full(500,.76),ch2=np.sin(x/10)+1.65,dig0=(x%50<25).astype(np.uint8),dig1=(x%50<25).astype(np.uint8),dig2=(x%50<25).astype(np.uint8))
w.on_binary_frame_received(frame())
w.on_timebase_changed(3)
assert w.canvas.current_tb_idx==2
w.on_ack_received('TB','3')
assert w.canvas.current_tb_idx==2 and w._pending_tb_idx==3
old=frame(2);w.on_binary_frame_received(old)
assert old['timebase']==2 and w.canvas.current_tb_idx==2
w.on_binary_frame_received(frame(3))
assert w.canvas.current_tb_idx==3 and w._pending_tb_idx is None
print('PASS: old frame remains on old scale until matching DATA arrives')
w.worker.running=True
for source,code in [('CH1',0),('CH2',1),('DIG0',3),('DIG1',4),('DIG2',2)]:
 w.cb_trig_src.setCurrentText(source)
 w.on_trigger_config_changed()
 assert w._pending_trigger.startswith(f'{code},')
 ack=w._pending_trigger
 w.on_ack_received('ARM_SINGLE','')
 assert w._pending_trigger == ack, 'stale ARM ACK must not commit a newer configuration'
 previous=w.last_captured_frame
 w.on_binary_frame_received(frame(3))
 assert w.last_captured_frame is previous
 w.on_ack_received('TRIG',ack)
 assert w._pending_trigger is None
print('PASS: five hardware sources, atomic commands and ACK barriers')
w.cb_trig_mode.setCurrentText('SINGLE');w.on_ack_received('TRIG',w._pending_trigger)
w.on_binary_frame_received(frame(3));assert w.canvas.freeze_display
captured=w.last_captured_frame
w.on_binary_frame_received(frame(0));assert w.last_captured_frame is captured
w.on_arm_single();assert w._pending_trigger=='ARM'
w.on_ack_received('ARM_SINGLE','');assert not w.canvas.freeze_display
w.on_binary_frame_received(frame(3));assert w.canvas.freeze_display
print('PASS: single capture freezes, ARM accepts exactly the next capture')
w.cb_trig_mode.setCurrentText('AUTO');w.on_ack_received('TRIG',w._pending_trigger)
assert not w.canvas.freeze_display
w.worker.running=False
w.on_text_line_received('$STATE,TB:3,MODE:1,GEN:1000,RUN:1,TRIG:DIG0(Sound),FALLING,NORMAL,LVL:128,TRGD:0')
assert w.cb_trig_src.currentText()=='DIG0' and w.canvas.trig_mode=='NORMAL' and w.canvas.trig_edge=='FALLING'
print('PASS: STATE synchronizes trigger source, edge, mode and level')
w.close()
