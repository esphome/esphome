"""vadrive.py HOST: play one fake Home Assistant voice pipeline against the device's voice_assistant.

Presses the "VA Start" template button, then sends run, STT, intent and TTS stream events with audio, so the
component walks through every state. API_KEY from the env (apiconn.py).
"""

import asyncio
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
from aioesphomeapi import ButtonInfo, VoiceAssistantEventType as E  # noqa: E402
from apiconn import connect  # noqa: E402


async def main(host: str) -> None:
    cli = await connect(host)
    started = asyncio.Event()

    async def on_start(conv, flags, audio, wake) -> int:
        print("handle_start", flush=True)
        started.set()
        return 0

    async def on_stop(abort) -> None:
        print("handle_stop", abort, flush=True)

    async def on_audio(*args) -> None:
        """Audio from the device is not checked."""

    cli.subscribe_voice_assistant(
        handle_start=on_start, handle_stop=on_stop, handle_audio=on_audio
    )
    ents, _ = await cli.list_entities_services()
    key = next(
        e.key for e in ents if isinstance(e, ButtonInfo) and e.name == "VA Start"
    )
    await asyncio.sleep(1)
    cli.button_command(key)
    await asyncio.wait_for(started.wait(), 10)
    await asyncio.sleep(1.5)
    ev = cli.send_voice_assistant_event
    ev(E.VOICE_ASSISTANT_RUN_START, {})
    ev(E.VOICE_ASSISTANT_STT_START, {})
    ev(E.VOICE_ASSISTANT_STT_VAD_START, {})
    await asyncio.sleep(1)
    ev(E.VOICE_ASSISTANT_STT_VAD_END, {})
    await asyncio.sleep(1)
    ev(E.VOICE_ASSISTANT_STT_END, {"text": "hello"})
    ev(E.VOICE_ASSISTANT_INTENT_START, {})
    ev(E.VOICE_ASSISTANT_INTENT_END, {"conversation_id": "abc"})
    ev(E.VOICE_ASSISTANT_TTS_START, {"text": "Hi there"})
    ev(E.VOICE_ASSISTANT_TTS_STREAM_START, {})
    await asyncio.sleep(0.3)
    ev(E.VOICE_ASSISTANT_TTS_END, {"url": "http://127.0.0.1/x.flac"})
    await asyncio.sleep(0.5)
    for _ in range(8):
        cli.send_voice_assistant_audio(bytes(1024))
        await asyncio.sleep(0.05)
    ev(E.VOICE_ASSISTANT_TTS_STREAM_END, {})
    await asyncio.sleep(3)
    ev(E.VOICE_ASSISTANT_RUN_END, {})
    await asyncio.sleep(3)
    print("DONE", flush=True)
    await cli.disconnect()


asyncio.run(main(sys.argv[1]))
