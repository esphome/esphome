"""Check true colour precision, AUTO assets, alpha, rotation and partial flushes."""

from pathlib import Path

from PIL import Image
import pytest

from .bmp_utils import capture_when_drawn
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.parametrize("depth", [16, 24])
@pytest.mark.parametrize("rotation", [0, 90, 180, 270])
@pytest.mark.asyncio
async def test_lvgl_rgb888(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
    depth: int,
    rotation: int,
) -> None:
    """A ramp must retain every level at 24 bits, including after rotation."""
    source = tmp_path / "ramp.png"
    ramp = Image.new("RGB", (256, 9))
    ramp.putdata([(x, 0x34, 0x56) for _ in range(9) for x in range(256)])
    ramp.save(source)
    alpha_path = tmp_path / "alpha.png"
    alpha = Image.new("RGBA", (3, 1))
    alpha.putdata([(255, 0, 0, 0), (255, 0, 0, 128), (255, 0, 0, 255)])
    alpha.save(alpha_path)
    animation_path = tmp_path / "animation.png"
    Image.new("RGBA", (3, 1), (255, 0, 0, 255)).save(
        animation_path,
        save_all=True,
        append_images=[Image.new("RGBA", (3, 1), (0, 0, 255, 255))],
        duration=100,
    )
    snapshots = tmp_path / "snapshots"
    monkeypatch.setenv("ESPHOME_SNAPSHOT_DIR", str(snapshots))
    width, height = (37, 263) if rotation in (90, 270) else (263, 37)
    for name, value in {
        "DEPTH": depth,
        "ROTATION": rotation,
        "WIDTH": width,
        "HEIGHT": height,
        "ANIMATION_PATH": animation_path,
        "SOURCE_PATH": source,
        "ALPHA_PATH": alpha_path,
    }.items():
        yaml_config = yaml_config.replace(name, str(value))

    def native(x: int, y: int) -> tuple[int, int]:
        if rotation == 90:
            return width - 1 - y, x
        if rotation == 180:
            return width - 1 - x, height - 1 - y
        if rotation == 270:
            return y, height - 1 - x
        return x, y

    def expected(rgb: tuple[int, int, int]) -> tuple[int, int, int]:
        if depth == 24:
            return rgb
        r, g, b = rgb
        return (r >> 3) * 255 // 31, (g >> 2) * 255 // 63, (b >> 3) * 255 // 31

    async with run_compiled(yaml_config), api_client_connected() as client:
        _, services = await client.list_entities_services()
        take_service = next(s for s in services if s.name == "take_screenshot")
        frame_service = next(s for s in services if s.name == "next_frame")
        change_service = next(s for s in services if s.name == "change_colour")

        async def take(name: str) -> None:
            await client.execute_service(take_service, {"name": name})

        _, path = await capture_when_drawn(take, snapshots)
        with Image.open(path) as frame:
            actual = [frame.getpixel(native(x, 4)) for x in range(256)]
            assert actual == [expected((x, 0x34, 0x56)) for x in range(256)]
            assert len(set(actual)) == (256 if depth == 24 else 32)
            assert frame.getpixel(native(2, 12)) == (0, 0, 0)
            half = frame.getpixel(native(3, 12))
            assert 120 <= half[0] <= 132 and half[1:] == (0, 0)
            assert frame.getpixel(native(4, 12)) == (255, 0, 0)
            assert frame.getpixel(native(10, 25)) == expected((0x56, 0x34, 0x12))
        await client.execute_service(frame_service, {})
        await client.execute_service(change_service, {})
        # Poll for the update, rather than relying on a fixed render delay.
        for attempt in range(30):
            _, path = await capture_when_drawn(
                take, snapshots, prefix=f"update{attempt}"
            )
            with Image.open(path) as frame:
                if frame.getpixel(native(10, 25)) == expected((0x12, 0x34, 0x56)):
                    assert frame.getpixel(native(31, 12)) == (0, 0, 255)
                    assert frame.getpixel(native(255, 4)) == expected((255, 0x34, 0x56))
                    break
        else:
            pytest.fail("partial redraw did not update the colour patch")
