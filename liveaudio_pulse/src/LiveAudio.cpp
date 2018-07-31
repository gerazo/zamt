#include "zamt/liveaudio_pulse/LiveAudio.h"

#include "zamt/core/CLIParameters.h"
#include "zamt/core/Core.h"
#include "zamt/core/Log.h"
#include "zamt/core/ModuleCenter.h"
#include "zamt/core/Scheduler.h"

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
    const pa_buffer_attr* buffer_attr = pa_stream_get_buffer_attr(la->stream_);
    assert(buffer_attr);
    la->log_->LogMessage("Sample rate: ", la->sample_rate_, "Hz");
    la->log_->LogMessage(
        "Total buffer size: ",
        (int)buffer_attr->maxlength / zamt::LiveAudio::kChannels, " samples");
    la->log_->LogMessage(
        "Fragment size: ",
        (int)buffer_attr->fragsize / zamt::LiveAudio::kChannels, " samples");
  }
}

void stream_read_callback(pa_stream* p, size_t /*nbytes*/, void* userdata) {
  zamt::LiveAudio* la = (zamt::LiveAudio*)userdata;
  assert(p == la->stream_);
  (void)p;
  const void* data = nullptr;
  size_t nbytes = 0;
  int err = pa_stream_peek(la->stream_, &data, &nbytes);
  assert(err == 0);
  (void)err;
  if (nbytes == 0) return;
  if (data == nullptr) {
    // TODO: pass silence
  } else {
    // TODO: pass data
  }
  err = pa_stream_drop(la->stream_);
  assert(err == 0);
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
    requested_latency_ = req_latency * kChannels;
  } else {
    // Heuristics to stay slightly below realtime with nice fragment size
    int latency = 16;
    int exact_latency = requested_sample_rate_ * kRealtimeLatencyInMs / 1000;
    while (latency < exact_latency) latency <<= 1;
    int step = latency >> 3;
    while (latency > exact_latency) latency -= step;
    requested_latency_ = latency * kChannels;
  }
  audio_loop_should_run_.store(true, std::memory_order_release);
}

LiveAudio::~LiveAudio() {
  log_->LogMessage("Stopping...");
  if (!IsRunning()) return;
  log_->LogMessage("Waiting for audio thread to stop...");
  audio_loop_should_run_.store(false, std::memory_order_release);
  audio_loop_->join();
  log_->LogMessage("Audio thread stopped.");
}

void LiveAudio::Initialize(const ModuleCenter* mc) {
  mc_ = mc;
  if (!audio_loop_should_run_.load(std::memory_order_acquire)) return;
  scheduler_ = &mc_->Get<Core>().scheduler();
  scheduler_->RegisterSource(scheduler_id_, 512, 16);
  log_->LogMessage("Launching audio thread...");
  audio_loop_.reset(new std::thread(&LiveAudio::RunMainLoop, this));
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
  buffer_attr.maxlength = (uint32_t)kAudioBufferSize;
  buffer_attr.tlength = (uint32_t)-1;
  buffer_attr.prebuf = (uint32_t)-1;
  buffer_attr.minreq = (uint32_t)-1;
  buffer_attr.fragsize = (uint32_t)requested_latency_;
  pa_stream_flags_t flags = (pa_stream_flags_t)(PA_STREAM_AUTO_TIMING_UPDATE |
                                                PA_STREAM_INTERPOLATE_TIMING |
                                                PA_STREAM_ADJUST_LATENCY);
  int err;
  err = pa_stream_connect_record(stream_, source_name, &buffer_attr, flags);
  assert(err == 0);
  (void)err;
}

void LiveAudio::PrintHelp() {
  Log::Print("ZAMT Live Audio Module using PulseAudio input");
  Log::Print(
      " -arNum         Set requested sample rate to Num Hz instead of the"
      " default 44100Hz.");
  Log::Print(
      " -atNum         Set requested latency to Num samples instead of the"
      " automatic setting putting latency below 10ms.");
  Log::Print(" -al            List all available audio sources.");
  Log::Print(
      " -adSrcNumber   Use SrcNumber audio source from the list of sources"
      " instead of the default one.");
}

}  // namespace zamt