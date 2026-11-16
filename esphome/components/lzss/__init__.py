"""LZSS compression with a preset dictionary, for short blocks of data such as packets."""

CODEOWNERS = ["@clydebarrow"]
DOMAIN = "lzss"

# A match reaches back at most this many bytes (WINDOW in lzss.h), so a longer dictionary would not help
MAX_DICTIONARY_SIZE = 512
