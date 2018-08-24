#include "zamt/liveaudio_pulse/LiveAudio.h"

#include "zamt/core/CLIParameters.h"
#include "zamt/core/Core.h"
#include "zamt/core/Log.h"
#include "zamt/core/ModuleCenter.h"

#include <pulse/context.h>
#include <pulse/def.h>
#include <pulse/error.h>
#include <pulse/introspect.h>
#include <pulse/mainloop.h>
#include <pulse/operation.h>
#include <pulse/proplist.h>
#include <pulse/stream.h>
#include <pulse/timeval.h>

#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstring>

namespace zamt_liveaudio_internal {

void context_notify_callback(pa_context* c, void* userdata) {
  zamt::LiveAudio* la = (zamt::LiveAudio*)userdata;
  assert(c == la->context_);
  (void)c;
  if (la->HadNormalOpen()) return;
  pa_context_state_t ctxst = pa_context_get_state(la->context_);
  if (ctxst == PA_CONTEXT_FAILED || ctxst == PA_CONTEXT_TERMINATED) {
    la->log_->LogMessage("PulseAudio context opening failed.");
    // la->log_->LogMessage(pa_strerror(pa_context_errno(la->context_)));
    la->audio_loop_should_run_.store(false, std::memory_order_release);
    pa_mainloop_quit(la->mainloop_, 1);
    la->mc_->Get<zamt::Core>().Quit(zamt::Core::kExitCodeAudioProblem);
    return;
  }
  if (ctxst == PA_CONTEXT_READY) {
    la->log_->LogMessage("PulseAudio context opened.");
    pa_operation* op = nullptr;
    if (la->selected_device_ == zamt::LiveAudio::kDeviceListSelected) {
      zamt::Log::Print("List of PulseAudio sources:");
      op = pa_context_get_source_info_list(la->context_, source_info_callback,
                                           la);
      assert(op);
      pa_operation_unref(op);
      la->sample_rate_ = 1;  // fake normal init for normal exit
    } else if (la->selected_device_ !=
               zamt::LiveAudio::kDefaultDeviceSelected) {
      op = pa_context_get_source_info_by_index(la->context_,
                                               (uint32_t)la->selected_device_,
                                               source_info_callback, la);
      assert(op);
      pa_operation_unref(op);
    } else {
      source_info_callback(la->context_, nullptr, 0, la);
    }
  }
}

void source_info_callback(pa_context* c, const pa_source_info* srci, int eol,
                          void* userdata) {
  const int kStrBufLength = 64;
  zamt::LiveAudio* la = (zamt::LiveAudio*)userdata;
  assert(c == la->context_);
  (void)c;
  if (la->selected_device_ == zamt::LiveAudio::kDeviceListSelected) {
    if (eol != 0) {
      zamt::Log::Print("End of PulseAudio sources.");
      la->audio_loop_should_run_.store(false, std::memory_order_release);
      pa_mainloop_quit(la->mainloop_, 2);
      la->mc_->Get<zamt::Core>().Quit(zamt::Core::kExitCodeAudioProblem);
      return;
    }
    assert(srci);
    char s[kStrBufLength];
    sprintf(s, "  %d. ", (int)srci->index);
    strncat(s, srci->description, kStrBufLength - 8);
    zamt::Log::Print(s);
    return;
  }
  if (la->stream_) return;  // OpenStream was already called
  const char* selected_device_name = nullptr;
  if (la->selected_device_ != zamt::LiveAudio::kDefaultDeviceSelected &&
      eol == 0) {
    assert(srci);
    selected_device_name = srci->name;
  }
  la->OpenStream(selected_device_name);
}

void stream_notify_callback(pa_stream* p, void* userdata) {
  zamt::LiveAudio* la = (zamt::LiveAudio*)userdata;
  assert(p == la->stream_);
  (void)p;
  if (la->HadNormalOpen()) return;
  pa_stream_state_t strst = pa_stream_get_state(la->stream_);
  if (strst == PA_STREAM_FAILED || strst == PA_STREAM_TERMINATED) {
    la->log_->LogMessage("PulseAudio stream opening failed.");
    la->audio_loop_should_run_.store(false, std::memory_order_release);
    pa_mainloop_quit(la->mainloop_, 1);
    la->mc_->Get<zamt::Core>().Quit(zamt::Core::kExitCodeAudioProblem);
    return;
  }
  if (strst == PA_STREAM_READY) {
    la->log_->LogMessage("Stream connected to source:");
    la->log_->LogMessage(pa_stream_get_device_name(la->stream_));
    const pa_sample_spec* sample_spec = pa_stream_get_sample_spec(la->stream_);
    assert(sample_spec);
    assert(sample_spec->channels == zamt::LiveAudio::kChannels);
    assert(sample_spec->format == PA_SAMPLE_S16LE);
    la->sample_rate_ = (int)sample_spec->rate;
    la->usec_per_sample_shl_ =
        (1000000 << zamt::LiveAudio::kUSecPerSampleShift) / la->sample_rate_;
    const pa_buffer_attr* buffer_attr = pa_stream_get_buffer_attr(la->stream_);
    assert(buffer_attr);
    la->hw_fragment_size_ =
        (int)buffer_attr->fragsize / zamt::LiveAudio::kChannels;
    la->log_->LogMessage("Sample rate: ", la->sample_rate_, "Hz");
    la->log_->LogMessage(
        "Total hardware buffer size: ",
        (int)buffer_attr->maxlength / zamt::LiveAudio::kChannels, " samples");
    la->log_->LogMessage(
        "Average hardware fragment size: ", la->hw_fragment_size_, " samples");
    la->hw_latency_in_us_ = 1000000 * la->hw_fragment_size_ / la->sample_rate_;
  }
}

void stream_read_callback(pa_stream* p, size_t nbytes, void* userdata) {
  zamt::LiveAudio* la = (zamt::LiveAudio*)userdata;
  assert(p == la->stream_);
  (void)p;
  assert(nbytes > 0);
  const void* data = nullptr;
  size_t bytes_in_buf = 0;
  int err;
  while (nbytes > 0) {
    err = pa_stream_peek(la->stream_, &data, &bytes_in_buf);
    assert(err == 0);
    nbytes -= bytes_in_buf;
    if (bytes_in_buf == 0) return;
    assert((int)bytes_in_buf % (int)sizeof(zamt::LiveAudio::StereoSample) == 0);
    la->ProcessFragment(
        (zamt::LiveAudio::StereoSample*)data,
        (int)bytes_in_buf / (int)sizeof(zamt::LiveAudio::StereoSample));
    err = pa_stream_drop(la->stream_);
    assert(err == 0);
  }
  (void)err;
}

}  // namespace zamt_liveaudio_internal

namespace zamt {

const char* LiveAudio::kModuleLabel = "liveaudio_pulse";
const char* LiveAudio::kApplicationName = "ZAMT";
const char* LiveAudio::kApplicationID = "zamt";
const char* LiveAudio::kMediaRole = "music";
const char* LiveAudio::kDeviceListParamStr = "-al";
const char* LiveAudio::kDeviceSelectParamStr = "-ad";
const char* LiveAudio::kLatencyParamStr = "-at";
const char* LiveAudio::kSampleRateParamStr = "-ar";

LiveAudio::LiveAudio(int argc, const char* const* argv)
    : audio_loop_should_run_(false) {
  CLIParameters cli(argc, argv);
  log_.reset(new Log(kModuleLabel, cli));
  log_->LogMessage("Starting...");
  if (cli.HasParam(Core::kHelpParamStr)) {
    PrintHelp();
    return;
  }
  scheduler_id_ = ModuleCenter::GetId<LiveAudio>();
  if (cli.HasParam(kDeviceListParamStr))
    selected_device_ = kDeviceListSelected;
  else {
    int selected_dev = cli.GetNumParam(kDeviceSelectParamStr);
    if (selected_dev != CLIParameters::kNotFound)
      selected_device_ = selected_dev;
  }
  int req_sample_rate = cli.GetNumParam(kSampleRateParamStr);
  if (req_sample_rate != CLIParameters::kNotFound)
    requested_sample_rate_ = req_sample_rate;
  int req_latency = cli.GetNumParam(kLatencyParamStr);
  if (req_latency != CLIParameters::kNotFound) {
    requested_overall_latency_ = req_latency;
  } else {
    int exact_latency = requested_sample_rate_ * kOverallLatencyInMs / 1000;
    requested_overall_latency_ = exact_latency;
  }
  audio_loop_should_run_.store(true, std::memory_order_release);
}

LiveAudio::~LiveAudio() {
  if (!WasStarted()) return;
  log_->LogMessage("Waiting for audio thread to stop...");
  audio_loop_->join();
  log_->LogMessage("Audio thread stopped.");
  if (sample_buffer_) delete[] sample_buffer_;
}

void LiveAudio::Initialize(const ModuleCenter* mc) {
  mc_ = mc;
  if (!audio_loop_should_run_.load(std::memory_order_acquire)) return;

  // Heuristic to find power of 2 submit buffer size and hw latency so overall
  // stays below limit and hw buffer is preferably larger
  log_->LogMessage("Requested overall latency: ", requested_overall_latency_,
                   " samples");
  submit_buffer_size_ = 65536;
  while (submit_buffer_size_ > requested_overall_latency_ >> 1)
    submit_buffer_size_ >>= 1;
  hw_fragment_size_ = requested_overall_latency_ - submit_buffer_size_;
  assert(hw_fragment_size_ >= submit_buffer_size_);
  log_->LogMessage("Requested hardware latency: ", hw_fragment_size_,
                   " samples");
  log_->LogMessage("Submit buffer size: ", submit_buffer_size_, " samples");
  int queue_capacity = requested_sample_rate_ *
                           kMaxLatencyForHardwareBufferInMs / 1000 /
                           submit_buffer_size_ +
                       1;
  log_->LogMessage("Queue capacity: ", queue_capacity, " packets");

  sample_buffer_ = new StereoSample[submit_buffer_size_];
  assert(sample_buffer_);

  Core& core = mc_->Get<Core>();
  core.RegisterForQuitEvent(
      std::bind(&LiveAudio::Shutdown, this, std::placeholders::_1));
  scheduler_ = &core.scheduler();
  scheduler_->RegisterSource(scheduler_id_,
                             submit_buffer_size_ * (int)sizeof(StereoSample),
                             queue_capacity);
  log_->LogMessage("Launching audio thread...");
  audio_loop_.reset(new std::thread(&LiveAudio::RunMainLoop, this));
}

void LiveAudio::Shutdown(int /*exit_code*/) {
  log_->LogMessage("Stopping...");
  if (!WasStarted()) return;
  audio_loop_should_run_.store(false, std::memory_order_release);
}

void LiveAudio::RunMainLoop() {
  log_->LogMessage("Audio mainloop starting up...");
  int err;
  proplist_ = pa_proplist_new();
  err = pa_proplist_sets(proplist_, PA_PROP_APPLICATION_ID, kApplicationID);
  assert(err == 0);
  err = pa_proplist_sets(proplist_, PA_PROP_APPLICATION_NAME, kApplicationName);
  assert(err == 0);
  err = pa_proplist_sets(proplist_, PA_PROP_MEDIA_ROLE, kMediaRole);
  assert(err == 0);
  mainloop_ = pa_mainloop_new();
  assert(mainloop_);
  context_ = pa_context_new_with_proplist(pa_mainloop_get_api(mainloop_),
                                          kApplicationName, proplist_);
  assert(context_);
  pa_context_set_state_callback(
      context_, zamt_liveaudio_internal::context_notify_callback, this);
  err = pa_context_connect(context_, nullptr, PA_CONTEXT_NOFAIL, nullptr);
  assert(err >= 0);

  while (audio_loop_should_run_.load(std::memory_order_acquire)) {
    err = pa_mainloop_prepare(mainloop_, PA_MSEC_PER_SEC * kWatchDogSeconds);
    assert(err >= 0);
    err = pa_mainloop_poll(mainloop_);
    assert(err >= 0);
    pa_mainloop_dispatch(mainloop_);
  }

  log_->LogMessage("Audio mainloop stopping...");
  if (stream_) {
    pa_stream_disconnect(stream_);
    pa_stream_unref(stream_);
  }
  if (context_) {
    pa_context_disconnect(context_);
    pa_context_unref(context_);
  }
  if (mainloop_) pa_mainloop_free(mainloop_);
  if (proplist_) pa_proplist_free(proplist_);
  (void)err;
}

void LiveAudio::OpenStream(const char* source_name) {
  log_->LogMessage("Opening source stream...");
  pa_sample_spec sample_spec;
  sample_spec.format = PA_SAMPLE_S16LE;
  sample_spec.rate = (uint32_t)requested_sample_rate_;
  sample_spec.channels = kChannels;
  pa_channel_map channel_map;
  // pa_channel_map_init_mono(&channel_map);
  pa_channel_map_init_stereo(&channel_map);
  assert(pa_channel_map_valid(&channel_map));
  stream_ = pa_stream_new_with_proplist(context_, kApplicationID, &sample_spec,
                                        &channel_map, proplist_);
  assert(stream_);
  pa_stream_set_state_callback(
      stream_, zamt_liveaudio_internal::stream_notify_callback, this);
  pa_stream_set_read_callback(
      stream_, zamt_liveaudio_internal::stream_read_callback, this);

  pa_buffer_attr buffer_attr;
  int hw_buffer_size =
      requested_sample_rate_ * kMaxLatencyForHardwareBufferInMs / 1000;
  buffer_attr.maxlength = (uint32_t)hw_buffer_size * kChannels;
  buffer_attr.tlength = (uint32_t)-1;
  buffer_attr.prebuf = (uint32_t)-1;
  buffer_attr.minreq = (uint32_t)-1;
  buffer_attr.fragsize = (uint32_t)hw_fragment_size_ * kChannels;
  pa_stream_flags_t flags = (pa_stream_flags_t)(
      PA_STREAM_AUTO_TIMING_UPDATE | PA_STREAM_INTERPOLATE_TIMING |
      PA_STREAM_NOT_MONOTONIC | PA_STREAM_ADJUST_LATENCY);
  int err;
  err = pa_stream_connect_record(stream_, source_name, &buffer_attr, flags);
  assert(err == 0);
  (void)err;
}

void LiveAudio::ProcessFragment(StereoSample* buffer, int samples) {
  Scheduler::Time current_time =
      (Scheduler::Time)std::chrono::duration_cast<std::chrono::microseconds>(
          std::chrono::high_resolution_clock::now().time_since_epoch())
          .count();
  assert(sample_buffer_);
  assert(sample_buffer_filled_ >= 0 &&
         sample_buffer_filled_ < submit_buffer_size_);
  assert(samples > 0);
  if (!audio_loop_should_run_.load(std::memory_order_acquire)) return;
  pa_usec_t latency;
  int is_negative;
  int err = pa_stream_get_latency(stream_, &latency, &is_negative);
  if (err == -PA_ERR_NODATA) {
    // fake it (this may be the 1st buffer and no timing update was done)
    assert(hw_latency_in_us_ > 0);
    latency = (pa_usec_t)hw_latency_in_us_;
    is_negative = 0;
  }
  Scheduler::Time buffer_timestamp;
  if (is_negative)
    buffer_timestamp = current_time + latency;
  else
    buffer_timestamp = current_time - latency;
  assert(usec_per_sample_shl_ > 0);

  assert(scheduler_);
  while (samples > 0) {
    int free_left_in_buffer = submit_buffer_size_ - sample_buffer_filled_;
    assert(free_left_in_buffer > 0);

    if (samples >= free_left_in_buffer) {
      StereoSample* packet =
          (StereoSample*)scheduler_->GetPacketForSubmission(scheduler_id_);
      assert(packet);
      if (packet == nullptr) {
        // drop buffer and signal error
        log_->LogMessage("Buffer overrun, data lost!!!");
        return;
      }

      if (sample_buffer_filled_ > 0) {
        memcpy(packet, sample_buffer_,
               (size_t)sample_buffer_filled_ * sizeof(StereoSample));
      }
      if (buffer)
        memcpy(packet + sample_buffer_filled_, buffer,
               (size_t)free_left_in_buffer * sizeof(StereoSample));
      else
        memset(packet + sample_buffer_filled_, 0,
               (size_t)free_left_in_buffer * sizeof(StereoSample));
      samples -= free_left_in_buffer;
      assert(samples >= 0);
      buffer += free_left_in_buffer;

      Scheduler::Time timestamp =
          buffer_timestamp -
          (unsigned)(sample_buffer_filled_ * usec_per_sample_shl_ >>
                     kUSecPerSampleShift);
      if (timestamp <= last_timestamp_) timestamp = last_timestamp_ + 1;
      last_timestamp_ = timestamp;
      scheduler_->SubmitPacket(scheduler_id_, (Scheduler::Byte*)packet,
                               timestamp);

      buffer_timestamp +=
          (unsigned)(free_left_in_buffer * usec_per_sample_shl_ >>
                     kUSecPerSampleShift);
      sample_buffer_filled_ = 0;
    } else {
      if (buffer)
        memcpy(sample_buffer_ + sample_buffer_filled_, buffer,
               (size_t)samples * sizeof(StereoSample));
      else
        memset(sample_buffer_ + sample_buffer_filled_, 0,
               (size_t)samples * sizeof(StereoSample));
      sample_buffer_filled_ += samples;
      assert(sample_buffer_filled_ < submit_buffer_size_);
      break;
    }
  }
}

void LiveAudio::PrintHelp() {
  Log::Print("ZAMT Live Audio Module using PulseAudio input");
  Log::Print(
      " -arNum         Set requested sample rate to Num Hz instead of the"
      " default 44100Hz.");
  Log::Print(
      " -atNum         Set requested latency to Num samples instead of the"
      " automatic setting putting latency to 10ms.");
  Log::Print(" -al            List all available audio sources.");
  Log::Print(
      " -adSrcNumber   Use SrcNumber audio source from the list of sources"
      " instead of the default one.");
}

}  // namespace zamt
