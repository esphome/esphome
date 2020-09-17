from esphome.core import HexInt

def get_md5sum(text):
  import subprocess
  process = subprocess.Popen(['md5sum'], stdin =subprocess.PIPE, stdout=subprocess.PIPE, universal_newlines=True)
  process.stdin.write(text)
  process.stdin.close()
  return bytes.fromhex(process.stdout.readline().split(" ")[0])

def get_md5sum_hexint(text, length=None):
    parts = get_md5sum(text)
    if length != None:
        parts = parts[:length]
    return [HexInt(i) for i in parts]
