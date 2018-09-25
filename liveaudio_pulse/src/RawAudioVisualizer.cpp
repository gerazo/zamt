#include "zamt/liveaudio_pulse/RawAudioVisualizer.h"

#ifdef ZAMT_MODULE_VIS_GTK

#include "zamt/core/ModuleCenter.h"
#include "zamt/vis_gtk/Visualization.h"

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

void RawAudioVisualizer::Show(LiveAudio::StereoSample* /*packet*/,
                              int /*stereo_samples*/,
                              Scheduler::Time /*timestamp*/) {
  // TODO: Put data away
  // TODO: Process some statistics
  // TODO: Ask for render pass vis.QueryRender(window_id_, )
}

}  // namespace zamt

#endif
