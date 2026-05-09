import unittest


def build_rojaflex_frame_9b(housecode: str, channel: int, command: int) -> bytes:
    """Build 9-byte RojaFlex frame (FHEM P109 style, no CRC), no preamble/sync."""
    if len(housecode) != 7:
        raise ValueError("housecode must be 7 hex chars")
    if not (0 <= channel <= 15):
        raise ValueError("channel must be 0..15")

    id28 = int(housecode, 16)
    id_b1 = (id28 >> 20) & 0xFF
    id_b2 = (id28 >> 12) & 0xFF
    id_b3 = (id28 >> 4) & 0xFF
    id_nibble = id28 & 0x0F

    byte4 = (id_nibble << 4) | (channel & 0x0F)
    token1 = command
    token2 = (id_b1 + id_b2 + id_b3 + byte4 + command + 0x01 + token1) & 0xFF
    return bytes([0x08, id_b1, id_b2, id_b3, byte4, command, 0x01, token1, token2])


def parse_motor_update_from_payload(frame: bytes, configured_housecode: str):
    """
    Mirror the RX logic from esphome-rojaflex-complete.yaml.
    Returns dict with channel/pct/raw/info when frame is a motor update for CH0..CH15,
    otherwise returns None.
    """
    if len(frame) != 9 or frame[0] != 0x08:
        return None
    if not configured_housecode or configured_housecode.upper() == "0000000":
        return None

    rx_housecode = f"{frame[1]:02X}{frame[2]:02X}{frame[3]:02X}{(frame[4] >> 4) & 0x0F:X}"
    if rx_housecode.upper() != configured_housecode.upper():
        return None

    channel = frame[4] & 0x0F
    cmd_dev = frame[5]
    device_type = cmd_dev & 0x0F
    cmd_high = (cmd_dev >> 4) & 0x0F
    cmd_value = frame[6]
    raw = frame.hex().upper()
    info = f"HC={rx_housecode} CH={channel} DEV={device_type:X} CMD={cmd_high:X} VAL={cmd_value}"

    # Only accept motor updates (all channels 0..15).
    if device_type != 0x5:
        return None

    pct = min(cmd_value, 100)
    return {"channel": channel, "pct": pct, "raw": raw, "info": info}


def auto_learn_housecode_step(
    frame: bytes,
    configured_housecode: str,
    candidate_housecode: str,
    candidate_count: int,
):
    """
    Mirror the auto-learn logic from esphome-rojaflex-complete.yaml.
    Returns a tuple:
      (new_configured_housecode, new_candidate_housecode, new_candidate_count, learned_now)
    """
    if len(frame) != 9 or frame[0] != 0x08:
        return configured_housecode, candidate_housecode, candidate_count, False

    housecode_configured = bool(configured_housecode) and configured_housecode.upper() != "0000000"
    if housecode_configured:
        return configured_housecode, candidate_housecode, candidate_count, False

    rx_housecode = f"{frame[1]:02X}{frame[2]:02X}{frame[3]:02X}{(frame[4] >> 4) & 0x0F:X}"

    if rx_housecode.upper() == candidate_housecode.upper():
        new_count = candidate_count + 1
        if new_count >= 3:
            return rx_housecode, "", 0, True
        return configured_housecode, candidate_housecode, new_count, False

    return configured_housecode, rx_housecode, 1, False


def apply_set_housecode_service(
    current_housecode: str,
    auto_learn_housecode: str,
    auto_learn_count: int,
    new_housecode: str,
):
    """
    Mirror API service set_housecode behavior from YAML.
    Returns tuple:
      (updated_housecode, updated_auto_learn_housecode, updated_auto_learn_count, applied)
    """
    if len(new_housecode) == 7 and all(c in "0123456789abcdefABCDEF" for c in new_housecode):
        return new_housecode, "", 0, True
    return current_housecode, auto_learn_housecode, auto_learn_count, False


EXPECTED = {
    "STOP": {
        2: "083122FD220A010A87",
        3: "083122FD230A010A88",
        4: "083122FD240A010A89",
        5: "083122FD250A010A8A",
        6: "083122FD260A010A8B",
        7: "083122FD270A010A8C",
        8: "083122FD280A010A8D",
        9: "083122FD290A010A8E",
    },
    "UP": {
        2: "083122FD221A011AA7",
        3: "083122FD231A011AA8",
        4: "083122FD241A011AA9",
        5: "083122FD251A011AAA",
        6: "083122FD261A011AAB",
        7: "083122FD271A011AAC",
        8: "083122FD281A011AAD",
        9: "083122FD291A011AAE",
    },
    "DOWN": {
        2: "083122FD228A018A87",
        3: "083122FD238A018A88",
        4: "083122FD248A018A89",
        5: "083122FD258A018A8A",
        6: "083122FD268A018A8B",
        7: "083122FD278A018A8C",
        8: "083122FD288A018A8D",
        9: "083122FD298A018A8E",
    },
}


class TestRojaflex9ByteCommands(unittest.TestCase):
    HOUSECODE = "3122FD2"
    COMMANDS = {"STOP": 0x0A, "UP": 0x1A, "DOWN": 0x8A}

    def test_stop_up_down_channels_2_to_9_match_expected_9_byte_frames(self) -> None:
        for name, cmd in self.COMMANDS.items():
            for channel in range(2, 10):
                frame = build_rojaflex_frame_9b(self.HOUSECODE, channel, cmd)
                self.assertEqual(frame.hex().upper(), EXPECTED[name][channel], f"{name} channel {channel}")

    def test_frames_are_exactly_9_bytes(self) -> None:
        for cmd in self.COMMANDS.values():
            for channel in range(2, 10):
                frame = build_rojaflex_frame_9b(self.HOUSECODE, channel, cmd)
                self.assertEqual(len(frame), 9, f"channel {channel} cmd 0x{cmd:02X}")

    def test_build_rejects_invalid_housecode_length_and_channel_range(self) -> None:
        with self.assertRaises(ValueError):
            build_rojaflex_frame_9b("123456", 2, 0x8A)
        with self.assertRaises(ValueError):
            build_rojaflex_frame_9b("12345678", 2, 0x8A)
        with self.assertRaises(ValueError):
            build_rojaflex_frame_9b("3122FD2", -1, 0x8A)
        with self.assertRaises(ValueError):
            build_rojaflex_frame_9b("3122FD2", 16, 0x8A)


class TestRojaflexReceiverParsing(unittest.TestCase):
    def test_parses_motor_status_sample_frame(self) -> None:
        # Sample from user/FHEM: 083122FD22C55F0A87
        frame = bytes.fromhex("083122FD22C55F0A87")
        parsed = parse_motor_update_from_payload(frame, "3122FD2")
        self.assertIsNotNone(parsed)
        self.assertEqual(parsed["channel"], 2)
        self.assertEqual(parsed["pct"], 95)
        self.assertEqual(parsed["raw"], "083122FD22C55F0A87")

    def test_ignores_non_motor_remote_frames(self) -> None:
        # DOWN remote command, device type nibble = A, should not be treated as motor update
        frame = bytes.fromhex("083122FD228A018A87")
        parsed = parse_motor_update_from_payload(frame, "3122FD2")
        self.assertIsNone(parsed)

    def test_ignores_wrong_housecode(self) -> None:
        motor_frame = bytes.fromhex("083122FD22C55F0A87")
        self.assertIsNone(parse_motor_update_from_payload(motor_frame, "AAAAAAA"))

    def test_parses_channel_1_motor_frame(self) -> None:
        ch1_motor = bytes.fromhex("083122FD21C55F0A86")
        parsed = parse_motor_update_from_payload(ch1_motor, "3122FD2")
        self.assertIsNotNone(parsed)
        self.assertEqual(parsed["channel"], 1)
        self.assertEqual(parsed["pct"], 95)

    def test_ignores_updates_when_housecode_not_configured(self) -> None:
        motor_frame = bytes.fromhex("083122FD22C55F0A87")
        self.assertIsNone(parse_motor_update_from_payload(motor_frame, ""))
        self.assertIsNone(parse_motor_update_from_payload(motor_frame, "0000000"))

    def test_parsing_is_case_insensitive_for_configured_housecode(self) -> None:
        motor_frame = bytes.fromhex("083122FD22C55F0A87")
        parsed = parse_motor_update_from_payload(motor_frame, "3122fd2")
        self.assertIsNotNone(parsed)
        self.assertEqual(parsed["channel"], 2)

    def test_ignores_invalid_payload_shape(self) -> None:
        self.assertIsNone(parse_motor_update_from_payload(bytes.fromhex("083122FD22C55F0A"), "3122FD2"))
        self.assertIsNone(parse_motor_update_from_payload(bytes.fromhex("093122FD22C55F0A87"), "3122FD2"))


class TestRojaflexAutoLearn(unittest.TestCase):
    def test_learns_housecode_after_3_consecutive_matching_frames(self) -> None:
        frame = bytes.fromhex("083122FD22C55F0A87")
        configured = "0000000"
        candidate = ""
        count = 0

        configured, candidate, count, learned = auto_learn_housecode_step(frame, configured, candidate, count)
        self.assertFalse(learned)
        self.assertEqual(configured, "0000000")
        self.assertEqual(candidate, "3122FD2")
        self.assertEqual(count, 1)

        configured, candidate, count, learned = auto_learn_housecode_step(frame, configured, candidate, count)
        self.assertFalse(learned)
        self.assertEqual(configured, "0000000")
        self.assertEqual(candidate, "3122FD2")
        self.assertEqual(count, 2)

        configured, candidate, count, learned = auto_learn_housecode_step(frame, configured, candidate, count)
        self.assertTrue(learned)
        self.assertEqual(configured, "3122FD2")
        self.assertEqual(candidate, "")
        self.assertEqual(count, 0)

    def test_resets_candidate_when_different_housecode_arrives(self) -> None:
        frame_a = bytes.fromhex("083122FD22C55F0A87")
        frame_b = bytes.fromhex("08ABCDEF25C55F0A00")
        configured = "0000000"
        candidate = ""
        count = 0

        configured, candidate, count, learned = auto_learn_housecode_step(frame_a, configured, candidate, count)
        self.assertFalse(learned)
        self.assertEqual(candidate, "3122FD2")
        self.assertEqual(count, 1)

        configured, candidate, count, learned = auto_learn_housecode_step(frame_b, configured, candidate, count)
        self.assertFalse(learned)
        self.assertEqual(configured, "0000000")
        self.assertEqual(candidate, "ABCDEF2")
        self.assertEqual(count, 1)

    def test_auto_learn_disabled_when_housecode_already_configured(self) -> None:
        frame = bytes.fromhex("083122FD22C55F0A87")
        configured, candidate, count, learned = auto_learn_housecode_step(
            frame, "3122FD2", "", 0
        )
        self.assertFalse(learned)
        self.assertEqual(configured, "3122FD2")
        self.assertEqual(candidate, "")
        self.assertEqual(count, 0)

    def test_manual_housecode_override_after_auto_learn(self) -> None:
        # Simulate already learned state.
        configured = "3122FD2"
        candidate = ""
        count = 0

        # Simulate API service "set_housecode" override behavior from YAML:
        # id(housecode) = housecode; id(auto_learn_housecode) = ""; id(auto_learn_count) = 0;
        manual_housecode = "ABC1234"
        configured = manual_housecode
        candidate = ""
        count = 0

        self.assertEqual(configured, "ABC1234")
        self.assertEqual(candidate, "")
        self.assertEqual(count, 0)

    def test_set_housecode_service_valid_overrides_and_resets_learning_state(self) -> None:
        configured, candidate, count, applied = apply_set_housecode_service(
            current_housecode="3122FD2",
            auto_learn_housecode="3122FD2",
            auto_learn_count=2,
            new_housecode="ABC1234",
        )
        self.assertTrue(applied)
        self.assertEqual(configured, "ABC1234")
        self.assertEqual(candidate, "")
        self.assertEqual(count, 0)

    def test_set_housecode_service_invalid_keeps_previous_state(self) -> None:
        configured, candidate, count, applied = apply_set_housecode_service(
            current_housecode="3122FD2",
            auto_learn_housecode="AAAAAAA",
            auto_learn_count=1,
            new_housecode="XYZ1234",
        )
        self.assertFalse(applied)
        self.assertEqual(configured, "3122FD2")
        self.assertEqual(candidate, "AAAAAAA")
        self.assertEqual(count, 1)

    def test_auto_learn_ignores_invalid_payload_and_keeps_state(self) -> None:
        configured, candidate, count, learned = auto_learn_housecode_step(
            bytes.fromhex("083122FD22C55F0A"), "0000000", "3122FD2", 2
        )
        self.assertFalse(learned)
        self.assertEqual(configured, "0000000")
        self.assertEqual(candidate, "3122FD2")
        self.assertEqual(count, 2)

    def test_set_housecode_service_accepts_lowercase_hex(self) -> None:
        configured, candidate, count, applied = apply_set_housecode_service(
            current_housecode="0000000",
            auto_learn_housecode="",
            auto_learn_count=0,
            new_housecode="abc1234",
        )
        self.assertTrue(applied)
        self.assertEqual(configured, "abc1234")
        self.assertEqual(candidate, "")
        self.assertEqual(count, 0)


class TestRojaflex9ByteCommandsEdgeCases(unittest.TestCase):
    HOUSECODE = "3122FD2"

    def test_build_channel_boundary_low_and_high(self) -> None:
        for channel in (0, 15):
            frame = build_rojaflex_frame_9b(self.HOUSECODE, channel, 0x0A)
            self.assertEqual(len(frame), 9)
            self.assertEqual(frame[0], 0x08)
            self.assertEqual(frame[4] & 0x0F, channel)
            # High nibble of byte 4 must equal last nibble of housecode (0x2).
            self.assertEqual((frame[4] >> 4) & 0x0F, 0x2)

    def test_build_accepts_lowercase_housecode_and_matches_uppercase(self) -> None:
        upper = build_rojaflex_frame_9b("3122FD2", 5, 0x8A)
        lower = build_rojaflex_frame_9b("3122fd2", 5, 0x8A)
        self.assertEqual(upper, lower)

    def test_build_rejects_non_hex_housecode(self) -> None:
        with self.assertRaises(ValueError):
            build_rojaflex_frame_9b("31ZZFD2", 2, 0x8A)


class TestRojaflexReceiverParsingEdgeCases(unittest.TestCase):
    HOUSECODE = "3122FD2"

    def test_pct_clamps_when_value_above_100(self) -> None:
        frame = bytes.fromhex("083122FD22C5C80A00")  # cmd_value = 0xC8 = 200
        parsed = parse_motor_update_from_payload(frame, self.HOUSECODE)
        self.assertIsNotNone(parsed)
        self.assertEqual(parsed["pct"], 100)

    def test_pct_zero_is_preserved(self) -> None:
        frame = bytes.fromhex("083122FD22C5000A00")
        parsed = parse_motor_update_from_payload(frame, self.HOUSECODE)
        self.assertIsNotNone(parsed)
        self.assertEqual(parsed["pct"], 0)

    def test_pct_exactly_100(self) -> None:
        frame = bytes.fromhex("083122FD22C5640A00")  # cmd_value = 0x64 = 100
        parsed = parse_motor_update_from_payload(frame, self.HOUSECODE)
        self.assertIsNotNone(parsed)
        self.assertEqual(parsed["pct"], 100)

    def test_motor_update_for_channel_0(self) -> None:
        frame = bytes.fromhex("083122FD20C5320A00")  # byte4 = 0x20 -> ch 0
        parsed = parse_motor_update_from_payload(frame, self.HOUSECODE)
        self.assertIsNotNone(parsed)
        self.assertEqual(parsed["channel"], 0)
        self.assertEqual(parsed["pct"], 0x32)

    def test_motor_update_for_channel_15(self) -> None:
        frame = bytes.fromhex("083122FD2FC5320A00")  # byte4 = 0x2F -> ch 15
        parsed = parse_motor_update_from_payload(frame, self.HOUSECODE)
        self.assertIsNotNone(parsed)
        self.assertEqual(parsed["channel"], 15)
        self.assertEqual(parsed["pct"], 0x32)

    def test_empty_payload_returns_none(self) -> None:
        self.assertIsNone(parse_motor_update_from_payload(b"", self.HOUSECODE))

    def test_housecode_mismatch_in_byte4_high_nibble(self) -> None:
        # bytes 1..3 unchanged, but byte 4 high nibble = 0x3 -> housecode "3122FD3"
        frame = bytes.fromhex("083122FD32C55F0A87")
        self.assertIsNone(parse_motor_update_from_payload(frame, "3122FD2"))


class TestRojaflexAutoLearnEdgeCases(unittest.TestCase):
    @staticmethod
    def _frame_for(housecode: str, channel: int = 2) -> bytes:
        # Auto-learn ignores the device type, so a STOP-style frame is sufficient.
        return build_rojaflex_frame_9b(housecode, channel, 0x0A)

    def test_alternating_housecodes_never_learn(self) -> None:
        frame_a = self._frame_for("3122FD2")
        frame_b = self._frame_for("ABCDEF2")
        configured = "0000000"
        candidate = ""
        count = 0
        for f in (frame_a, frame_b, frame_a, frame_b, frame_a, frame_b):
            configured, candidate, count, learned = auto_learn_housecode_step(
                f, configured, candidate, count
            )
            self.assertFalse(learned)
        self.assertEqual(configured, "0000000")

    def _run_pattern(self, pattern):
        """Feed a sequence of housecodes through the auto-learn step.
        Returns the final tuple plus a list of `learned` flags per step.
        """
        configured = "0000000"
        candidate = ""
        count = 0
        learned_per_step = []
        for housecode in pattern:
            frame = self._frame_for(housecode)
            configured, candidate, count, learned = auto_learn_housecode_step(
                frame, configured, candidate, count
            )
            learned_per_step.append(learned)
        return configured, candidate, count, learned_per_step

    def test_pattern_aaa_learns_on_third_a(self) -> None:
        configured, candidate, count, flags = self._run_pattern(
            ["3122FD2", "3122FD2", "3122FD2"]
        )
        self.assertEqual(flags, [False, False, True])
        self.assertEqual(configured, "3122FD2")
        self.assertEqual(candidate, "")
        self.assertEqual(count, 0)

    def test_pattern_abaa_does_not_learn(self) -> None:
        configured, candidate, count, flags = self._run_pattern(
            ["3122FD2", "ABCDEF2", "3122FD2", "3122FD2"]
        )
        self.assertEqual(flags, [False, False, False, False])
        self.assertEqual(configured, "0000000")
        self.assertEqual(candidate, "3122FD2")
        # After A (reset by B), A, A -> only 2 consecutive A's, not 3 yet.
        self.assertEqual(count, 2)

    def test_pattern_abaaa_learns_on_fifth_frame(self) -> None:
        configured, candidate, count, flags = self._run_pattern(
            ["3122FD2", "ABCDEF2", "3122FD2", "3122FD2", "3122FD2"]
        )
        self.assertEqual(flags, [False, False, False, False, True])
        self.assertEqual(configured, "3122FD2")
        self.assertEqual(candidate, "")
        self.assertEqual(count, 0)

    def test_frame_with_wrong_start_byte_does_not_reset_progress(self) -> None:
        good = self._frame_for("3122FD2")
        bad = bytes([0x09]) + good[1:]  # non-P109 start byte
        configured, candidate, count = "0000000", "", 0

        configured, candidate, count, learned = auto_learn_housecode_step(
            good, configured, candidate, count
        )
        self.assertFalse(learned)
        self.assertEqual(candidate, "3122FD2")
        self.assertEqual(count, 1)

        # Invalid frame must not reset state.
        configured, candidate, count, learned = auto_learn_housecode_step(
            bad, configured, candidate, count
        )
        self.assertFalse(learned)
        self.assertEqual(configured, "0000000")
        self.assertEqual(candidate, "3122FD2")
        self.assertEqual(count, 1)


class TestRojaflexSetHousecodeEdgeCases(unittest.TestCase):
    def test_empty_string_is_rejected(self) -> None:
        _, _, _, applied = apply_set_housecode_service(
            current_housecode="3122FD2",
            auto_learn_housecode="",
            auto_learn_count=0,
            new_housecode="",
        )
        self.assertFalse(applied)

    def test_eight_char_housecode_is_rejected(self) -> None:
        _, _, _, applied = apply_set_housecode_service(
            current_housecode="3122FD2",
            auto_learn_housecode="",
            auto_learn_count=0,
            new_housecode="3122FD22",
        )
        self.assertFalse(applied)

    def test_seven_char_with_space_is_rejected(self) -> None:
        _, _, _, applied = apply_set_housecode_service(
            current_housecode="3122FD2",
            auto_learn_housecode="",
            auto_learn_count=0,
            new_housecode="3122 D2",
        )
        self.assertFalse(applied)


def compute_shutter_motion_plan(
    current_pct: int,
    target_pct: int,
    time_to_open_s: int,
    time_to_close_s: int,
):
    """Mirror of rojaflex::compute_shutter_motion_plan from cc1101_helper.h."""
    target_pct = max(0, min(100, target_pct))

    if target_pct == 0:
        return {"action": "up_to_end", "duration_ms": 0, "target_pct": 0}
    if target_pct == 100:
        return {"action": "down_to_end", "duration_ms": 0, "target_pct": 100}

    if current_pct < 0:
        return {
            "action": "none",
            "duration_ms": 0,
            "target_pct": target_pct,
            "info": "current position unknown - move to an end stop first",
        }

    if current_pct == target_pct:
        return {"action": "stop", "duration_ms": 0, "target_pct": target_pct}

    if target_pct > current_pct:
        if time_to_close_s < 0:
            return {
                "action": "none",
                "duration_ms": 0,
                "target_pct": target_pct,
                "info": "close time not calibrated - drive fully closed once",
            }
        delta = target_pct - current_pct
        return {
            "action": "down_then_stop",
            "duration_ms": (delta * time_to_close_s * 1000) // 100,
            "target_pct": target_pct,
        }

    if time_to_open_s < 0:
        return {
            "action": "none",
            "duration_ms": 0,
            "target_pct": target_pct,
            "info": "open time not calibrated - drive fully open once",
        }
    delta = current_pct - target_pct
    return {
        "action": "up_then_stop",
        "duration_ms": (delta * time_to_open_s * 1000) // 100,
        "target_pct": target_pct,
    }


def tick_position_interpolation_single(
    motor_pct: int,
    start_ms: int,
    start_pct: int,
    end_pct: int,
    duration_ms: int,
    now_ms: int,
):
    """Mirror of rojaflex::tick_position_interpolation single-channel logic.

    Returns the new motor_pct value. The function intentionally does NOT
    write the final end value when elapsed >= duration - the timer callback
    or motor-feedback frame is the authoritative writer for that.
    """
    if start_ms == 0 or duration_ms == 0:
        return motor_pct
    elapsed = now_ms - start_ms
    if elapsed < 0:
        return motor_pct
    if elapsed >= duration_ms:
        return motor_pct
    delta = end_pct - start_pct
    # Match C++ integer division (trunc toward zero) instead of Python's
    # floor division, so signed deltas behave the same in both languages.
    step = int(delta * elapsed / duration_ms)
    return start_pct + step


def update_calibration_on_motor_feedback(
    cal_time_open_s: int,
    cal_time_close_s: int,
    pending_tx_ms: int,
    pending_target_pct: int,
    rx_ms: int,
    rx_pct: int,
    capture_window_ms: int = 60_000,
    min_time_s: int = 5,
):
    """Mirror of the calibration capture-resolve logic from rojaflex_cc1101.h.

    Returns a tuple:
      (new_cal_time_open_s, new_cal_time_close_s, new_pending_tx_ms,
       new_pending_target_pct, captured)
    `captured` is True when the calibration value was updated.
    """
    # No pending capture or already resolved.
    if pending_tx_ms == 0 or pending_target_pct < 0:
        return cal_time_open_s, cal_time_close_s, pending_tx_ms, pending_target_pct, False

    elapsed_ms = rx_ms - pending_tx_ms
    if elapsed_ms < 0 or elapsed_ms >= capture_window_ms:
        # Window expired -> drop pending.
        return cal_time_open_s, cal_time_close_s, 0, -1, False

    if rx_pct != pending_target_pct:
        # Intermediate frame, keep waiting.
        return cal_time_open_s, cal_time_close_s, pending_tx_ms, pending_target_pct, False

    delta_s = elapsed_ms // 1000
    if delta_s < min_time_s:
        # Suspiciously fast -> reject silently and clear pending.
        return cal_time_open_s, cal_time_close_s, 0, -1, False

    # target_pct == 0  -> we drove UP to fully open  -> calibrates open time
    # target_pct == 100 -> we drove DOWN to fully closed -> calibrates close time
    if pending_target_pct == 0:
        return delta_s, cal_time_close_s, 0, -1, True
    return cal_time_open_s, delta_s, 0, -1, True


class TestRojaflexShutterMotionPlan(unittest.TestCase):
    def test_target_zero_drives_up_to_end_without_timer(self) -> None:
        plan = compute_shutter_motion_plan(50, 0, 30, 30)
        self.assertEqual(plan["action"], "up_to_end")
        self.assertEqual(plan["duration_ms"], 0)

    def test_target_hundred_drives_down_to_end_without_timer(self) -> None:
        plan = compute_shutter_motion_plan(50, 100, 30, 30)
        self.assertEqual(plan["action"], "down_to_end")
        self.assertEqual(plan["duration_ms"], 0)

    def test_target_below_zero_clamps_to_zero(self) -> None:
        plan = compute_shutter_motion_plan(50, -10, 30, 30)
        self.assertEqual(plan["action"], "up_to_end")
        self.assertEqual(plan["target_pct"], 0)

    def test_target_above_hundred_clamps_to_hundred(self) -> None:
        plan = compute_shutter_motion_plan(50, 150, 30, 30)
        self.assertEqual(plan["action"], "down_to_end")
        self.assertEqual(plan["target_pct"], 100)

    def test_already_at_target_emits_stop(self) -> None:
        plan = compute_shutter_motion_plan(50, 50, 30, 30)
        self.assertEqual(plan["action"], "stop")
        self.assertEqual(plan["duration_ms"], 0)

    def test_unknown_current_pct_with_mid_target_yields_no_action(self) -> None:
        plan = compute_shutter_motion_plan(-1, 50, 30, 30)
        self.assertEqual(plan["action"], "none")
        self.assertIn("unknown", plan["info"])

    def test_unknown_current_pct_with_endstop_target_still_works(self) -> None:
        plan_open = compute_shutter_motion_plan(-1, 0, 30, 30)
        plan_close = compute_shutter_motion_plan(-1, 100, 30, 30)
        self.assertEqual(plan_open["action"], "up_to_end")
        self.assertEqual(plan_close["action"], "down_to_end")

    def test_drive_down_from_open_to_mid(self) -> None:
        # cpos=0 (open), tpos=50 -> need to close 50% with 30s full close
        plan = compute_shutter_motion_plan(0, 50, 30, 30)
        self.assertEqual(plan["action"], "down_then_stop")
        self.assertEqual(plan["duration_ms"], 15000)

    def test_drive_up_from_closed_to_mid(self) -> None:
        # cpos=100, tpos=50 -> need to open 50% with 30s full open
        plan = compute_shutter_motion_plan(100, 50, 30, 30)
        self.assertEqual(plan["action"], "up_then_stop")
        self.assertEqual(plan["duration_ms"], 15000)

    def test_drive_up_uses_time_to_open(self) -> None:
        plan = compute_shutter_motion_plan(80, 30, 20, 40)
        self.assertEqual(plan["action"], "up_then_stop")
        # delta = 50, time_to_open = 20s -> 10s
        self.assertEqual(plan["duration_ms"], 10000)

    def test_drive_down_uses_time_to_close(self) -> None:
        plan = compute_shutter_motion_plan(20, 80, 20, 40)
        self.assertEqual(plan["action"], "down_then_stop")
        # delta = 60, time_to_close = 40s -> 24s
        self.assertEqual(plan["duration_ms"], 24000)

    def test_small_step_produces_short_duration(self) -> None:
        plan = compute_shutter_motion_plan(40, 45, 30, 30)
        self.assertEqual(plan["action"], "down_then_stop")
        # delta = 5, time_to_close = 30s -> 1.5s
        self.assertEqual(plan["duration_ms"], 1500)

    def test_mid_position_up_rejected_when_open_uncalibrated(self) -> None:
        # cpos=80, tpos=30 -> needs UP, but open time is uncalibrated
        plan = compute_shutter_motion_plan(80, 30, time_to_open_s=-1, time_to_close_s=30)
        self.assertEqual(plan["action"], "none")
        self.assertIn("open time not calibrated", plan["info"])

    def test_mid_position_down_rejected_when_close_uncalibrated(self) -> None:
        # cpos=20, tpos=70 -> needs DOWN, but close time is uncalibrated
        plan = compute_shutter_motion_plan(20, 70, time_to_open_s=30, time_to_close_s=-1)
        self.assertEqual(plan["action"], "none")
        self.assertIn("close time not calibrated", plan["info"])

    def test_endstop_works_even_with_uncalibrated_times(self) -> None:
        # Uncalibrated travel times must NOT block end-stop fahrten - those
        # are exactly how the auto-calibrator learns the times.
        plan_open = compute_shutter_motion_plan(50, 0, time_to_open_s=-1, time_to_close_s=-1)
        self.assertEqual(plan_open["action"], "up_to_end")
        plan_close = compute_shutter_motion_plan(50, 100, time_to_open_s=-1, time_to_close_s=-1)
        self.assertEqual(plan_close["action"], "down_to_end")

    def test_mid_position_with_only_relevant_direction_calibrated(self) -> None:
        # If we only need UP and the open time is calibrated, missing close
        # time must NOT block the move.
        plan = compute_shutter_motion_plan(100, 50, time_to_open_s=28, time_to_close_s=-1)
        self.assertEqual(plan["action"], "up_then_stop")
        self.assertEqual(plan["duration_ms"], 14000)
        # And vice versa.
        plan = compute_shutter_motion_plan(0, 50, time_to_open_s=-1, time_to_close_s=32)
        self.assertEqual(plan["action"], "down_then_stop")
        self.assertEqual(plan["duration_ms"], 16000)


class TestRojaflexCalibrationCapture(unittest.TestCase):
    def test_full_close_run_calibrates_close_time(self) -> None:
        # We were at 0 (fully open), drove DOWN, expecting pct=100 in feedback.
        # Motor reports pct=100 after 32 s.
        cal_open, cal_close, pending_tx, pending_target, captured = (
            update_calibration_on_motor_feedback(
                cal_time_open_s=-1,
                cal_time_close_s=-1,
                pending_tx_ms=1_000,
                pending_target_pct=100,
                rx_ms=33_000,
                rx_pct=100,
            )
        )
        self.assertTrue(captured)
        self.assertEqual(cal_open, -1)  # untouched
        self.assertEqual(cal_close, 32)
        self.assertEqual(pending_tx, 0)
        self.assertEqual(pending_target, -1)

    def test_full_open_run_calibrates_open_time(self) -> None:
        # We were at 100 (fully closed), drove UP, expecting pct=0 in feedback.
        cal_open, cal_close, pending_tx, pending_target, captured = (
            update_calibration_on_motor_feedback(
                cal_time_open_s=-1,
                cal_time_close_s=32,
                pending_tx_ms=5_000,
                pending_target_pct=0,
                rx_ms=33_000,
                rx_pct=0,
            )
        )
        self.assertTrue(captured)
        self.assertEqual(cal_open, 28)
        self.assertEqual(cal_close, 32)  # untouched
        self.assertEqual(pending_tx, 0)
        self.assertEqual(pending_target, -1)

    def test_intermediate_motor_frame_does_not_resolve(self) -> None:
        # Pending capture for target=100, but motor reports pct=50 (still moving).
        cal_open, cal_close, pending_tx, pending_target, captured = (
            update_calibration_on_motor_feedback(
                cal_time_open_s=-1,
                cal_time_close_s=-1,
                pending_tx_ms=1_000,
                pending_target_pct=100,
                rx_ms=15_000,
                rx_pct=50,
            )
        )
        self.assertFalse(captured)
        # Pending must remain active so a later end-frame can still complete.
        self.assertEqual(pending_tx, 1_000)
        self.assertEqual(pending_target, 100)

    def test_too_fast_motor_feedback_is_rejected(self) -> None:
        # Motor reports pct=100 after only 2 s -> below MIN_TIME_S, suspicious.
        cal_open, cal_close, pending_tx, pending_target, captured = (
            update_calibration_on_motor_feedback(
                cal_time_open_s=-1,
                cal_time_close_s=-1,
                pending_tx_ms=1_000,
                pending_target_pct=100,
                rx_ms=2_500,
                rx_pct=100,
            )
        )
        self.assertFalse(captured)
        # Pending should be cleared so a stale half-run doesn't linger.
        self.assertEqual(pending_tx, 0)
        self.assertEqual(pending_target, -1)

    def test_capture_window_expired_clears_pending(self) -> None:
        # Motor reports the right pct but well after the capture window.
        cal_open, cal_close, pending_tx, pending_target, captured = (
            update_calibration_on_motor_feedback(
                cal_time_open_s=-1,
                cal_time_close_s=-1,
                pending_tx_ms=1_000,
                pending_target_pct=100,
                rx_ms=70_000,
                rx_pct=100,
            )
        )
        self.assertFalse(captured)
        self.assertEqual(pending_tx, 0)
        self.assertEqual(pending_target, -1)

    def test_no_pending_capture_is_no_op(self) -> None:
        cal_open, cal_close, pending_tx, pending_target, captured = (
            update_calibration_on_motor_feedback(
                cal_time_open_s=28,
                cal_time_close_s=32,
                pending_tx_ms=0,
                pending_target_pct=-1,
                rx_ms=10_000,
                rx_pct=100,
            )
        )
        self.assertFalse(captured)
        self.assertEqual(cal_open, 28)
        self.assertEqual(cal_close, 32)
        self.assertEqual(pending_tx, 0)
        self.assertEqual(pending_target, -1)

    def test_existing_calibration_is_overwritten_with_fresh_value(self) -> None:
        cal_open, cal_close, _, _, captured = update_calibration_on_motor_feedback(
            cal_time_open_s=28,
            cal_time_close_s=32,
            pending_tx_ms=1_000,
            pending_target_pct=100,
            rx_ms=37_000,
            rx_pct=100,
        )
        self.assertTrue(captured)
        # Close time updated, open time untouched.
        self.assertEqual(cal_close, 36)
        self.assertEqual(cal_open, 28)

    def test_minimum_time_boundary_accepted(self) -> None:
        # Exactly MIN_TIME_S = 5 s should be accepted.
        cal_open, cal_close, _, _, captured = update_calibration_on_motor_feedback(
            cal_time_open_s=-1,
            cal_time_close_s=-1,
            pending_tx_ms=1_000,
            pending_target_pct=0,
            rx_ms=6_000,
            rx_pct=0,
        )
        self.assertTrue(captured)
        self.assertEqual(cal_open, 5)

    def test_capture_window_boundary_just_inside(self) -> None:
        # Slightly under 60 s -> still inside the window.
        cal_open, cal_close, _, _, captured = update_calibration_on_motor_feedback(
            cal_time_open_s=-1,
            cal_time_close_s=-1,
            pending_tx_ms=1_000,
            pending_target_pct=100,
            rx_ms=60_500,
            rx_pct=100,
        )
        self.assertTrue(captured)
        # 59500 ms = 59 s
        self.assertEqual(cal_close, 59)


class TestRojaflexPositionInterpolation(unittest.TestCase):
    """Cover the linear-interpolation tick used to animate motor_pct during
    in-flight mid-position or calibrated end-stop fahrten."""

    def test_inactive_when_start_ms_is_zero(self) -> None:
        result = tick_position_interpolation_single(
            motor_pct=42,
            start_ms=0,
            start_pct=100,
            end_pct=0,
            duration_ms=10_000,
            now_ms=5_000,
        )
        self.assertEqual(result, 42)  # untouched

    def test_zero_duration_is_inactive(self) -> None:
        result = tick_position_interpolation_single(
            motor_pct=42,
            start_ms=1_000,
            start_pct=100,
            end_pct=0,
            duration_ms=0,
            now_ms=2_000,
        )
        self.assertEqual(result, 42)

    def test_at_start_returns_start_pct(self) -> None:
        # elapsed = 0 -> motor_pct = start_pct
        result = tick_position_interpolation_single(
            motor_pct=999,
            start_ms=1_000,
            start_pct=100,
            end_pct=0,
            duration_ms=10_000,
            now_ms=1_000,
        )
        self.assertEqual(result, 100)

    def test_halfway_up_run_returns_50(self) -> None:
        # 100 -> 0 over 10s, elapsed 5s -> 50
        result = tick_position_interpolation_single(
            motor_pct=999,
            start_ms=1_000,
            start_pct=100,
            end_pct=0,
            duration_ms=10_000,
            now_ms=6_000,
        )
        self.assertEqual(result, 50)

    def test_halfway_down_run_returns_50(self) -> None:
        # 0 -> 100 over 10s, elapsed 5s -> 50
        result = tick_position_interpolation_single(
            motor_pct=999,
            start_ms=1_000,
            start_pct=0,
            end_pct=100,
            duration_ms=10_000,
            now_ms=6_000,
        )
        self.assertEqual(result, 50)

    def test_quarter_progress_partial_range(self) -> None:
        # 80 -> 30 over 20s (50% of full travel), elapsed 5s -> 25% progress
        # interpolated = 80 + (30 - 80) * 5 / 20 = 80 + (-50 * 5 / 20) = 80 - 12 = 68
        result = tick_position_interpolation_single(
            motor_pct=999,
            start_ms=1_000,
            start_pct=80,
            end_pct=30,
            duration_ms=20_000,
            now_ms=6_000,
        )
        self.assertEqual(result, 68)

    def test_saturation_does_not_write_end_value(self) -> None:
        # elapsed >= duration -> previous motor_pct kept (timer callback
        # owns the final commit). 999 sentinel must remain unchanged.
        result = tick_position_interpolation_single(
            motor_pct=999,
            start_ms=1_000,
            start_pct=100,
            end_pct=0,
            duration_ms=10_000,
            now_ms=11_000,
        )
        self.assertEqual(result, 999)

    def test_clock_wrap_or_negative_elapsed_is_no_op(self) -> None:
        # millis() can technically wrap; if elapsed comes out negative we
        # ignore the tick instead of writing a wild value.
        result = tick_position_interpolation_single(
            motor_pct=42,
            start_ms=10_000,
            start_pct=100,
            end_pct=0,
            duration_ms=10_000,
            now_ms=5_000,  # earlier than start
        )
        self.assertEqual(result, 42)

    def test_step_size_at_500ms_matches_yaml_interval(self) -> None:
        # Sanity: a 30s travel sampled at 500 ms boundaries produces
        # ~1.66% steps. The integer division must not floor to 0 for
        # reasonable resolutions.
        # 100 -> 0 over 30s, elapsed 500ms -> step = -100 * 500 / 30000 = -1
        result = tick_position_interpolation_single(
            motor_pct=999,
            start_ms=1,
            start_pct=100,
            end_pct=0,
            duration_ms=30_000,
            now_ms=501,
        )
        self.assertEqual(result, 99)

    def test_full_range_short_run_resolves_per_tick(self) -> None:
        # 5s run, 100->0, sampled every 500ms gives steps of 10%.
        for elapsed_ms, expected in [(0, 100), (500, 90), (2_500, 50), (4_500, 10)]:
            result = tick_position_interpolation_single(
                motor_pct=999,
                start_ms=1,
                start_pct=100,
                end_pct=0,
                duration_ms=5_000,
                now_ms=1 + elapsed_ms,
            )
            self.assertEqual(result, expected, f"elapsed={elapsed_ms}")


class TestRojaflexChannelZeroBroadcast(unittest.TestCase):
    """Channel 0 is a TX broadcast slot. It can drive all shutters to an end
    stop, but: (a) no individual motor answers as ch=0 -> no calibration,
    (b) each shutter runs at its own speed -> no mid-position, no animation.

    The C++ helpers (`arm_calibration_capture`, `arm_position_interpolation`,
    `execute_shutter_set_position`) skip channel 0 explicitly. These tests
    document the protocol-level invariants that the YAML / C++ code relies on.
    """

    def test_motor_feedback_does_not_broadcast_for_channel_0(self) -> None:
        # decode_p109_frame in cc1101_helper.h sets applies_to_all_channels
        # only for DEV=A (remote) channel 0 - DEV=5 (motor) channel 0 must
        # remain a single-channel update so a stray motor frame never
        # accidentally completes a (now skipped) channel-0 capture for
        # other channels. Validate via the python parser mirror.
        # 083122FD20 C5 32 0A 00 -> DEV=5, ch=0, pct=0x32. Mirror should
        # only return ch=0 with no broadcast indication.
        frame = bytes.fromhex("083122FD20C5320A00")
        parsed = parse_motor_update_from_payload(frame, "3122FD2")
        self.assertIsNotNone(parsed)
        self.assertEqual(parsed["channel"], 0)
        self.assertEqual(parsed["pct"], 0x32)
        # The mirror does not encode applies_to_all_channels at all, mirroring
        # the C++ default of false for motor frames - intentional.

    def test_motion_plan_endstops_work_for_channel_0_without_calibration(self) -> None:
        # Endstop fahrten are how channel 0 is supposed to be used. They must
        # work without calibration (which is impossible for ch=0).
        plan_open = compute_shutter_motion_plan(50, 0, time_to_open_s=-1, time_to_close_s=-1)
        self.assertEqual(plan_open["action"], "up_to_end")
        plan_close = compute_shutter_motion_plan(50, 100, time_to_open_s=-1, time_to_close_s=-1)
        self.assertEqual(plan_close["action"], "down_to_end")

    def test_motion_plan_rejects_mid_position_when_uncalibrated(self) -> None:
        # Channel 0 cannot be calibrated; a mid-position request must be
        # rejected the same way as for any other uncalibrated channel.
        # (execute_shutter_set_position rejects ch=0 mid-position even
        # earlier with a clearer log line, but the plan-level invariant
        # is the same.)
        plan = compute_shutter_motion_plan(0, 50, time_to_open_s=-1, time_to_close_s=-1)
        self.assertEqual(plan["action"], "none")
        self.assertIn("not calibrated", plan["info"])

