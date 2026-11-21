# Test and handoff / 测试与交接 — 20261121.3

2026-11-21. **The independently built public BIN is compiled only, not installed or functionally tested.** Size/hash are in the release manifest. Internal results describe equivalent code paths, not acceptance of the public binary. Only the main application BIN is for OTA/SD. Preserve manuscripts/settings; do not flash C6 or format unknown storage. Native Android testing remains paused.

## Changes / 修改

- Fix mobile settings action-row width and left/right button alignment.
- At ordinary speeds, defer look-ahead text preparation when a frame has playback-bar calculation or chrome repaint work. Preserve pinned visible tiles, 32/64-row preparation, high-speed scheduling and panel timing.
- Correct Chinese/English Previous/Next, Stop and current-library help. Manual navigation stops; natural automatic advance may start playback.
- Retain unaffected passed baselines. Test changes and their direct/indirect impact; supplement uncovered paths instead of repeating every test for each version.

## Coverage and limits / 覆盖与边界

The prior shared UI audit covered 186 Chromium/WebKit language/theme/viewport/page combinations, including 21 hardware gallery scenes. Library queue/motion, playback drawer, feedback, network, selection and required-update modal checks remain valid outside this change's impact. The mobile action fix passed 32 combinations across both engines, languages/themes and 320/390/650/1440 widths. Navigation/storage help passed both engines and languages. Virtual screenshots do not establish physical panel smoothness or actual phone compatibility.

Supplemental internal checks passed five native BLE discoveries/reads, five fresh HTTPS Web Bluetooth contexts, Chinese manuscript read/write, bold persistence, BLE plus two LAN controllers and restart reconnect. Web readiness ranged about8.3–14.0seconds; an earlier12-second discovery failure remains recorded, not erased by successful retries. Actual 29-manuscript pagination/full-text search and countdown/shuttle/pause/end/loop behavior passed. ExFAT SD boot installation and retained same-version reboot checks passed on the internal `.2` image. Final internal installation and targeted motion/AP results are recorded in the private combined handoff; they do not validate this public BIN.

Retain earlier actual SD/internal-library CRUD, style/ID/order isolation and reboot persistence; header/footer exclusivity and six-item enforcement; sleeping-screen black capture without wake; DHCP/static-IP validation; and real LAN/BLE/online installation baselines where their paths are unaffected. Current exFAT SD update behavior is separately supplemented because filesystem integration changed. A manifest, mock response or USB verification does not prove an OTA path.

**Not all-feature stable acceptance:** occasional physical scrolling/guide stalls, intermittent BLE discovery, physical no-card/FAT32/full/interrupted-storage fixtures, complete physical factory reset and phone lock/background behavior remain open or need dedicated hardware. Six extreme long footer fields fit by shrinking text as far as10pixels; optical readability is not proven. mDNS varies across phones; use the displayed IP when needed.

## Remaining acceptance / 待测流程

Back up originals. Record exact version/hash, browser/OS, transport and timestamps. Use disposable manuscripts, delete only your test data and restore changed settings. Do not repeat an unaffected passed baseline solely because of a new version.

| Item | Steps | Expected / feedback |
| --- | --- | --- |
| Public binary | Install this public application image; verify healthy/version; save/read a temporary Chinese manuscript and style, then reboot | Correct public hash/version; preserved text/style/ID/order and settings; keep installation logs |
| Motion | Real landscape/portrait, short/near8KB, bars/highlight on/off; reader-to-guide transition and Wi-Fi loading circle | No visible backward jumps or stalls; record speed, font, video and time. Static captures or average FPS do not establish smoothness |
| Phone lifecycle | Actual supported phone browser; lock screen and background during use, then return; disconnect/reconnect | Reconnect and retain valid state without overwriting another controller; record OS/browser and elapsed times |
| Dedicated storage | Backed-up FAT32; shutdown then physical no-card boot; expendable fixtures only for full/format/write-interruption | Safe internal fallback, persistent text and independent libraries; preserve original card and unknown flash |
| Power/reset | Shutdown then physical RST; restart/USB re-enumeration; factory reset only after separate configuration backup | Healthy recovery without cyan flash or manuscript deletion; reset clears configuration only; restore backup |
| Extreme bars | Six long fields, both orientations; date/clock/timecode and previous/next exclusivity | All selected fields present; report unreadable size and clipping; restore settings |

## Device capture / 真机截图

`python tools/capture-device-screen.py --url http://DEVICE_IP --output screen.png` captures the completed screen without waking it or renewing presence. PNG needs Pillow; BMP uses Python only. Scale2 is default, scale4 smaller, scale1 native. Do not continuously capture during motion sampling. Review private network/manuscript contents before sharing; release images use virtual data.

中文：公开版独立编译未安装，不能套用内部实测。已通过且未受影响的结果继续保留，补缺口并只回归改动相关项。真实BLE、分页、播放控制和exFAT启动更新已补相应内部路径；偶发扫描失败、肉眼滚动／引导卡顿及手机／专用存储／物理电源边界没有全部验收。固件ZIP内这一份合并说明即可交接，不另要求复核记录。
