# Test and handoff / 测试与交接 — 20261120.18

Completed across November 20–21, 2026. The public BIN is independently compiled from sanitized source: **6166000 bytes**, SHA-256 **`753cc268c29efdcac0435984f2c33b6bfd67f4cfeb0a1f2f5093c38b0e16f2ff`**. It has not been installed or functionally tested on hardware. Private/internal-image results below are context, not acceptance of this public image. Verify the ZIP manifest before installing; use only the main application BIN for OTA/SD update. Do not flash C6 or erase existing manuscripts/settings. Android is excluded.

## Changes / 本版修改

- Add on-demand snapshots of the actual completed display frame, rotation-aware and safe during sleep; fix the header playback/pause/stop indicator being placed outside the physical panel. Static captures do not establish optical smoothness.
- Translate the cached guide QR/text rectangle with DMA instead of rasterizing it every frame; complete a 320 ms smoothstep transition. Keep guide fonts independent of footer fitting.
- Make ordinary automatic SD and online update settings mutually exclusive, enforced by the backend. Manual/mandatory updates remain available.
- Choose one of date/clock/playback timecode in the upper-left header and one of previous/next manuscript; show “已循环：N” / “Loops: N”. Keep all six selected footer fields by reducing effective font size when necessary; configured size is the upper limit.
- Place the internal-library switch next to current storage status and keep automatic sleep/shutdown side by side on mobile. Shared controller, embedded web and virtual hardware gallery are synchronized, including English translations.
- Include project-local exFAT support alongside FAT12/16/32, independent internal storage and capacity meters. Never automatically format SD or erase unknown internal contents.

## Evidence boundaries / 证据边界

The exact final **private** image passed USB application-only installation/hash verification, automatic-update and header exclusivity/conflicting-request rejection, real exFAT temporary Chinese manuscript CRUD, real landscape/portrait six-field captures, actual red-dot pixels inside the header, 90-second playback with heartbeats and four highlight changes without unintended pause/reboot, six guide/reader returns, a completely black sleeping frame without screenshot wake-up, and two normal reboots preserving the test manuscript/settings. Temporary files were removed and baseline settings restored.

Chromium and WebKit completed 186 virtual UI combinations across Chinese/English, light/dark, mobile/desktop and the hardware gallery. Focused footer selection, queue, motion, storage, network, sheet, search, button and required-update checks passed. Two older test fixtures did not complete because of an existing confirmation modal and an outdated asynchronous sheet locator; they are not counted as passes. New focused checks do not turn the old scripts into passes.

These results do not validate this independently compiled public BIN, physical smoothness, every transport or every feature. User-reported visible scrolling jitter and slow cold loads remain open. A single USB enumeration failure recovered after a normal LAN restart; its root cause is not closed. A prior shutdown observation found no cyan flash once; broader optical acceptance is still pending.

## Preparation and acceptance / 准备与验收

Back up manuscripts/card and settings. Record exact BIN hash, browser/OS and transport. Use temporary manuscripts and restore every changed setting afterwards; delete only your test data. Preserve originals and do not format the original card. Each item needs steps, expected/actual behavior, duration and private logs/video when relevant.

| Item | Steps | Expected result and evidence |
| --- | --- | --- |
| 1 | Install the public main application BIN with a supported existing controller; observe boot, version and healthy state. Reboot twice after creating a temporary Chinese manuscript with custom style | Version/hash match; text/style/ID and settings persist. A successful manifest download is not an installed update. Record update route, duration, progress and recovery |
| 2 | Play 5–10 minutes in landscape/portrait with normal/highlight styles. Enter/leave the reader six times; observe QR/text guide transitions and the connecting spinner | No backward jumps, visible speed pulses, missing animation or reboot. QR and text enter together; connecting spinner turns. Record a physical video; static screenshots and software FPS cannot establish smoothness |
| 3 | Select six footer fields with large values/long names, Chinese/English and landscape/portrait. Switch date/clock/timecode and previous/next; enable looping | Every selected footer field stays visible, shrinking font if required, without silently dropping/truncating a field. Header groups are exclusive; loop label counts completed cycles. Report unreadably small text as a failure |
| 4 | Enable SD automatic update, then online automatic check; save/reload/reboot. Check updates with and without internet | At most one ordinary automatic policy is enabled; explicit conflicting requests are refused. Offline failure is understandable. Manual and mandatory update flows remain available; test actual local-BIN, online, SD and BLE installations separately |
| 5 | Use backed-up exFAT and FAT32 cards; separately boot with the card physically absent. Create same-title/different-text manuscripts in SD/internal libraries, switch libraries with an old editor open and try saving | Independent libraries; stale editor is refused; no silent cross-library overwrite. No-card startup supports internal storage. Full-storage and interrupted-write recovery require an expendable separate test library; never interrupt valuable writes |
| 6 | Compare cold/warm HTML, a small API and reader preview over hotspot/router LAN, with/without internet; return from phone background/lock; reconnect after actual reboot | Font fallback/cache works, no preview bounce, coherent recovered state. Record network timing. Page cold-load latency and optical scrolling jitter remain open, not presumed fixed |
| 7 | Perform five fresh public Web Bluetooth connections, refresh/disconnect/reconnect and background recovery; repeat a shortest save/play/delete over hotspot and LAN | No false connection or indefinite wait; failures remain recoverable and bar controls editable. Keep timings and failure stage. Private HTTP success does not validate BLE/AP/OTA |
| 8 | Observe software restart/shutdown and physical RST, and USB enumeration three times, after saving data | Normal status page then black, no cyan/green flash, USB and preserved settings recover. Report when any flash occurs and device/serial state |
| 9 | Check settings at 320px and desktop, both themes/languages: library switch, capacity, auto-sleep/shutdown, tap-to-open info | Aligned compact layout, mobile timeout fields side by side, real known capacity and no invented percentage for unknown values. Restore original backend/timeouts |
| 10 | Capture actual display frames in landscape, portrait and sleep; try sequential captures using distinct files | Correct pixels/orientation, sleeping capture is black and does not wake the panel; old outputs are not overwritten. Record HTTP error and capture header on failure |

Initial internal writable budget is approximately 2.58 MB with transaction headroom; each manuscript is limited to 8192 UTF-8 bytes. Physical no-card/FAT32, full-storage/interrupted writes, BLE/AP and all OTA routes were not exhaustively retested on the final private image and require public-image acceptance here.

## Actual device screenshots / 真机截图

Use the current reachable device address, not a saved historical address:

```powershell
py tools/capture-device-screen.py --url http://DEVICE_IP --output screen.png
```

PNG needs Pillow; BMP needs only Python. The output must not exist. Default `--scale 2` returns roughly 512 KB; `--scale 1` preserves native 1280×800 or 800×1280 pixels and is about 2 MB; `--scale 4` is smaller. Endpoint: `GET /api/display/capture.bmp?scale=2`. Captures are on demand, one background transfer at a time, not a video stream or presence heartbeat. A half-size capture typically takes seconds; native capture can take tens of seconds. Do not continuously poll it while assessing performance.

Screenshots show completed panel-buffer content, not optical flicker/backlight/color or animation smoothness. They may contain private manuscripts/network details; review before sharing. Public release images are virtual examples, not hardware photographs.

中文：公开BIN独立构建未安装实测，不能套用内部通过。内部最终版完成互斥、exFAT临时稿、横竖六项/红点真实截图、90秒播放高亮、引导往返、息屏不唤醒和两次重启保留；临时稿与原设置已恢复。186网页组合不是全部硬件验收，两个旧测试脚本未完成也不算通过。请按上表逐项补测并反馈版本/哈希、入口、时间与日志/视频。滚动抖动、冷开慢、偶发USB识别及各传输/升级路径仍需关注。二维码/文字采用缓存滑动，底栏六项自动缩字号；互斥和说明已同步网页与Demo。不要格式化原卡、擦未知分区或刷C6。仅主应用BIN用于OTA，ZIP中的其他BIN仅供按说明USB安装。
