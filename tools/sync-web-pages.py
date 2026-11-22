"""Generate the embedded LAN controller and public BLE page."""
from pathlib import Path
import json
import shutil
import gzip
import hashlib
import re
import subprocess

root = Path(__file__).resolve().parent.parent
web = root / 'web'
motion = json.loads((root / 'shared/ui/motion.json').read_text(encoding='utf-8'))
(root / 'firmware/p4/main/ui_motion_settings.h').write_text(
    '/* Generated from shared/ui/motion.json. */\n#pragma once\n' +
    ''.join(f'#define TP_MOTION_{key.upper()} {value}\n' for key, value in motion.items()), encoding='utf-8')
system_screen = json.loads((root / 'shared/ui/system-screen.json').read_text(encoding='utf-8'))
screen_bars = json.loads((root / 'shared/ui/screen-bars.json').read_text(encoding='utf-8'))
bar_names = {'showHeader':'HEADER','showFooter':'FOOTER','headerHeight':'HEADER_HEIGHT',
             'footerHeight':'FOOTER_HEIGHT','headerFontSize':'HEADER_FONT_SIZE',
             'footerFontSize':'FOOTER_FONT_SIZE','barFields':'FIELDS'}
(root / 'firmware/p4/main/screen_bar_defaults.h').write_text(
    '/* Generated from shared/ui/screen-bars.json. */\n#pragma once\n' +
    ''.join(f'#define TP_BAR_DEFAULT_{bar_names[key]} {str(value).lower()}\n'
            for key, value in screen_bars.items()), encoding='utf-8')
(root / 'firmware/p4/main/ui_system_settings.h').write_text(
    '/* Generated from shared/ui/system-screen.json. */\n#pragma once\n' +
    ''.join(f'#define TP_SYSTEM_{key.upper()} {value}\n' for key, value in system_screen.items()), encoding='utf-8')
guide = json.loads((root / 'shared/ui/connection-guide.json').read_text(encoding='utf-8'))
quote = lambda value: json.dumps(value, ensure_ascii=False)
guide_header = ['/* Generated from shared/ui/connection-guide.json. */', '#pragma once']
guide_header += [f'#define TP_GUIDE_{key.upper()} {value}' for key, value in guide['layout'].items()]
guide_header += [f'#define TP_GUIDE_STEP_COUNT {len(guide["steps"])}', 'static const char * const tp_guide_steps[TP_GUIDE_STEP_COUNT][2] = {']
guide_header += ['{' + ','.join(map(quote, pair)) + '},' for pair in guide['steps']]
guide_header += ['};', 'static const tp_guide_copy_t tp_guide_copy[] = {']
for name, item in guide['states'].items():
    guide_header.append('[TP_GUIDE_' + name + ']={' + str(item['step']) + ',{' +
                        ','.join(map(quote, item['title'])) + '},{' + ','.join(map(quote, item['body'])) + '},' +
                        ','.join('true' if flag else 'false' for flag in (bool(item.get('qr')),item.get('centered',False),item.get('loading',False))) + '},')
guide_header += ['};', '']
(root / 'firmware/p4/main/connection_guide_copy.h').write_text('\n'.join(guide_header), encoding='utf-8')
page = (web / 'controller.html').read_text(encoding='utf-8')
css = (web / 'controller.css').read_text(encoding='utf-8')
# Keep authored comments in the source checkout, not in both embedded payloads.
# Only standalone CSS comments are removed; declarations and string values stay intact.
css = re.sub(r'(?m)^[ \t]*/\*.*?\*/', '', css, flags=re.S)
css = subprocess.run(['node',str(root/'tools/minify-web.cjs'),'--css'],
    input=css,text=True,encoding='utf-8',capture_output=True,check=True).stdout
page = page.replace('/* CONTROLLER_CSS */', css)
i18n=(web/'i18n.js').read_text(encoding='utf-8').replace('/* TRANSLATIONS_JSON */',
    json.dumps(json.loads((web/'translations.json').read_text(encoding='utf-8')),ensure_ascii=False,separators=(',',':')))
version=re.search(r'set\(PROJECT_VER "([^"]+)"\)',(root/'firmware/p4/CMakeLists.txt').read_text(encoding='utf8')).group(1)
i18n=i18n.replace('/* PRODUCT_VERSION */',json.dumps(version))
controller=(web / 'controller.js').read_text(encoding='utf-8').replace('/* MOTION_JS */',(web / 'motion.js').read_text(encoding='utf-8'))
controller=controller.replace('/* LIBRARY_JS */',(web / 'library.js').read_text(encoding='utf-8'))
font_index_hash=hashlib.sha256((web/'reader-cdn.json').read_bytes()).hexdigest()
font_index_url='https://0047ol.github.io/openprompt-p4/fonts/noto-sc-4.5.12/reader-'+font_index_hash[:16]+'.json'
reader=(web/'reader-font.js').read_text(encoding='utf-8').replace('/* READER_CDN_SOURCE */',json.dumps(font_index_url))
font_index=json.loads((web/'reader-cdn.json').read_text(encoding='utf8'))
# Offline/bundled loading only needs glyph coverage and shard filenames. Keep
# the full audited URL/hash manifest on the server, without embedding metadata
# that the offline loader never reads in both identity and gzip HTML payloads.
offline_index={key:font_index[key] for key in ('sourceSha256','supplement','localOnly')}
offline_index['files']=[{key:file[key] for key in ('file','points')} for file in font_index['files']]
reader=reader.replace('/* READER_OFFLINE_INDEX */',json.dumps(offline_index,separators=(',',':')))
bars=(web/'screen-bars.js').read_text(encoding='utf-8').replace('/* SCREEN_BAR_DEFAULTS */',json.dumps(screen_bars))
controller=controller.replace('/* PREVIEW_JS */',reader+'\n'+(web / 'page-layout.js').read_text(encoding='utf-8')+'\n'+(web / 'preview.js').read_text(encoding='utf-8')+'\n'+bars)
controller=controller.replace('/* FIRMWARE_JS */',(web / 'power.js').read_text(encoding='utf-8')+'\n'+(web / 'diagnostics.js').read_text(encoding='utf-8')+'\n'+(web / 'bluetooth-remote.js').read_text(encoding='utf-8')+'\n'+(web / 'wifi-scan.js').read_text(encoding='utf-8')+'\n'+(web / 'firmware.js').read_text(encoding='utf-8'))
controller=controller.replace('/* TOOLTIPS_JS */',(web / 'tooltips.js').read_text(encoding='utf-8')+'\n'+(web / 'zoom-warning.js').read_text(encoding='utf-8'))
controller=controller.replace('/* SESSION_JS */',(web / 'session.js').read_text(encoding='utf-8'))
controller=controller.replace('/* PLAYBACK_TOOLBAR_JS */',(web / 'playback-toolbar.js').read_text(encoding='utf-8'))
sortable=(root/'third_party/SortableJS/Sortable.min.js').read_text(encoding='utf-8')
controller_js=subprocess.run(['node',str(root/'tools/minify-web.cjs')],input=sortable+'\n'+i18n+'\n'+controller,
    text=True,encoding='utf-8',stdout=subprocess.PIPE,check=True).stdout
page = page.replace('/* CONTROLLER_JS */', controller_js)
(root / 'firmware/p4/main/web_editor.html').write_text(page, encoding='utf-8')
(web / 'index.html').write_text(page, encoding='utf-8')
(web / 'hosted.html').write_text(page.replace('data-default-transport="lan"',
    'data-default-transport="ble" data-hosted="true"', 1), encoding='utf-8')
stale_ble_page = web / 'bluetooth.html'
if stale_ble_page.exists():
    stale_ble_page.unlink()
print('Updated embedded LAN controller and public BLE page')

# Compress once during the build; serving requires no ESP32 CPU or RAM buffer.
asset_lines=['/* Generated by tools/sync-web-pages.py. */','#pragma once']
for name,key in [('web_editor.html','EDITOR')]:
    data=(root/'firmware/p4/main'/name).read_bytes()
    compressed=gzip.compress(data,compresslevel=9,mtime=0)
    (root/'firmware/p4/main'/(name+'.gz')).write_bytes(compressed)
    for encoding,payload in [('IDENTITY',data),('GZIP',compressed)]:
        digest=hashlib.sha256(payload).hexdigest()
        asset_lines.append(f'#define TP_{key}_{encoding}_ETAG "\\\"{digest}\\\""')
    print(f'{name}: {len(data)} -> {len(compressed)} bytes')
data=(web/'reader.woff2').read_bytes()
asset_lines.append(f'#define TP_FONT_ETAG "\\\"{hashlib.sha256(data).hexdigest()}\\\""')
(root/'firmware/p4/main/web_asset_tags.h').write_text('\n'.join(asset_lines)+'\n',encoding='utf8')
