#ifndef ZAMT_LIVEAUDIO_PULSE_RAWAUDIOVISUALIZER_H_
#define ZAMT_LIVEAUDIO_PULSE_RAWAUDIOVISUALIZER_H_

#ifdef ZAMT_MODULE_VIS_GTK

#include <atomic>
#include <vector>

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
  void ClearStatistics(int& max_latency_us, int& min_latency_us,
                       int& avg_latency_us, int& buffers, int& samples,
                       float& rms_db);
  void UpdateBuffer(LiveAudio::StereoSample* packet, int stereo_samples);
  void Draw(const Cairo::RefPtr<Cairo::Context>& cctx, int width, int height);

  const ModuleCenter* mc_;
  int window_id_;
  int buffer_position_;
  std::atomic_flag buffer_mutex_ = ATOMIC_FLAG_INIT;
  std::vector<LiveAudio::Sample> center_buffer_;
  std::vector<LiveAudio::Sample> side_buffer_;

  std::atomic_flag statistics_mutex_ = ATOMIC_FLAG_INIT;
  int max_latency_us_ = -99999999;
  int min_latency_us_ = 99999999;
  int buffers_in_stat_ = 0;
  int samples_in_stat_ = 0;
  int64_t sum_latency_us_ = 0;
  double sample_square_sum_ = 0.0;
};

}  // namespace zamt

#endif

#endif  // ZAMT_LIVEAUDIO_PULSE_RAWAUDIOVISUALIZER_H_
