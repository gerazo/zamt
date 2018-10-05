#include "zamt/liveaudio_pulse/RawAudioVisualizer.h"

#ifdef ZAMT_MODULE_VIS_GTK

#include "zamt/core/ModuleCenter.h"

#include <cassert>

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
  Visualization& vis = mc_->Get<Visualization>();
  vis.QueryRender(
      window_id_,
      std::bind(&RawAudioVisualizer::Draw, this, std::placeholders::_1,
                std::placeholders::_2, std::placeholders::_3));
}

void RawAudioVisualizer::UpdateStatistics(LiveAudio::StereoSample* /*packet*/,
                                          int /*stereo_samples*/,
                                          Scheduler::Time /*timestamp*/) {
  // TODO
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
  cctx->restore();
}

}  // namespace zamt

#endif
