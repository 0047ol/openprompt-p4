"""Save an on-demand screenshot of P4's completed panel frame (firmware .17+).

Example: py tools/capture-device-screen.py --url http://DEVICE_IP --output screen.png
PNG conversion requires Pillow; BMP output uses only the Python standard library.
Screenshots can contain private manuscript text: review before sharing publicly.
"""
import argparse
import io
from pathlib import Path
import struct
import urllib.parse
import urllib.request


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--url', required=True, help='Current P4 HTTP address; no saved device address is used')
    parser.add_argument('--output', type=Path, required=True, help='Destination .png or .bmp (must not exist)')
    parser.add_argument('--scale',type=int,choices=(1,2,4),default=2,help='Downsample factor; 1 preserves every panel pixel, 2 is faster')
    args = parser.parse_args()
    address = urllib.parse.urlsplit(args.url)
    if address.scheme not in ('http', 'https') or not address.netloc:
        parser.error('--url must be an HTTP(S) device address')
    if args.output.suffix.lower() not in ('.bmp', '.png'):
        parser.error('--output must end in .bmp or .png')
    if args.output.exists():
        parser.error('Output already exists; choose a new filename')
    image_module = None
    if args.output.suffix.lower() == '.png':
        try:
            from PIL import Image
            image_module = Image
        except ImportError:
            parser.error('PNG needs Pillow (py -m pip install Pillow); .bmp needs no extra package')
    url = args.url.rstrip('/') + '/api/display/capture.bmp?scale=' + str(args.scale)
    http = urllib.request.build_opener(urllib.request.ProxyHandler({}))
    with http.open(urllib.request.Request(url, headers={'Cache-Control': 'no-cache'}), timeout=30) as response:
        if response.headers.get_content_type() != 'image/bmp':
            raise SystemExit('Device did not return a screenshot')
        data = response.read(2_048_067)
        metadata = response.headers.get('X-P4-Capture', '')
    if len(data) != 66+2_048_000//args.scale**2 or data[:2] != b'BM':
        raise SystemExit('Incomplete or unexpected screenshot; no output saved')
    width, height = struct.unpack_from('<ii', data, 18)
    if (width, height) not in ((1280//args.scale, -800//args.scale), (800//args.scale, -1280//args.scale)):
        raise SystemExit('Unexpected panel dimensions; no output saved')
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open('xb') as output:
        if image_module:
            with image_module.open(io.BytesIO(data)) as image:
                image.save(output, format='PNG')
        else:
            output.write(data)
    print(f'Saved {width}x{-height}: {args.output}')
    print(metadata)
    print('Completed panel frame; backlight, optical flicker and motion smoothness require separate observation.')


if __name__ == '__main__':
    main()
