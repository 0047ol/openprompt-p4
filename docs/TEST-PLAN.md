# Test and handoff / 测试与交接 — 20261121.5

2026-11-21. **The independently compiled public BIN has not been installed or functionally tested.** Its own size/hash are in the release manifest. Internal-device observations below cover equivalent source paths, not acceptance of this public binary. Use only the main application BIN for OTA/SD; preserve manuscripts/settings and unknown storage. Android testing is paused; never flash C6.

## Changes / 修改

- Bound glyph rasterization to 32-row strips instead of a full 256-row scratch canvas. Reserve an 80 KiB internal work buffer early, keeping a small PSRAM fallback. Preserve all rotations/mirrors, tile ownership, 32/64-row look-ahead budgets and panel timing.
- Present every panel refresh during guide transitions, even when rounded animation coordinates repeat. Keep complete transitions instead of shortening or removing them.
- Use font-scaled separator gaps in firmware and shared web/Demo bars. All six selected fields remain visible; extreme cases can still require small type.
- Use shorter steady BLE advertising intervals and finite leases with watchdog recovery. Leave established GATT connections intact. Historical timeout root cause remains unproven.
- Pin TinyUSB processing away from the display core, avoid USB logging inside its callbacks, and explicitly detach before deep sleep. Physical shutdown/reset remains a manual gate.

## Evidence and limits / 证据与边界

The installed internal .5 used internal memory for its raster strip. Eight actual frozen screen captures, covering four rotations, normal/mirrored and bold/highlight text, were pixel-identical to the .3 reference. A scalar oracle covered 160 production tile-transform cases; repeated-frame geometry covered 24,732 cases. Host checks do not prove physical smoothness.

Internal samples: short/no-bars 51.50fps, jitter0.04ms, zero late frames in the sample; short/bars49.85fps, jitter3.72ms; long/no-bars51.22fps, jitter0.66ms; long/bars49.65fps, jitter3.66ms. Long/no-bars improves substantially over the intermediate PSRAM fallback, but bars still produce late frames. Sampling windows differ; do not infer precise percentage gains or optical acceptance. Some guide transitions still show approximately38–39ms gaps.

Six actual BLE discovery/read rounds passed, including35-second idle periods. First matching packets took0.22–11.55seconds; complete ready times2.31–15.63seconds. These successes do not close intermittent slow discovery. Chromium and WebKit each retained all selected fields across16,016 extreme six-field combinations; minimum fitted text improved from10 to11pixels. Actual small-type readability remains a human decision.

Four internal high-speed forward/reverse x8 cases with rotations, mirrors, bold/highlight and stop/reset passed. Three software restarts recovered USB enumeration and healthy firmware in about21seconds with preferences and the original library preserved. Twenty subsequent status requests took15–141ms (median47ms). These results do not prove physical shutdown/reset or all network environments.

Test-tool failures remain recorded separately: a content+style etag was wrongly compared after deliberately changing style, and an initial USB assertion ran before the intentional15-second healthy confirmation. Corrected content checks and the independent three-round healthy-waiting USB check passed; the original full-script failures are not relabeled as passes.

Prior unaffected library/search/queue, playback semantics, drawer, network form, storage isolation, actual OTA and UI baselines remain retained. Do not repeat unrelated full suites solely because a version number changed, and do not treat compilation, virtual screenshots or USB writes as proof of an OTA path. No destructive storage, physical power/reset or actual phone-background checks were performed for this change.

## Remaining acceptance / 待验收

Back up originals. Record exact binary hash/version, browser/OS, transport, time and settings. Use disposable manuscripts; delete only test content and restore changed settings. Private actual screenshots and logs are excluded from public releases.

| Item | Steps | Expected / feedback |
| --- | --- | --- |
| Public image | Install this public application image; verify healthy/version; save/read a temporary Chinese manuscript and style, then reboot | Correct public hash/version; content/style/ID/order and settings retained; keep installation logs |
| Physical scrolling | Landscape/portrait, short/near8KB, bars/highlight on/off, ordinary/high speed | No backward jumps or visible stalls; provide video, exact time, font/speed and bar selections. Average FPS and static captures are insufficient |
| Guide/loading | Enter/leave playback several times; observe actual connection loading | QR and text enter together smoothly, loading circle keeps moving. Occasional38–39ms gaps remain open |
| Discovery | Fresh supported phone/PC browser after long idle and after disconnect; record time until visible and ready | Device discoverable without repeated clicking; retain timeout and slow successes, not just retries |
| Extreme bars | Six long fields, landscape/portrait, date/clock/timecode and previous/next exclusivity | Every selected field present and readable. Report small type as an issue even when no text is clipped |
| Phone lifecycle | Lock/background a supported real phone, then resume; use a second controller | Valid reconnect and retained position without overwriting another controller; record OS/browser and elapsed time |
| Dedicated media | Backed-up FAT32; physical no-card boot; expendable card only for format/full/write-interruption | Safe independent internal fallback and persistence; original card and unknown partitions untouched |
| Power/reset | Shutdown then physical RST, check USB; optical cyan flash; factory reset only after separate configuration backup | Healthy USB recovery, no optical flash, manuscripts retained; restore configuration backup |

## Actual screen capture / 真机截图

python tools/capture-device-screen.py --url http://DEVICE_IP --output screen.png reads the completed panel frame without waking it or renewing control presence. PNG requires Pillow, BMP does not. Default scale2; scale4 is smaller and scale1 native. Do not capture continuously while measuring performance. Review private manuscript/network content before sharing; public illustrations use virtual data.

中文：本版已对文字缓存、引导调度、六参数间距、蓝牙广播与USB隔离继续修复。公开BIN独立编译未安装，不套用内部通过。内部截图8组逐像素一致、六轮蓝牙读取成功；长稿隐藏栏改善明显，但显示栏晚帧、引导间歇卡顿及慢发现仍保留。人工/备用卡/手机/物理电源验收等用户安排，不冒充已完成。唯一交接文档随固件ZIP提供。
