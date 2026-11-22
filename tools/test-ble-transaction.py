"""Run the production BLE transaction gate on the host without ESP-IDF mocks."""
import ctypes
import os
from pathlib import Path
import subprocess
import tempfile
import _ctypes

root = Path(__file__).resolve().parents[1]
llvm = Path(os.environ.get('TP_LLVM_BIN', r'C:\Espressif\tools\esp-clang\16.0.1-fe4f10a809\esp-clang\bin'))
header = (root/'firmware/p4/main/ble_transaction.h').read_text(encoding='utf8')
header = '\n'.join(line for line in header.splitlines() if not line.startswith(('#include', '#pragma')))
source = '''
typedef _Bool bool; typedef unsigned char uint8_t; typedef unsigned short uint16_t;
typedef long long int64_t; typedef unsigned long long size_t;
#define true 1
#define false 0
#define INT64_C(x) x##LL
void *memset(void *p,int v,size_t n){volatile uint8_t *b=p;while(n--)*b++=(uint8_t)v;return p;}
void *memcpy(void *p,const void *q,size_t n){uint8_t *b=p;const uint8_t *a=q;while(n--)*b++=*a++;return p;}
void *memmove(void *p,const void *q,size_t n){uint8_t *b=p;const uint8_t *a=q;if(b>a){while(n)b[n-1]=a[n-1],n--;}else while(n--)*b++=*a++;return p;}
int memcmp(const void *p,const void *q,size_t n){const uint8_t *a=p,*b=q;while(n--){if(*a!=*b)return *a-*b;a++;b++;}return 0;}
int strcmp(const char *a,const char *b){while(*a&&*a==*b)a++,b++;return (uint8_t)*a-(uint8_t)*b;}
''' + header + '''
__declspec(dllexport) int checks(void){
 tp_ble_transaction_t t={0};
 uint8_t a[19]={0x24,1,2,3,4,5,6,0,1,2,3,4,5,6,7,8,9,0xaa,0xff},b[19];memcpy(b,a,19);b[1]=9;
 if(tp_ble_transaction_claim(&t,a,0,100)||tp_ble_transaction_claim(&t,a,18,100)||tp_ble_transaction_claim(&t,a,20,100))return 1;
 if(!tp_ble_transaction_claim(&t,a,19,100)||strcmp(t.client,"00010203040506070809aaff"))return 2;
 if(tp_ble_transaction_claim(&t,b,19,200)||t.until!=8000100)return 3;
 b[1]=1;b[7]=99;if(tp_ble_transaction_claim(&t,b,19,200))return 4;
 if(!tp_ble_transaction_claim(&t,a,19,200)||t.until!=8000200)return 5;
 uint8_t frame[12]={0x26,1,2,3,4,5,6,0x21,0,0,'A','B'};uint16_t n=12;
 if(!tp_ble_transaction_unwrap(&t,frame,&n,300)||n!=5||memcmp(frame,"\\x21\\0\\0AB",5))return 6;
 uint8_t bad[9]={0x26,9,2,3,4,5,6,0x22};n=8;
 if(tp_ble_transaction_unwrap(&t,bad,&n,400)||n!=8)return 7;
 n=7;if(tp_ble_transaction_unwrap(&t,bad,&n,400))return 8;
 uint8_t release[7]={0x25,9,2,3,4,5,6};
 if(tp_ble_transaction_release(&t,release,7)||!tp_ble_transaction_active(&t,500))return 9;
 if(tp_ble_transaction_active(&t,8000300))return 10;
 memcpy(b,a,19);b[1]=9;if(!tp_ble_transaction_claim(&t,b,19,8000300))return 11;
 release[1]=1;if(tp_ble_transaction_release(&t,release,7))return 12;
 release[1]=9;if(tp_ble_transaction_release(&t,release,6)||!tp_ble_transaction_release(&t,release,7)||t.until)return 13;
 memset(a+1,0,6);if(tp_ble_transaction_claim(&t,a,19,9000000))return 14;
 return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='tp-ble-transaction-') as temporary:
    work = Path(temporary)
    (work/'test.c').write_text(source, encoding='utf8')
    subprocess.run([str(llvm/'clang.exe'), '--target=x86_64-pc-windows-msvc', '-ffreestanding', '-fno-builtin', '-fno-stack-protector', '-O2', '-c', str(work/'test.c'), '-o', str(work/'test.obj')], check=True)
    subprocess.run([str(llvm/'lld.exe'), '-flavor', 'link', '/dll', '/noentry', '/nodefaultlib', '/out:'+str(work/'test.dll'), str(work/'test.obj')], check=True)
    dll = ctypes.CDLL(str(work/'test.dll'))
    result = dll.checks()
    _ctypes.FreeLibrary(dll._handle)
    assert result == 0, result
print('PASS production BLE transaction claim, owner/client isolation, fragmented payload bounds, expiry recovery and stale release rejection')
