"""Run the firmware's real page-span builder on Windows, without a device."""
import ctypes
import os
from pathlib import Path
import subprocess
import tempfile
import _ctypes

root=Path(__file__).resolve().parents[1]
llvm=Path(os.environ.get('TP_LLVM_BIN',r'C:\Espressif\tools\esp-clang\16.0.1-fe4f10a809\esp-clang\bin'))
header=(root/'firmware/p4/main/page_layout.h').read_text(encoding='utf8')
header='\n'.join(line for line in header.splitlines() if not line.startswith(('#include','#pragma')))
source='typedef unsigned short uint16_t;typedef unsigned int uint32_t;\n'+header+'''
__declspec(dllexport) unsigned build(const char *text,const uint32_t *offsets,unsigned rows,unsigned capacity,unsigned paragraphs,tp_page_span_t *pages){return tp_page_build(text,offsets,rows,capacity,paragraphs,pages);}
'''
class Span(ctypes.Structure):
    _fields_=[('first',ctypes.c_uint16),('count',ctypes.c_uint16)]

with tempfile.TemporaryDirectory(prefix='tp-page-layout-') as directory:
    folder=Path(directory);(folder/'test.c').write_text(source,encoding='utf8')
    subprocess.run([str(llvm/'clang.exe'),'--target=x86_64-pc-windows-msvc','-ffreestanding','-fno-builtin','-fno-stack-protector','-O2','-c',str(folder/'test.c'),'-o',str(folder/'test.obj')],check=True)
    subprocess.run([str(llvm/'lld.exe'),'-flavor','link','/dll','/noentry','/nodefaultlib','/out:'+str(folder/'test.dll'),str(folder/'test.obj')],check=True)
    dll=ctypes.CDLL(str(folder/'test.dll'))
    dll.build.argtypes=[ctypes.c_char_p,ctypes.POINTER(ctypes.c_uint32),ctypes.c_uint,ctypes.c_uint,ctypes.c_uint,ctypes.POINTER(Span)]
    def check(rows,capacity,on,expected):
        data=''.join(rows).encode('utf8');offsets=[0]
        for row in rows:offsets.append(offsets[-1]+len(row.encode('utf8')))
        output=(Span*(len(rows)+2))();output[-1]=Span(65535,65535)
        count=dll.build(data,(ctypes.c_uint32*len(offsets))(*offsets),len(rows),capacity,on,output)
        result=[(output[i].first,output[i].count) for i in range(count)]
        assert result==expected,(rows,capacity,on,result,expected)
        assert (output[-1].first,output[-1].count)==(65535,65535)
    check(['第一行\n','第二行\n','\n','第二段\n'],5,1,[(0,2),(3,1)])
    check(['A\n','\n','\n',' \t\r\n','B\n'],5,1,[(0,1),(4,1)])
    check(['\r\n','A\r\n','\r\n','B\r\n','\r\n'],5,1,[(1,1),(3,1)])
    check(['A','B','C','D','E','\n','F'],2,1,[(0,2),(2,2),(4,1),(6,1)])
    check(['A\n','\n','B\n'],2,0,[(0,2),(2,1)])
    check(['\n','\t\r\n'],5,1,[(0,1)])
    check([],5,1,[(0,1)])
    check(['A\n']*8192,1,1,[(i,1) for i in range(8192)])
    check(['A\n','\n']*4096,8192,1,[(i*2,1) for i in range(4096)])
    _ctypes.FreeLibrary(dll._handle)
print('PASS production C pagination: paragraphs, repeated/leading/trailing/space-only blank rows, CRLF, long paragraphs, switch off, all blank, maximum rows and buffer bounds')
