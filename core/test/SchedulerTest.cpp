#include "zamt/core/Scheduler.h"
#include "zamt/core/TestSuite.h"

using namespace zamt;

static const int packets_to_arrive = (int)sizeof(long) * 8 - 2;

void RunsWellEmpty() {
  Scheduler sch;
  for (int i = 0; i < 9; ++i) std::this_thread::yield();
  sch.Shutdown();
}

void RunsWellWithoutShutdown() {
  Scheduler sch;
  EXPECT(sch.GetNumberOfWorkers() > 0);
  for (int i = 0; i < 9; ++i) std::this_thread::yield();
}

void ImmediateShutdownIsOk() {
  Scheduler sch;
  sch.Shutdown();
}

void SourceWithoutSinks() {
  Scheduler sch;
  sch.RegisterSource(1, 1024, 4);
  EXPECT(sch.GetPacketSize(1) == 1024);
  for (int i = 0; i < 3; ++i) std::this_thread::yield();
  uint8_t* p = sch.GetPacketForSubmission(1);
  sch.SubmitPacket(1, p, 0);
  for (int i = 0; i < 3; ++i) std::this_thread::yield();
  sch.Shutdown();
}

void NeverCalled(void*, Scheduler::SourceId, const Scheduler::Byte*,
                 Scheduler::Time) {
  EXPECT(false);
}

void EmptyJob(void*, Scheduler::SourceId, const Scheduler::Byte*,
              Scheduler::Time) {}

void QueueWorksAfterUnsubscribe() {
  Scheduler sch;
  sch.RegisterSource(1, 1024, 4);
  sch.Subscribe(1, &NeverCalled, nullptr, false);
  sch.Unsubscribe(1, &NeverCalled);
  uint8_t* p = sch.GetPacketForSubmission(1);
  sch.SubmitPacket(1, p, 0);
  for (int i = 0; i < 9; ++i) std::this_thread::yield();
  sch.Shutdown();
}

void OutOfBufferGivesNull() {
  Scheduler sch;
  sch.RegisterSource(1, 1024, 1);
  sch.Subscribe(1, &EmptyJob, nullptr, false);
  uint8_t* p = sch.GetPacketForSubmission(1);
  EXPECT(p);
  uint8_t* pp = sch.GetPacketForSubmission(1);
  sch.SubmitPacket(1, p, 0);
  ASSERT(!pp);
  sch.Shutdown();
}

static std::atomic<long> packets_arrived;

void CheckPackets(void* schp, Scheduler::SourceId source_id,
                  const Scheduler::Byte* packet, Scheduler::Time timestamp) {
  Scheduler& sch = *static_cast<Scheduler*>(schp);
  EXPECT(source_id == 1);
  int num = (int)packet[0];
  EXPECT(timestamp == (Scheduler::Time)num * 1000);
  long status = packets_arrived.fetch_or(1l << num);
  // printf("Arrived: %d, prev status is %lx\n", num, status);
  EXPECT(!(status & (1l << num)));
  sch.ReleasePacket(1, packet);
}

void SinkGetsAllPacketsSent() {
  packets_arrived = 0;
  Scheduler sch;
  sch.RegisterSource(1, 1024, packets_to_arrive);
  sch.Subscribe(1, &CheckPackets, &sch, false);
  for (int i = 0; i < packets_to_arrive; ++i) {
    uint8_t* p = sch.GetPacketForSubmission(1);
    p[0] = (uint8_t)i;
    sch.SubmitPacket(1, p, (Scheduler::Time)i * 1000);
  }
  while (packets_arrived != (1l << packets_to_arrive) - 1)
    std::this_thread::yield();
  sch.Shutdown();
  for (int i = 0; i < 3; ++i) std::this_thread::yield();
  ASSERT(packets_arrived == (1l << packets_to_arrive) - 1);
}

static std::atomic<long> packets_arrived2;

void CheckPackets2(void* schp, Scheduler::SourceId source_id,
                   const Scheduler::Byte* packet, Scheduler::Time timestamp) {
  Scheduler& sch = *static_cast<Scheduler*>(schp);
  EXPECT(source_id == 1);
  int num = (int)packet[0];
  EXPECT(timestamp == (Scheduler::Time)num * 1000);
  long status = packets_arrived2.fetch_or(1l << num);
  // printf("Arrived: %d, prev status is %lx\n", num, status);
  EXPECT(!(status & (1l << num)));
  sch.ReleasePacket(1, packet);
}

void AllSinksGetAllPackets() {
  packets_arrived = 0;
  packets_arrived2 = 0;
  Scheduler sch;
  sch.RegisterSource(1, 1024, packets_to_arrive);
  sch.Subscribe(1, &CheckPackets, &sch, false);
  sch.Subscribe(1, &CheckPackets2, &sch, false);
  for (int i = 0; i < packets_to_arrive; ++i) {
    uint8_t* p = sch.GetPacketForSubmission(1);
    p[0] = (uint8_t)i;
    sch.SubmitPacket(1, p, (Scheduler::Time)i * 1000);
  }
  while (packets_arrived != (1l << packets_to_arrive) - 1 ||
         packets_arrived2 != (1l << packets_to_arrive) - 1)
    std::this_thread::yield();
  sch.Shutdown();
  for (int i = 0; i < 3; ++i) std::this_thread::yield();
  ASSERT(packets_arrived == (1l << packets_to_arrive) - 1);
  ASSERT(packets_arrived2 == (1l << packets_to_arrive) - 1);
}

void CheckPackets12(void* schp, Scheduler::SourceId source_id,
                    const Scheduler::Byte* packet, Scheduler::Time timestamp) {
  Scheduler& sch = *static_cast<Scheduler*>(schp);
  int num = (int)packet[0];
  Scheduler::SourceId source = (Scheduler::SourceId)packet[1];
  ASSERT(source == 1 || source == 2);
  ASSERT(source_id == source);
  ASSERT(timestamp == (Scheduler::Time)num * 1000);
  auto& packs_arrived = (source == 1) ? packets_arrived : packets_arrived2;
  long status = packs_arrived.fetch_or(1l << num);
  // printf("Arrived: src:%ld, pack:%d, prev status is
  // %lx\n",source,num,status);
  ASSERT(!(status & (1l << num)));
  sch.ReleasePacket(source_id, packet);
}

void MultipleSourcesWithOneSink() {
  packets_arrived = 0;
  packets_arrived2 = 0;
  Scheduler sch;
  sch.RegisterSource(1, 1024, packets_to_arrive);
  sch.RegisterSource(2, 1024, packets_to_arrive);
  sch.Subscribe(1, &CheckPackets12, &sch, false);
  sch.Subscribe(2, &CheckPackets12, &sch, false);
  for (int i = 0; i < packets_to_arrive; ++i) {
    uint8_t* p = sch.GetPacketForSubmission(1);
    p[0] = (uint8_t)i;
    p[1] = 1;
    sch.SubmitPacket(1, p, (Scheduler::Time)i * 1000);
    p = sch.GetPacketForSubmission(2);
    p[0] = (uint8_t)i;
    p[1] = 2;
    sch.SubmitPacket(2, p, (Scheduler::Time)i * 1000);
  }
  while (packets_arrived != (1l << packets_to_arrive) - 1 ||
         packets_arrived2 != (1l << packets_to_arrive) - 1)
    std::this_thread::yield();
  sch.Shutdown();
  for (int i = 0; i < 3; ++i) std::this_thread::yield();
  ASSERT(packets_arrived == (1l << packets_to_arrive) - 1);
  ASSERT(packets_arrived2 == (1l << packets_to_arrive) - 1);
}

static std::atomic<long> packets_arrived3;

void CheckPackets123(void* schp, Scheduler::SourceId source_id,
                     const Scheduler::Byte* packet, Scheduler::Time timestamp) {
  Scheduler& sch = *static_cast<Scheduler*>(schp);
  int num = (int)packet[0];
  Scheduler::SourceId source = (Scheduler::SourceId)packet[1];
  ASSERT(source == 1 || source == 2 || source == 3);
  ASSERT(source_id == source);
  ASSERT(timestamp == (Scheduler::Time)num * 1000);
  auto& packs_arrived =
      (source == 1) ? packets_arrived
                    : ((source == 2) ? packets_arrived2 : packets_arrived3);
  long status = packs_arrived.fetch_or(1l << num);
  // printf("Arrived: src:%ld, pack:%d, prev status is
  // %lx\n",source,num,status);
  ASSERT(!(status & (1l << num)));
  if (source < 3) {
    uint8_t* p = sch.GetPacketForSubmission(source + 1);
    p[0] = (uint8_t)num;
    p[1] = (uint8_t)(source + 1);
    sch.SubmitPacket(source + 1, p, timestamp);
  }
  sch.ReleasePacket(source_id, packet);
}

void SourceSinkChainWorks() {
  packets_arrived = 0;
  packets_arrived2 = 0;
  packets_arrived3 = 0;
  Scheduler sch;
  sch.RegisterSource(1, 1024, packets_to_arrive);
  sch.RegisterSource(2, 1024, packets_to_arrive);
  sch.RegisterSource(3, 1024, packets_to_arrive);
  sch.Subscribe(1, &CheckPackets123, &sch, false);
  sch.Subscribe(2, &CheckPackets123, &sch, false);
  sch.Subscribe(3, &CheckPackets123, &sch, false);
  for (int i = 0; i < packets_to_arrive; ++i) {
    uint8_t* p = sch.GetPacketForSubmission(1);
    p[0] = (uint8_t)i;
    p[1] = 1;
    sch.SubmitPacket(1, p, (Scheduler::Time)i * 1000);
  }
  while (packets_arrived3 != (1l << packets_to_arrive) - 1)
    std::this_thread::yield();
  sch.Shutdown();
  for (int i = 0; i < 3; ++i) std::this_thread::yield();
  ASSERT(packets_arrived == (1l << packets_to_arrive) - 1);
  ASSERT(packets_arrived2 == (1l << packets_to_arrive) - 1);
  ASSERT(packets_arrived3 == (1l << packets_to_arrive) - 1);
}

TEST_BEGIN() {
  RunsWellEmpty();
  RunsWellWithoutShutdown();
  ImmediateShutdownIsOk();
  SourceWithoutSinks();
  QueueWorksAfterUnsubscribe();
  OutOfBufferGivesNull();
  SinkGetsAllPacketsSent();
  AllSinksGetAllPackets();
  MultipleSourcesWithOneSink();
  SourceSinkChainWorks();
}
TEST_END()
