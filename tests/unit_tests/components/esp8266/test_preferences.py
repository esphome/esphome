"""The config-time fit rule must match make_preference() in preferences.cpp."""

import pytest

from esphome.components.esp8266.const import preference_slots, preference_words


@pytest.mark.parametrize(
    ("value_bytes", "words"), [(0, 1), (1, 2), (4, 2), (5, 3), (94, 25)]
)
def test_preference_words_rounds_up_and_adds_the_checksum(
    value_bytes: int, words: int
) -> None:
    assert preference_words(value_bytes) == words


def test_rtc_values_cannot_straddle_the_regions() -> None:
    # 25 words each: 3 fit in the 78-word region and 1 in the 32-word one
    assert preference_slots(94, in_flash=False, restore_from_flash=False) == 4
    # 32 words each: 2 + 1, where 110 // 32 would also say 3
    assert preference_slots(124, in_flash=False, restore_from_flash=False) == 3
    # 39 words each: 2 + 0, where 110 // 39 would say 2 as well but the eboot region is skipped
    assert preference_slots(152, in_flash=False, restore_from_flash=False) == 2
    # Larger than the biggest region
    assert preference_slots(400, in_flash=False, restore_from_flash=False) == 0


def test_flash_store_size_follows_restore_from_flash() -> None:
    assert preference_slots(94, in_flash=True, restore_from_flash=True) == 5
    assert preference_slots(94, in_flash=False, restore_from_flash=True) == 5
    assert preference_slots(94, in_flash=True, restore_from_flash=False) == 2
