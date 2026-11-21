"""Run the actual generated SDK cleanup under deterministic allocation faults."""
import ctypes
import os
from pathlib import Path
import re
import subprocess
import tempfile
import _ctypes

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / 'firmware/p4/build-idf61/tp-components/esp_driver_spi/src/gpspi/spi_master.c'


def function(source, name):
    match = re.search(r'^static [^\n]*\b' + name + r'\(', source, re.M)
    assert match, name
    end = source.index('{', match.start()) + 1
    depth = 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[match.start():end]


STUB = r'''
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#define SPI_MASTER_ISR_ATTR
#define ESP_OK 0
#define SPI_TRANS_USE_RXDATA 1
#define SPI_TRANS_USE_TXDATA 2
#define SPI_TRANS_DMA_USE_PSRAM 4
#define SPI_TRANS_DMA_BUFFER_ALIGN_MANUAL 8
typedef int esp_err_t;
typedef struct {unsigned flags,length,rxlength;uint32_t tx_data[2],rx_data[2];void *tx_buffer,*rx_buffer;} spi_transaction_t;
typedef struct {spi_transaction_t *trans;const uint32_t *buffer_to_send;uint32_t *buffer_to_rcv;} spi_trans_priv_t;
typedef struct {bool dma_enabled;} spi_bus_attr_t;
typedef struct {const spi_bus_attr_t *bus_attr;int id;} spi_host_t;
static uint32_t scratch_tx[8],scratch_rx[8];
static int mode,direct_mask,calls,frees,bad_free,copies,bad_copy;
void free(void *ptr){if(!ptr)return;if(ptr!=scratch_tx&&ptr!=scratch_rx)++bad_free;else ++frees;}
void *memcpy(void *dst,const void *src,size_t bytes){++copies;if(!dst||!src)++bad_copy;return dst;}
static esp_err_t spicommon_dma_setup_priv_buffer(int id,uint32_t *ptr,uint32_t len,bool tx,bool psram,bool automatic,uint32_t **result){
 ++calls;if(!ptr||!len){*result=ptr;return ESP_OK;}
 int before=tx?1:2,after=tx?3:4;
 if(mode==before)return 100+mode;
 *result=(direct_mask&(tx?1:2))?ptr:(tx?scratch_tx:scratch_rx);
 if(mode==after)return 100+mode;
 return ESP_OK;
}
'''
WRAPPER = r'''
__declspec(dllexport) int check(int failure,int dma,int flags,int pointers,int direct){
 mode=failure;direct_mask=direct;calls=frees=bad_free=copies=bad_copy=0;
 uint32_t original_tx[8]={1},original_rx[8]={2};
 spi_transaction_t trans={.flags=flags,.length=64,.rxlength=64,.tx_buffer=pointers&1?original_tx:0,.rx_buffer=pointers&2?original_rx:0};
 spi_trans_priv_t priv={.trans=&trans};spi_bus_attr_t bus={.dma_enabled=dma};spi_host_t host={.bus_attr=&bus};
 uint32_t *tx=flags&SPI_TRANS_USE_TXDATA?trans.tx_data:trans.tx_buffer,*rx=flags&SPI_TRANS_USE_RXDATA?trans.rx_data:trans.rx_buffer;
 bool fail_tx=dma&&tx&&(failure==1||failure==3),fail_rx=dma&&rx&&(failure==2||failure==4)&&!fail_tx;
 int result=setup_priv_desc(&host,&priv);
 if(result!=(fail_tx||fail_rx?100+failure:0))return 1;
 if(bad_free||bad_copy)return 2;
 if(fail_tx||fail_rx){
  if(copies)return 3;
  int expected=0;
  if(tx&&!(direct&1)&&(!fail_tx||failure==3))++expected;
  if(rx&&!(direct&2)&&fail_rx&&failure==4)++expected;
  if(frees!=expected)return 4;
  if(priv.buffer_to_send!=tx||priv.buffer_to_rcv!=rx)return 5;
 }else{
  if(frees||copies)return 6;
  if(!dma&&calls)return 7;
  uninstall_priv_desc(&priv);
  if(bad_free||bad_copy)return 8;
 }
 return 0;
}
'''

source = BUILD.read_text(encoding='utf8')
production = function(source, 'uninstall_priv_desc') + '\n' + function(source, 'setup_priv_desc')
assert 'No transaction ran' in production, 'Build must use the project-local safety fix'
llvm = Path(os.environ.get('TP_LLVM_BIN', r'C:\Espressif\tools\esp-clang\16.0.1-fe4f10a809\esp-clang\bin'))
with tempfile.TemporaryDirectory(prefix='tp-spi-cleanup-') as directory:
    folder = Path(directory)
    (folder / 'test.c').write_text(STUB + production + WRAPPER, encoding='utf8')
    subprocess.run([str(llvm / 'clang.exe'), '--target=x86_64-pc-windows-msvc', '-ffreestanding', '-fno-builtin', '-fno-stack-protector', '-O2', '-c', str(folder / 'test.c'), '-o', str(folder / 'test.obj')], check=True)
    subprocess.run([str(llvm / 'lld.exe'), '-flavor', 'link', '/dll', '/noentry', '/nodefaultlib', '/out:' + str(folder / 'test.dll'), str(folder / 'test.obj')], check=True)
    dll = ctypes.CDLL(str(folder / 'test.dll'))
    dll.check.argtypes = [ctypes.c_int] * 5
    cases = 0
    try:
        for failure in range(5):
            for dma in range(2):
                for flags in (0, 1, 2, 3, 4, 8, 12):
                    for pointers in range(4):
                        for direct in range(4):
                            result = dll.check(failure, dma, flags, pointers, direct)
                            assert result == 0, (failure, dma, flags, pointers, direct, result)
                            cases += 1
    finally:
        _ctypes.FreeLibrary(dll._handle)
print(f'PASS actual project-local SDK SPI allocation cleanup: {cases} cases; TX/RX failures before/after allocation, no invalid copy/free, error retained, normal paths preserved')
