"""Back up P4 storage read-only, or summarize existing private backup files.

No writes, erases, formatting, partition changes or automatic device discovery.
The operator must put the P4 into ROM download mode and select its current port.
Raw backups can contain private manuscripts; share only storage-summary.json.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys


EXPECTED = [
    ("nvs", 1, 2, 0x9000, 0x6000),
    ("phy_init", 1, 1, 0xF000, 0x1000),
    ("ota_0", 0, 0x10, 0x10000, 0x5F0000),
    ("ota_1", 0, 0x11, 0x600000, 0x600000),
    ("otadata", 1, 0, 0xC00000, 0x2000),
    ("storage", 1, 0x82, 0xC10000, 0x3F0000),
]


def validate_table(data):
    if len(data) != 0x1000:
        raise ValueError("Partition-table backup must contain exactly 4096 bytes.")
    entries = []
    for cursor in range(0, len(data), 32):
        entry = data[cursor:cursor + 32]
        if entry[:2] == b"\xeb\xeb":
            if entry[:16] != b"\xeb\xeb" + b"\xff" * 14:
                raise ValueError("Invalid partition-table checksum marker.")
            if hashlib.md5(data[:cursor]).digest() != entry[16:]:
                raise ValueError("Partition-table checksum mismatch; stop without reading storage.")
            if any(byte != 0xFF for byte in data[cursor + 32:]):
                raise ValueError("Unexpected data after the partition-table checksum.")
            break
        magic, kind, subtype, offset, size, label, flags = struct.unpack("<HBBII16sI", entry)
        if magic != 0x50AA or flags != 0:
            raise ValueError("Unexpected or encrypted partition table; stop without reading storage.")
        entries.append((label.rstrip(b"\0").decode("ascii"), kind, subtype, offset, size))
    else:
        raise ValueError("Missing partition-table checksum; stop without reading storage.")
    if entries != EXPECTED:
        raise ValueError("Layout differs from the P4 .12 layout; stop without reading storage.")
    return entries[-1][3:5]


def summarize(data, table):
    offset, size = validate_table(table)
    if len(data) != size:
        raise ValueError("Storage backup length does not match the verified partition size.")
    first = next((i for i, byte in enumerate(data) if byte != 0xFF), None)
    last = next((len(data) - 1 - i for i, byte in enumerate(reversed(data)) if byte != 0xFF), None)
    return {
        "schema": 1,
        "mode": "read-only-backup",
        "partitionTableSha256": hashlib.sha256(table).hexdigest(),
        "partitionTableChecksumValid": True,
        "expectedLayoutConfirmed": True,
        "storageOffset": offset,
        "storageBytes": size,
        "storageSha256": hashlib.sha256(data).hexdigest(),
        "allErasedFF": first is None,
        "allZero": not any(data),
        "firstNonFFOffsetWithinStorage": first,
        "lastNonFFOffsetWithinStorage": last,
        "sectorBytes": 4096,
        "nonErasedSectors": sum(any(byte != 0xFF for byte in data[i:i + 4096])
                                for i in range(0, len(data), 4096)),
        "interpretation": "erased" if first is None else "nonempty-contents-not-identified",
        "doesNotEstablish": ["flash-damage", "filesystem-type", "ownership", "permission-to-erase"],
        "rawBackupContainsPotentiallyPrivateData": True,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", help="Operator-selected P4 ROM port; never the C6 port")
    parser.add_argument("--out", required=True, type=Path, help="New private output directory; must not exist")
    parser.add_argument("--baud", type=int, choices=(460800, 921600), default=460800)
    parser.add_argument("--partition-file", type=Path, help="Analyze an existing backup without device access")
    parser.add_argument("--storage-file", type=Path, help="Existing storage backup, paired with --partition-file")
    args = parser.parse_args()
    offline = args.partition_file is not None or args.storage_file is not None
    if offline and (args.port or not args.partition_file or not args.storage_file):
        parser.error("Use both backup files without --port for offline analysis.")
    if not offline and not args.port:
        parser.error("Specify --port, or both existing backup files.")
    if args.out.exists():
        parser.error("Output directory already exists; backups are never overwritten.")
    if offline:
        table = args.partition_file.read_bytes()
        validate_table(table)
        data = args.storage_file.read_bytes()
        result = summarize(data, table)
        args.out.mkdir(parents=True, exist_ok=False)
    else:
        args.out.mkdir(parents=True, exist_ok=False)
        table_path = args.out / "partition-table-private.bin"
        storage_path = args.out / "storage-private.bin"
        base = [sys.executable, "-m", "esptool", "--chip", "esp32p4", "--port", args.port,
                "--baud", str(args.baud), "--before", "no-reset", "--after", "no-reset"]
        # The explicit chip guard rejects C6; the operator already entered ROM mode.
        with (args.out / "read-private.log").open("w", encoding="utf-8") as log:
            print("Reading partition table. No flash will be written or erased.", flush=True)
            subprocess.run(base + ["read-flash", "0x8000", "0x1000", str(table_path.resolve())],
                           check=True, stdout=log, stderr=subprocess.STDOUT, timeout=120)
            table = table_path.read_bytes()
            offset, size = validate_table(table)
            print("Verified expected partition layout; reading storage backup.", flush=True)
            subprocess.run(base + ["read-flash", hex(offset), hex(size), str(storage_path.resolve())],
                           check=True, stdout=log, stderr=subprocess.STDOUT, timeout=300)
        data = storage_path.read_bytes()
        result = summarize(data, table)
    (args.out / "storage-summary.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(result, indent=2))
    if not offline:
        print("Backup complete. Press P4 RST to leave ROM mode. Keep raw files and log private.")


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, subprocess.SubprocessError) as exc:
        print(f"Stopped safely: {exc}. No erase/write operation was issued; keep partial backups.", file=sys.stderr)
        sys.exit(1)
