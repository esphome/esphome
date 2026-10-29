"""Tests for the nRF52 flash-partition overlays."""

from esphome.components.nrf52 import partitions


def test_open_dfu_zigbee_overlay_fits_below_storage() -> None:
    overlay = partitions.open_dfu_zigbee_overlay()

    assert partitions.OPEN_DFU_APP_END == 0xD3000
    assert "reg = <0x76000 0x5d000>;" in overlay  # slot1 ends at the zboss start
    assert "zboss_nvram: partition@d3000" in overlay
    assert "reg = <0xd3000 0x8000>;" in overlay
    assert "zboss_product_config: partition@db000" in overlay
    assert "reg = <0xdb000 0x1000>;" in overlay  # ends at storage_partition (0xdc000)
