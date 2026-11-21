"""Exercise the actual source rectangle splitter with expanded short-loop tiles."""
import ctypes
import os
from pathlib import Path
import re
import subprocess
import tempfile
import _ctypes

root = Path(__file__).resolve().parents[1]
source = (root/'firmware/p4/main/teleprompter_ui.c').read_text(encoding='utf8')
start = source.index('typedef struct {int index,offset,destination,count;} visible_piece_t;')
function = source.index('static visible_piece_t visible_piece(', start)
end = source.index('{',function)+1
depth=1
while depth:
    depth+=(source[end]=='{')-(source[end]=='}');end+=1
production = source[start:end]
llvm = Path(os.environ.get('TP_LLVM_BIN', r'C:\Espressif\tools\esp-clang\16.0.1-fe4f10a809\esp-clang\bin'))
stub = r'''
#include <stdbool.h>
#define TILE_HEIGHT 256
static int s_height,s_playback_rows,s_loop_period,s_repeat_tile_span,s_text_height;
static struct {bool loop;unsigned loop_count;} s_previous;
'''
checks = r'''
__declspec(dllexport) int check(int height,int period,int y,int loop,int completed){
 s_height=height;s_playback_rows=1;s_loop_period=period;s_text_height=period;
 s_previous.loop=loop;s_previous.loop_count=completed;
 s_repeat_tile_span=loop&&period<TILE_HEIGHT?(TILE_HEIGHT/period)*period:0;
 int row=0,pieces=0;
 while(row<height){
  visible_piece_t p=visible_piece(y,row);
  if(p.count<=0||p.destination!=row||row+p.count>height)return 1;
  if(p.offset<0||p.offset+p.count>TILE_HEIGHT)return 2;
  for(int n=0;n<p.count;++n){
   int logical=row+n-y,expected=-1,actual=-1;
   if(loop&&(logical>=0||completed))expected=((logical%period)+period)%period;
   else if(logical>=0&&logical<period)expected=logical;
   if(p.index>=0){actual=p.index*TILE_HEIGHT+p.offset+n;if(s_repeat_tile_span)actual%=period;}
   if(actual!=expected)return 3;
  }
  row+=p.count;++pieces;
 }
 if(loop&&completed&&period<256&&pieces>height/s_repeat_tile_span+2)return 4;
 return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='tp-repeat-') as tmp:
    tmp=Path(tmp)
    (tmp/'test.c').write_text(stub+production+checks,encoding='utf8')
    subprocess.run([str(llvm/'clang.exe'),'--target=x86_64-pc-windows-msvc','-ffreestanding','-fno-stack-protector','-O2','-c',str(tmp/'test.c'),'-o',str(tmp/'test.obj')],check=True)
    subprocess.run([str(llvm/'lld.exe'),'-flavor','link','/dll','/noentry','/nodefaultlib','/out:'+str(tmp/'test.dll'),str(tmp/'test.obj')],check=True)
    dll=ctypes.CDLL(str(tmp/'test.dll'))
    count=0
    try:
        for height in (704,1184):
            for period in (1,23,43,127,255,256,295,500,1500):
                for y in range(-1600,1601,7):
                    for loop,completed in ((0,0),(1,0),(1,1)):
                        result=dll.check(height,period,y,loop,completed)
                        assert result==0,(height,period,y,loop,completed,result)
                        count+=1
    finally:
        _ctypes.FreeLibrary(dll._handle)
print(f'PASS production loop splitter: {count} full frames, first/repeated cycles, blank edges and bounded DMA rectangles')
