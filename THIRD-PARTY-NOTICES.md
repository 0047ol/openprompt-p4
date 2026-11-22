# Third-party notices

The AGPL license covers project-authored source, not a relicensing of upstream dependencies.

| Component | License / source |
| --- | --- |
| SortableJS | MIT; `third_party/SortableJS/LICENSE` |
| Noto Sans SC, Fontsource Noto Sans SC shards and subsets | SIL Open Font License 1.1; `third_party/NotoSansSC/OFL.txt`, font shard metadata in `web/reader-cdn.json`. Subsets retain their font license. |
| LVGL | MIT; retrieved by ESP-IDF component manager, version/checksum recorded in `firmware/p4/dependencies.lock`. |
| ESP-IDF and Espressif managed components | Upstream component licenses; exact sources recorded in the dependency lock file. ESP-IDF 6.1 is an external build requirement. |
| qrcode (Demo build utility) | BSD; installed separately via `web/demo/requirements.txt`. |
| esbuild (web build utility) | MIT; pinned in `tools/package-lock.json`. Its license is included in the installed package. |

Vendor display initialization data and existing upstream notices remain in their source files. Preserve all applicable notices when redistributing a modified build. See each upstream distribution for the complete terms.
