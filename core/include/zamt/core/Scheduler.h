#ifndef ZAMT_CORE_SCHEDULER_H_
#define ZAMT_CORE_SCHEDULER_H_

/// Does all job dispatching and scheduling in system.
/**
 * The main thread is used as the UI thread, dedicated to do certain tasks.
 * Other worker threads are created according to the number of CPUs.
 * Sources produce packets which are submitted to subscribed sinks.
 * A packet submission means a work unit for each sink.
 * Scheduling priority takes earliest packets 1st to minimize latency.
 * One source always produces fixed size packets for efficiency.
 * The scheduler labels all work units by the sample (time) they belong to.
 * All buffers between sources and sinks contain a fixed number of packets
 * which are allocated at configuration time (RegisterSource()).
 * It is a scaling problem when the number of packets in any queue is too low.
 * If a sink needs to get packets in order, it has to wait with yield() for
 * earlier jobs to finish.
 */

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace zamt {

class Scheduler {
 public:
  using Byte = uint8_t;
  using SourceId = size_t;
  using Time = uint64_t;
  using SinkCallback = void (*)(void* _this, SourceId source_id,
                                const Byte* packet, Time timestamp);

  /// Launches all worker threads.
  Scheduler();

  /// Waits all threads to finish before destruction.
  ~Scheduler();

  Scheduler(const Scheduler&) = delete;
  Scheduler(Scheduler&&) = delete;
  Scheduler& operator=(const Scheduler&) = delete;
  Scheduler& operator=(Scheduler&&) = delete;

  /**
   * Returns how many worker threads are created.
   * The original thread (the UI thread) is an extra thread in this regard.
   */
  int GetNumberOfWorkers() const;

  /**
   * Sources register the fixed packet size they produce
   * and the queue size used to transmit work units to sinks.
   * It is a slow operation done in configuration time.
   */
  void RegisterSource(SourceId source_id, int packet_size,
                      int packets_in_queue);

  /// Returns the fixed packet size a source is using.
  int GetPacketSize(SourceId source_id);

  /**
   * A sink registers itself via a callback into its code to get all
   * data packets produced by a source.
   * It can ask for its code to be run on the single UI thread.
   * It is a slow operation done in configuration time.
   */
  void Subscribe(SourceId source_id, SinkCallback sink_callback, void* _this,
                 bool on_UI);

  /**
   * A sink no longer wants to get packets from a source.
   * It is a slow operation done in configuration time.
   * Pending tasks will still be emitted if there are any.
   */
  void Unsubscribe(SourceId source_id, SinkCallback sink_callback);

  /// Caller source acquires a packet which can be loaded with data.
  Byte* GetPacketForSubmission(SourceId source_id);

  /// Packet is put into queue, all subscribed sinks will be assigned a task.
  void SubmitPacket(SourceId source_id, Byte* packet, Time timestamp);

  /// The sink processed the data (earlier is better) and releases it.
  void ReleasePacket(SourceId source_id, const Byte* packet);

  /// Call from main thread! Returns only on shutdown.
  void DoUITasks();

  /// Tells all threads to stop working and quit. Destructor waits for them.
  void Shutdown();

 protected:
  /// Returns only on shutdown.
  void DoWorkerTasks();

  /**
   * The general task dispatcher of the scheduler where scheduling is done.
   * Only one UI thread can be present.
   */
  void DispatchTasks(bool UI_thread_mode = true);

 private:
  struct Subscription {
    Subscription(SinkCallback _sink_callback, void* __this, bool _on_UI);
    bool operator==(const Subscription& o) const;

    SinkCallback sink_callback;
    void* _this;
    bool on_UI;
  };

  struct Source {
    std::atomic_flag source_mtx_;
    int packet_size;
    std::vector<int> free_packets;    // packet number
    std::vector<bool> packet_usages;  // true if used
    std::vector<int> packet_refcounts;
    std::vector<Byte> packet_buffer;  // concatenated packets
    std::vector<Subscription> subscriptions;
  };

  struct SourceRef {
    SourceRef(SourceId _source_id);
    SourceRef(SourceId _source_id, int packet_size, int packets_in_queue);
    bool operator<(const SourceRef& o) const;

    SourceId source_id;
    std::unique_ptr<Source> ptr;
  };

  struct Task {
    SourceId source_id;
    SinkCallback sink_callback;
    void* _this;
    Byte* packet;
  };

  struct TaskRef {
    TaskRef(Time _timestamp, SourceId source_id, SinkCallback sink_callback,
            void* _this, Byte* packet);
    bool operator<(const TaskRef& o) const;

    Time timestamp;
    std::unique_ptr<Task> ptr;
  };

  Source& GetSourceById(SourceId source_id);

  // Locking of sources_ container
  void WriteLockSources();
  void WriteUnlockSources();
  void ReadLockSources();
  void ReadUnlockSources();

  // Locking of a single source's queue
  static void LockSource(Source& src);
  static void UnlockSource(Source& src);

  std::vector<SourceRef> sources_;
  std::priority_queue<TaskRef> tasks_for_workers_;
  std::priority_queue<TaskRef> tasks_for_UI_;
  std::vector<std::thread> workers_;

  std::atomic<bool> shutdown_initiated_;
  std::atomic<int> sources_semaphore_;
  std::mutex worker_queue_mtx_;
  std::condition_variable worker_queue_cv_;
  std::mutex UI_queue_mtx_;
  std::condition_variable UI_queue_cv_;
  static int max_spin_cycles_before_yield;
};

}  // namespace zamt

#endif  // ZAMT_CORE_SCHEDULER_H_
