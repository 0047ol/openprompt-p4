# Test and handoff / 测试与交接 — 20261121.7

2026-11-21. The independently compiled public BIN has not been installed. Internal-device checks below validate the corresponding source paths, not this separate public artifact. Never flash diagnostic images, erase unknown storage or flash C6. Retain unaffected valid baselines instead of repeating unrelated suites for a new version number.

Changed: manual web shutdown becomes soft-off with remote power-on/progress; confirmed automatic deep shutdown still requires physical reset/power. Ordinary offline state is not classified as shutdown. Backend write/control paths reject operations while soft-off. Crash summaries retain up to four abnormal resets with code locations and fixed stages, survive normal reboots, and can be exported/acknowledged without deletion. Panic capture avoids allocation, locks, SD and NVS writes; persistence occurs after reboot. Abrupt power loss cannot guarantee detailed capture. The panic path darkens and holds the backlight; optical flash acceptance remains a separate observation.

Internal results: actual injected panic captured reset reason/core/code/stage; NVS history and acknowledgement survived a normal reboot without a false new crash. Actual HTTP and BLE soft-off retained connections, rejected mutations and presence-driven wake, exposed startup progress and preserved the boot ID/manuscripts/settings; a requested off-screen capture was entirely black. Chromium/WebKit virtual HTTP/GATT checked shared power dialogs, draft retention and diagnostics export/acknowledgement. Delayed-countdown unit/browser cases passed. Guide entry/step/return/language/address/reduced-motion policy passed in both engines. The exact normal internal production image also passed actual LAN browser diagnostics/details/export/acknowledgement, soft power/progress with draft retention, real-font monotonic countdown/playback, owned-draft cleanup and a final healthy reboot preserving originals/settings/history. A single697ms ready sample does not close cold-load latency.

Real 64GB FAT32 card: CRUD, style, identity, order and safe eject/remount passed. All free FAT clusters were allocated (319.64 s; payload sectors were not all overwritten); full-filesystem create/save failed cleanly and left old documents intact. Removing only diagnostic filler files restored writes. Actual resets after transaction rename phases1/3/5/7 rolled back text/style/metadata together, preserved IDs/order and were not misclassified as panic. This is CPU-reset interruption, not physical card-power removal. The original exFAT format and every original file hash/order token were restored. The initial diagnostic FIL cache initialization fault was corrected in the diagnostic tool and is not reported as a production manuscript failure.

Targeted follow-up: export a naturally occurring fault from device settings; verify cause/version match, acknowledge, reboot and confirm retained history without repeated alert. Observe panic/restart backlight behavior optically when a fault occurs. Check soft-off from each web page, startup progress, disabled writes, and an unaffected draft. Check countdown under slow packets, replay/cancel and manuscript/boot changes. Looping must default off for new documents while saved custom styles remain. Observe only real guide step changes for the480ms slide; first entry/return/wake must remain immediate.

Keep open: occasional optical scrolling/guide stutter, cold page loading and intermittent BLE discovery delays; actual SD power removal during writes, arbitrary failing media, public-BIN installation and phone playback background/lock acceptance. Do not claim full stable acceptance. Available-phone browsers did not support Web Bluetooth; the observed phone LAN initial load was under3s, and guide-only lock/background behavior was reported normal. These do not validate playback recovery in every mobile OS.

中文：内部真实故障捕获/普通重启持久性、HTTP/BLE软开关机及拒写、FAT32满簇与释放恢复、四阶段CPU复位事务回滚已补测；已恢复原exFAT及原文件/排序。公开BIN未实装，不套用内部通过。物理写入断电、肉眼流畅和闪帧等保留边界，既有未修项不称通过。新增测试仅覆盖本批直接/间接影响项。

---

# Test and handoff for 20261121.6 / 测试与交接

2026-11-21. The public BIN was independently built from sanitized sources and has NOT been installed. Do not treat internal-device results as acceptance of this exact public image. Public SHA256: `8e57d5cfb02ea84fc3324fea6d1bf3d6059ef414e1515aa92487d6da7312d1cf` (6169216 bytes). Screenshots are virtual examples, not hardware photographs. Android native testing is paused; never flash C6.

`.6` animates real guide-step changes while continuously visible. Initial entry, return, wake, language/address refresh and equivalent READY/RECONNECT states reveal immediately; loaders continue. Pure C sanitizer checks and Chromium/WebKit normal/reduced-motion combinations passed. On internal `.6`, three actual reading returns and one wake produced no slides; actual step transitions logged 19 frames in330–331ms with maximum intervals near19.4–19.7ms. Optical acceptance is pending.

Supplemental internal results: actual desktop LAN load/refresh, font preview, play/pause, network recovery retaining paused position and receiving another controller's update passed without browser errors. Confirmed-awake idle sleep set to2s triggered in2.587s and retained brightness on wake; an earlier smoke check had an inadequate starting-state assertion and is not the timing evidence. Actual configuration reset/reboot returned defaults, cleared Wi-Fi, reconnected BLE and preserved SD content/style/identity/order; the private original NVS backup then restored Wi-Fi/preferences. Actual6s idle shutdown triggered in6.250s, with the original disabled timer restored before USB detachment and HTTP loss. Startup after deep sleep requires physical reset/power.

Unchanged internal `.5` storage evidence remains valid: no-card internal-library persistence, idle hot insertion/manual reread, same-name library isolation and stale-write rejection, idle hot removal recovery, explicitly authorized64GB exFAT formatting in36.28s, post-format CRUD/style/identity/order, safe eject/remount and reboot persistence. Owned drafts were cleaned and original settings restored. There is no immediate card-detect signal; reread is needed after insertion. These results do not establish FAT32, full-card or interrupted-write behavior.

Human observations: six selected footer fields were clear and aligned in portrait; a physical startup had no observed flash. Scrolling still occasionally stutters; `.6` does not change the scrolling path. A Xiaomi Android system browser completed initial LAN connection in under3s. Lock/background recovery has not been tested. A separate approximate3s BLE success was a desktop result, not a phone result; the available phone browsers do not support Web Bluetooth.

Remaining manual checks: physical restart after shutdown; phone lock60s/return and background-app60s/return on an assistant-prepared playback page; optical guide-step motion/loaders/shutdown flash. Keep known scrolling stutter, cold-load delays, intermittent slow BLE discovery, FAT32/full-card/interrupted-write and this uninstalled public binary explicitly open. Do not repeat unrelated valid baselines solely because the version changed. Back up unknown media; never format or erase it without authorization.

中文：公开BIN仅独立编译，未实装；上述真机结果仅属内部版本。电脑可测的LAN加载/播放/断网保位、多端修改、实际自动息屏/关机和恢复出厂保稿已补测，原设置已恢复。手机锁屏/后台、光学动画、关机闪屏、FAT32/满库/写入中断及公开BIN实装仍待验，滚动偶发跳动未关闭；不宣称全功能稳定全通过。

---

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
