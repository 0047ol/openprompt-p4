# Test and handoff / 测试与交接 — 20261121.1

2026-11-21. Independently built public BIN: 6166704 bytes, SHA-256 `5db52a4140bd49c04f4af68d9454659ac9c2312657a1f2a3155b990ad44b1e72`. **Compiled only; this public BIN has not been installed or functionally tested.** Exact private-image hardware results are context, not acceptance of the public binary. Verify the ZIP manifest; only the main application BIN is for OTA/SD updates. Preserve manuscripts/settings, do not flash C6, and do not format an unknown card/data partition. Android is excluded.

## Changes / 修改

- Preserve confirmed loop length while the matching preview layout is pending, including a loop seam. Genuine script/boot/stop resets remain. The old production model reproduced an unwrapped 1170-to-370 backward jump; the new regression passes.
- Put higher-priority HTTP/WebSocket handling on core 0, separate from the display on core 1. Calculate periodic playback bar metadata during immutable text DMA copies; low-speed, stopped and geometry-change states retain correct updates.
- Keep CPU rotation, 32/64-row preparation and panel timing. Withdraw the unsuccessful pixel-copy experiment. Regenerate embedded/hosted/Demo resources from shared source; native application projects are excluded.

## Evidence and limits / 结果边界

The exact internal image was application-only USB installed and checked. Four short/long, hidden/visible-bar profiles completed and restored the original configuration. Short hidden-bar playback improved from about50.5fps/2.7ms jitter to51.5fps/0.03ms with zero sampled late frames; other profiles were mixed. **Long text, visible bars and optical motion are not resolved or accepted.** Guide transitions still sometimes have about39ms gaps. These are software measurements, not physical smoothness proof.

A separate48-second long-script run and four highlight changes had no unexpected reboot/pause; all six footer fields remained visible. Real landscape/portrait captures, minimum legal speed10 timecode, pause/stop/zero-position and six reader/guide returns passed. The initial test accidentally sent illegal speed1 and was correctly rejected; it was corrected and rerun, not counted as a firmware failure or pass. Temporary manuscripts and original settings were restored.

Internal LAN cold library readiness was2434ms; warm841ms. Twenty single-page small APIs had median11ms/max101ms; two-page median14ms/max95ms. First preview with internet was830ms, cached424ms; blocked-internet local fallback3095ms, cached224ms. Eight required exact-font shards were loaded; none on the library page. These samples do not validate every router/mobile/hotspot or establish general cold-load acceleration. The corrected offline fixture preserves HTTP cache; request-routing fixtures which disable cache were discarded for warm-load conclusions.

Chromium/WebKit, light/dark and two virtual controller transports completed eight Demo workflow combinations. Production motion/font-window/resilience and DMA publication/queue/damage/cache checks passed. A scalar pixel oracle checks160 production transform combinations. Public screenshots are14 virtual-data illustrations, not hardware photographs. No claim of all-feature/all-transport testing; BLE/AP/OTA, interrupted/full storage and optical smoothness need separate acceptance. Historical storage/exFAT/field exclusivity checks from prior versions do not become this version's results.

## Acceptance / 待测步骤

Back up originals; record exact version/hash, browser/OS, transport, timestamps and expected/actual outcomes. Only use temporary manuscripts, delete only your own data and restore every changed preference afterwards.

| Item | Steps | Expected and feedback |
| --- | --- | --- |
| 1 | Install the public main BIN; verify healthy/version; save/read a temporary Chinese script with style and reboot | Exact version/hash, preserved text/style/ID and settings. A manifest download or private USB check is not public-image/OTA acceptance |
| 2 | Loop short and near8KB scripts on phone/PC for5–10minutes; change highlight/bar preferences and observe delayed layout; pause/stop/select another script | No preview bounce, correct seam, immediate zero on stop, stable pause and no stale previous script. Capture a video and packet timing on failure |
| 3 | Observe real landscape/portrait motion, normal/highlight and hidden/visible bars; six reader/guide returns and connecting spinner | No visible jumps/pulses or dropped transitions. **Still pending**; software counters/static snapshots are insufficient. Report speed/font/scenario and a timed video |
| 4 | Cold/warm open over hotspot and router LAN, with/without internet; one/two pages; phone background/lock then return | Exact font fallback/cache, responsive API, coherent recovery. Record TTFB/download/first frame and maximum API latency; desktop LAN numbers are not phone/AP acceptance |
| 5 | Select six long/large footer parameters, date/clock/timecode and previous/next; lowest speed10, pause, stop, rotation/style changes | All selected footer fields retained, readable and aligned, updated at low speed; explicit stop resets. Header and adjacent-title groups remain exclusive; restore preferences |
| 6 | Fresh public BLE connect/reconnect/background; actual local/online/BLE/SD installation routes separately | No false connect/infinite wait, correct progress/recovery; separate evidence per path. Not rerun for this private image; record version/hash and complete logs |
| 7 | Backed-up exFAT/FAT32, physical no-card startup, same-title files in independent libraries, stale editor after library switch; expendable full/interrupted-write fixture only | Preserved originals and separation, internal fallback where safe, stale save refused; never erase unknown occupied data. Physical storage edge cases remain pending |

## Device captures / 真机截图

`python tools/capture-device-screen.py --url http://DEVICE_IP --output screen.png` captures the completed display frame. PNG needs Pillow; BMP needs only Python. Default scale2 is about512KB, native scale1 about2MB, quarter scale4 about128KB. Existing output files are refused. Captures do not wake sleep or count as control presence; one background transfer at a time. Do not continuously capture while evaluating motion. Frames may contain private manuscript/network data; review before sharing. Static captures cannot measure optical flicker/backlight/color or prove smoothness.

中文：公开BIN独立编译未安装，不套用内部通过。本版修复循环预览延迟排版回跳并隔离HTTP/显示任务；短稿隐藏栏有明确提升，长稿及显示栏仍有晚帧，引导偶发约39ms间隔，冷开和手机/热点场景继续验收。内部48秒高亮/六项栏、横竖实屏截图、低速时间码、暂停/停止及6轮引导往返通过，原配置与临时稿已恢复。按上表提供版本/哈希、场景、步骤、时间与日志/视频；不用另发复核文档。仅主应用BIN用于OTA/SD，Android不测、不刷C6、不盲目格式化。
