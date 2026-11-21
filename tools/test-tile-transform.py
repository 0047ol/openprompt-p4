"""Compare the production partial tile transform with a scalar pixel oracle."""
import ctypes
import os
from pathlib import Path
import random
import re
import subprocess
import tempfile
import _ctypes

root = Path(__file__).resolve().parents[1]
source = (root / 'firmware/p4/main/teleprompter_ui.c').read_text(encoding='utf8')
match = re.search(r'^static void tile_transform\(', source, re.M)
end = source.index('{', match.start()) + 1
depth = 1
while depth:
    depth += (source[end] == '{') - (source[end] == '}')
    end += 1
production = source[match.start():end]
stub = r'''
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#define TILE_HEIGHT 256
typedef struct { uint16_t *pixels,*dim_pixels; } script_tile_t;
typedef struct { struct { int stride; } header; uint8_t *data; } canvas_t;
static canvas_t canvas,*s_canvas_buffer=&canvas;
static int s_width,s_rotation,s_mirror;
static bool s_center_highlight;
static void *memcpy(void *d,const void *s,size_t n){
 uint8_t *dst=d;const uint8_t *src=s;while(n--)*dst++=*src++;return d;
}
'''
wrapper = r'''
__declspec(dllexport) void transform(int rotation,int mirror,int highlight,int width,
 int first,int rows,uint16_t *src,int stride,uint16_t *dst,uint16_t *dim){
 s_rotation=rotation;s_mirror=mirror;s_center_highlight=highlight;s_width=width;
 canvas.header.stride=stride*2;canvas.data=(uint8_t*)src;
 script_tile_t tile={dst,dim};tile_transform(&tile,first,first+rows);
}
'''
llvm = Path(os.environ.get('TP_LLVM_BIN', r'C:\Espressif\tools\esp-clang\16.0.1-fe4f10a809\esp-clang\bin'))
with tempfile.TemporaryDirectory(prefix='tp-tile-transform-') as directory:
    folder = Path(directory)
    (folder / 'test.c').write_text(stub + production + wrapper, encoding='utf8')
    subprocess.run([str(llvm / 'clang.exe'), '--target=x86_64-pc-windows-msvc', '-ffreestanding', '-fno-builtin', '-fno-stack-protector', '-O2', '-c', str(folder / 'test.c'), '-o', str(folder / 'test.obj')], check=True)
    subprocess.run([str(llvm / 'lld.exe'), '-flavor', 'link', '/dll', '/noentry', '/nodefaultlib', '/out:' + str(folder / 'test.dll'), str(folder / 'test.obj')], check=True)
    dll = ctypes.CDLL(str(folder / 'test.dll'))
    pixel_ptr = ctypes.POINTER(ctypes.c_uint16)
    dll.transform.argtypes = [ctypes.c_int] * 6 + [pixel_ptr, ctypes.c_int, pixel_ptr, pixel_ptr]
    rng = random.Random(261121)
    cases = 0
    try:
        for rotation in (0, 90, 180, 270):
            for mirror in range(4):
                for highlight in (False, True):
                    for width, first, rows in ((17, 0, 32), (63, 32, 64), (800, 224, 32), (1264, 0, 256), (31, 128, 17)):
                        stride = (width + 31) // 32 * 32
                        src = (ctypes.c_uint16 * (stride * 256))(*(rng.randrange(65536) for _ in range(stride * 256)))
                        sentinel = 0xa55a
                        dst = (ctypes.c_uint16 * (width * 256))(*([sentinel] * (width * 256)))
                        dim = (ctypes.c_uint16 * (width * 256))(*([sentinel] * (width * 256)))
                        expected = [sentinel] * (width * 256)
                        for y in range(first, first + rows):
                            for x in range(width):
                                mx = width - 1 - x if mirror & 1 else x
                                my = 255 - y if mirror & 2 else y
                                target = (width - 1 - mx) * 256 + my if rotation == 90 else mx * 256 + 255 - my if rotation == 270 else (255 - my) * width + width - 1 - mx if rotation == 180 else my * width + mx
                                expected[target] = src[y * stride + x]
                        dll.transform(rotation, mirror, highlight, width, first, rows, src, stride, dst, dim)
                        assert list(dst) == expected, (rotation, mirror, highlight, width, first, rows)
                        if highlight:
                            # Track touched addresses separately: a source pixel can equal the sentinel.
                            expected_dim = [sentinel] * len(expected)
                            for y in range(first, first + rows):
                                for x in range(width):
                                    mx = width - 1 - x if mirror & 1 else x
                                    my = 255 - y if mirror & 2 else y
                                    target = (width - 1 - mx) * 256 + my if rotation == 90 else mx * 256 + 255 - my if rotation == 270 else (255 - my) * width + width - 1 - mx if rotation == 180 else my * width + mx
                                    expected_dim[target] = (src[y * stride + x] >> 1) & 0x7bef
                            assert list(dim) == expected_dim
                        else:
                            assert all(pixel == sentinel for pixel in dim)
                        cases += 1
    finally:
        _ctypes.FreeLibrary(dll._handle)
print(f'PASS production tile transform: {cases} cases, all rotations/mirrors, partial/full/padded rows, bright/dim pixels and untouched guards')
