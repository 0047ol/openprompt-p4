# Changes / 修改记录

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
