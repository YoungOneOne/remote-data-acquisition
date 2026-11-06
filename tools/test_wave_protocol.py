"""Exercise the real desktop serial decoder with fragmented/new/bad frames."""
import runpy,struct,sys
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT / 'Display_Simulator'))
ns=runpy.run_path(str(ROOT / 'Display_Simulator/display_app.py'),run_name='decoder_test')
import serial
cls=ns['SerialWorker'] if 'SerialWorker' in ns else next(v for k,v in ns.items() if isinstance(v,type) and hasattr(v,'send_cmd') and hasattr(v,'open_port'))
def frame(tb=2,seq=0):
 b=bytearray(b'\xaa\x55\x02'+bytes([tb])+struct.pack('<HI',500,seq))
 b.extend(bytes([100,150,5])*500)
 chk=0
 for v in b[2:]: chk^=v
 return bytes(b+bytes([chk])+b'\r\n')
def check(chunks,expected):
 w=cls();w.running=True; received=[]; errors=[]
 w.sig_binary_frame.connect(received.append)
 w.sig_status.connect(lambda text,ok: errors.append(text) if not ok else None)
 class Port:
  is_open=True
  @property
  def in_waiting(self):return len(chunks[0]) if chunks else 0
  def read(self,n):
   if not chunks: w.running=False;return b''
   data=chunks[0][:n];chunks[0]=chunks[0][n:]
   if not chunks[0]:chunks.pop(0)
   return data
  def write(self,b):pass
  def flush(self):pass
  def close(self):self.is_open=False
 old=serial.Serial;serial.Serial=lambda **kw:Port()
 try:w.run()
 finally:serial.Serial=old
 assert len(received)==expected,(len(received),expected,errors)
 for f in received:
  assert f['count']==500 and len(f['ch1'])==500
  assert all(f['dig0']==1) and all(f['dig1']==0) and all(f['dig2']==1)
for tb in range(5):
 b=frame(tb);check([b[i:i+37] for i in range(0,len(b),37)],1)
check([frame(0)+frame(1)+frame(2)],3)
bad=bytearray(frame());bad[60]^=1
check([bytes(bad),frame()],1)
bad=bytearray(frame());bad[4:6]=struct.pack('<H',501)
check([bytes(bad),frame()],1)
print('PASS: 5 timebases, fragmentation, concatenation, checksum rejection, invalid count recovery')

import re
acq=(ROOT/'Acquisition/Src/bsp_acquisition.c').read_text(encoding='utf-8-sig')
display=(ROOT/'Display/quiz/quiz/App/app_proto.c').read_text(encoding='utf-8-sig')
arr=[int(v) for v in re.search(r's_arr_table\[TB_COUNT\] = \{([^}]+)',acq)[1].split(',')]
dt=[int(v.rstrip('u')) for v in re.search(r's_tb_dt_ns\[5\] = \{([^}]+)',display)[1].split(',')]
assert [(v+1)*1000 for v in arr] == dt
for div,t in zip([10000000,5000000,1000000,500000,100000],dt):
 assert div == 50*t
 assert 499*t*48//div == 479
print('PASS: hardware/display timing agree, 50 samples/div, final sample at pixel 479')
