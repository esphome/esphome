"""MCUboot slot helpers for nRF52 boards built with the partition manager and swap using move.

usage: mcuboot_slot.py limit BUILD          image size against the largest image MCUboot will swap in
       mcuboot_slot.py blank-uf2 BUILD OUT  UF2 that writes 0xFF over the secondary slot's header page
       mcuboot_slot.py active ADDR BIN [--wait S] [--expect running|pending]

BUILD is .esphome/build/<name>/.pioenvs/<name> (it holds partitions.yml, mcuboot/zephyr/.config and
zephyr/app_update.bin). Read the layout from partitions.yml: the partition manager overrides zephyr.dts.

limit exits 1 when the image is over the swap limit: such an image links and uploads, but MCUboot never swaps it in
and it then blocks every later upload with NO_FREE_SLOT. blank-uf2 is the way out of that state over USB: the SMP
image erase is refused while the image is pending. active polls the image states over SMP UDP until the image in BIN
runs (or is still pending with --expect pending), exit 1 otherwise.
"""

import argparse
import asyncio
import pathlib
import re
import struct
import sys

SECTOR = 0x1000
WRITE_SIZE = 4
MOVE_STATES = 3  # BOOT_STATUS_MOVE_STATE_COUNT + BOOT_STATUS_SWAP_STATE_COUNT
TRAILER_INFO = 4 * 8 + 16
UF2_MAGIC = (0x0A324655, 0x9E5D5157, 0x0AB16F30)


def partitions(build: pathlib.Path) -> dict:
    import yaml

    return yaml.safe_load((build / "partitions.yml").read_text())


def swap_limit(build: pathlib.Path) -> int:
    """app_max_size() in MCUboot's swap_move.c: primary sectors less the trailer sectors and the move sector."""
    config = (build / "mcuboot" / "zephyr" / ".config").read_text()
    if "CONFIG_BOOT_SWAP_USING_MOVE=y" not in config.splitlines():
        sys.exit("MCUboot is not built with swap using move; this limit does not apply")
    sectors = int(
        re.search(r"^CONFIG_BOOT_MAX_IMG_SECTORS=(\d+)$", config, re.MULTILINE)[1]
    )
    trailer = sectors * MOVE_STATES * WRITE_SIZE + TRAILER_INFO
    parts = partitions(build)
    primary = parts["mcuboot_primary"]["size"] // SECTOR
    usable = (primary - -(-trailer // SECTOR) - 1) * SECTOR
    return min(usable, parts["mcuboot_secondary"]["size"])


def cmd_limit(args: argparse.Namespace) -> None:
    build = pathlib.Path(args.build)
    size = (build / "zephyr" / "app_update.bin").stat().st_size
    limit = swap_limit(build)
    print(
        f"image {size} B, MCUboot swaps in up to {limit} B ({limit - size:+d} B left)"
    )
    if size > limit:
        print("FAIL: MCUboot will not swap this image in")
        sys.exit(1)


def cmd_blank_uf2(args: argparse.Namespace) -> None:
    base = partitions(pathlib.Path(args.build))["mcuboot_secondary"]["address"]
    blocks = [base + i * 256 for i in range(SECTOR // 256)]
    out = b"".join(
        struct.pack(
            "<8I",
            UF2_MAGIC[0],
            UF2_MAGIC[1],
            0x2000,
            addr,
            256,
            n,
            len(blocks),
            args.family,
        )
        + b"\xff" * 256
        + b"\x00" * 220
        + struct.pack("<I", UF2_MAGIC[2])
        for n, addr in enumerate(blocks)
    )
    pathlib.Path(args.out).write_bytes(out)
    print(f"{args.out}: {len(blocks)} blocks blanking 0x{base:x}..0x{base + SECTOR:x}")


async def image_states(addr: str):
    from smpclient import SMPClient
    from smpclient.requests.image_management import ImageStatesRead
    from smpclient.transport.udp import SMPUDPTransport

    async with SMPClient(SMPUDPTransport(), addr) as client:
        return (await client.request(ImageStatesRead(), timeout_s=5)).images


async def cmd_active(args: argparse.Namespace) -> None:
    from smpclient.mcuboot import IMAGE_TLV, ImageInfo

    want = ImageInfo.load_file(args.bin).get_tlv(IMAGE_TLV.SHA256).value
    loop = asyncio.get_running_loop()
    deadline = loop.time() + args.wait
    images = None
    while loop.time() < deadline:
        try:
            images = await image_states(args.addr)
        except Exception:  # noqa: BLE001 - the device is rebooting or rejoining the network
            await asyncio.sleep(3)
            continue
        ours = next((im for im in images if im.hash == want), None)
        if args.expect == "running" and ours is not None and ours.active:
            print(f"PASS: the new image runs (slot {ours.slot})")
            return
        if (
            args.expect == "pending"
            and ours is not None
            and ours.pending
            and not ours.active
        ):
            print("PASS: the old image runs and the new one is pending")
            return
        await asyncio.sleep(3)
    for im in images or []:
        print(
            f"  slot {im.slot} {im.hash.hex()[:16]} active={im.active} pending={im.pending} confirmed={im.confirmed}"
        )
    print(f"FAIL: the new image is not {args.expect} after {args.wait} s")
    sys.exit(1)


def main() -> None:
    parser = argparse.ArgumentParser()
    sub = parser.add_subparsers(dest="cmd", required=True)
    p = sub.add_parser("limit")
    p.add_argument("build")
    p = sub.add_parser("blank-uf2")
    p.add_argument("build")
    p.add_argument("out")
    p.add_argument("--family", type=lambda v: int(v, 0), default=0xADA52840)
    p = sub.add_parser("active")
    p.add_argument("addr")
    p.add_argument("bin")
    p.add_argument("--wait", type=float, default=90)
    p.add_argument("--expect", choices=("running", "pending"), default="running")
    args = parser.parse_args()
    if args.cmd == "limit":
        cmd_limit(args)
    elif args.cmd == "blank-uf2":
        cmd_blank_uf2(args)
    else:
        asyncio.run(cmd_active(args))


if __name__ == "__main__":
    main()
