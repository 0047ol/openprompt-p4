# Release procedure / 发布流程

1. Increment `PROJECT_VER`; document changes and dates in `CHANGES.md`.
2. Regenerate shared web pages, Demo and app-free public resources from this repository.
3. Compile using ESP-IDF 6.1. Do not mark the image tested unless that exact image was installed and checked. Capture virtual-data screenshots in both languages; distinguish screen simulations from hardware photos.
4. Run `python tools/package-firmware.py` and `python tools/package-review-firmware.py`. The firmware ZIP includes one combined `TEST-AND-HANDOFF.md`; no separate review document is required.
5. Audit tracked sources, generated pages, BIN/ZIP and screenshots for secrets, personal data and deployment endpoints. Exclude native apps, `.env`, keys, saved device settings, build logs, local paths and private history.
6. Commit the matching source, create a version tag and publish a GitHub Release with application BIN, ZIP, manifest, SHA-256 sums, matching screenshot archive and changelog. Never overwrite a released version with different contents. State which checks remain untested.
7. The Pages workflow publishes the HTTPS web controller, licensed font assets and the latest firmware manifest/image. Verify the downloadable image before publishing `latest.json`. Keep firmware/source/screenshots in sync on every release.

Normal releases use `forceUpdate: false`. To require an update, add `--force-update` when packaging, or run `python tools/set-update-policy.py --force-update on` on the local manifest, then publish the manifest explicitly. This setting never publishes itself and does not alter the binary. Only a strictly newer valid version triggers the required dialog. Older controllers must first gain this capability; remote policy cannot retrofit an old page which has not been refreshed/upgraded.

Required update dialogs cannot be dismissed by cancel/Esc/backdrop; failures retain retry. Connection changes invalidate the old target. No internet means no policy can be fetched: local/SD updates remain available. The updater still verifies project, chip, version, length and SHA-256. Do not set required updates on an unvalidated release for ordinary users.

自建服务器需同时发布控制台、字体和固件清单，凭据只放 CI Secret 或部署环境，不能写入源码。每次同步源码、截图和固件发行版，保留修改说明；设备不在时交付包与待测步骤，不宣称真机测试通过。
