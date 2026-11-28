# audio_http host tests

Run with `script/cpp_unit_test.py audio_http`. The production adapter, the micro-decoder reader and decoder
pthreads and FLAC decoding are real; only the HTTP client and the FreeRTOS headers (borrowed from
speaker_source) are fixtures. Apple's linker has no `--wrap`, so on macOS the worker create and join counts are
not checked; the decoded samples still are.

`reference.pcm.bin` is 16000 signed 16-bit little-endian mono samples at 16 kHz, `(i * 73) % 30000 - 15000`;
`fixture.flac.bin` encodes exactly those bytes.
