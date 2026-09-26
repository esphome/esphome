"""Exercise stop ownership using the production player command/state handlers."""

from pathlib import Path
import shutil
import subprocess
import sys

import pytest

ROOT = Path(__file__).resolve().parents[3]


def test_stop_pending_and_active_sources(tmp_path: Path) -> None:
    compiler = shutil.which("g++") or shutil.which("clang++")
    if compiler is None:
        pytest.skip("A C++20 host compiler is required")
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
#include <atomic>
#include <cassert>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>
namespace media_source {
enum class MediaSourceState {IDLE, PLAYING};
enum class MediaSourceCommand {PLAY, PAUSE, STOP, NEXT, PREVIOUS, REPEAT_ONE,
 REPEAT_OFF, REPEAT_ALL, CLEAR_PLAYLIST, SHUFFLE, UNSHUFFLE};
struct MediaSource {
 MediaSourceState state=MediaSourceState::IDLE;
 bool internal=false; int stops=0; std::function<void()> on_stop;
 bool has_internal_playlist(){return internal;}
 MediaSourceState get_state(){return state;}
 void handle_command(MediaSourceCommand cmd){
  if(cmd==MediaSourceCommand::STOP){++stops;if(on_stop)on_stop();}
 }
};
}
namespace media_player {
enum MediaPlayerCommand {MEDIA_PLAYER_COMMAND_TOGGLE, MEDIA_PLAYER_COMMAND_PLAY,
 MEDIA_PLAYER_COMMAND_PAUSE, MEDIA_PLAYER_COMMAND_STOP, MEDIA_PLAYER_COMMAND_NEXT,
 MEDIA_PLAYER_COMMAND_PREVIOUS, MEDIA_PLAYER_COMMAND_REPEAT_ONE, MEDIA_PLAYER_COMMAND_REPEAT_OFF,
 MEDIA_PLAYER_COMMAND_REPEAT_ALL, MEDIA_PLAYER_COMMAND_CLEAR_PLAYLIST, MEDIA_PLAYER_COMMAND_SHUFFLE,
 MEDIA_PLAYER_COMMAND_UNSHUFFLE};
}
enum Repeat {REPEAT_OFF,REPEAT_ONE,REPEAT_ALL};
enum class MediaPlayerControlCommand {PLAY_CURRENT,PLAYLIST_ADVANCE};
constexpr uint32_t PIPELINE_TIMEOUT_IDS[]={1};
struct Speaker {void finish(){}};
struct PipelineContext {
 std::atomic<media_source::MediaSource*> active_source{nullptr};
 media_source::MediaSource *pending_source=nullptr,*last_source=nullptr,*stopping_source=nullptr;
 Speaker sink; Speaker *speaker=&sink;
 std::vector<std::string> playlist;
 std::vector<size_t> shuffle_indices;
 size_t playlist_index=0;Repeat repeat_mode=REPEAT_OFF;
};
struct SpeakerSourceMediaPlayer {
 PipelineContext pipelines_[1];std::vector<MediaPlayerControlCommand> commands;
 void handle_player_command_(media_player::MediaPlayerCommand,uint8_t);
 void handle_media_state_changed_(uint8_t,media_source::MediaSource*,media_source::MediaSourceState);
 void cancel_timeout(uint32_t){}
 void queue_command_(MediaPlayerControlCommand c,uint8_t){commands.push_back(c);}
 void shuffle_playlist_(uint8_t){}
 void unshuffle_playlist_(uint8_t){}
 size_t get_playlist_position_(uint8_t){return 0;}
};
"""
    checks = r"""
int main(){
 using namespace media_source;
 constexpr auto stop=media_player::MEDIA_PLAYER_COMMAND_STOP;
 // Opening source is still IDLE and sends no extra state callback on cancellation.
 {
  SpeakerSourceMediaPlayer p;MediaSource opening,previous;
  auto &ps=p.pipelines_[0];ps.pending_source=&opening;ps.last_source=&previous;
  ps.playlist={"first","second"};p.handle_player_command_(stop,0);
  assert(opening.stops==1 && previous.stops==0);
  assert(ps.pending_source==nullptr && ps.stopping_source==nullptr && ps.playlist.empty());
  assert(p.commands.empty());
 }
 // Explicit stop must not queue the next item, whether IDLE arrives now or later.
 for(bool immediate:{false,true}){
  SpeakerSourceMediaPlayer p;MediaSource playing;auto &ps=p.pipelines_[0];
  playing.state=MediaSourceState::PLAYING;ps.active_source=&playing;
  auto finish=[&]{playing.state=MediaSourceState::IDLE;p.handle_media_state_changed_(0,&playing,playing.state);};
  if(immediate)playing.on_stop=finish;
  p.handle_player_command_(stop,0);
  if(!immediate)finish();
  assert(playing.stops==1 && ps.active_source==nullptr && ps.stopping_source==nullptr);
  assert(p.commands.empty());
 }
 // Natural completion still advances the playlist.
 {
  SpeakerSourceMediaPlayer p;MediaSource playing;p.pipelines_[0].active_source=&playing;
  p.handle_media_state_changed_(0,&playing,MediaSourceState::IDLE);
  assert(p.commands.size()==1 && p.commands[0]==MediaPlayerControlCommand::PLAYLIST_ADVANCE);
 }
}
"""
    cpp = tmp_path / "stop.cpp"
    cpp.write_text(
        stub
        + method("void SpeakerSourceMediaPlayer::handle_player_command_")
        + method("void SpeakerSourceMediaPlayer::handle_media_state_changed_")
        + checks
    )
    binary = tmp_path / ("stop.exe" if sys.platform == "win32" else "stop")
    subprocess.run(
        [compiler, "-std=c++20", str(cpp), "-o", str(binary)],
        check=True,
        capture_output=True,
        text=True,
    )
    subprocess.run([str(binary)], check=True, capture_output=True, text=True)
