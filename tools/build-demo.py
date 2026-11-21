"""Build a local visual demo from the shared controller and portal sources."""
from pathlib import Path
import json
import subprocess
import sys
import shutil
import argparse
import hashlib

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--port', type=int, default=4173)
args = parser.parse_args()

root = Path(__file__).resolve().parents[1]
web = root / "web"
demo = web / "demo"
site = demo / "site"
sys.path.insert(0, str(root / '.tools' / 'demo-python'))
try:
    import qrcode
    import qrcode.image.svg
except ImportError:
    raise SystemExit('Install Demo dependencies: py -m pip install --target .tools/demo-python -r web/demo/requirements.txt')
subprocess.run([sys.executable, str(root / "tools" / "sync-web-pages.py")], check=True)
site.mkdir(parents=True, exist_ok=True)
qr_directory = site / 'portal'
qr_directory.mkdir(exist_ok=True)
targets = {
    'hotspot': f'http://127.0.0.1:{args.port}/console.html',
    'wifi': f'http://127.0.0.1:{args.port}/console.html',
    'bluetooth': 'https://0047ol.github.io/openprompt-p4/',
}
for mode, target in targets.items():
    qr = qrcode.make(target, image_factory=qrcode.image.svg.SvgPathImage, border=4)
    qr.save(qr_directory / f'qr-{mode}.svg')
screen_bars = json.loads((root / 'shared/ui/screen-bars.json').read_text(encoding='utf-8'))
fixture_data = json.loads((demo / "fixtures.json").read_text(encoding="utf-8"))
fixture_data['settings'] = {**screen_bars, **fixture_data['settings']}
fixture = json.dumps(fixture_data, ensure_ascii=False)
mock_version = hashlib.sha256((demo / 'mock-api.js').read_bytes()).hexdigest()[:12]
mock_tag = f'<script>window.TP_DEMO_FIXTURE={fixture};window.tpDemoFont=new FontFace("DeviceReader","url(reader.woff2)");window.tpDemoFont.load().then(face=>document.fonts.add(face));</script><script src="/mock-api.js?v={mock_version}"></script>'

for source, target in [("index.html", "console.html")]:
    page = (web / source).read_text(encoding="utf-8")
    page = page.replace('<meta charset="utf-8">', '<meta charset="utf-8">' + mock_tag, 1)
    page = page.replace('async function loadReaderTextFont(text){', 'async function loadReaderTextFont(text){await window.tpDemoFont.load();return;')
    page = page.replace('</body>', '<script src="/review-nav.js"></script></body>')
    (site / target).write_text(page, encoding="utf-8")
for filename in ('bluetooth.html', 'hosted.html'):
    stale_ble_page = site / filename
    if stale_ble_page.exists():
        stale_ble_page.unlink()

# Legacy bookmark redirects directly to the control page; no choice screen.
(site / "portal.html").write_text('<!doctype html><meta charset="utf-8"><meta http-equiv="refresh" content="0;url=/console.html"><script>location.replace("/console.html")</script>', encoding="utf-8")
shutil.copy2(demo / "mock-api.js", site / "mock-api.js")
shutil.copy2(demo / 'review-nav.js', site / 'review-nav.js')
shutil.copy2(web / "reader.woff2", site / "reader.woff2")

# The hardware gallery uses the same copy and reading renderer as the firmware
# guide / controller preview. Do not hand-edit the generated Demo files.
guide = json.loads((root / 'shared/ui/connection-guide.json').read_text(encoding='utf-8'))
device = (demo / 'device.html').read_text(encoding='utf-8')
device = device.replace('/* GUIDE_COPY */', json.dumps(guide, ensure_ascii=False))
device = device.replace('/* MOTION_SETTINGS */', (root / 'shared/ui/motion.json').read_text(encoding='utf-8'))
device = device.replace('/* SYSTEM_SETTINGS */', (root / 'shared/ui/system-screen.json').read_text(encoding='utf-8'))
(site / 'device.html').write_text(device, encoding='utf-8')
shutil.copy2(demo / 'device.js', site / 'device.js')
device_qrs = site / 'device-qr'
device_qrs.mkdir(exist_ok=True)
for name, target in {
    'join': 'WIFI:T:nopass;S:提词器P4-4455;P:;;',
    'join-en': 'WIFI:T:nopass;S:Teleprompter-P4-4455;P:;;',
    'lan': 'http://192.0.2.10/',
    'bluetooth': targets['bluetooth'],
    'hotspot': 'http://192.168.4.1/',
    'local': 'http://teleprompter-demo.local/',
}.items():
    qrcode.make(target, image_factory=qrcode.image.svg.SvgPathImage, border=4).save(device_qrs / f'{name}.svg')

def source_section(text, start, end):
    """Fail the build when a shared renderer is renamed instead of silently drifting."""
    return text[text.index(start):text.index(end, text.index(start))]

preview_source = (web / 'preview.js').read_text(encoding='utf-8')
bars_source = (web / 'screen-bars.js').read_text(encoding='utf-8').replace('/* SCREEN_BAR_DEFAULTS */',json.dumps(screen_bars))
reader = source_section(preview_source, 'function readerText(', 'function drawDevicePreview(')
bars = bars_source[bars_source.index('function screenBarVisible('):]
bar_options = source_section(bars_source, 'const screenBarDefaults=', 'var screenTelemetry=')
visibility = source_section(bars_source, 'function playbackBars(', 'function renderPlaybackBarVisibility(')
document_match = source_section(preview_source, 'function previewMatchesDocument(', 'function fitDevicePreview(')
frame = source_section(preview_source, '  const state=lastState,layout=previewLayout,', '\nfunction previewLifecycle(')
frame = frame.replace("previewCanvas.hidden=false;$('previewLoading').hidden=true;", '')
# The gallery supplies its own scroll clock and fixes the screen dimensions
# in resize(); controller DOM/layout helpers do not exist on device.html.
frame = frame.replace('previewMotion.position(now)', '(state.scroll||0)')
frame = frame.replace('previewCountdown.remaining(now)',
    'Math.max(0,(state.countdownRemainingMs||0)-Math.max(0,now-previewReceivedAt))')
frame = frame.replace("$('preview').style.aspectRatio=`${width} / ${height}`;", '')
frame = frame.replace('fitDevicePreview();', '')
layout = source_section((demo / 'mock-api.js').read_text(encoding='utf-8'),
                        '  async function preview(){', '  async function library(q){')
layout = layout.replace('async function preview(){',
    'async function getDemoLayout(){\n    const state=lastState,settings=deviceSettings,script=demoScript;')
(site / 'device-renderer.js').write_text(
    '// Generated from the real controller preview and local virtual-device layout.\n' +
    'const previewSelectionRevision=null;\n' + document_match + bar_options + visibility + bars + reader + 'function renderReadingFrame(now){\n' + frame + '\n' + layout,
    encoding='utf-8')

index = """<!doctype html>
<html lang="zh-CN"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><meta name="color-scheme" content="dark light"><title>提词器界面 Demo</title><style>
:root{color-scheme:dark;--bg:#11191e;--panel:#1c272d;--line:#34434a;--ink:#eaf0f1;--muted:#aab8bd;--blue:#1674d1;--green:#31976d}*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--ink);font:16px/1.55 system-ui,"Microsoft YaHei",sans-serif}.wrap{width:min(100% - 40px,1040px);margin:0 auto}.head{padding:48px 0 30px;border-bottom:1px solid var(--line)}h1{font-size:28px;margin:0 0 8px}.note{color:var(--muted);margin:0}.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(min(100%,310px),1fr));gap:14px;padding:24px 0}.link{display:flex;align-items:center;justify-content:space-between;gap:14px;min-height:104px;padding:20px;background:var(--panel);border:1px solid var(--line);border-radius:8px;color:inherit;text-decoration:none;transition:border-color .16s,transform .16s}.link:hover{border-color:#5e9bd7;transform:translateY(-2px)}.link strong{display:block;font-size:17px}.link small{display:block;color:var(--muted);margin-top:4px}.arrow{font-size:22px;color:#78b9f0}.foot{border-top:1px solid var(--line);padding:18px 0 30px;color:var(--muted);font-size:13px}@media(prefers-reduced-motion:reduce){*,*::before,*::after{scroll-behavior:auto!important;transition:none!important}}
</style></head><body><header class="head"><div class="wrap"><h1>提词器界面 Demo</h1><p class="note">虚拟设备数据，仅供检查布局与操作反馈；不会连接设备、服务器或真实稿件。</p></div></header><main class="wrap"><nav class="grid" aria-label="Demo 页面"><a class="link" href="/console.html"><span><strong>热点 / 局域网控制台</strong><small>设备本地页面，使用 Wi-Fi 控制并配置网络</small></span><span class="arrow">›</span></a><a class="link" href="https://0047ol.github.io/openprompt-p4/" target="_blank" rel="noopener"><span><strong>公网蓝牙控制台</strong><small>HTTPS 页面，通过 Web Bluetooth 连接设备</small></span><span class="arrow">›</span></a></nav></main><footer class="wrap foot">浏览器刷新会重置虚拟数据。真机通信、Web Bluetooth 和固件更新均不在此 Demo 中测试。</footer></body></html>"""
index = index.replace('虚拟设备数据，仅供检查布局与操作反馈；不会连接设备、服务器或真实稿件。', '本地页面使用虚拟数据；公网蓝牙入口打开实际服务器。')
index = index.replace('HTTPS 页面，通过 Web Bluetooth 连接设备', '打开实际服务器，通过蓝牙连接提词器')
index = index.replace('<nav class="grid" aria-label="Demo 页面">', '<nav class="grid" aria-label="Demo 页面"><a class="link" href="/device.html"><span><strong>真机屏幕 · 全部场景</strong><small>播放、倒计时、开关机、重启、升级、休眠和分步连接引导</small></span><span class="arrow">›</span></a>')
index = index.replace('</nav></main>', '''<a class="link" href="/console.html?transport=ble"><span><strong>蓝牙控制台 · 虚拟设备</strong><small>本地预览蓝牙连接、设置和断线提示，不占用真机</small></span><span class="arrow">›</span></a></nav><p class="note">网页快速预览</p><nav class="grid" aria-label="页面快速入口">
<a class="link" href="/console.html?view=cards"><span><strong>稿件库 · 卡片</strong><small>搜索、多选、排序和稿件操作</small></span><span class="arrow">›</span></a>
<a class="link" href="/console.html?view=list"><span><strong>稿件库 · 列表</strong><small>连续列表与元信息布局</small></span><span class="arrow">›</span></a>
<a class="link" href="/console.html?view=play"><span><strong>播放设置与阅读预览</strong><small>滚动、镜像、倒计时和移动端抽屉</small></span><span class="arrow">›</span></a>
<a class="link" href="/console.html?view=edit"><span><strong>编辑稿件</strong><small>标题、正文和保存反馈</small></span><span class="arrow">›</span></a>
<a class="link" href="/console.html?view=new"><span><strong>新建稿件</strong><small>新建后保存至虚拟稿件库</small></span><span class="arrow">›</span></a>
<a class="link" href="/console.html?view=settings"><span><strong>设备设置</strong><small>网络、SD 卡、更新、标题栏和状态栏</small></span><span class="arrow">›</span></a>
<a class="link" href="/console.html?storage=internal&amp;view=settings"><span><strong>无 SD 卡 · 内置存储</strong><small>独立稿件库与剩余容量的虚拟预览</small></span><span class="arrow">›</span></a>
<a class="link" href="/console.html?update=required"><span><strong>强制更新弹窗</strong><small>不可取消，仅使用虚拟固件，不连接真机</small></span><span class="arrow">›</span></a>
<a class="link" href="/console.html?transport=ble&amp;view=offline"><span><strong>离线与重新连接</strong><small>蓝牙断开、禁用状态和重新连接</small></span><span class="arrow">›</span></a>
</nav></main>''')
(site / "index.html").write_text(index, encoding="utf-8")
print(f"Demo built: {site}")
