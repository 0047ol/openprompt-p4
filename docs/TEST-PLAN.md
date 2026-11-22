# 20261122.16 Bluetooth remote / 蓝牙遥控器（2026-11-22）

P4 now pairs directly with compatible BLE HID Consumer Control remotes. The settings card provides discovery/pairing, enable/reconnect, single/double click mappings, disconnect and confirmed forgetting. Default single click toggles play/pause; double click advances one page or the next scrolling manuscript (paused). A single click waits about 350ms; independent long-press actions are not supported. A remote consumes one of three BLE connection slots and an idle link alone does not hold the reader session awake. Maintenance, update and power transitions block input.

Standard battery level is read/notified when available. At or below20%, a short Toast warns once per connection/low interval; the card retains the battery warning, resets after recovery above25%, and hides stale values after disconnect. Actual ULANZI battery was97%; actual low battery was NOT forced.

New card rows and custom selects match the shared theme; bar-field settings entries disable offline/during power/update. Date+time defaults on (timecode off), spacing5px and countdown3s preserve saved styles/preferences. Source/generated controller/Demo resources match.

Production C parsing/gesture bounds pass. Chromium/WebKit × virtual LAN/GATT pass pairing/UI/mappings, bilingual light/dark narrow layouts, low-battery dedup/recovery, offline guards and affected editor/page/default/queue regressions. Virtual GATT and simulated battery do not prove physical behavior. Actual internal P4 paired ULANZI, read97%, received single→play, single→pause, double→page2 of3 with exactly3 actions and no restart; originals restored. The final web-layout-only refinement was reinstalled healthy; real LAN and Web Bluetooth read persisted pairing, custom menus and offline guards. Pairing persisted across reboot, but the remote did not reconnect before the observer left; automatic reconnection, real simultaneous remote+BLE webpage, scrolling next/edge/wake/update and other devices remain pending. No new human action is required now.

Independent public BIN: 6225120bytes, SHA256 `950630f1130b8b63251a74bfbb032e2f41faae2c7a1cbcc22d3a6d6dbd63f896`. It is built separately from sanitized sources and NOT installed this round; internal results do not establish its hardware acceptance. Both existing OTA slots are unchanged. Final publication assets/manifests are verified separately after packaging. Android testing remains paused; no coprocessor firmware changes. Unchanged accepted baselines stay valid.

中文：新增P4原生蓝牙遥控器卡片/按键/电量；实物ULANZI配对、97%及单击播放/暂停、双击下一页通过。最终排版版已刷入，真实LAN/网页蓝牙通过统一下拉菜单和离线禁用；重启配对信息保留，实际自动回连和遥控器+网页蓝牙并发尚待唤醒实测。低电量去重使用虚拟值验证，不冒充真实低电量。公开独立BIN未实装；不称所有硬件组合全验收。

Optional follow-up: wake the remote without connecting a phone/PC, check auto-reconnect/battery, use a simultaneous BLE webpage, verify next manuscript remains paused/no last-item wrap and first press wakes an asleep display. Test disabled/update/power input only when ready. Do not erase original manuscripts/media. Report exact edition/version, browser/transport, device model, settings, click timing and logs. Long press/other models/connection-capacity boundaries remain unclaimed.

---

# 20261122.15 targeted scope / 定向范围（2026-11-22）

Header date and time are now on together by default, with timecode off and still mutually exclusive. New/unstyled manuscripts and restore-defaults use 5 px letter spacing and a 3-second initial countdown. Controller range/numeric fields, device firmware and shared screen demos match. Existing saved manuscript styles and device preferences are preserved.

Chromium/WebKit × virtual LAN/GATT passed new defaults, form values, date/time fields, saved-style retention and restore-defaults; virtual GATT is not physical Bluetooth. The exact internal image was installed healthy by writing only its validated active app slot. Actual HTTP checks passed new/restored defaults, retained custom styles and a monotonic roughly 3-second countdown. Its actual embedded browser passed form values, header restore-defaults and timecode mutual exclusion. Original manuscripts/styles/settings were restored and no unexpected reboot was observed. Unchanged accepted .14 playback/layout/media/power baselines remain valid without needless repetition. Android testing stays paused; no coprocessor flashing.

The independent public image (6188704 bytes, SHA256 `9865f26aee32fbba1591539d4ebadd9b27e22418b72c6c88998d9021e682bf7b`) was separately built but NOT installed this round; internal results do not establish its hardware acceptance. The combined review ZIP includes this handoff; remote assets/manifests are verified separately during publication.

Bluetooth remote research only: a physically connected ULANZI device was identified as BLE HID (service 0x1812) with Consumer Control reports. Recorded input includes Report ID 2, usages 0x00EA/0x00E9 and zero reports. No reliable long-hold report is established. Proposed mapping is single click play/pause, double click next page in paged mode or next manuscript in scrolling mode. Direct P4 pairing/HID host/reconnect support is NOT implemented in this release. Do not interpret PC observations as P4 remote acceptance.

中文：日期与时间默认同时开启，时间码默认关闭且互斥；新稿／恢复默认字间距5px、倒计时3秒，保留已保存样式／设备偏好。双引擎虚拟LAN/GATT、精确内部实装与实际嵌入网页定向验证通过，原稿／配置已恢复；公开独立BIN未实装，结果不可套用。实物ULANZI已识别BLE HID并采到消费键数据，遥控器直连仍为研究，不包含配对／按键控制功能。

Optional checks: create a temporary manuscript, verify spacing 5/countdown 3, customize and reopen, restore defaults and test once-only countdown. Restore header defaults: date/time on, timecode off; selecting timecode disables both. Delete only the temporary manuscript and restore your original active document/style/header preferences. Report exact edition/version, transport/browser, saved style, steps and screenshots/logs on failure. No new mandatory human action.

---

# 20261122.14 targeted scope / 定向范围（2026-11-22）

Blank-line pagination is enabled by default for whole-page reading and saved with each manuscript. A normal newline stays in the current page; blank/ASCII-space/Tab rows start the next page; repeated separators create no empty pages. Long paragraphs split by capacity. Turning the switch off restores continuous capacity pages without changing source text. Scrolling preferences remain isolated and the last page still stops. Other Unicode whitespace is not promised as a separator.

Brightness is again a top-right shortcut/popover; alignment buttons regain 44px height/directional tooltips, divider spacing is balanced, toasts fit content, and sticky headers retain their upper inset. Twelve banner templates share a 48px single-line minimum, 7px/12px padding, 18px severity icons and 32px actions/progress area. Multiline content expands with centered actions; hidden actions consume no spare column. Generated JavaScript minifies function-local identifiers only, preserving shared global names and readable authored code; existing partitions remain unchanged.

The actual production C page builder passed blank/CRLF/capacity/off/empty/max-row/buffer-bound checks. Chromium/WebKit × virtual LAN/GATT passed paragraph pagination, anchor reflow, persistence/defaults, legacy page playback/stop and scrolling preference isolation. Both engines at 1366/390/320px in light/dark passed all 12 banner templates, hidden actions, spinner/100% progress, toast width, sticky inset, divider, alignment help and playback shortcuts/popovers/dialogs. Virtual GATT is not physical Bluetooth.

Internal firmware was installed healthy and actual LVGL/HTTP paragraph, CRLF, long-paragraph, switch persistence/reopening, orientation anchor and final-page stop checks passed. After the final banner-only CSS refinement, the final exact internal image was reinstalled and its embedded browser verified pagination/switch/brightness, toast/header/divider on three widths. Original manuscripts/styles/IDs/order/settings and brightness were restored; no unexpected restart observed. Passed unchanged logic was not unnecessarily rerun. The independent sanitized public image (6188672 bytes, SHA256 `f779012e2dc27c26e9a5640080586471c481d7711537095455946465bfb7f6da`) is built but NOT installed this round. Internal results do not establish public-image hardware acceptance. Physical Bluetooth for the new switch, phone/power/media/optical combinations are not newly validated; unaffected accepted baselines remain valid. Android paused; no coprocessor flashing.

中文：空行分页默认开且按稿保存，保留原稿；单换行不翻页、连续空行无空白页、长段落按容量拆页，关闭恢复连续分页。统一12种条幅高度/边距/图标/操作区，长文自然增高居中；Toast随文字、sticky留白保留，恢复亮度快捷入口与对齐tooltip。生产算法、双引擎虚拟LAN/GATT与布局、内部精确实装及嵌入页定向检查通过；公开独立BIN未实装，新开关物理蓝牙未重测，结果不可套用，不称全硬件验收。

Optional inspection: use a temporary two-paragraph manuscript separated by a blank line. Expect two pages when on; off uses capacity and survives reopening. Check repeated separators and a long paragraph, then final-page stop. Check narrow/light/dark banners, short toasts and sticky upper whitespace; open/restore brightness. Restore original manuscript/style/brightness after testing. Report exact firmware edition/version, transport/browser/viewport/theme, manuscript line breaks and steps/screenshots/logs if incorrect. No new mandatory human action. The public package includes this combined handoff; remote assets/manifests are verified separately during publication.

---

# 20261122.13 targeted scope / 定向范围（2026-11-22）

Paged reading always stops after the final page. Hidden saved scrolling loop/auto-next preferences are retained and apply again only after switching back to scrolling. The shared web countdown keeps one deadline per page, preventing late/duplicate packets from restoring elapsed seconds; finished documents show zero until replayed. Defaults are scrolling/cut/landscape/100% brightness/left alignment/zero margins/zero initial countdown, with sight guide/center highlight/both bars on, mirror/loop/auto-next off, 15 seconds per page, 100px/s, 0px letter spacing and 20px line spacing. Existing stored styles/device preferences are retained; new unstyled documents and explicit reset use the new product defaults.

The exact internal image was installed healthy. Actual device checks cover old saved loop/auto-next with one page at one second, continued stopped state, multi-page completion, pause/manual navigation/reflow, restored scrolling loop/auto-next, and new/reset defaults. The embedded browser page confirmed one-second playback reaches zero and remains stopped. Original manuscripts/styles/IDs/order/configuration were restored. The independently sanitized public BIN is separately compiled and NOT installed this round; these internal checks do not prove public-image hardware acceptance. Unaffected .12 UI checks remain baseline evidence, not new hardware tests. Android paused; no coprocessor flashing.

Shared Chromium/WebKit with virtual LAN/GATT covered defaults/new/reset/saved styles and page completion/isolation/navigation. The production timer model was exercised with late and duplicate packets, pause/resume, replay, reflow and page changes. Virtual GATT is not physical Bluetooth. Accepted optical/network/browser limitations remain; no claim of every hardware combination passing.

中文：整页末页必停，不执行隐藏的旧循环/自动下一篇；切回滚动恢复原配置。网页同页倒计时不被延迟状态拉回，停止显示0。默认滚动/直接切换/横屏/亮度100%/左对齐/边距0/倒计时0，视线框/中心高亮/上下栏开启，镜像/循环/自动下一篇关闭；每页15秒、100px/s、字间距0、行间距20px。旧稿自定义保留。内部精确BIN实装及定向检查已通过，独立公开BIN仅编译未安装，不能套用内部结果。

Optional review: create a disposable one-line manuscript, enable looping while scrolling, switch to paged mode, set 1 second and play. It must stop once at the end, show zero, and remain stopped. Returning to scrolling restores the saved loop behavior. Test a multi-page manuscript at 1 second per page, pause/resume and manual navigation. New unstyled/reset documents use the defaults above; already customized styles remain after switching documents. Remove only disposable drafts and restore personal settings; never format media for these checks. Report firmware/browser/transport, sequence, page/time/play state and screenshot/error. No new mandatory human observation.

---

# 20261122.12 targeted scope / 定向范围（2026-11-22）

Exact internal .12 was installed healthy and its embedded page passed 38 targeted checks at desktop/390/320px, including both themes, rich-dialog sizing, dividers, back arrows, browser layout memory and actual ordinary-sort persistence. The temporary sort was restored; manuscript text/identity/order were verified read-only, preserving another active controller's playback. The independent public .12 is compiled only, not installed; internal results do not establish public-image hardware acceptance. Shared playback geometry/tooltips also passed both engines after CSS whitespace compression. No new mandatory human test. Android paused, C6 untouched.

内部精确.12已实装healthy并核对实际嵌入页38项；独立公开.12本轮未实装，不套用内部结果。复用未受影响基线，未重复介质/关机/蓝牙测试；无新增必须人工项。

Changed shared UI passed Chromium/WebKit checks at 1366, 390 and 320px in both themes: rich-dialog width/viewport limits/long-record scrolling, compact confirmations, card dividers, transparent back-arrow hover/press and remembered layout across reload/new tabs/resize. Playback geometry and tooltip checks are rerun because embedded CSS whitespace is minified. Ordinary sorting remains device-persistent; layout is browser-local, defaulting to cards above 650px and lists at or below 650px only when no valid previous choice exists.

中文：内容丰富的配置/故障记录弹窗电脑端760px、手机端贴合可用宽度，长内容内部滚动；普通消息及确认保持紧凑。设备操作区、多人控制与后续选项增加分割线；返回箭头仅颜色反馈。卡片/列表各浏览器分别记忆，首次按650px断点选择默认；普通排序继续设备持久保存。真机结果与独立公开BIN的实装状态需分别记录，不把共享Demo结果称为硬件通过。

Optional inspection: open Settings and its field/diagnostic dialogs on desktop/mobile; confirm readable widths, reachable actions and no horizontal overflow. Hover/press back arrows and confirm only glyph colors change. Switch library layout, reload/open another tab; verify your choice remains. A fresh browser uses the appropriate default. Choose an ordinary sort, reload and verify it is retained; restore the previous sort after checking. No shutdown, formatting or manuscript modifications needed. Report firmware version/browser/viewport/theme/screenshot if incorrect.

---

# 20261122.11 targeted scope / 定向范围（2026-11-22）

This independently sanitized public BIN is compiled separately and not installed this round. The installed public .9 remains an accepted historical baseline, not installation evidence for .11. Changed shared UI checks cover Chromium/WebKit with virtual HTTP/GATT: page timing, selection exit, storage refresh, diagnostics export and settings, plus desktop/mobile light/dark layout, target orientation, single tooltips, numeric controls and wider configuration dialogs. Real internal-image checks are recorded separately. Unaffected accepted tests are reused. Android paused, C6 untouched; no claim of all hardware/optical combinations passing.

中文：本轮独立公开BIN单独构建，未实装；公开.9实装仅为历史基线。定向范围为整页计时、取消最后一项退出多选、重读SD不切换内置库、日志导出及设置；电脑/手机明暗主题核对首行左速度/阅读时间右字号、对齐在字间距上方、下方亮度、紧凑标题栏和圆形反馈、仅图标启用色、目标横竖屏图标、tooltip及大配置窗。内部真实设备结果另记，不称全部实物/光学验收。

Optional: use disposable manuscripts. Switch prompting mode and check speed/reading time stays left, font size stays right; alignment is above spacing, brightness is below. Open/fullscreen/scroll/close the mobile drawer: scrollbar stays inside its rounded edge, header actions fit, feedback circles stay square. Orientation shows the target direction. Hover playback actions, connection/current/next badges and multi-select for a single tooltip. Transition info sits beside its label; desktop header/footer configuration is wider than confirmations. Check page number then remaining time beside web state; hardware/canvas header remaining time and footer page count. Deselect the final item; normal actions return. While internal library is selected, SD reload must not switch it; explicit backend switches confirm. Export logs without acknowledging/clearing them. Remove only disposable drafts and restore personal settings; never format media for this check. Report version, transport, sequence and screenshot/error. No new mandatory human observation.

---

# 20261122.9 targeted results / 定向结果（2026-11-22）

This exact independently compiled public image was installed app-only and confirmed healthy. Targeted hardware checks passed manuscript create/read/save/delete, complete-line pages, per-page timer/pause/manual steps, last-page stop/loop/advance, orientation/mirror and scaled screen captures. The actual embedded controller passed editor-save-stays, save-before-prompting, both new-save choices, and a four-tab actual reboot/reload preserving paused manuscript/progress, settings, new draft and unsaved existing draft. Original manuscripts/styles/IDs/order/settings were restored, then the private installation was restored. This is targeted validation, not exhaustive optical/hardware acceptance.

Public BIN 6219840 bytes, SHA256 `3002dc8c16a23041457d473399b55aa5791cc606c44ca3bbd2832fdd7809b0f7`. The private image is a separate build: its GATT page/guarded-seek checks and earlier internal checks are supporting evidence, not substituted for this public image's tests. Chromium/WebKit ×LAN/virtualGATT passed editor choices/error drafts, page UI, access, tooltip/bubble/dialogs and per-tab recovery, including10 delayed preview-not-ready responses. Unaffected accepted baselines remain valid. Android testing paused; C6 untouched.

中文：本次精确公开独立BIN已实装healthy，真实稿件读写/删除、整页分页/计时/暂停/上下翻页/结束循环/下一篇、横竖镜像和缩放截图通过；真实嵌入网页保存留页、提词前保存、新建两选择及四标签重启/刷新恢复通过，未保存新稿与已有稿内容均保留并可继续保存。原稿/样式/ID/排序/设置恢复；不代表全部硬件或光学验收。真实GATT使用独立内部构建，不能冒充公开BIN的蓝牙实测。

## Changes and optional review / 修改与可选复查

- Editor toolbar Prompt saves dirty/new content first; existing Save stays; new Save waits for Start prompting or Home, Esc stays. Failed saves keep draft and route.
- Whole-page mode uses complete lines,1–600s per page (default15), cut or short upward slide. Pause freezes the clock; arrows change page without starting paused playback. Hide loop/auto-next entrances including shortcuts, retain saved behavior after the last page. Disable scroll-only controls while retaining their values.
- Reorganized status/preview/settings shortcuts, per-tab draft/route/paused-progress restoration that waits for preview layout and yields to other active controllers; tooltip deduplication and arrow layering; whole-item multi-select, scoped internal clear and network/storage confirmations; product version field synchronized to the device.
- Optional: create disposable drafts and check Save/Prompt/new-save choices; open playback/settings/new/existing editor tabs, leave unsaved edits, restart and reload, verify route/content/paused progress; check page timer/arrows, hidden/disabled irrelevant settings and mobile layout. Save personal edits first; do not format media. Close test tabs/remove only disposable drafts and restore personal settings afterwards.
- Report version, transport, action order, multi-tab state, errors/screenshots and whether draft content survived. No new mandatory human step. Occasional scrolling/loading/discovery delays and known browser/domain/deep-shutdown/logging limits remain; do not interpret these targeted results as full acceptance.

Build: install tools dependencies with `npm --prefix tools ci` before page generation. Whitespace-only esbuild compression retains names/syntax/legal comments; image packaging rejects an image that exceeds either unchanged OTA slot. Screenshots use virtual bilingual sample data, not actual private documents.

---

Private exact .8 hardware evidence: app-only USB installation verified healthy; original SD manuscripts/settings retained. HTTP editor-save/delete gates, real30s lease expiry and playback single/multiple ownership passed. Two actual embedded pages passed read-only/save-refresh/handover and aggregate presence; handshake-bound WebSocket viewer commands were refused. Actual BLE respected HTTP editor leases and handed over after HTTP exit; raw GATT saves/controls were denied to viewers; acquired BLE playback/shuttle/pause and inactive-shuttle refusal passed. Temporary manuscripts removed and original active manuscript/multiple-control preference restored; no unexpected restart. These private-image results do not certify the independently built public BIN. No new mandatory human acceptance step.

# 20261122.8 targeted acceptance / 定向验收

Host playback-state checks and Chromium/WebKit × HTTP/virtual GATT cover inactive shuttle rejection, editor occupancy, latest-text handover after save, exit/expiry/late-release safety, draft preservation, independent manuscripts and playback single/multi handover. Hover/press, semantic banner colors, switch hit targets/keyboard and slider release checks passed in both themes/engines. These virtual checks do not establish real BLE transport results. Exact private installation results are recorded separately; independent public .8 is not installed.

Optional review: open the same manuscript from two controllers; the second can preview/copy only. Save or leave on the first and observe the latest text become editable on the second. With multiple playback controllers off, the second playback page cannot operate; the first leaving/backgrounding transfers control while remaining viewers keep the display active. Enable multiple controllers and verify simultaneous operation; turn it off and close test pages afterwards. Keep original drafts/data; report version, transports, action order and errors if unexpected. Abnormal editor disconnect expires in30s, playback in10s. Reuse unaffected accepted baselines.

---

# 20261122.7 affected results / 定向核对

Chromium/WebKit screenshots at 390/650px in light/dark themes: rounded notices no longer leave a clipped rectangular shadow; multiline/stacked notices and exit hiding were also checked. Exact private .7 installed healthy, original library/settings retained and internal selection restored. Its embedded page displayed the same notice in both themes without a shadow or extra reboot. This CSS check showed the text directly; it did not repeat shutdown/media tests. Accepted .6 storage results and older unaffected baselines remain valid.

Independent public .7 is compiled only, not installed. Demo/App shared resources and bilingual virtual previews are synchronized; Android paused, C6 untouched. No new mandatory human test. Optional check: observe an ordinary transient notification's rounded border and clean underside, then its normal disappearance. Report version/theme/browser/screenshot if wrong; no need to shut down or format storage.

---

# 20261122.7 notification shadow / 通知阴影

Shared CSS removes the outer toast shadow that the scrolling stack cropped into a rectangular strip. Check light/dark at mobile/desktop widths, multiline and stacked notices, and entry/exit animations. Storage/notification categories/device behavior are unchanged; accepted .6 and older unaffected results remain valid. Exact current results will be recorded after completion. Public .7 is independently compiled, not installed; never reuse private hardware results for it. Android paused, C6 untouched, no new mandatory human test.

---

# 20261122.6 affected validation / 定向回归

Chromium and WebKit each passed 32 viewport/theme/language combinations, bidirectional virtual library switching, current-library-only capacity, canceled active-manuscript switching, recovery-entry visibility and both configuration dialogs. The exact private .6 image was installed and passed real SD/internal switching, capacity visibility, responsive geometry and dialogs, retaining both original lists and settings; the pre-flash internal selection was restored. Only boot/revision/client-count dynamic response fields are excluded from settings comparison. No formatting, media-removal or power-cut test was repeated.

Public independent .6 is compiled only, not installed. Private results do not validate this separate public image. Existing accepted unaffected baselines continue. Android testing paused; C6 untouched. Demo/App shared assets and English/Chinese virtual previews synchronized. No new mandatory human observation.

Optional reproduction: Settings → SD card → switch to internal → verify only internal capacity plus “Switch to SD card” → switch to SD → verify only SD capacity and original SD list. Active manuscripts require confirmation and stop; no files are copied/merged. Restore your original selection. Open/close header/status configuration dialogs without saving. Report version, active library, physical SD state, screenshots and console errors if any; do not format to troubleshoot.

中文：内部精确版实装定向回归通过，两库及配置保留；公开独立版未安装。原SD开机优先规则保留。无新增必须人工测试，不重复未受影响项。

---

# Test and handoff / 测试与交接 — 20261122.6

The SD/internal manuscript-library switch stays available in both directions. Only active-library capacity is displayed; SD physical status remains separate. Field-picker buttons explicitly say “Configure”. Two libraries remain independent, no file copying/overwriting; restart selection stays unchanged. Affected browser/installed results are recorded when complete. Public independent .6 is compiled, not installed. Preserve originals/settings; no new media/power-cut tests, Android paused, C6 untouched.

中文：仅回归双向切库、当前库容量、无卡恢复入口、取消确认及响应布局/翻译/设置弹窗；保留未受影响基线，无新增必须人工测试。

---

# Test and handoff / 测试与交接 — 20261122.5

Download hash verification, transfer initialization and device final verification show indeterminate banner spinners instead of stale 100%. Local stages clear on success/failure and leave native OTA state unchanged. Retain .4 banner/countdown and unaffected accepted results. Chromium/WebKit upload initialization, final verification, success/failure cleanup and lifecycle regressions passed; online SHA verification and corrupt rejection passed. Internal .5 was USB-installed and passed embedded-page wait-stage/stale-progress isolation and real startup circular progress with originals preserved; unaffected .4 layout baselines remain valid; publication is separately verified; public independent .5 is not installed. Android paused, C6 untouched, no new mandatory physical tests.

中文：准备、下载后 SHA 校验、设备最终校验等待用加载圈；成功失败均清理本地阶段。其余沿用 .4 通知规则和有效基线，不重复介质测试。

---

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
