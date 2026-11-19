# Changes / 修改记录

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
