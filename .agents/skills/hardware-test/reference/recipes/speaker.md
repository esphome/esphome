# speaker

## Media player pipeline tasks (#21203)

The `speaker` media player platform (`speaker/media_player/`) creates `ann_read`, `ann_decode`, `med_read` and
`med_decode` FreeRTOS tasks on every play and deletes them after a stop. The AUX-2 and sendspin packages use the
`speaker_source` player instead, which has its own pipeline and never runs that code, so a test of
`speaker/media_player/*` has to swap the player: `templates/aux-2-preprod-speaker-player-dut.yaml` removes the
package's `external_media_player` with `id: !remove` and adds a `speaker` platform player on the package's resampler
speakers, plus a "Task names" button whose lambda calls `xTaskGetHandle()` on the four names and logs internal free
heap. Serve FLAC files from the host (`ffmpeg -f lavfi -i "sine=frequency=440:duration=90" -ac 2 -ar 48000 -c:a flac
media90.flac`, a 3 s mono one for announcements, `python3 -m http.server 8765 --directory www`); `audio_http`
refuses WAV and MP3. `scripts/speaker_media_player/cases.sh HOST KEY MEDIA_URL ANN_URL CAPTURE LABEL` runs three
cycles through `drive.py` (`play:URL | ann:URL | stop | press:NAME | wait:S` on one API connection) with the probe
pressed idle, during media, during the announcement, after it and after the stop. Pass: idle none, media `med_*`
only, announcement all four, after stop none, every cycle. API commands land a second or two late over Wi-Fi, so read
the probe against the `State changed to` lines rather than the driver's timestamps. Internal free heap after a stop
moved by 2.5 kB between two boots of the same build, so a heap delta needs more than one boot per build. Results:
https://github.com/esphome/esphome/pull/21203#issuecomment-6299581943
