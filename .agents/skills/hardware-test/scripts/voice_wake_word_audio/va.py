"""Act as Home Assistant for a voice satellite with wake word audio and answer each wake word run per MODES.

usage: va.py HOST KEY OUTDIR MODE[,MODE...]
MODE: accept | reject | timeout | noflag (the whole session without the wake_word_audio flag)

Say the wake word near the device for every run (run.sh plays it). Each run's two audio channels are saved as
OUTDIR/run<N>-<mode>-ch<1|2>.wav with a JSON summary (wake word id, probability, wake word audio sizes); every
summary is also printed as a RESULT line, followed by "SUMMARY MODE CH1_MS". Exits 1 when a run's audio stream never
ended, a verdict task failed, the session timed out, or a START came before READY or during a run (that run's RESULT
says incomplete); a START after the last run is ignored. Needs aioesphomeapi 46.27.0 or later.
"""

import asyncio
from dataclasses import dataclass, field
import json
import pathlib
import re
import sys
import time
import wave

from aioesphomeapi import APIClient, VoiceAssistantEventType as E
from aioesphomeapi.model import LogLevel
from va_common import connect, log_text

HOST, KEY, MODES = sys.argv[1], sys.argv[2], sys.argv[4].split(",")
OUT = pathlib.Path(sys.argv[3])
NOFLAG = MODES[0] == "noflag"
OUT.mkdir(parents=True, exist_ok=True)
T0 = time.monotonic()


@dataclass
class Run:
    n: int
    mode: str
    ww: object
    a1: bytearray = field(default_factory=bytearray)
    a2: bytearray = field(default_factory=bytearray)
    started: float = field(default_factory=time.monotonic)
    first_audio: float | None = None
    stopped: asyncio.Event = field(default_factory=asyncio.Event)


@dataclass
class State:
    cli: APIClient | None = None
    run: Run | None = None
    count: int = 0
    pending: list[str] = field(default_factory=list)
    events: list[str] = field(default_factory=list)
    done_all: asyncio.Event = field(default_factory=asyncio.Event)
    tasks: list[asyncio.Task] = field(default_factory=list)
    failures: list[str] = field(default_factory=list)
    ready: bool = False


S = State()


def mark(what: str) -> None:
    line = f"{time.monotonic() - T0:8.3f}  {what}"
    S.events.append(line)
    print(line, flush=True)


def save(r: Run, incomplete: bool = False) -> None:
    for i, a in ((1, r.a1), (2, r.a2)):
        with wave.open(str(OUT / f"run{r.n}-{r.mode}-ch{i}.wav"), "wb") as w:
            w.setnchannels(1)
            w.setsampwidth(2)
            w.setframerate(16000)
            w.writeframes(bytes(a))
    ww = r.ww
    meta = {
        "n": r.n,
        "mode": r.mode,
        "a1": len(r.a1),
        "a2": len(r.a2),
        "ww": None
        if ww is None
        else {
            "id": ww.wake_word_id,
            "bytes": ww.audio_bytes,
            "bytes2": ww.audio_bytes2,
            "p": ww.probability,
            "cutoff": ww.probability_cutoff,
        },
        "stopped": r.stopped.is_set(),
        "incomplete": incomplete,
        "start_to_first_audio_ms": round((r.first_audio - r.started) * 1000)
        if r.first_audio
        else None,
    }
    (OUT / f"run{r.n}-{r.mode}.json").write_text(json.dumps(meta), encoding="utf-8")
    mark(f"RESULT {json.dumps(meta)}")
    mark(f"SUMMARY {r.mode} {round(len(r.a1) / 32)}")


async def verdict(r: Run) -> None:
    cli = S.cli
    if r.mode in ("accept", "noflag"):
        await asyncio.sleep(0.5)
        cli.send_voice_assistant_event(E.VOICE_ASSISTANT_RUN_START, None)
        cli.send_voice_assistant_event(E.VOICE_ASSISTANT_WAKE_WORD_END, None)
        mark(f"sent WAKE_WORD_END run {r.n}")
        cli.send_voice_assistant_event(E.VOICE_ASSISTANT_STT_START, None)
        await asyncio.sleep(2.5)
        cli.send_voice_assistant_event(E.VOICE_ASSISTANT_STT_END, {"text": "hwtest"})
        cli.send_voice_assistant_event(E.VOICE_ASSISTANT_RUN_END, None)
        mark(f"sent STT_END + RUN_END run {r.n}")
    elif r.mode == "reject":
        await asyncio.sleep(0.5)
        cli.send_voice_assistant_event(E.VOICE_ASSISTANT_WAKE_WORD_REJECTED, None)
        mark(f"sent WAKE_WORD_REJECTED run {r.n}")
    else:
        mark(f"sending no verdict run {r.n}")
    try:
        await asyncio.wait_for(r.stopped.wait(), 8)
    except TimeoutError:
        mark(f"RUN {r.n} device never ended the audio stream")
        S.failures.append(f"run {r.n} stream never ended")
    await asyncio.sleep(1.0)
    if S.run is r:
        save(r)
        S.run = None
        if not S.pending:
            S.done_all.set()
        else:
            mark(f"READY for run {S.count + 1} {S.pending[0]}")


async def handle_start(
    conv_id: str,
    flags: int,
    audio_settings: object,
    phrase: str | None,
    ww: object = None,
) -> int:
    if S.run is not None:
        mark(f"START while run {S.run.n} active, closing it as incomplete")
        S.failures.append(f"START during run {S.run.n}")
        save(S.run, incomplete=True)
        S.run = None
    if not S.pending:
        if S.ready:
            mark(f"START after the last run, ignored phrase={phrase!r}")
            S.done_all.set()  # the closed run may have been the last one
        else:
            mark(f"START before READY phrase={phrase!r}")
            S.failures.append("START before READY")
        return 0
    mode = S.pending.pop(0)
    S.count += 1
    S.run = Run(S.count, mode, ww)
    mark(f"START run {S.count} mode={mode} phrase={phrase!r} flags={flags} ww={ww}")
    S.tasks.append(asyncio.get_running_loop().create_task(verdict(S.run)))
    return 0


async def handle_stop(abort: bool) -> None:
    r = S.run
    mark(
        f"STOP run {r.n if r else '-'} abort={abort} a1={len(r.a1) if r else 0} a2={len(r.a2) if r else 0}"
    )
    if r:
        r.stopped.set()


async def handle_audio(d1: bytes, d2: bytes) -> None:
    if (r := S.run) is None:
        return
    if r.first_audio is None:
        r.first_audio = time.monotonic()
        mark(f"first audio chunk run {r.n} ({len(d1)} B)")
    r.a1 += d1
    r.a2 += d2


async def main() -> int:
    S.cli = cli = await connect(HOST, KEY)
    keep = re.compile(
        r"voice_assistant|micro_wake_word|wake|hwt|Free Memory|\[E\]|\[W\]"
    )

    def on_log(msg) -> None:
        line = log_text(msg)
        if keep.search(line) and "runtime_stats" not in line:
            mark("LOG " + line[:170])

    cli.subscribe_logs(on_log, log_level=LogLevel.LOG_LEVEL_DEBUG)
    info = await cli.device_info()
    mark(
        f"voice_assistant_feature_flags={info.voice_assistant_feature_flags:#x} esphome={info.esphome_version}"
    )
    cli.subscribe_voice_assistant(
        handle_start=handle_start,
        handle_stop=handle_stop,
        handle_audio=handle_audio,
        wake_word_audio=not NOFLAG,
    )
    await asyncio.sleep(8)
    S.pending.extend(MODES * (2 if NOFLAG else 1))
    S.ready = True
    mark(f"READY for run 1 {S.pending[0]}")
    try:
        await asyncio.wait_for(S.done_all.wait(), 30 + 30 * len(S.pending))
    except TimeoutError:
        mark("session timed out")
        S.failures.append("session timed out")
    await asyncio.sleep(3)
    for res in await asyncio.gather(*S.tasks, return_exceptions=True):
        if isinstance(res, BaseException):
            S.failures.append(f"verdict task failed: {res!r}")
    await cli.disconnect()
    if S.failures:
        mark(f"FAIL: {S.failures}")
    return 1 if S.failures else 0


rc = asyncio.run(main())
(OUT / "events.txt").write_text("\n".join(S.events) + "\n", encoding="utf-8")
sys.exit(rc)
