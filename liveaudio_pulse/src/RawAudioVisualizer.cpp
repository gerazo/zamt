#include "zamt/liveaudio_pulse/RawAudioVisualizer.h"

#ifdef ZAMT_MODULE_VIS_GTK

#include "zamt/core/ModuleCenter.h"

#include <cassert>
#include <chrono>
#include <cmath>

namespace zamt {

const char* RawAudioVisualizer::kVisualizationTitle = "Audio In";

RawAudioVisualizer::RawAudioVisualizer(const ModuleCenter* mc)
    : mc_(mc),
      center_buffer_(kVisualizationBufferSize, 0),
      side_buffer_(kVisualizationBufferSize, 0) {
  assert(mc_);
  buffer_position_ = 0;
  Visualization& vis = mc_->Get<Visualization>();
  vis.OpenWindow(kVisualizationTitle, kVisualizationWidth, kVisualizationHeight,
                 window_id_);
}

RawAudioVisualizer::~RawAudioVisualizer() {
  assert(mc_);
  Visualization& vis = mc_->Get<Visualization>();
  vis.CloseWindow(window_id_);
}

void RawAudioVisualizer::Show(LiveAudio::StereoSample* packet,
                              int stereo_samples, Scheduler::Time timestamp) {
  UpdateStatistics(packet, stereo_samples, timestamp);
  UpdateBuffer(packet, stereo_samples);
  Visualization& vis = mc_->Get<Visualization>();
  vis.QueryRender(
      window_id_,
      std::bind(&RawAudioVisualizer::Draw, this, std::placeholders::_1,
                std::placeholders::_2, std::placeholders::_3));
}

void RawAudioVisualizer::UpdateStatistics(LiveAudio::StereoSample* packet,
                                          int stereo_samples,
                                          Scheduler::Time timestamp) {
  while (statistics_mutex_.test_and_set(std::memory_order_acquire))
    ;
  buffers_in_stat_++;
  Scheduler::Time current_time =
      (Scheduler::Time)std::chrono::duration_cast<std::chrono::microseconds>(
          std::chrono::high_resolution_clock::now().time_since_epoch())
          .count();
  int64_t latency = (int64_t)(current_time - timestamp);
  sum_latency_us_ += latency;
  if (latency < (int64_t)min_latency_us_) min_latency_us_ = (int)latency;
  if (latency > (int64_t)max_latency_us_) max_latency_us_ = (int)latency;
  for (int i = 0; i < stereo_samples; ++i) {
    int mono_sample = (packet[i].left + packet[i].right) >> 1;
    sample_square_sum_ += mono_sample * mono_sample;
    samples_in_stat_++;
  }
  statistics_mutex_.clear(std::memory_order_release);
}

void RawAudioVisualizer::ClearStatistics(int& max_latency_us,
                                         int& min_latency_us,
                                         int& avg_latency_us, int& buffers,
                                         int& samples, float& rms_db) {
  while (statistics_mutex_.test_and_set(std::memory_order_acquire))
    ;
  max_latency_us = max_latency_us_;
  max_latency_us_ = -99999999;
  min_latency_us = min_latency_us_;
  min_latency_us_ = 99999999;
  buffers = buffers_in_stat_;
  buffers_in_stat_ = 0;
  samples = samples_in_stat_;
  samples_in_stat_ = 0;
  avg_latency_us = (int)(sum_latency_us_ / (buffers ? buffers : 1));
  sum_latency_us_ = 0;
  rms_db =
      (float)log10(sqrt(sample_square_sum_ / (samples ? samples : 1))) * 20.0f;
  sample_square_sum_ = 0.0;
  statistics_mutex_.clear(std::memory_order_release);
}

void RawAudioVisualizer::UpdateBuffer(LiveAudio::StereoSample* packet,
                                      int stereo_samples) {
  while (buffer_mutex_.test_and_set(std::memory_order_acquire))
    ;
  for (int i = 0; i < stereo_samples; ++i) {
    LiveAudio::Sample center =
        (LiveAudio::Sample)((packet[i].left + packet[i].right) >> 1);
    LiveAudio::Sample side =
        (LiveAudio::Sample)((packet[i].left - packet[i].right) >> 1);
    center_buffer_[(size_t)buffer_position_] = center;
    side_buffer_[(size_t)buffer_position_] = side;
    if (++buffer_position_ >= kVisualizationBufferSize) buffer_position_ = 0;
  }
  buffer_mutex_.clear(std::memory_order_release);
}

void RawAudioVisualizer::Draw(const Cairo::RefPtr<Cairo::Context>& cctx,
                              int width, int height) {
  float middle = (float)(height >> 1);
  float value_coef = (float)height / 65536.0f;
  float center[kVisualizationBufferSize];
  float side[kVisualizationBufferSize];
  while (buffer_mutex_.test_and_set(std::memory_order_acquire))
    ;
  int pos = buffer_position_;
  for (int i = 0; i < kVisualizationBufferSize; ++i) {
    center[(size_t)i] = middle + center_buffer_[(size_t)pos] * value_coef;
    side[(size_t)i] = middle + side_buffer_[(size_t)pos] * value_coef;
    if (++pos >= kVisualizationBufferSize) pos = 0;
  }
  buffer_mutex_.clear(std::memory_order_release);

  int max_latency, min_latency, avg_latency, buffers, samples;
  float rms;
  ClearStatistics(max_latency, min_latency, avg_latency, buffers, samples, rms);

  cctx->save();
  cctx->set_source_rgb(0.0, 0.0, 0.0);
  cctx->paint();

  float x_diff = (float)width / kVisualizationBufferSize;
  int i;
  float x;
  cctx->begin_new_path();
  cctx->set_line_width(0.5);
  cctx->set_source_rgb(0.75, 0.75, 0.75);
  cctx->move_to(0.0, side[0]);
  for (i = 1, x = x_diff; i < kVisualizationBufferSize; ++i, x += x_diff)
    cctx->line_to(x, side[(size_t)i]);
  cctx->stroke();

  cctx->set_line_width(1.0);
  cctx->set_source_rgb(1.0, 1.0, 1.0);
  cctx->move_to(0.0, center[0]);
  for (i = 1, x = x_diff; i < kVisualizationBufferSize; ++i, x += x_diff)
    cctx->line_to(x, center[(size_t)i]);
  cctx->stroke();

  float rms_bar = rms / 100 * (float)(height >> 1);
  cctx->set_source_rgba(1.0, 1.0, 0.0, 0.75);
  cctx->rectangle(4, middle - rms_bar, 40, rms_bar * 2);
  cctx->fill();

  char str[128];
  cctx->set_source_rgb(0.0, 1.0, 0.0);
  sprintf(str, "Min latency %.3f ms", min_latency / 1000.0);
  cctx->move_to(64, 20);
  cctx->show_text(str);
  sprintf(str, "Max latency %.3f ms", max_latency / 1000.0);
  cctx->move_to(64, 40);
  cctx->show_text(str);
  sprintf(str, "Avg latency %.3f ms", avg_latency / 1000.0);
  cctx->move_to(64, 60);
  cctx->show_text(str);
  sprintf(str, "Buffers %d", buffers);
  cctx->move_to(64, 80);
  cctx->show_text(str);
  sprintf(str, "Samples %d", samples);
  cctx->move_to(64, 100);
  cctx->show_text(str);
  sprintf(str, "SPL %.2f db", (double)rms);
  cctx->move_to(64, 120);
  cctx->show_text(str);

  cctx->restore();
}

}  // namespace zamt

#endif
