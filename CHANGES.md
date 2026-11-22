## 20261122.21 — 2026-11-22

- Keep Wi-Fi and remote card search icons static and accessible during scanning. Show scan activity on the dialog refresh control. Explain an already-connected remote instead of silently disabling the card entry.
- Cancel the originating device scan when discovery closes, receives a back/Escape event, or the page changes. Token-check cancellations, discard stale replies, and handle immediate reopening without leaving busy controls behind. Closing remote discovery restores its previous reconnect policy.
- Start other pages at their own top while restoring the library scroll position on return. Keep the editor layout stable across mobile keyboard resize and blur; truncate long editor and prompting headings to one line.
- Show actionable SD recovery advice in the persistent storage notice and storage card. Offer explicitly confirmed formatting when the card can be read but its filesystem cannot mount, retain internal-library isolation, and distinguish I/O faults from format recovery. Never format automatically.
- Pack embedded font-index codepoints as UTF-8 and restore the exact arrays in the loader. Glyph files, repertoire, shard ordering and external manifests remain unchanged; both original OTA slots are retained.

---

## 20261122.20 — 2026-11-22

- Preserve the active Bluetooth page after an update when the browser lacks remembered-device discovery; show an explicit refresh banner instead of silently losing the connection. Capable browsers still refresh automatically, and unsaved drafts remain protected.
- Serialize complete Bluetooth transactions between same-origin browser tabs. Add device-side reservations with explicit client identities, bounded expiry and token-checked fragments so separate origins sharing a Windows BLE connection cannot mix uploads or responses.
- Keep reader heartbeats independent of manuscript transfers. Preserve single-owner playback and settings permissions when another page remains in device settings; release abandoned reservations without resetting the device.
- Retain compatibility with older firmware and legacy clients. Retry only GATT operations explicitly rejected as already in progress, and reject queued work after a device/connection change.
- Use operation-specific confirmation errors: only SD formatting warns about erasing the whole card. Include production transaction-gate host tests; preserve valid installation, media, playback and power baselines.

---

## 20261122.17 — 2026-11-22

- Move remote rediscovery to a header refresh icon, show immediate connecting/pairing/HID progress, close after ready, and render a compact battery glyph. Show measured RSSI and signal bars in remote and Wi-Fi discovery.
- Add asynchronous 2.4 GHz Wi-Fi discovery, security labels, encrypted-network password entry, direct open-network connection, real connection/DHCP phases, success closing and failure retry. Retain the existing network handoff without a device reboot; unsupported legacy/enterprise authentication stays identified and disabled.
- Reduce the single/double click window to 250 ms and ready-state timer interval to 20 ms. Dispatch single clicks immediately when double click is mapped to no action; retain debouncing and one action per click group.
- Embed only offline font metadata used by the loader; keep all glyph coverage, shard ordering and font bytes unchanged, and retain complete external manifests. Existing OTA partitions and stored manuscripts/settings remain unchanged.

---

## 20261122.16 — 2026-11-22

- Add native BLE HID remote discovery, pairing, persistent bonding, reconnect, bounded Consumer Control report parsing and debounced single/double click mappings. Default single toggles playback; double advances a page or selects the next manuscript paused. Idle remote links do not hold a reader session; maintenance/update/power gates remain enforced.
- Add the shared remote settings card with consistent custom dropdowns, battery display and once-per-connection/low-interval warning at20% (rearmed above25%). Hide stale battery values after disconnect.
- Fix offline bar-field dialog entries and arrange remote status/options/mappings/actions as complete rows. Reuse the same custom-select component as playback controls.
- Retain the intermediate .15 default-parameter changes and existing saved preferences; minify generated JavaScript syntax while preserving shared top-level names and both existing OTA partitions. Public independent image remains separately built and uninstalled this round.

---

## 20261122.15 — 2026-11-22

- Enable header date and time together by default; keep timecode mutually exclusive and off by default. Set letter spacing to 5 px and initial countdown to 3 seconds across firmware, controller, restore-defaults and shared previews.
- Preserve saved manuscript styles and device preferences; Bluetooth remote identification is research only in this release.

---

## 20261122.14 — 2026-11-22

- Add per-document blank-line pagination, enabled by default in whole-page mode, with matching device/web layout. Preserve original manuscript text; skip repeated blank separators and split long paragraphs by screen capacity. Retain continuous pagination when disabled.
- Restore the brightness shortcut/popover, alignment button size/directional help, and balanced divider spacing.
- Keep sticky-header top whitespace when scrolling and size transient toasts to their content. Unify all 12 persistent banner templates, including network-address recovery: 48px single-line height, 7px/12px padding, 18px severity icons and 32px action/progress area; multiline content grows with centered actions and hidden actions consume no column.
- Minify function-local JavaScript names in generated payloads while retaining top-level shared entry points and readable authored source, keeping both existing OTA partitions unchanged.

---

## 20261122.13 — 2026-11-22

- Stop paged reading after its final interval even when old scrolling loop/auto-next preferences are saved; preserve those preferences for returning to scrolling.
- Keep a monotonic web deadline for each page so delayed status packets cannot restore elapsed seconds. Show zero after automatic completion.
- Apply consistent product defaults to firmware, controller forms and Demo without overwriting existing saved document/device preferences. Fix the default-form maintenance helper so it cannot re-enable looping.
- Add one-second/single-page, saved-configuration isolation, pause/navigation/reflow and default/reset regressions. Actual internal-image device/browser checks pass; independently built public image is not installed this round.

---

## 20261122.12 — 2026-11-22

- Use a shared 760px desktop layout for content-rich configuration and diagnostics dialogs, fit mobile viewports and scroll long records internally; keep ordinary information and confirmations compact.
- Separate device actions and multiple-controller settings with subtle dividers. Back arrows change glyph color on hover/press without circular backgrounds.
- Remember library card/list layout in each browser; first visits default to cards on desktop and lists on narrow mobile screens. Ordinary sorting remains a persistent device preference, preserving playback order across controllers.
- Minify CSS whitespace for embedded payload size while retaining authored source. Preserve existing partitions and manuscripts; synchronize shared Demo/App resources.

---

## 20261122.11 — 2026-11-22

- Put mode-specific scroll speed / per-page reading time on the left and font size on the right; move alignment above letter spacing and brightness below the parameter groups.
- Keep playback state and compact actions on one row where space permits, clip drawer scrollbars within rounded corners, and preserve circular hover/press feedback without filled selected shortcut backgrounds.
- Use the target orientation icon, restore single tooltips for playback actions, connection/manuscript badges and multi-select, align transition help with its label and widen field configuration dialogs on desktop.
- Show web page number then remaining time beside playback state; hardware/canvas show remaining time in the header and page count in the footer. Explain page transitions and rename the short slide to Scroll transition; provide desktop numeric speed/brightness inputs.
- Exit multi-selection on final deselection. Separate SD refresh from explicit library switching to preserve the active internal manuscript. Restore safe structured log export on the developer card and use unfilled left dialog actions.
- Defer preview size observer adjustments to the next frame. Shared Demo/native resources synchronized; Android testing paused. Public BIN is independently built and not installed this round; accepted .9 public installation remains historical evidence.

---

## 20261122.9 — 2026-11-22

- Add timed whole-page prompting with complete-line pagination, pause-aware device clock, manual page steps and optional short slide. Hide loop/auto-next shortcuts in page mode, retaining their saved end-of-manuscript behavior; retain scroll-only preferences while disabling incompatible controls.
- Reorganize playback status and icon shortcuts. Add independent per-tab route/draft/paused-progress recovery after restart/reload; retry preview layout readiness and yield to another controller's activity.
- Keep existing manuscript Save in the editor; offer Start prompting or Home after a new Save. Add editor Prompt that saves changes first; preserve drafts on failure.
- Deduplicate tooltips, align bubble arrows/top-layer controls and multiple-control row, enforce whole-item selection, add scoped storage/network confirmations and dialog resets; synchronize product/version fields to hardware.
- Add whitespace-only esbuild page compression and enforce original OTA slot limits in packaging. Preserve partitions/data/C6. Exact independent public BIN installed and passed targeted HTTP/embedded-browser checks; see TEST-PLAN for evidence and retained limits. Android testing paused.

---

## 20261122.8 — 2026-11-22

- Add bounded per-manuscript editing leases shared by HTTP/BLE. Other pages remain read-only; successful save/exit releases the lease and viewers fetch the latest text before editing. Preserve disconnected drafts and ETag conflict checks.
- Rename the chrome settings card to Playback controls; add persistent opt-in multiple-controller operation, off by default. The first foreground playback page controls; other viewers take over on exit/expiry. Enforce in HTTP, WebSocket and BLE handlers as well as the UI.
- Reject inactive/countdown shuttle commands and mixtures before any mutation. Fix semantic banner buttons, mouse hover, switch hit targets/press, slider values/release and equal footer controls.
- Use size optimization for request/storage handlers to retain both original OTA slots; display hot paths remain optimized for speed. Do not change partitions.
- Affected host/browser checks passed; exact private installation/publication results are recorded separately. Independent public .8 is not installed. Preserve manuscripts, preferences and media; native app testing paused.

---

## 20261122.7 — 2026-11-22

- Remove the large outer shadow from floating notifications: the scrolling container cropped it into a rectangular gray strip beneath rounded notices. Retain the theme-colored border and rounded shape, matching persistent banners.
- Shared CSS only; the .6 storage switching, active-library capacity and configuration labels remain. Notification categories and device logic are unchanged.
- Review light/dark themes, desktop/mobile widths and entering/leaving notifications. Reuse unaffected accepted baselines. Independent public .7 is not installed.

---

## 20261122.6 — 2026-11-22

- Keep the storage selector visible in both directions. The active internal library offers “Switch to SD card”; the SD library offers “Switch to internal storage”. Use the existing isolated library-switch operations without merging or overwriting manuscripts.
- Display only the active library's total, used, available capacity and usage bar. Retain the SD physical status separately.
- Rename field-picker buttons to “Configure header information” and “Configure status bar information”, including live language switching.
- Validate affected layout, capacity visibility, library roundtrip, unavailable recovery and cancellation. Unaffected accepted baselines remain valid. Public independent .6 is not installed.

---

## 20261122.5 — 2026-11-22

- Show indeterminate banner spinners while checking the downloaded SHA, waiting for transfer initialization, and finalizing the device image. Clear local stages on success or failure; never retain the old transfer percentage during verification.
- Retain .4 banner severity, silent playback/countdown and unaffected accepted baselines. Test affected LAN/BLE transfer, hash-rejection and local-stage cleanup. Independent public .5 is not installed.

---

## 20261122.4 — 2026-11-22

- Keep SD scanning/verification indeterminate even when a previous upload left byte counters. Never reuse startup progress for a higher-priority firmware/reset task.
- Retain .3 task banners, severity icons, centered circular progress and silent countdown behavior. Run affected progress-source regression; retain unaffected accepted baselines. Public independent .4 is not installed.

---

## 20261122.3 — 2026-11-22

- Remove the countdown-start notification, including repeated feedback when changing a control during countdown. Normal playback state remains in its own status UI.
- Show lifecycle/update tasks in banners, using the right action slot for a spinner or a circular progress meter with centered percentage. Only show real known progress; reconnect/reset without a total stays indeterminate.
- Give every banner an information, warning or error icon and matching severity color, including settings recovery/network banners.
- Verify shared Chromium/WebKit responsive geometry, lifecycle and update reconnect behavior; retain unaffected accepted native baselines. Public independent .3 is not installed; native App testing remains paused.

---

## 20261122.2 — 2026-11-22

- Keep each help icon beside its label, reusing explicit help rather than stacking icons on an update switch. Give developer summaries the full card width.
- Style unavailable range, color and numeric inputs visibly; retain the active appearance of brief pending locks to avoid flicker.
- Route shutdown/startup/restart/reset/update progress through one task-bound Toast overlay. Keep stable off/recovery states in compact banners without extra headings, and preserve necessary actions. Do not reopen expired Toasts during polling.
- Replace the editor-conflict error Toast with a persistent reload action while retaining the local unsaved draft and discard confirmation.
- Inventory all notification and confirmation call sites. Verify shared Chromium/WebKit layouts, LAN/virtual BLE power feedback, progress/expiry, reconnect, diagnostics and update integrity; retain unrelated accepted hardware baselines. Native App testing remains paused.

---

## 20261122.1 — 2026-11-22

- Keep persistent and actionable notices in compact in-page banners, with wrapped text and a right-centered action. Reserve Toast for transient feedback. Use one shutdown banner with remote-start progress for soft-off; show physical power instructions only for a confirmed deep shutdown. Ordinary connectivity loss remains unknown.
- Limit the abnormal-restart banner to the immediate recovery boot, retaining fault history across normal restarts. Show only one close/acknowledgement action; move fault history into Developer Settings. Report SD I/O fallback persistently without claiming immediate physical card detection.
- Preserve consistent top spacing on every controller page. Show storage as used/total, keep paired bar size controls in one mobile row, and move header/footer field checklists into dialogs. Date and time coexist as one group exclusive with playback timecode. Hide zero countdown values and normal-speed x1 labels. Disable auto-next when the actual playlist has no following manuscript, including shared backend validation.
- Sample native screen captures directly at half/quarter resolution to reduce PSRAM requirements. Reuse warmed glyph metrics for geometry changes and update only mirror metadata instead of reparsing the complete long-script preview.
- Allocate JSON trees and output in PSRAM once available, leaving internal SRAM for SD/USB DMA. Apply a narrowly checked, project-local IDF SPI cleanup fix: allocation failures free scratch buffers without copying an unreceived NULL buffer. Do not modify the installed SDK. Deterministic cleanup and preview metadata regressions accompany the source.
- Distinguish invalid release metadata/current diagnostic versions from actual update-server connectivity failure. Keep release-manifest validation strict.
- Retain unaffected accepted baselines and explicitly deferred hardware/browser limits. Exact-image hardware and publication results are recorded in the combined test/handoff document; compilation alone is not acceptance.

---

## 20261121.10 — 2026-11-21

- Size header/footer vector icons from the actual fitted text, eliminating double shrinking. Expand skip/fast-forward/rewind/T paths and vertically center them; keep all six selected footer values, including at minimum bar height. Share geometry with web previews and Demo.
- Keep only the blinking red dot for Playing. Paused/stopped/preparing states use plain text. Replace previous/next prefixes with media vectors while retaining document titles; use forward/reverse icons for playback magnitude and T for font size. Show loop counts as 已循环X次 / Loops X, without a colon. Playback timecode stays enabled in new default settings without overwriting saved preferences.
- Shorten actual guide-step slides from 480 to 300 ms. Keep immediate entry, return, wake and unchanged steps.
- During stable playback, overlap LVGL chrome work with immutable body-tile DMA. Drain chrome-source copies before modifying chrome and preserve both scan buffers' damage. Countdown, transitions, configuration changes and idle frames keep their normal timer service.
- Keep unaffected storage/power/connectivity baselines. Internally installed production image; independently compiled public image is not installed. Residual late frames and optical stutter remain open, as do intermittent cold-load and BLE discovery delays.

---

## 20261121.7 — 2026-11-21

- Separate manual soft-off from automatic deep shutdown. Retain networking/BLE/USB while the screen is off, reject mutations in shared backend paths, preserve unsaved web drafts and expose remote startup progress. Do not interpret ordinary loss of connectivity as shutdown.
- Wrap the IDF panic entry in project code without changing the SDK or partition table. Switch the backlight GPIO away from PWM to a held low level before chaining to IDF. Capture bounded reset/code/stage summaries in RTC noinit and persist at most four records to NVS on recovery. No raw stack, manuscript content or network credentials are retained; this is not a complete core dump. Detailed capture is best effort for power loss or severe hardware faults.
- Show an abnormal-restart recovery notice and provide shared web fault history, JSON export and acknowledgement that preserves history. Include matching device/web Demo scenes.
- Keep countdown preview deadlines monotonic across delayed packets; disable automatic looping by default while retaining deliberately saved document styles.
- Use 480 ms linear guide step motion. Preserve immediate first entry/return/wake and unchanged-step behavior. Optical smoothness is not established by compilation or virtual checks.
- Supplement internal real FAT32 CRUD/identity/order/eject/remount, allocated-cluster full-filesystem handling, recovery after freeing space and four transaction CPU-reset phases. The diagnostic preallocator needed a zero-initialized FIL for IDF's dynamic cache; its panic was a test-tool defect. Fragmented free space requires smaller extents. Diagnostic commands are excluded from distributable firmware. Software reset interruption does not establish physical SD power-cut behavior.
- Retain unaffected valid test baselines; independently build and label the public BIN as not installed. Android remains excluded.

---

# Changes for 20261121.6 — 2026-11-21

- Animate only actual guide step changes while continuously visible. First entry, return, wake and same-step language/address refresh reveal immediately. READY/RECONNECT standby lease changes do not replay the slide; loading circles keep turning. Firmware and shared Demo match.
- Exercise the pure C transition policy with sanitizers and both browser engines in normal/reduced motion. Retain optical scrolling/guide stutter as an open acceptance issue.
- Supplement internal power-cycle, no-card persistence, idle hot insertion/removal and explicitly authorized SD format checks. These do not establish public-binary, FAT32, full-card or interrupted-write acceptance.

---

# Changes / 修改记录

## 20261121.5 — 2026-11-21

- Replace the 256-row raster scratch canvas with bounded 32-row strips. Reserve an 80 KiB internal work buffer early, before radio/storage heap fragmentation, with a small PSRAM fallback. Keep tile geometry, all rotations/mirrors, visible-tile pinning and 32/64-row preparation budgets.
- Present each guide animation refresh even when integer easing rounds to the previous position; avoid the idle RTOS delay during active movement. Physical smoothness remains an acceptance condition.
- Scale header/footer separator gaps with font size in firmware and shared web/Demo rendering. Keep all six selected fields rather than silently dropping fields.
- Shorten steady BLE advertising intervals and renew finite leases more frequently. Established GATT connections remain intact. This mitigates discovery delays without claiming the historical timeout root cause is proven.
- Move TinyUSB handling off the display core, keep USB callbacks free of USB logging, and explicitly detach before deep sleep. Software re-enumeration checks cannot prove physical shutdown/reset or optical-flash acceptance.
- Retain unaffected test baselines. Independently compile public firmware; exclude native applications, actual screen captures, credentials and private logs from the public source and release.

## 20261121.3 — 2026-11-21

- Let the device settings action row span the full mobile form, with its two actions aligned left and right. Check both browser engines, languages and themes at 320, 390, 650 and 1440 pixels.
- Avoid combining ordinary-speed look-ahead glyph preparation with playback bar calculations or chrome repaint in one frame. Preserve 32/64-row preparation, visible-tile pinning and the original high-speed shuttle path. This does not establish that physical scrolling or guide animation is fully smooth.
- Correct Previous/Next help: manual selection stops and requires Play. Correct Stop and independent-library persistence descriptions in Chinese and English.
- Record a feature coverage inventory. Retain passed results outside the change impact; supplement actual BLE discovery/workflows, pagination, playback/navigation and exFAT SD update coverage. Keep intermittent discovery, optical motion and physical test prerequisites explicit. Do not repeat unrelated full suites solely because the version changed.
- Independently build the public firmware and keep private credentials, device screenshots, native applications and raw test evidence out of the public repository.

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
