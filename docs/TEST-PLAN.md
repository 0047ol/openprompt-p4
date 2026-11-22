# Test and handoff / 测试与交接 — 20261122.4

SD scanning/verification has no measured progress and always shows a spinner, even if old transfer counters remain. A higher-priority firmware/reset task cannot reuse startup progress. Ordinary playback/countdown remains silent; all task banners retain severity icons and right-side spinner or real centered circular percentage. .3 affected browser/native baselines remain valid. Chromium/WebKit progress-source and lifecycle regressions passed. Internal .4 was USB-installed and passed embedded-page stale-progress suppression, real startup circular progress and four-page soft-off state with originals preserved; publication is separately verified; independent public .4 is not installed. No new mandatory physical tests, Android paused, C6 untouched.

中文：本版防止扫描/校验复用旧传输百分比及高优先级任务误用开机进度；未知进度只显示加载圈。保留 .3 通知规则和未受影响的有效结果，不重跑介质边界。

---

# Test and handoff / 测试与交接 — 20261122.3

Normal countdown/playback states do not emit notifications. Power/startup/restart/reset and firmware-transfer/install/restart tasks use banners with severity icons and a fixed right action slot: indeterminate spinner, or a circular progress meter with a centered actual percentage. No total means no invented progress. Other notification/confirmation uses are in [NOTIFICATIONS.md](NOTIFICATIONS.md), with all 100 source entry points in NOTIFICATION-CALLS.json.

Chromium/WebKit: each 32 viewport/theme/language combinations; severity icons, wrapped text and right action geometry, 0/1/42/100 percent ring and centered text, unknown-total spinner, silent countdown controls passed. Four LAN/virtual BLE soft-power groups, lifecycle and update reconnect passed. Source inventory is not a functional test. The independent public .3 BIN is compiled, not installed; internal .3 was USB-installed and passed actual task banners, advancing centered circular startup progress and four-page soft-off state without reboot; originals/settings were preserved. Keep unaffected prior native/physical acceptance and deferred limits; do not repeat card/power-cut tests or label all hardware combinations accepted. Android paused, C6 untouched.

中文：本轮任务过程改为条幅、右侧加载圈/真实环形进度，全部条幅加类型图标，正常倒计时不弹提示。两引擎受影响项回归通过；公开独立.3未安装，不套用.1实装结论。原稿配置保留，无新增强制人工测试。

---

# Test and handoff / 测试与交接 — 20261122.2

This release changes shared controller markup/styles and notification routing; native playback/storage/partitions are unchanged. Progress for startup, shutdown, restart, reset and firmware update uses a task-bound Toast. Persistent recovery/offline/off/conflict states retain compact banners with necessary actions and no separate headings. See [the complete inventory](NOTIFICATIONS.md) and [source call sites](NOTIFICATION-CALLS.json).

Current browser regression: Chromium/WebKit across32 viewport/theme/language combinations, help/switch separation, full-width diagnostics, unavailable-versus-pending styling, all lifecycle progress routes, toast expiry/deduplication, persistent banner geometry, LAN/virtual BLE soft power, diagnostic actions, update reconnect, SHA rejection-before-write and update locks passed. Source inventory is not itself a functional test. The exact internal .2 image was USB-installed and passed actual controller geometry, unavailable styles, four-page soft-off banners and startup Toast/progress checks without reboot; original documents/settings were preserved; public .2 is independently compiled and is not installed, while the preceding public .1 exact-image results remain a historical baseline.

No new mandatory physical power/card tests. Retain accepted unaffected guide/icons/phone lock/background/storage tests and explicitly deferred limits; do not repeat them for a version number change. Current occasional motion/loading issues are accepted, not proven eliminated. Android remains paused; do not flash C6 or erase unknown data.

中文：本轮只改共享网页，修复图标/换行/禁用外观及通知路由。持续条幅无额外标题；短时与任务过程用Toast。两引擎定向回归通过；内部.2已实装，真实网页几何/禁用样式/四页关机条幅/开机Toast进度通过，原稿配置保留；公开独立.2尚未实装。下方.1的实装不当作.2实装，沿用未受影响原生基线。不要求重做已接受的人工/介质测试。

---

# Test and handoff / 测试与交接 — 20261122.1

2026-11-22. **This exact independently compiled public BIN was installed and passed targeted checks**, then the private device build was restored. Public SHA256: `9be537bd1819cf5362709bf386802589ebaff22a4b21c3c73dd1e77b85df16dc`, 6204160 bytes. Original manuscripts, styles, identities, timestamps and configuration were preserved. Android remains paused; never flash C6. Older sections below are historical, including their older public-image installation limits.

Changes: compact persistent/actionable banners with wrapped text and a right-centered action; transient Toasts remain separate. Soft/deep shutdown use one banner with an applicable remote-start action or physical-start instructions. Ordinary connection loss alone never proves shutdown. Fault notices apply only to the immediate recovery boot, while Developer Settings retains history. Only one close/ack action remains. All controller pages retain equal top spacing.

Screen-bar field checklists move into dialogs; paired height/font controls remain side-by-side on mobile. Date and time may coexist as one group exclusive with timecode. Zero countdown and normal-speed x1 labels disappear. Storage uses used/total labels. The common HTTP/BLE backend rejects auto-next with no following manuscript. SD I/O fallback has a lasting banner; no immediate physical card-detect signal exists.

JSON trees/serialization use PSRAM after initialization, protecting SD/USB internal memory. Mirror changes update numeric metadata without parsing the complete long preview. Geometry-only changes preserve warmed glyphs. Half/quarter captures allocate one-quarter/one-sixteenth of full pixel storage. A narrowly checked project-local IDF SPI cleanup fix frees only allocated scratch after setup failure, without copying unreceived NULL RX data. The installed SDK stays unchanged. Current diagnostic suffixes no longer masquerade as update-server failure; release metadata remains strict.

| Coverage | Result and limits |
| --- | --- |
| Shared controller | Chromium/WebKit; LAN/virtual BLE; 320/390/650/1366px light/dark banners, wrapping action centering, equal page spacing, single close/ack, power feedback, countdown, field dialogs/limits, update verification/lock and diagnostic-version checks passed |
| Internal hardware | Four rotations × two mirror states:16 scaled captures exactly matched full-frame samples; long preview, capture without waking sleep, original-data verification and owned-test cleanup passed |
| Common backend | Actual HTTP/BLE no-next rejection and has-next acceptance; date+time coexistence; contradictory timecode request400 without mutation; zero-countdown live footer empty; offset index/selective hydration/stale-etag protection passed |
| Real browser/BLE | Large manuscript create/three saves/read converged with LAN. Header/footer switches and device settings synchronized. Final LAN soft-off/on used one banner on all four pages, no duplicate power modal or reboot |
| Allocation checks |1120 actual generated SDK-function fault cases passed, including TX/RX failures before/after allocation and normal paths;180 production metadata cases covered long/empty layouts, all mirrors, numeric limits and unchanged non-target bytes |
| Exact public image | Actual installation, healthy startup, manuscript CRUD, long mirror preview and quarter capture passed; SD stayed mounted, boot unchanged, originals restored |
| Fault/physical baseline | Actual diagnostic panic: first boot pending;3 normal boots no pending, history/USB preserved. Human observed no cyan flash and a recovery notice. Actual card removal during continuous writes recovered through internal fallback; reinsertion/reread restored exFAT and originals. Real interrupted save126/127 recovered coherent committed126; dedicated test files removed. Not every interruption phase or faulty card |
| Network sample | Hotspot first/warm ready754/768ms; single status median10/max60ms, two controllers10/54ms; offline font251ms then1ms. Samples are not all-client or weak-signal guarantees |
| Accepted unaffected baseline | Keep existing FAT32/exFAT/full-cluster/CPU-reset transactions, OTA checks, library workflows and sleep results. Tester accepted guide/icons, actual phone playback lock/background recovery and current motion/loading experience; occasional delays/jitter are not claimed eliminated |
| Deferred | Remote accessory, more failing media/all interruption phases, extreme bar layouts, mobile BLE/iOS and native Android; deferred by request, not marked passed |

An intermediate image reproduced SDK SPI cleanup panic. The first mitigation prevented panic but still allowed internal JSON pressure to fail SD reads. Both were withheld. Only the later PSRAM/metadata/driver fix obtained final long-preview/storage results. Harness assumptions about list-limit parameters, contradictory settings, compact BLE settings and confirmation awaits were corrected; these are not device failures or passes.

Optional human review: check compact banners on a real small phone (readability, wrapped text and right-centered action), and judge rotation/mirror latency on a paused temporary long manuscript. Pixel correctness is already checked. Report browser/width/settings and a screenshot/video if needed; restore direction/mirror and remove only owned drafts. Do not repeat accepted guide, lockscreen, shutdown or physical-card observations. Preserve current valid baselines rather than rerunning unrelated tests for every version.

Capture may still return503 under unavailable resources; successful samples do not prove all-load success. Card removal is detected by actual I/O and insertion through reread. Fault logging is best effort; deep shutdown needs power/RST. Only the application BIN is for controller/SD updates; other BINs are USB resources. Never erase unknown storage or flash C6.

中文：精确公开独立BIN已实装并完成healthy、稿件读写、长稿镜像/截图定向验证，随后恢复内部版。本轮修复及范围见上表；已接受的手机锁屏后台、引导/图标、开关机、物理拔卡断电不重复催测，只需可选看新条幅尺寸和切换主观速度。暂缓项不写通过，偶发现象不声称根除。

---

# Test and handoff / 测试与交接 — 20261121.10

2026-11-21. The public image is independently compiled and NOT installed. Internal-device results validate corresponding source paths, not this separate public artifact. Android remains paused; never flash C6 or erase user data. Retain unaffected accepted baselines.

Changes: media/T icon size follows fitted text without double shrinking; centered vector geometry, direction-specific playback multiplier, plain non-playing states, looping label 已循环X次 / Loops X. Default playback timecode stays enabled, saved custom settings retained. Guide actual-step slide is300ms; initial entry/return/wake stay immediate. Stable body DMA overlaps pending chrome work, with copy-source lifetime and per-buffer damage preserved.

Browser results: Chromium and WebKit, landscape/portrait, playing dot only, full loop count, previous/next vectors and document names, positive/negative magnitude, T icon, six footer fields and24px bars with40px requested text passed. Timecode/exclusive fields and50 numerical font-window updates passed. Guide normal/reduced-motion policy passed at300ms. Four internal completed screen frames before/after the scheduling change were pixel-identical (landscape/portrait, highlight off/on); icon edits intentionally change pixels afterward.

Internal production .10: installed only into the active app slot, validated healthy, original library and OTA selector preserved. Targeted live media-state/six-field/countdown/highlight/orientation/small-bar checks are recorded separately in the private production evidence; the final status must be read with the publication record.

Final .10 timing samples, bars enabled: short50.50fps, mean jitter2.68ms,14late frames; long51.10fps,1.23ms,8late frames. Earlier .7 reference: short49.93fps/3.52ms/21late, long50.00fps/3.29ms/30late. Sampling windows/conditions and changing icons limit precision; residual late frames remain and these numbers do not establish optical smoothness. No screen capture was taken during timing samples.

Read-only network samples: cold-ready701ms, warm566ms;20status calls median8ms/max72ms; two controllers median7ms/max56ms; embedded offline font first267ms/repeat1ms. Six BLE rounds all connected, first matching advertisement0.17–1.81s, ready1.06–3.80s. Intermittent historical slow loading/discovery was not reproduced, not closed.

Targeted human checks: view a real guide step change (300ms), verify it is neither sluggish nor stuttering; entry/return must reveal immediately. With one temporary draft, inspect playback red dot, text-only pause/stop, loop count without colon, skip icons with names, forward/reverse magnitude and T icon; check normal/small bar heights in both orientations. Watch a long draft with bars for occasional jumping. Report approximate time, settings and a short video if possible; restore settings and delete only owned drafts afterward.

Unchanged FAT32/exFAT CRUD/full-cluster/CPU-reset transaction, soft-power and crash-history results remain valid. Normal shutdown/startup observations already accepted by the user are not requested again. Physical SD power removal during writes, arbitrary failing media, phone playback lock/background and independently compiled public-BIN installation remain distinct limits. Available phone browsers lacked Web Bluetooth; LAN guide-only background recovery was reported normal. Do not call all features fully accepted.

中文：本版内部固件已安装并健康，公开独立BIN未实装；两者不可混淆。图标字号/对齐、状态/方向及时间码等两引擎虚拟验证通过，带栏滚动性能改善但仍有晚帧，肉眼偶发跳动未关闭。关机/引导后台已接受的观察不重复要求，其他未完成边界按上文保留。

---

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
