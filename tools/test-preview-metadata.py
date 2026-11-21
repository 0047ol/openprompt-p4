"""Verify bounded metadata changes preserve actual long preview JSON byte-for-byte."""
import ctypes
import json
import os
from pathlib import Path
import subprocess
import tempfile
import _ctypes

ROOT = Path(__file__).resolve().parents[1]
header = (ROOT / 'firmware/p4/main/preview_metadata.h').read_text(encoding='utf8')
header = '\n'.join(line for line in header.splitlines() if not line.startswith(('#include', '#pragma')))
STUB = r'''
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdarg.h>
size_t strlen(const char *p){const char *s=p;while(*p)++p;return p-s;}
void *memcpy(void *d,const void *s,size_t n){char *a=d;const char *b=s;while(n--)*a++=*b++;return d;}
char *strcpy(char *d,const char *s){char *a=d;while((*a++=*s++));return d;}
const char *strstr(const char *s,const char *n){for(;*s;++s){size_t i=0;while(n[i]&&s[i]==n[i])++i;if(!n[i])return s;}return 0;}
int snprintf(char *out,size_t cap,const char *format,...){
 va_list args;va_start(args,format);unsigned long value;bool negative=false;
 if(format[1]=='l')value=va_arg(args,unsigned long);
 else{int signed_value=va_arg(args,int);negative=signed_value<0;value=negative?(unsigned long)(-(int64_t)signed_value):(unsigned long)signed_value;}
 va_end(args);char rev[24];unsigned n=0;do{rev[n++]='0'+value%10;value/=10;}while(value);
 unsigned written=0;if(negative&&written+1<cap)out[written++]='-';while(n&&written+1<cap)out[written++]=rev[--n];out[written]=0;return written;
}
'''
WRAPPER = r'''
__declspec(dllexport) int update(const char *src,char *dst,size_t cap,uint32_t revision,int mirror,int top,int end){return tp_preview_metadata_update(src,dst,cap,revision,mirror,top,end);}
'''
llvm = Path(os.environ.get('TP_LLVM_BIN', r'C:\Espressif\tools\esp-clang\16.0.1-fe4f10a809\esp-clang\bin'))
with tempfile.TemporaryDirectory(prefix='tp-preview-metadata-') as directory:
    folder = Path(directory)
    (folder / 'test.c').write_text(STUB + header + WRAPPER, encoding='utf8')
    subprocess.run([str(llvm / 'clang.exe'), '--target=x86_64-pc-windows-msvc', '-ffreestanding', '-fno-builtin', '-fno-stack-protector', '-O2', '-c', str(folder / 'test.c'), '-o', str(folder / 'test.obj')], check=True)
    subprocess.run([str(llvm / 'lld.exe'), '-flavor', 'link', '/dll', '/noentry', '/nodefaultlib', '/out:' + str(folder / 'test.dll'), str(folder / 'test.obj')], check=True)
    dll = ctypes.CDLL(str(folder / 'test.dll'))
    dll.update.argtypes = [ctypes.c_char_p, ctypes.c_void_p, ctypes.c_size_t, ctypes.c_uint32, ctypes.c_int, ctypes.c_int, ctypes.c_int]
    cases = 0
    try:
        for rows in (0, 300, 8192):
            source = {'ok': True, 'revision': 9, 'scriptRevision': 3, 'textTop': 7, 'mirror': 0, 'text': '\u63d0\u8bcd\u5668 "revision":42 "mirror":99\n' * 100, 'lines': [[i, 20, 240] for i in range(rows)], 'endScroll': 9999, 'advances': [[65, 22, 1]]}
            raw = json.dumps(source, ensure_ascii=False, separators=(',', ':')).encode()
            for revision in (0, 10, 99, 100, 4294967295):
                for mirror in range(4):
                    for top, end in ((-999, 0), (24, 1234567), (2147483647, -2147483648)):
                        out = ctypes.create_string_buffer(len(raw) + 64)
                        assert dll.update(raw, out, len(out), revision, mirror, top, end)
                        expected = {**source, 'revision': revision, 'mirror': mirror, 'textTop': top, 'endScroll': end}
                        assert out.value == json.dumps(expected, ensure_ascii=False, separators=(',', ':')).encode()
                        assert not dll.update(raw, out, len(raw), revision, mirror, top, end)
                        cases += 1
        for raw in (b'{}', b'{"revision":"bad"}', b'{"revision":1.2}'):
            out = ctypes.create_string_buffer(256)
            assert not dll.update(raw, out, len(out), 2, 0, 0, 0)
    finally:
        _ctypes.FreeLibrary(dll._handle)
print(f'PASS production preview metadata: {cases} long/empty layouts, all mirror modes, revision/position boundaries; all other bytes unchanged; invalid/short destinations rejected')
