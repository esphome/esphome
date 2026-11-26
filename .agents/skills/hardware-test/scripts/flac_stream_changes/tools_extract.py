"""tools_extract.py HEADER OUT: write the bytes of a C array header (micro-flac's
examples/decode_benchmark/main/test_audio_flac*.h) to OUT."""

import pathlib
import re
import sys

txt = pathlib.Path(sys.argv[1]).read_text(encoding="utf-8")
body = txt[txt.index("{", txt.index("[]")) + 1 : txt.rindex("}")]
data = bytes(int(x, 16) for x in re.findall(r"0x([0-9a-fA-F]{2})", body))
pathlib.Path(sys.argv[2]).write_bytes(data)
print(sys.argv[2], len(data))
