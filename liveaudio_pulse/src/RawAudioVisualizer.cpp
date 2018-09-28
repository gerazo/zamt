#include "zamt/liveaudio_pulse/RawAudioVisualizer.h"

#ifdef ZAMT_MODULE_VIS_GTK

#include "zamt/core/ModuleCenter.h"

#include <cassert>

namespace zamt {

const char* RawAudioVisualizer::kVisualizationTitle = "Audio In";

RawAudioVisualizer::RawAudioVisualizer(const ModuleCenter* mc) : mc_(mc) {
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
  // TODO: Put data away
  Visualization& vis = mc_->Get<Visualization>();
  vis.QueryRender(window_id_, std::bind(&RawAudioVisualizer::Draw, this,
                                        std::placeholders::_1));
}

void RawAudioVisualizer::UpdateStatistics(LiveAudio::StereoSample* /*packet*/,
                                          int /*stereo_samples*/,
                                          Scheduler::Time /*timestamp*/) {
  // TODO
}

void RawAudioVisualizer::Draw(const Cairo::RefPtr<Cairo::Context>& /*cctx*/) {
  // TODO
}

}  // namespace zamt

#endif
