#include "zamt/core/Scheduler.h"

#include <algorithm>
#include <cassert>
#include <system_error>

namespace zamt {

Scheduler::Scheduler(int worker_threads) : shutdown_initiated_(false) {
  size_t workers = (size_t)worker_threads;
  if (workers == 0) workers = (size_t)std::thread::hardware_concurrency();
  if (workers == 0) workers = 1;
  sources_semaphore_.store((int)workers + 1, std::memory_order_release);
  if (workers == 1) {
    max_spin_cycles_before_yield = 4;
  }
  workers_.reserve(workers);
  while (workers-- > 0) {
    workers_.emplace_back(&Scheduler::DoWorkerTasks, this);
  }
}

Scheduler::~Scheduler() {
  if (!shutdown_initiated_.load(std::memory_order_acquire)) {
    Shutdown();
  }
  for (std::thread& worker : workers_) {
    try {
      worker.join();
    } catch (const std::system_error& e) {
    }
  }
}

int Scheduler::GetNumberOfWorkers() const { return (int)workers_.size(); }

void Scheduler::RegisterSource(SourceId source_id, int packet_size,
                               int packets_in_queue) {
  WriteLockSources();
  assert(std::is_sorted(sources_.begin(), sources_.end()));
  assert(!std::binary_search(sources_.begin(), sources_.end(),
                             SourceRef(source_id)));
  sources_.emplace_back(source_id, packet_size, packets_in_queue);
  std::sort(sources_.begin(), sources_.end());
  WriteUnlockSources();
}

int Scheduler::GetPacketSize(SourceId source_id) {
  return GetSourceById(source_id).packet_size;
}

void Scheduler::Subscribe(SourceId source_id, SinkCallback sink_callback,
                          void* _this, bool on_UI) {
  Source& src = GetSourceById(source_id);
  LockSource(src);
  assert(std::find(src.subscriptions.begin(), src.subscriptions.end(),
                   Subscription(sink_callback, nullptr, false)) ==
         src.subscriptions.end());
  src.subscriptions.emplace_back(sink_callback, _this, on_UI);
  UnlockSource(src);
}

void Scheduler::Unsubscribe(SourceId source_id, SinkCallback sink_callback) {
  Source& src = GetSourceById(source_id);
  LockSource(src);
  auto& subs = src.subscriptions;
  auto subs_it = std::find(subs.begin(), subs.end(),
                           Subscription(sink_callback, nullptr, false));
  assert(subs_it != subs.end());
  std::swap(*subs_it, subs.back());
  subs.pop_back();
  UnlockSource(src);
}

Scheduler::Byte* Scheduler::GetPacketForSubmission(SourceId source_id) {
  Source& src = GetSourceById(source_id);
  LockSource(src);
  if (src.free_packets.empty()) {
    UnlockSource(src);
    return nullptr;
  }
  int packet_num = src.free_packets.back();
  assert(src.packet_refcounts.size() == src.packet_usages.size());
  assert(src.free_packets.size() <= src.packet_refcounts.size());
  assert(packet_num >= 0 && packet_num < (int)src.packet_refcounts.size());

  src.free_packets.pop_back();
  assert(src.packet_usages[(size_t)packet_num] == false);
  src.packet_usages[(size_t)packet_num] = true;
  assert(src.packet_refcounts[(size_t)packet_num] == 0);
  UnlockSource(src);
  return &src.packet_buffer[(size_t)packet_num * (size_t)src.packet_size];
}

void Scheduler::SubmitPacket(SourceId source_id, Byte* packet, Time timestamp) {
  Source& src = GetSourceById(source_id);
  int packet_num =
      static_cast<int>(packet - &src.packet_buffer[0]) / src.packet_size;
  assert(src.packet_refcounts.size() == src.packet_usages.size());
  assert(src.free_packets.size() < src.packet_refcounts.size());
  assert(packet_num >= 0 && packet_num < (int)src.packet_refcounts.size());
  assert(src.packet_usages[(size_t)packet_num] == true);
  assert(src.packet_refcounts[(size_t)packet_num] == 0);

  LockSource(src);
  src.packet_refcounts[(size_t)packet_num] = 0;
  int UI_subscribers = 0;
  int normal_subscribers = 0;
  for (auto& subscription : src.subscriptions) {
    if (subscription.on_UI)
      UI_subscribers++;
    else
      normal_subscribers++;
  }
  if (UI_subscribers) {
    std::lock_guard<std::mutex> lock(UI_queue_mtx_);
    for (auto& subscription : src.subscriptions) {
      if (!subscription.on_UI) continue;
      tasks_for_UI_.emplace(timestamp, source_id, subscription.sink_callback,
                            subscription._this, packet);
      ++src.packet_refcounts[(size_t)packet_num];
    }
  }
  if (normal_subscribers) {
    std::lock_guard<std::mutex> lock(worker_queue_mtx_);
    for (auto& subscription : src.subscriptions) {
      if (subscription.on_UI) continue;
      tasks_for_workers_.emplace(timestamp, source_id,
                                 subscription.sink_callback, subscription._this,
                                 packet);
      ++src.packet_refcounts[(size_t)packet_num];
    }
  }
  for (auto& subscription : src.subscriptions) {
    bool on_UI = subscription.on_UI;
    auto& cond_var = on_UI ? UI_queue_cv_ : worker_queue_cv_;
    cond_var.notify_one();
  }
  if (UI_subscribers) UI_queue_cv_.notify_one();
  while (normal_subscribers--) worker_queue_cv_.notify_one();
  if (src.packet_refcounts[(size_t)packet_num] == 0) {
    src.free_packets.push_back(packet_num);
    src.packet_usages[(size_t)packet_num] = false;
  }
  UnlockSource(src);
}

void Scheduler::ReleasePacket(SourceId source_id, const Byte* packet) {
  Source& src = GetSourceById(source_id);
  int packet_num =
      static_cast<int>(packet - &src.packet_buffer[0]) / src.packet_size;
  assert(src.packet_refcounts.size() == src.packet_usages.size());
  assert(src.free_packets.size() < src.packet_refcounts.size());
  assert(packet_num >= 0 && packet_num < (int)src.packet_refcounts.size());
  assert(src.packet_usages[(size_t)packet_num] == true);
  assert(src.packet_refcounts[(size_t)packet_num] > 0);

  LockSource(src);
  if (--src.packet_refcounts[(size_t)packet_num] == 0) {
    src.free_packets.push_back(packet_num);
    src.packet_usages[(size_t)packet_num] = false;
  }
  UnlockSource(src);
}

void Scheduler::DoUITaskStep() { DispatchTasks(true); }

void Scheduler::Shutdown() {
  {
    std::lock_guard<std::mutex> lock(worker_queue_mtx_);
    shutdown_initiated_.store(true, std::memory_order_release);
  }
  worker_queue_cv_.notify_all();
  UI_queue_cv_.notify_all();
}

void Scheduler::DoWorkerTasks() { DispatchTasks(false); }

void Scheduler::DispatchTasks(bool UI_thread_mode) {
  auto& tasks = UI_thread_mode ? tasks_for_UI_ : tasks_for_workers_;
  auto& mutex = UI_thread_mode ? UI_queue_mtx_ : worker_queue_mtx_;
  auto& cond_var = UI_thread_mode ? UI_queue_cv_ : worker_queue_cv_;
  while (!shutdown_initiated_.load(std::memory_order_acquire)) {
    SinkCallback sink_callback = nullptr;
    SourceId source_id;
    void* _this;
    Byte* packet;
    Time timestamp;
    {
      std::unique_lock<std::mutex> lock(mutex);
      while (!UI_thread_mode && tasks.empty() &&
             !shutdown_initiated_.load(std::memory_order_acquire)) {
        cond_var.wait(lock);
      }
      if (!tasks.empty() &&
          !shutdown_initiated_.load(std::memory_order_acquire)) {
        const TaskRef& task_ref = tasks.top();
        Task& task = *task_ref.ptr;
        sink_callback = task.sink_callback;
        source_id = task.source_id;
        _this = task._this;
        packet = task.packet;
        timestamp = task_ref.timestamp;
        tasks.pop();
      }
    }
    if (sink_callback) {
      (*sink_callback)(_this, source_id, packet, timestamp);
    }
    if (UI_thread_mode) return;
  }
}

Scheduler::Subscription::Subscription(SinkCallback _sink_callback, void* __this,
                                      bool _on_UI) {
  sink_callback = _sink_callback;
  _this = __this;
  on_UI = _on_UI;
}

bool Scheduler::Subscription::operator==(const Subscription& o) const {
  return sink_callback == o.sink_callback;
}

Scheduler::SourceRef::SourceRef(SourceId _source_id) : source_id(_source_id) {}

Scheduler::SourceRef::SourceRef(SourceId _source_id, int packet_size,
                                int packets_in_queue)
    : SourceRef(_source_id) {
  assert(packet_size >= 0);
  assert(packets_in_queue > 0);
  ptr.reset(new Source());
  ptr->source_mtx_.clear(std::memory_order_release);
  ptr->packet_size = packet_size;
  ptr->free_packets.reserve((size_t)packets_in_queue);
  ptr->packet_usages.resize((size_t)packets_in_queue, false);
  ptr->packet_refcounts.resize((size_t)packets_in_queue, 0);
  ptr->packet_buffer.resize((size_t)packets_in_queue * (size_t)packet_size, 0);
  for (int i = packets_in_queue - 1; i >= 0; --i) {
    ptr->free_packets.push_back(i);
  }
}

bool Scheduler::SourceRef::operator<(const SourceRef& o) const {
  return source_id < o.source_id;
}

Scheduler::TaskRef::TaskRef(Time _timestamp, SourceId source_id,
                            SinkCallback sink_callback, void* _this,
                            Byte* packet) {
  timestamp = _timestamp;
  ptr.reset(new Task());
  ptr->source_id = source_id;
  ptr->sink_callback = sink_callback;
  ptr->_this = _this;
  ptr->packet = packet;
}

bool Scheduler::TaskRef::operator<(const TaskRef& o) const {
  return timestamp > o.timestamp;  // finish the earliest job first
}

Scheduler::Source& Scheduler::GetSourceById(SourceId source_id) {
  ReadLockSources();
  auto src_it =
      std::lower_bound(sources_.begin(), sources_.end(), SourceRef(source_id));
  assert(src_it != sources_.end() && src_it->source_id == source_id);
  Source& src = *src_it->ptr;
  ReadUnlockSources();
  return src;
}

void Scheduler::WriteLockSources() {
  int cycles_left = max_spin_cycles_before_yield;
  int all_readers;
  do {
    if (--cycles_left == 0) {
      std::this_thread::yield();
      cycles_left = max_spin_cycles_before_yield;
    }
    all_readers = GetNumberOfWorkers() + 1;
  } while (!sources_semaphore_.compare_exchange_weak(
      all_readers, 0, std::memory_order_acq_rel));
}

void Scheduler::WriteUnlockSources() {
  int all_readers = GetNumberOfWorkers() + 1;
  sources_semaphore_.store(all_readers, std::memory_order_release);
}

void Scheduler::ReadLockSources() {
  int cycles_left = max_spin_cycles_before_yield;
  int current_readers;
  do {
    if (--cycles_left == 0) {
      std::this_thread::yield();
      cycles_left = max_spin_cycles_before_yield;
    }
    current_readers = sources_semaphore_.load(std::memory_order_acquire);
    if (current_readers <= 0) continue;
    if (sources_semaphore_.compare_exchange_weak(
            current_readers, current_readers - 1, std::memory_order_acq_rel)) {
      break;
    }
  } while (1);
}

void Scheduler::ReadUnlockSources() {
  int readers_left = sources_semaphore_.fetch_add(1, std::memory_order_acq_rel);
  assert(readers_left <= GetNumberOfWorkers() + 1);
  (void)readers_left;
}

void Scheduler::LockSource(Source& src) {
  int cycles_left = max_spin_cycles_before_yield;
  while (src.source_mtx_.test_and_set(std::memory_order_acq_rel)) {
    if (--cycles_left == 0) {
      std::this_thread::yield();
      cycles_left = max_spin_cycles_before_yield;
    }
  }
}

void Scheduler::UnlockSource(Source& src) {
  src.source_mtx_.clear(std::memory_order_release);
}

int Scheduler::max_spin_cycles_before_yield = 256;

}  // namespace zamt
