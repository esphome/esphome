"""Completion callbacks during a blocking speaker write must remain accounted."""

from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[3]


def test_completion_during_partial_write(tmp_path: Path) -> None:
    source = (
        ROOT / "esphome/components/speaker_source/speaker_source_media_player.cpp"
    ).read_text()

    def method(signature: str) -> str:
        start = source.index(signature)
        opening = source.index("{", start)
        depth, end = 1, opening + 1
        while depth:
            depth += (source[end] == "{") - (source[end] == "}")
            end += 1
        return source[start:end]

    stub = r"""
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstddef>
#include <cassert>
#include <functional>
#include <cstdio>
void vTaskDelay(unsigned) {}
unsigned pdMS_TO_TICKS(unsigned x) {return x;}
namespace audio {struct AudioStreamInfo {
  uint32_t bytes_to_frames(size_t n) const {return n/2;}
  bool operator!=(const AudioStreamInfo&)const{return false;}
};}
namespace media_source {struct MediaSource {
  uint32_t completed=0;
  void notify_audio_played(uint32_t n,int64_t){completed+=n;}
};}
struct Sink {
  size_t accept=0; std::function<void()> during_write;
  audio::AudioStreamInfo get_audio_stream_info(){return {};}
  void set_audio_stream_info(const audio::AudioStreamInfo&){}
  size_t play(const uint8_t*,size_t,unsigned){during_write();return accept;}
};
struct PipelineContext {
  std::atomic<media_source::MediaSource*> active_source{nullptr};
  std::atomic<uint32_t> pending_frames{0}; Sink *speaker;
};
struct SpeakerSourceMediaPlayer {
 PipelineContext pipelines_[1];
 void handle_speaker_playback_callback_(uint32_t,int64_t,uint8_t);
 size_t handle_media_output_(uint8_t,media_source::MediaSource*,const uint8_t*,size_t,uint32_t,const audio::AudioStreamInfo&);
};
"""
    checks = r"""
int main() {
 for(unsigned queued:{0u,20u}) for(unsigned accepted:{0u,60u,100u}) for(unsigned early:{0u,accepted/2,accepted}) {
  SpeakerSourceMediaPlayer p;media_source::MediaSource src;Sink sink;
  p.pipelines_[0].active_source=&src;p.pipelines_[0].speaker=&sink;
  p.pipelines_[0].pending_frames=queued;sink.accept=accepted*2;
  sink.during_write=[&]{p.handle_speaker_playback_callback_(queued+early,100,0);};
  uint8_t data[200]{};
  assert(p.handle_media_output_(0,&src,data,200,20,{})==accepted*2);
  p.handle_speaker_playback_callback_(accepted-early,200,0);
  std::printf("accepted=%u completed=%u pending=%u\n",accepted,src.completed,p.pipelines_[0].pending_frames.load());
  assert(src.completed==queued+accepted);
  assert(p.pipelines_[0].pending_frames==0);
 }
}
"""

    code = stub + method(
        "void SpeakerSourceMediaPlayer::handle_speaker_playback_callback_"
    )
    code += (
        "\n" + method("size_t SpeakerSourceMediaPlayer::handle_media_output_") + checks
    )
    cpp = tmp_path / "completion.cpp"
    cpp.write_text(code)
    binary = tmp_path / "completion"
    subprocess.run(
        ["g++", "-std=c++20", str(cpp), "-o", str(binary)],
        check=True,
        capture_output=True,
        text=True,
    )
    subprocess.run(
        ["bash", "-c", 'ulimit -c 0; exec "$1"', "bash", str(binary)],
        check=True,
        capture_output=True,
        text=True,
    )
