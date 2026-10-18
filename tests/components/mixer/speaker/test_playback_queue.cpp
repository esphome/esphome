#include "../common.h"

#include <deque>
#include <condition_variable>
#include <mutex>
#include <random>
#include <thread>

namespace esphome::mixer_speaker::testing {

static PlaybackQueue::Completions complete_output(PlaybackQueue &queue, uint32_t frames) {
  auto result = queue.consume(frames);
  for (uint8_t i = 0; i < PlaybackQueue::MAX_SOURCES; ++i) {
    if (result[i].frames != 0)
      queue.acknowledge(i, result[i].frames);
  }
  return result;
}

TEST(MixerPlaybackQueue, SourceRemainsPendingUntilNotificationReturns) {
  PlaybackQueue queue;
  ASSERT_TRUE(queue.append(3, 100));
  const auto completed = queue.consume(100);
  EXPECT_EQ(completed[0].frames, 100);
  EXPECT_EQ(queue.pending_frames(), 0);
  // SourceSpeaker::loop must not retire/restart the source before its listeners
  // have received this completion, including while another source is notified.
  EXPECT_EQ(queue.pending_frames(0), 100);
  EXPECT_EQ(queue.pending_frames(1), 100);
  queue.acknowledge(0, completed[0].frames);
  EXPECT_EQ(queue.pending_frames(0), 0);
  EXPECT_EQ(queue.pending_frames(1), 100);
  queue.acknowledge(1, completed[1].frames);
  EXPECT_EQ(queue.pending_frames(1), 0);
}

TEST(MixerPlaybackQueue, DiscardDoesNotEraseAnInFlightNotification) {
  PlaybackQueue queue;
  ASSERT_TRUE(queue.append(3, 100));
  const auto completed = queue.consume(100);
  ASSERT_TRUE(queue.append(3, 50));
  queue.discard_source(0);
  EXPECT_EQ(queue.pending_frames(0), 100);
  EXPECT_EQ(queue.pending_frames(1), 150);
  queue.clear();
  EXPECT_EQ(queue.pending_frames(), 0);
  EXPECT_EQ(queue.pending_frames(0), 100);
  EXPECT_EQ(queue.pending_frames(1), 100);
  queue.acknowledge(0, completed[0].frames);
  queue.acknowledge(1, completed[1].frames);
  EXPECT_EQ(queue.pending_frames(0), 0);
  EXPECT_EQ(queue.pending_frames(1), 0);
}

TEST(MixerPlaybackQueue, OtherSourcesDoNotFillAGap) {
  PlaybackQueue queue;
  ASSERT_TRUE(queue.append(1, 100));
  ASSERT_TRUE(queue.append(2, 100));
  ASSERT_TRUE(queue.append(1, 100));
  EXPECT_EQ(complete_output(queue, 100)[0].frames, 100);
  const auto gap = complete_output(queue, 100);
  EXPECT_EQ(gap[0].frames, 0);
  EXPECT_EQ(gap[1].frames, 100);
  EXPECT_EQ(queue.pending_frames(0), 100);
  EXPECT_EQ(complete_output(queue, 100)[0].frames, 100);
  EXPECT_EQ(queue.pending_frames(), 0);
}

TEST(MixerPlaybackQueue, OverlappingSourcesShareOutputFrames) {
  PlaybackQueue queue;
  ASSERT_TRUE(queue.append(3, 100));
  EXPECT_EQ(queue.pending_frames(), 100);
  EXPECT_EQ(queue.pending_frames(0), 100);
  EXPECT_EQ(queue.pending_frames(1), 100);
  const auto completed = complete_output(queue, 100);
  EXPECT_EQ(completed[0].frames, 100);
  EXPECT_EQ(completed[1].frames, 100);
  EXPECT_EQ(queue.pending_frames(), 0);
}

TEST(MixerPlaybackQueue, CompletionEndsAtTheSourcesLastFrame) {
  PlaybackQueue queue;
  ASSERT_TRUE(queue.append(1, 20));
  ASSERT_TRUE(queue.append(2, 10));
  ASSERT_TRUE(queue.append(1, 5));
  ASSERT_TRUE(queue.append(2, 15));
  const auto completed = complete_output(queue, 50);
  EXPECT_EQ(completed[0].frames, 25);
  EXPECT_EQ(completed[0].trailing_frames, 15);
  EXPECT_EQ(completed[1].frames, 25);
  EXPECT_EQ(completed[1].trailing_frames, 0);
}

TEST(MixerPlaybackQueue, CapacityAppliesBackpressureWithoutConsumingInput) {
  PlaybackQueue queue;
  for (uint8_t i = 0; i < PlaybackQueue::CAPACITY; ++i)
    ASSERT_TRUE(queue.append(1U << (i % 2), 1));
  EXPECT_FALSE(queue.append(1, 2));
  EXPECT_EQ(queue.pending_frames(), 32);
  EXPECT_EQ(queue.pending_frames(0), 16);
  // A contiguous contribution needs no new metadata slot.
  EXPECT_TRUE(queue.append(2, 2));
  EXPECT_EQ(complete_output(queue, 1)[0].frames, 1);
  EXPECT_TRUE(queue.append(1, 2));
  EXPECT_EQ(queue.pending_frames(), 35);
  const auto completed = complete_output(queue, 35);
  EXPECT_EQ(completed[0].frames, 17);
  EXPECT_EQ(completed[1].frames, 18);
}

TEST(MixerPlaybackQueue, RestartDoesNotInheritDiscardedSourceFrames) {
  PlaybackQueue queue;
  ASSERT_TRUE(queue.append(3, 100));
  queue.discard_source(0);
  ASSERT_TRUE(queue.append(1, 50));
  EXPECT_EQ(queue.pending_frames(), 150);
  const auto old_output = complete_output(queue, 100);
  EXPECT_EQ(old_output[0].frames, 0);
  EXPECT_EQ(old_output[1].frames, 100);
  EXPECT_EQ(queue.pending_frames(0), 50);
  EXPECT_EQ(complete_output(queue, 50)[0].frames, 50);
}

TEST(MixerPlaybackQueue, SharedStopDiscardsEverySourcesPendingFrames) {
  PlaybackQueue queue;
  ASSERT_TRUE(queue.append(3, 100));
  queue.clear();
  EXPECT_EQ(queue.pending_frames(), 0);
  EXPECT_EQ(queue.pending_frames(0), 0);
  EXPECT_EQ(queue.pending_frames(1), 0);
  EXPECT_EQ(complete_output(queue, 100)[0].frames, 0);
  queue.clear();
  ASSERT_TRUE(queue.append(2, 20));
  EXPECT_EQ(complete_output(queue, 20)[1].frames, 20);
}

TEST(MixerPlaybackQueue, ZeroAndExcessCompletionDoNotUnderflow) {
  PlaybackQueue queue;
  ASSERT_TRUE(queue.append(1, 0));
  EXPECT_EQ(complete_output(queue, 100)[0].frames, 0);
  ASSERT_TRUE(queue.append(1, 20));
  EXPECT_EQ(complete_output(queue, 0)[0].frames, 0);
  const auto completed = complete_output(queue, 30);
  EXPECT_EQ(completed[0].frames, 20);
  EXPECT_EQ(completed[0].trailing_frames, 10);
  EXPECT_EQ(queue.pending_frames(), 0);
  EXPECT_EQ(queue.pending_frames(0), 0);
}

TEST(MixerPlaybackQueue, AllEightSourcesAndCounterLimit) {
  PlaybackQueue queue;
  const uint32_t maximum = std::numeric_limits<uint32_t>::max();
  ASSERT_TRUE(queue.append(0xff, maximum));
  EXPECT_FALSE(queue.append(0xff, 1));
  const auto completed = complete_output(queue, maximum);
  for (uint8_t i = 0; i < PlaybackQueue::MAX_SOURCES; ++i) {
    EXPECT_EQ(completed[i].frames, maximum);
    EXPECT_EQ(queue.pending_frames(i), 0);
  }
  EXPECT_EQ(queue.pending_frames(), 0);
}

TEST(MixerPlaybackQueue, PendingNotificationCannotOverflowOnNewInput) {
  PlaybackQueue queue;
  const uint32_t maximum = std::numeric_limits<uint32_t>::max();
  ASSERT_TRUE(queue.append(1, maximum));
  const auto completed = queue.consume(maximum);
  EXPECT_FALSE(queue.append(1, 1));
  EXPECT_EQ(queue.pending_frames(0), maximum);
  queue.acknowledge(0, completed[0].frames);
  EXPECT_TRUE(queue.append(1, 1));
}

TEST(MixerPlaybackQueue, MatchesIndependentPerFrameHistory) {
  PlaybackQueue queue;
  std::deque<uint8_t> output;
  std::mt19937 random(42);
  auto complete = [&](uint32_t requested) {
    std::array<uint32_t, 8> frames{}, trailing{};
    const auto count = std::min<size_t>(requested, output.size());
    for (size_t i = 0; i < count; ++i) {
      const uint8_t sources = output.front();
      output.pop_front();
      for (uint8_t source = 0; source < 8; ++source) {
        if (sources & (1U << source)) {
          ++frames[source];
          trailing[source] = requested - i - 1;
        }
      }
    }
    const auto actual = complete_output(queue, requested);
    for (uint8_t source = 0; source < 8; ++source) {
      ASSERT_EQ(actual[source].frames, frames[source]);
      if (frames[source] != 0)
        ASSERT_EQ(actual[source].trailing_frames, trailing[source]);
    }
  };
  for (unsigned step = 0; step < 10000; ++step) {
    const uint32_t count = 1 + random() % 31;
    const uint8_t sources = random() & 0xff;
    if (step % 97 == 0) {
      const uint8_t source = random() % 8;
      queue.discard_source(source);
      for (auto &mask : output)
        mask &= ~(1U << source);
    } else if (random() % 3 == 0) {
      complete(count);
    } else if (queue.append(sources, count)) {
      for (uint32_t i = 0; i < count; ++i)
        output.push_back(sources);
    } else {
      complete(count);
    }
    ASSERT_EQ(queue.pending_frames(), output.size());
  }
  while (!output.empty())
    complete(17);
  EXPECT_EQ(queue.pending_frames(), 0);
}

TEST(MixerPlaybackQueue, ProducerAndCompletionUseSeparateTasks) {
  PlaybackQueue queue;
  std::mutex mutex;
  std::condition_variable changed;
  bool done = false;
  std::array<uint32_t, 2> expected{}, observed{};
  std::thread producer([&]() {
    for (unsigned i = 0; i < 10000; ++i) {
      const uint8_t sources = 1 + i % 3;
      std::unique_lock lock(mutex);
      while (!queue.append(sources, 7))
        changed.wait(lock);
      for (uint8_t source = 0; source < 2; ++source) {
        if (sources & (1U << source))
          expected[source] += 7;
      }
      changed.notify_all();
    }
    std::lock_guard lock(mutex);
    done = true;
    changed.notify_all();
  });
  std::thread consumer([&]() {
    for (;;) {
      PlaybackQueue::Completions completed;
      {
        std::unique_lock lock(mutex);
        changed.wait(lock, [&]() { return done || queue.pending_frames() != 0; });
        if (done && queue.pending_frames() == 0)
          break;
        completed = queue.consume(17);
        changed.notify_all();
      }
      // Listener delivery is outside the queue lock, as in MixerSpeaker::setup().
      for (uint8_t source = 0; source < 2; ++source) {
        observed[source] += completed[source].frames;
        EXPECT_GE(queue.pending_frames(source), completed[source].frames);
        queue.acknowledge(source, completed[source].frames);
      }
    }
  });
  producer.join();
  consumer.join();
  EXPECT_EQ(observed, expected);
  EXPECT_EQ(queue.pending_frames(), 0);
  EXPECT_EQ(queue.pending_frames(0), 0);
  EXPECT_EQ(queue.pending_frames(1), 0);
}

}  // namespace esphome::mixer_speaker::testing
