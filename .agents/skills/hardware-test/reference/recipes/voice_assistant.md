# voice_assistant

## Voice PE late TTS start (#19408)

Only the subscribed client (Home Assistant) may send `VoiceAssistantAnnounceRequest`, so both builds carry
`scripts/voice_announce/slow-announce.yaml`, a template button that calls `id(va).on_announce(msg)` with
`start_conversation = true` and a URL on the host. `drive.py HOST KEY DELAY OUT` serves a FLAC (WAV as `audio/wav`
is rejected by micro_decoder) only after DELAY seconds, presses the button and timestamps the device logs. It
holds the voice assistant subscription throughout, since without a subscriber the device drops the announcement
state at once. It imports `va_common.py` from `scripts/voice_wake_word_audio/`, so keep the two directories side
by side.
`run.sh DEVICE LABEL=CONFIG=TREE...` builds, OTAs and runs both builds twice. The player gives up after 6 header
attempts of 5 s.

The wake word audio flow is in [`speaker.md`](speaker.md).
