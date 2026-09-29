#pragma once

#include <stddef.h>
#include <stdint.h>

namespace smartlock {
namespace events {

enum class Action : uint8_t {
  Boot,
  AccessGranted,
  AccessDenied,
  Unlock,
  Relock,
  AdminLogin,
  DeviceEnrolled,
  DeviceRevoked,
  UserAdded,
  NetworkChanged,
  Recovery,
  EmergencyUnlockAdminPin,
  EmergencyRelock
};

enum class Result : uint8_t { Success, Denied, StorageFault };
enum class NetworkMode : uint8_t { AP, AP_STA, STA };
enum class Source : uint8_t { Unknown, Lan, RecoveryAp, PhysicalAdmin };

class EventLog {
 public:
  static constexpr size_t kQueueCapacity = 16;
  static constexpr size_t kMaxSegments = 64;
  static constexpr size_t kMaxRecordsPerSegment = 32;
  static constexpr size_t kMaxSegmentBytes = 8192;
  static constexpr size_t kBasenameCapacity = 47;
  static constexpr size_t kSegmentBufferCapacity = kMaxSegmentBytes + 1;

  struct Status {
    bool enabled;
    bool storageFault;
    bool outboxFull;
    bool currentSegmentBlocked;
    bool hasCorruptSegments;
    bool hasUnrecognizedFiles;
    bool sequenceExhausted;
    uint8_t queueDepth;
    uint8_t segmentCount;
    uint32_t droppedEvents;
    uint32_t retainedEvents;
    bool retainedCountKnown;
    uint32_t historyLostEvents;
    uint32_t historyLostSegments;
    bool pendingHistoryProtected;
    bool protectedQueueFull;
    bool lossPersistenceFault;
    // Conservative persisted upper bound; may overcount by less than one
    // 64-event reservation block after reboot.
    uint32_t unrecordedEvents;
    bool currentSegmentHasEvents;
    uint32_t currentSegmentFirstEventUptimeMs;
  };

  struct SegmentInfo {
    char id[kBasenameCapacity];
    uint8_t records;
    uint32_t counter;
  };

  EventLog();

  // Call after StorageHealth has mounted SD. This never touches database paths.
  bool begin(bool sdAvailable);

  // Valid inputs only copy bounded fields and perform no I/O. Rejected events
  // synchronously persist loss evidence. Return value never decides access.
  bool enqueue(Action action, Result result, const char* userId,
               const char* deviceId, uint32_t epochSeconds,
               uint32_t uptimeMs, NetworkMode networkMode,
               Source source = Source::Unknown,
               const char* claimedDeviceId = "",
               const char* identityName = "", const char* role = "",
               const char* identityContext = "UNKNOWN",
               const char* actorIdentityId = "",
               const char* subjectIdentityId = "");

  // Strict mode persists once enabled and never returns to destructive rotation
  // until Factory Reset clears sl-audit.
  bool protectPendingHistory();
  bool protectedHistory() const;

  // Call from the owning Arduino loop. At most one event is drained per call.
  void update();
  void status(Status& output) const;
  // Closes the in-memory active segment without modifying historical bytes.
  // It refuses while events remain queued.
  bool sealCurrentSegment();

  // Selects a validated, closed segment in persisted counter order.
  bool selectOldestClosedSegment(char* basename, size_t capacity);
  // Returns a validated raw CSV segment (NUL-terminated) within the 32-record
  // bound. The caller supplies kSegmentBufferCapacity bytes.
  bool readClosedSegment(const char* basename, char* output,
                         size_t capacity, size_t& outputLength);
  // Call only after the cloud layer has verified the ACK for this selected
  // basename. This can remove only the exact selected, revalidated segment.
  bool deleteAckedSegment(const char* basename);
  // Enumerates validated retained segments oldest-first. Does not alter cloud ACK selection.
  bool enumerateSegments(SegmentInfo* output, size_t capacity, size_t& count);
  // Reads a validated, explicitly named canonical segment without changing ACK state.
  bool readSegmentById(const char* id, char* output, size_t capacity,
                       size_t& outputLength);
  // Pins only a segment whose cloud ACK is actively pending. Export/read calls never pin.
  bool pinPendingAckSegment(const char* id, bool pin);

 private:
  struct Event {
    uint32_t sequence;
    uint32_t epochSeconds;
    uint32_t uptimeMs;
    Action action;
    Result result;
    NetworkMode networkMode;
    Source source;
    char userId[8];
    char deviceId[8];
    char claimedDeviceId[8];
    char identityName[41];
    char role[6];
    char identityContext[8];
    char actorIdentityId[8];
    char subjectIdentityId[8];
  };

  Event queue_[kQueueCapacity];
  uint8_t queueHead_;
  uint8_t queueCount_;
  uint32_t droppedEvents_;
  uint32_t retainedEvents_=0;
  bool retainedCountKnown_=false;
  uint32_t historyLostEvents_;
  uint32_t historyLostSegments_;
  uint32_t unrecordedEvents_;
  uint32_t unrecordedObserved_;
  uint8_t unrecordedSinceReservation_;
  uint32_t nextSequence_;
  bool sequenceExhausted_;
  uint8_t bootNonce_[16];
  bool enabled_;
  bool storageFault_;
  bool outboxFull_;
  bool pendingHistoryProtected_;
  bool protectedQueueFull_;
  bool lossPersistenceFault_;
  bool currentSegmentBlocked_;
  bool hasCorruptSegments_;
  bool hasUnrecognizedFiles_;
  uint8_t segmentCount_;
  uint32_t nextSegmentCounter_;
  bool segmentCounterExhausted_;
  bool currentSegmentOpen_;
  uint8_t currentRecordCount_;
  uint32_t currentSegmentCounter_;
  uint32_t currentSegmentFirstEventUptimeMs_;
  bool currentSegmentHasFirstEvent_;
  size_t currentSegmentBytes_;
  char currentBasename_[kBasenameCapacity];
  char selectedBasename_[kBasenameCapacity];
  char pendingAckBasename_[kBasenameCapacity];
  bool selectedReadValidated_;

  bool inspectOutbox();
  bool createCurrentSegment();
  bool appendQueuedEvent();
  void dropFront();
  void noteDrop();
  bool evictOldestClosedSegment();
  bool persistHistoryLoss(uint32_t events, uint32_t segments);
  bool persistUnrecordedEvent();
};

}  // namespace events
}  // namespace smartlock
