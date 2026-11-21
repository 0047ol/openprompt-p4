# Changes / 修改记录

## 20261121.1 — 2026-11-21

- Preserve the last confirmed loop length while a matching preview layout is pending. Previously a status packet could discard all completed-loop distance, moving the preview backwards; receiving the layout then moved it forward again. Keep genuine manuscript, boot, stop/rewind and loop-mode resets. Add a production-model regression covering delayed layout across a loop seam.
- Calculate periodic playback bar metrics while DMA copies pinned immutable text tiles, instead of using the short interval before frame composition. LVGL draws the changed labels in the next UI iteration; stopped, guide and geometry-change states still update immediately, and low-speed frames cannot starve metrics. The physical scrolling issue remains subject to hardware measurements and optical acceptance.
- Pin HTTP/WebSocket request handling to core 0, keeping its higher-priority callbacks off the display task on core 1. Preserve API behavior, controller concurrency and panel timing; compare API latency and frame intervals on the exact installed image.
- Retain the verified CPU tile transform, 32/64-row prefetch, display timing and framebuffer ownership contract. An experimental pixel-copy change did not demonstrate a repeatable benefit and was withdrawn.
- Add a scalar-oracle check of the production tile transform for partial/full rows, padded strides, all rotations/mirrors and highlight surfaces. Browser/host checks do not replace real panel observations.
- Keep embedded, hosted and virtual Demo pages generated from the same preview source. Published public firmware is independently compiled and has its own hash; internal-image results do not validate the public BIN.

## 20261120.18 — 2026-11-21

- Add an on-demand completed-panel-buffer BMP endpoint and a PNG/BMP download helper. UI-thread snapshot ownership survives timeout/cancellation, one background HTTP transfer is permitted at a time, orientation is preserved, and sleeping panels are not awakened. Default half-size and optional native/quarter-size images avoid mandatory full-frame downloads. Do not publish private screenshots or infer optical smoothness from static frames.
- Fix the real header playback/pause/stop indicator position: its right-aligned x value is an offset, not an absolute coordinate. Real-device captures exposed the icon being outside the panel while the canvas Demo was correct.
- Replace repeated guide glyph/QR redraws with a DMA translation of the completed content rectangle, clear only exposed strips, and use a complete 320 ms smoothstep transition. Guide fonts are independent of automatically resized footer fonts. Physical motion acceptance remains separate.
- Make ordinary automatic SD and online update policies mutually exclusive, including saved-setting normalization and backend rejection of conflicting requests. Manual and mandatory update policies remain available.
- Choose one of date/clock/playback timecode for the upper-left header, and one of previous/next manuscript. Display the loop label as “已循环：” / “Loops:”. Keep all six selected footer parameters on one line by reducing the effective footer font when needed; explain the behavior beside the field group.
- Move the internal-library switch beside its current-library status and keep mobile automatic sleep/shutdown fields side by side. Shared controller, embedded resources and hardware gallery use the same choices and descriptions.
- Public binaries are independently built. Exact internal hardware checks and remaining acceptance items are recorded in the combined handoff; historical results are not public-image acceptance.

## 20261120.15 — 2026-11-20

- Move the guide QR and text together after the first frame has been submitted and the startup/notice overlay has been removed. Use a 320 ms translation without a large alpha layer; keep Demo motion consistent. This closes startup paths that could consume an animation while hidden; physical acceptance remains separate.
- Turn off and hold the backlight before software reset or deep sleep, configure its low output at the start of boot, and fade normal restart to black before resetting. This targets a reported cyan/green flash; hardware observation is still required.
- Include the exFAT and independent internal-storage/capacity changes from .13/.14. Public firmware is independently built and requires its own validation. Real scrolling jitter and slow cold page loading remain open.

## 20261120.14 — 2026-11-20

- Enable SDK FatFs exFAT in a project-local component override, with consistent VFS headers and no global SDK mutation. Retain FAT12/16/32 and never automatically format SD.
- Provide a diagnostic-only card backup build that blocks writes and discard/erase at the SD disk driver. Release packaging refuses that build; private backups are excluded.
- Start guide entry after layout, defer hidden reader tile preparation, and keep an existing loading spinner running instead of restarting it.
- Add internal capacity/usage alongside the existing SD meter, explain reserved write space, and avoid inventing percentages for unavailable readings. Keep Demo and embedded/public resources shared.
- Targeted internal hardware results are recorded separately. Public firmware remains independently built and unvalidated on hardware. Visible scrolling jitter and slow cold page loading are still open.

## 20261120.13 — 2026-11-20

- Recognise only a byte-exact pristine empty legacy SPIFFS image after scanning the complete partition. This repairs the empty 3 MiB factory image/current 3.9375 MiB geometry mismatch. Unknown, occupied, damaged and partially scanned images remain protected; SD is never automatically formatted.
- Cache unsuccessful probes; explicitly retry storage, preserve a working internal library after failed SD rereads, and expose ownership/recovery diagnostics.
- Add bounded, low-speed read-only SD boot-sector diagnosis and an explicit bilingual unsupported-exFAT notice. The inspected card is exFAT and readable at sector level; this does not diagnose all historical card I/O errors. SD support remains FAT12/16/32.
- Include the read-only storage backup helper and a portable strict-recogniser test. Private backups and hardware logs are excluded from the public export.
- Internal development firmware has preliminary on-device CRUD/playback/persistence checks. Exact final internal results are recorded in the private handoff; the independently built public firmware requires separate hardware acceptance.

## 20261120.12 — 2026-11-20

- Add an independent persistent internal manuscript library when SD storage cannot mount or fails at runtime. Retain manuscript transactions, identity, style and ordering; never retry a failed write on a different backend.
- Use only the existing data partition. Initialise it only when completely erased; preserve unknown contents and all SD data. Reserve filesystem/transaction headroom and report usable capacity and full-storage errors.
- Reprobe SD through a fresh mount; let users explicitly switch libraries. Reject stale cross-library operations, retain editor drafts and support internal storage through HTTP/BLE and the virtual Demo.
- Guard metadata rereads against card I/O failure, keep physical-SD firmware updates separate, and keep pure playback controls independent of card availability.
- Compiled only; SD root cause, internal persistence/recovery and hardware acceptance remain pending. New external evidence reports FatFs 13 before/after replacement and FatFs 1 after formatting; this does not prove physical card damage. The replacement is reported as exFAT or another format; current SD support is FAT12/16/32, with exFAT disabled.

## 20261120.11 — 2026-11-20

- Remove the standalone Wi-Fi failure screen. Failed Wi-Fi setup returns directly to the hotspot connection step, then advances to the hotspot controller address/QR when a phone joins, even while router connection retries continue.
- Remove the failed-screen Demo option and redirect its legacy scene bookmark to the hotspot guide. Shared Chinese/English guide text and device rendering stay in sync.
- Preserve Wi-Fi credentials and retry behavior. No playback, manuscript, Bluetooth or update implementation changes. Compiled only; targeted failure/join/reconfigure checks remain pending, and .10 results do not validate this new image.

## 20261120.10 — 2026-11-20

- Cancel pending manuscript copy/delete jobs after an earlier job fails, before further writes reach the device. Existing originals and completed operations are retained.
- Route native App back through the same confirmation policy as Escape; required-update prompts cannot be dismissed that way. An unexpected dismissal also returns to the required retry flow while the same device remains connected.
- Keep factory reset and save actions on one row at narrow English mobile widths, with labels wrapping inside their respective buttons.
- Restart BLE advertisements on separate NimBLE host events, renew finite advertising leases and restore the fast discovery window after disconnection without terminating established GATT links. Rediscovery still requires hardware verification.
- Keep header fields whole at large sizes instead of truncating loop counts before their values. Prioritize playback status over a timecode that cannot fit; shared web/device previews follow the same policy.
- Incorporated external .9 regression evidence into the handoff. The new .10 build is compiled only; no new hardware or automated functional test was run. Physical jitter, mixed-load latency and BLE rediscovery remain pending.

## Deployment follow-up — 2026-11-20

- Fetch published firmware through the public Releases REST API and verify its size, checksum, embedded version and tag before Pages deployment. A missing release now fails deployment instead of silently publishing an empty firmware directory.
- Permit version tags in the GitHub Pages environment. Firmware bytes and the existing release tag remain unchanged.

## 20261120.9 — 2026-11-20

- Initial sanitized public release as OpenPrompt P4, combining P4 firmware and the web controller. Native applications, device data, logs, credentials and private history are excluded.
- Public deployment endpoints replace private infrastructure; legacy deployment-specific factory password migration is omitted.
- Required-update flag in the release manifest, a non-dismissible update dialog, retry flow and device-connection scoping.
- Mobile factory-reset/save actions align to opposite sides. Loop counts use an explicit label and colon. A larger red playback indicator alternates bright/dim.
- Copy operations show a distinct duplicate placeholder, preserving the original and ordered mutation queue.
- Chinese/English README, shared-screen preview images, third-party notices and release/test instructions.
- Compiled only. Hardware jitter, networking and update regression remain pending; no hardware test pass is claimed.

When distributing modifications, add the date, changed areas and a description here and retain the required license notices. / 分发修改版时，请补充修改日期、修改范围与具体内容，保留协议要求的声明。
