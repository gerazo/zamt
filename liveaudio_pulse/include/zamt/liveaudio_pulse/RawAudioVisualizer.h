#ifndef ZAMT_LIVEAUDIO_PULSE_RAWAUDIOVISUALIZER_H_
#define ZAMT_LIVEAUDIO_PULSE_RAWAUDIOVISUALIZER_H_

#ifdef ZAMT_MODULE_VIS_GTK

#include <deque>

#include "zamt/core/Scheduler.h"
#include "zamt/liveaudio_pulse/LiveAudio.h"
#include "zamt/vis_gtk/Visualization.h"

namespace zamt {

class ModuleCenter;
class Visualization;

class RawAudioVisualizer {
 public:
  const static char* kVisualizationTitle;
  const static int kVisualizationWidth = 640;
  const static int kVisualizationHeight = 480;
  const static int kVisualizationBufferSize = 512;

  RawAudioVisualizer(const ModuleCenter* mc);
  ~RawAudioVisualizer();

  void Show(LiveAudio::StereoSample* packet, int stereo_samples,
            Scheduler::Time timestamp);

 private:
  void UpdateStatistics(LiveAudio::StereoSample* packet, int stereo_samples,
                        Scheduler::Time timestamp);
  void Draw(const Cairo::RefPtr<Cairo::Context>& cctx, int width, int height);

  const ModuleCenter* mc_;
  int window_id_;
  std::deque<LiveAudio::Sample> center_buffer_;
  std::deque<LiveAudio::Sample> side_buffer_;
};

}  // namespace zamt

#endif

#endif  // ZAMT_LIVEAUDIO_PULSE_RAWAUDIOVISUALIZER_H_
