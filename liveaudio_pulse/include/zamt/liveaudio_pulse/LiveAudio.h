#ifndef ZAMT_LIVEAUDIO_PULSE_LIVEAUDIO_H_
#define ZAMT_LIVEAUDIO_PULSE_LIVEAUDIO_H_

/// This LiveAudio implementation uses PulseAudio to record live audio stream
/// from different sources. Besides microphone and live inputs, system monitor
/// is also supported so other software generated input can also be used live.
/// The idea is to test how the system works in a realistic environment.
/// Own thread is used to interact with audio library for skipless recording.

#include "zamt/core/Module.h"
#include "zamt/core/Scheduler.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <thread>

struct pa_proplist;
struct pa_context;
struct pa_mainloop;
struct pa_source_info;
struct pa_stream;

namespace zamt_liveaudio_internal {

void context_notify_callback(pa_context* c, void* userdata);
void source_info_callback(pa_context* c, const pa_source_info* srci, int eol,
                          void* userdata);
void stream_notify_callback(pa_stream* p, void* userdata);
void stream_read_callback(pa_stream* p, size_t nbytes, void* userdata);

}  // namespace zamt_liveaudio_internal

namespace zamt {

class Log;
class Scheduler;

class LiveAudio : public Module {
 public:
  using Sample = int16_t;

  const static char* kModuleLabel;
  const static char* kApplicationName;
  const static char* kApplicationID;
  const static char* kMediaRole;
  const static char* kDeviceListParamStr;
  const static char* kDeviceSelectParamStr;
  const static char* kLatencyParamStr;
  const static char* kSampleRateParamStr;
  const static int kChannels = 2;  // stereo
  const static int kMaxLatencyForHardwareBufferInMs = 200;
  const static int kOverallLatencyInMs = 10;
  const static int kDefaultSampleRate = 44100;

  LiveAudio(int argc, const char* const* argv);
  ~LiveAudio();

  void Initialize(const ModuleCenter* mc);
  bool IsRunning() const { return (bool)audio_loop_; }
  int sample_rate() const { return sample_rate_; }
  int requested_overall_latency() const { return requested_overall_latency_; }

 private:
  const static int kWatchDogSeconds = 3;
  const static int kDefaultDeviceSelected = -1;
  const static int kDeviceListSelected = -2;

  friend void zamt_liveaudio_internal::context_notify_callback(pa_context* c,
                                                               void* userdata);
  friend void zamt_liveaudio_internal::source_info_callback(
      pa_context* c, const pa_source_info* srci, int eol, void* userdata);
  friend void zamt_liveaudio_internal::stream_notify_callback(pa_stream* p,
                                                              void* userdata);
  friend void zamt_liveaudio_internal::stream_read_callback(pa_stream* p,
                                                            size_t nbytes,
                                                            void* userdata);

  bool HadNormalOpen() const { return sample_rate_ != 0; }
  void RunMainLoop();
  void OpenStream(const char* source_name);
  void ProcessFragment(Sample* buffer, int samples);
  void PrintHelp();

  std::unique_ptr<Log> log_;
  const ModuleCenter* mc_ = nullptr;
  Scheduler* scheduler_ = nullptr;
  size_t scheduler_id_;
  int selected_device_ = kDefaultDeviceSelected;
  int requested_overall_latency_;
  int requested_sample_rate_ = kDefaultSampleRate;
  int submit_buffer_size_ = 0;
  int hw_fragment_size_ = 0;
  int sample_rate_ = 0;
  Scheduler::Time last_timestamp_;
  int hw_latency_in_us_ = 0;

  std::atomic<bool> audio_loop_should_run_;
  std::unique_ptr<std::thread> audio_loop_;
  pa_proplist* proplist_ = nullptr;
  pa_mainloop* mainloop_ = nullptr;
  pa_context* context_ = nullptr;
  pa_stream* stream_ = nullptr;
};

}  // namespace zamt

#endif  // ZAMT_LIVEAUDIO_PULSE_LIVEAUDIO_H_
