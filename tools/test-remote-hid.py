"""Execute the production bounded HID parser and click discriminator on Windows."""
import ctypes,os,subprocess,tempfile,_ctypes
from pathlib import Path
root=Path(__file__).resolve().parents[1]
llvm=Path(os.environ.get('TP_LLVM_BIN',r'C:\Espressif\tools\esp-clang\16.0.1-fe4f10a809\esp-clang\bin'))
header=(root/'firmware/p4/main/remote_hid.h').read_text(encoding='utf8')
header='\n'.join(line for line in header.splitlines() if not line.startswith(('#include','#pragma')))
source='void *memset(void *p,int v,unsigned long long n){volatile unsigned char *b=p;while(n--)*b++=(unsigned char)v;return p;}\n typedef _Bool bool; typedef unsigned char uint8_t;typedef unsigned short uint16_t;typedef unsigned int uint32_t;typedef unsigned long long uint64_t;typedef unsigned long long size_t;\n#define true 1\n#define false 0\n'+header+''' 
__declspec(dllexport) int checks(void){
 tp_hid_map m;
 unsigned char descriptor[]={0x05,0x0c,0x09,0x01,0xa1,0x01,0x85,0x02,0x19,0x00,0x2a,0xff,0x03,0x15,0x00,0x26,0xff,0x03,0x75,0x10,0x95,0x01,0x81,0x00,0xc0};
 unsigned char down[]={0xea,0},up[]={0,0},other[]={0xcd,0};
 if(!tp_hid_parse(&m,descriptor,sizeof descriptor)||m.count!=1||m.fields[0].id!=2)return 1;
 if(!tp_hid_report(&m,2,down,2)||tp_hid_report(&m,2,up,2)||tp_hid_report(&m,2,other,2)||tp_hid_report(&m,1,down,2)||tp_hid_report(&m,2,down,1))return 2;
 unsigned char variable[]={0x05,0x0c,0x85,0x03,0x19,0xe9,0x29,0xea,0x75,0x01,0x95,0x02,0x81,0x02};
 unsigned char bit[]={2};if(!tp_hid_parse(&m,variable,sizeof variable)||!tp_hid_report(&m,3,bit,1))return 3;
 unsigned char bad[]={0x05};if(tp_hid_parse(&m,bad,1))return 4;
 unsigned char huge[]={0x05,0x0c,0x75,0x20,0x95,0xff,0x81,0};if(tp_hid_parse(&m,huge,sizeof huge))return 5;
 tp_remote_clicks c={0};if(tp_remote_input(&c,1,1000000,true)||tp_remote_input(&c,1,1100000,true)||tp_remote_input(&c,0,1120000,true)||tp_remote_tick(&c,1249999)||tp_remote_tick(&c,1250001)!=1||tp_remote_tick(&c,1400000))return 6;
 c=(tp_remote_clicks){0};if(tp_remote_input(&c,1,2000000,true)||tp_remote_input(&c,0,2020000,true)||tp_remote_input(&c,1,2200000,true)!=2||tp_remote_input(&c,0,2210000,true)||tp_remote_tick(&c,2600000))return 7;
 c=(tp_remote_clicks){0};tp_remote_input(&c,1,3000000,true);tp_remote_input(&c,0,3001000,true);if(tp_remote_input(&c,1,3010000,true))return 8;tp_remote_input(&c,0,3011000,true);if(tp_remote_tick(&c,3400000)!=1)return 9;
 c=(tp_remote_clicks){0};if(tp_remote_input(&c,1,4000000,false)!=1||tp_remote_input(&c,1,4010000,false)||tp_remote_input(&c,0,4020000,false)||tp_remote_tick(&c,4400000))return 10;
 if(tp_remote_input(&c,1,4500000,false)!=1)return 11;
 c=(tp_remote_clicks){0};tp_remote_input(&c,1,5000000,true);tp_remote_input(&c,0,5020000,true);if(tp_remote_input(&c,1,5250000,true)!=2||tp_remote_tick(&c,5500001))return 12;
 c=(tp_remote_clicks){0};tp_remote_input(&c,1,6000000,true);tp_remote_input(&c,0,6020000,true);if(tp_remote_tick(&c,6250000)||tp_remote_tick(&c,6250001)!=1)return 13;
 return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='tp-remote-') as tmp:
 p=Path(tmp);(p/'test.c').write_text(source,encoding='utf8')
 subprocess.run([str(llvm/'clang.exe'),'--target=x86_64-pc-windows-msvc','-ffreestanding','-fno-builtin','-fno-stack-protector','-O2','-c',str(p/'test.c'),'-o',str(p/'test.obj')],check=True)
 subprocess.run([str(llvm/'lld.exe'),'-flavor','link','/dll','/noentry','/nodefaultlib','/out:'+str(p/'test.dll'),str(p/'test.obj')],check=True)
 dll=ctypes.CDLL(str(p/'test.dll'));result=dll.checks();_ctypes.FreeLibrary(dll._handle);assert result==0,result
print('PASS production HID arrays/variable bits, report IDs/releases, unsupported and truncated bounds; single/double/duplicate/held timing')
