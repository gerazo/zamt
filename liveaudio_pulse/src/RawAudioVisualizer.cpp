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
  for (int i = 0; i < stereo_samples; ++i) {
    LiveAudio::Sample center =
        (LiveAudio::Sample)((packet[i].left + packet[i].right) >> 1);
    LiveAudio::Sample side =
        (LiveAudio::Sample)((packet[i].left - packet[i].right) >> 1);
    center_buffer_.pop_front();
    center_buffer_.push_back(center);
    side_buffer_.pop_front();
    side_buffer_.push_back(side);
  }
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
  cctx->save();
  cctx->set_source_rgb(0.0, 0.0, 0.0);
  cctx->paint();
  cctx->set_line_width(1.0);
  cctx->begin_new_path();
  double x_diff = (double)width / kVisualizationBufferSize;
  double middle = (double)(height >> 1);
  double value_coef = height / 65536.0;
  cctx->set_source_rgb(1.0, 1.0, 1.0);
  cctx->move_to(0.0, middle + center_buffer_[0] * value_coef);
  for (
      struct {
        size_t i;
        double x;
      } c = {1, x_diff};
      c.i < (size_t)kVisualizationBufferSize; ++c.i, c.x += x_diff)
    cctx->line_to(c.x, middle + center_buffer_[c.i] * value_coef);
  cctx->stroke();
  cctx->set_source_rgba(1.0, 1.0, 1.0, 0.5);
  cctx->move_to(0.0, middle + side_buffer_[0] * value_coef);
  for (
      struct {
        size_t i;
        double x;
      } c = {1, x_diff};
      c.i < (size_t)kVisualizationBufferSize; ++c.i, c.x += x_diff)
    cctx->line_to(c.x, middle + side_buffer_[c.i] * value_coef);
  cctx->stroke();
  cctx->restore();
}

}  // namespace zamt

#endif
