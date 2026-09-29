#include "RestoreCoordinator.h"

#include <cstdlib>
#include <iostream>
#include <map>
#include <string>

namespace {
unsigned assertions = 0;

void check(bool condition, const char* label) {
  ++assertions;
  if (!condition) {
    std::cerr << "FAIL: " << label << '\n';
    std::exit(1);
  }
}

struct State {
  bool exactlyOneOwner = true;
  bool ownerIsD000001 = true;
  bool ownerActive = true;
  bool ownerVerifier = true;
  bool identityAuthOneToOne = true;
  bool uniqueNames = true;
  bool uniqueIds = true;
  bool noOrphanVerifiers = true;
  bool rolesValid = true;
  bool statusesValid = true;
  bool configValid = true;
  bool wifiValid = true;
  bool boardBound = true;
  bool authenticated = true;
  bool snapshotCrcValid = true;
  unsigned payload = 0;
};

struct Generation {
  State state;
  unsigned sdPayload = 0;
  unsigned nvsPayload = 0;
  bool sdWritten = false;
  bool sdRenamed = false;
  bool nvsStaged = false;
  bool corrupt = false;
};

enum class FaultOp {
  None, SdWrite, SdRename, NvsStage,
  JournalPrepared, JournalStarted, ActiveSelector, JournalActivated,
  Finalize
};
enum class FaultMoment { Before, After };

const char* phaseOp(RestoreCoordinator::Phase phase) {
  using RestoreCoordinator::Phase;
  switch (phase) {
    case Phase::Prepared: return "journal prepared";
    case Phase::Started: return "journal started";
    case Phase::Activated: return "journal activated";
    case Phase::Finalized: return "finalize";
    case Phase::RolledBack: return "journal rolled back";
    case Phase::None: return "journal none";
  }
  return "unknown journal";
}

struct DurableFakeBackend {
  std::map<uint32_t, Generation> generations;
  uint32_t activeSelector = 0;
  RestoreCoordinator::Journal journal;
  FaultOp fault = FaultOp::None;
  FaultMoment moment = FaultMoment::Before;
  bool failed = false;
  bool journalReadFails = false;
  bool selectionReadFails = false;
  std::string lastMutation;

  void arm(FaultOp op, FaultMoment when) { fault = op; moment = when; failed = false; }
  void disarm() { fault = FaultOp::None; failed = false; }

  bool hit(FaultOp op, FaultMoment when) {
    if (op != FaultOp::None && !failed && fault == op && moment == when) {
      failed = true;
      return true;
    }
    return false;
  }

  static FaultOp journalFault(RestoreCoordinator::Phase phase) {
    using RestoreCoordinator::Phase;
    switch (phase) {
      case Phase::Prepared: return FaultOp::JournalPrepared;
      case Phase::Started: return FaultOp::JournalStarted;
      case Phase::Activated: return FaultOp::JournalActivated;
      case Phase::Finalized: return FaultOp::Finalize;
      default: return FaultOp::None;
    }
  }

  bool valid(uint32_t id, bool strict) {
    const auto found = generations.find(id);
    if (found == generations.end()) return false;
    const Generation& g = found->second;
    if (g.corrupt || !g.sdWritten || !g.sdRenamed || !g.nvsStaged ||
        g.sdPayload != g.nvsPayload) return false;
    if (!strict) return true;
    const State& s = g.state;
    return s.exactlyOneOwner && s.ownerIsD000001 && s.ownerActive && s.ownerVerifier &&
           s.identityAuthOneToOne && s.uniqueNames && s.uniqueIds && s.noOrphanVerifiers &&
           s.rolesValid && s.statusesValid && s.configValid && s.wifiValid &&
           s.boardBound && s.authenticated && s.snapshotCrcValid;
  }

  bool matchesCurrent(uint32_t id) { return activeSelector == id; }

  bool writeJournal(const RestoreCoordinator::Journal& next) {
    const FaultOp op = journalFault(next.phase);
    if (hit(op, FaultMoment::Before)) { lastMutation = std::string("before ") + phaseOp(next.phase); return false; }
    journal = next;
    lastMutation = std::string("write ") + phaseOp(next.phase);
    if (hit(op, FaultMoment::After)) { lastMutation = std::string("after ") + phaseOp(next.phase); return false; }
    return true;
  }

  bool select(uint32_t id) {
    if (hit(FaultOp::ActiveSelector, FaultMoment::Before)) { lastMutation = "before selector"; return false; }
    if (!valid(id, false)) return false;
    activeSelector = id;
    lastMutation = "selector";
    if (hit(FaultOp::ActiveSelector, FaultMoment::After)) { lastMutation = "after selector"; return false; }
    return true;
  }

  bool readJournal(RestoreCoordinator::Journal& output) {
    if (journalReadFails) return false;
    output = journal;
    return true;
  }

  bool selection(uint32_t& output) {
    if (selectionReadFails) return false;
    output = activeSelector;
    return output != 0;
  }

  void addComplete(uint32_t id, unsigned payload) {
    Generation g;
    g.state.payload = payload;
    g.sdPayload = payload;
    g.nvsPayload = payload;
    g.sdWritten = true;
    g.sdRenamed = true;
    g.nvsStaged = true;
    generations[id] = g;
  }

  bool stageCandidate(uint32_t id, unsigned payload) {
    Generation& g = generations[id];
    g.state.payload = payload;
    g.sdPayload = payload;
    if (hit(FaultOp::SdWrite, FaultMoment::Before)) { lastMutation = "before SD write"; return false; }
    g.sdWritten = true; lastMutation = "SD write";
    if (hit(FaultOp::SdWrite, FaultMoment::After)) { lastMutation = "after SD write"; return false; }
    if (hit(FaultOp::SdRename, FaultMoment::Before)) { lastMutation = "before SD rename"; return false; }
    g.sdRenamed = true; lastMutation = "SD rename";
    if (hit(FaultOp::SdRename, FaultMoment::After)) { lastMutation = "after SD rename"; return false; }
    if (hit(FaultOp::NvsStage, FaultMoment::Before)) { lastMutation = "before NVS stage"; return false; }
    g.nvsPayload = payload;
    g.nvsStaged = true; lastMutation = "NVS stage";
    if (hit(FaultOp::NvsStage, FaultMoment::After)) { lastMutation = "after NVS stage"; return false; }
    return true;
  }

  bool coherentSelection() const {
    const auto found = generations.find(activeSelector);
    return found != generations.end() && found->second.sdWritten && found->second.sdRenamed &&
           found->second.nvsStaged && found->second.sdPayload == found->second.nvsPayload &&
           !found->second.corrupt;
  }
};

void seed(DurableFakeBackend& b, uint32_t oldId = 1, uint32_t newId = 2) {
  b.addComplete(oldId, 100 + oldId);
  b.addComplete(newId, 100 + newId);
  b.activeSelector = oldId;
  b.journal = {};
}

bool authorityReady(DurableFakeBackend& b) {
  const auto result = RestoreCoordinator::recover(b);
  uint32_t selected = 0;
  return result == RestoreCoordinator::Result::Ready && b.selection(selected) && b.valid(selected, true);
}

bool resultAllowsAuthority(DurableFakeBackend& b, RestoreCoordinator::Result result) {
  uint32_t selected = 0;
  return result == RestoreCoordinator::Result::Ready && b.selection(selected) && b.valid(selected, true);
}

void assertFailClosed(DurableFakeBackend& b, RestoreCoordinator::Result result, const char* context) {
  check(result == RestoreCoordinator::Result::InitializationRequired ||
        result == RestoreCoordinator::Result::Blocked, context);
  check(!resultAllowsAuthority(b, result), "InitializationRequired/Blocked never grants authority");
}

void stageFailureMatrix() {
  const FaultOp operations[] = {FaultOp::SdWrite, FaultOp::SdRename, FaultOp::NvsStage};
  for (size_t i = 0; i < 3; ++i) for (FaultMoment moment : {FaultMoment::Before, FaultMoment::After}) {
    DurableFakeBackend b; b.addComplete(1, 101); b.activeSelector = 1; b.journal = {};
    b.arm(operations[i], moment);
    check(!b.stageCandidate(2, 202), "injected candidate staging interruption is reported");
    b.disarm();
    check(b.activeSelector == 1, "staging interruption leaves old selector active");
    check(authorityReady(b), "boot after staging interruption keeps old state ready");
    check(b.generations.at(1).state.payload == 101, "old SD/NVS payload remains coherent after staging interruption");
    const bool fullyStagedDespiteReportedInterruption =
        operations[i] == FaultOp::NvsStage && moment == FaultMoment::After;
    check(b.valid(2, true) == fullyStagedDespiteReportedInterruption,
          "candidate validity reflects whether all durable SD and NVS staging writes completed");
  }
}

void activationFaultMatrix() {
  struct Case { FaultOp op; FaultMoment moment; uint32_t selected; bool candidateExpected; };
  const Case cases[] = {
      {FaultOp::JournalPrepared, FaultMoment::Before, 1, false},
      {FaultOp::JournalPrepared, FaultMoment::After, 1, false},
      {FaultOp::JournalStarted, FaultMoment::Before, 1, false},
      {FaultOp::JournalStarted, FaultMoment::After, 1, false},
      {FaultOp::ActiveSelector, FaultMoment::Before, 1, false},
      {FaultOp::ActiveSelector, FaultMoment::After, 2, true},
      {FaultOp::JournalActivated, FaultMoment::Before, 2, true},
      {FaultOp::JournalActivated, FaultMoment::After, 2, true},
  };
  for (const Case& c : cases) {
    DurableFakeBackend b; seed(b); b.arm(c.op, c.moment);
    const auto result = RestoreCoordinator::activate(b, 1, 2);
    check(result == RestoreCoordinator::Result::Blocked, "activation injection returns blocked to caller");
    b.disarm();
    check(b.activeSelector == c.selected, "selector reflects only a complete old or new generation");
    check(b.coherentSelection(), "selected generation has matching complete SD and NVS fixture state");
    const auto recovered = RestoreCoordinator::recover(b);
    if (c.candidateExpected) {
      assertFailClosed(b, recovered, "candidate activation awaits boot initialization");
      check(b.activeSelector == 2, "valid candidate is not rolled back after selector switched");
      check(RestoreCoordinator::finalize(b), "post-validation candidate can finalize");
      check(authorityReady(b), "finalized candidate becomes ready");
    } else if (c.op == FaultOp::JournalStarted && c.moment == FaultMoment::After) {
      assertFailClosed(b, recovered, "rollback after activation start requires initialization");
      check(b.activeSelector == 1, "rollback selects transaction previous generation");
      check(RestoreCoordinator::finalize(b), "rolled back old generation finalizes after initialization");
      check(authorityReady(b), "finalized rollback old generation becomes ready");
    } else if (c.op == FaultOp::ActiveSelector && c.moment == FaultMoment::Before) {
      assertFailClosed(b, recovered, "started transaction rollback requires initialization");
      check(b.activeSelector == 1, "pre-selector failure keeps transaction previous generation");
      check(RestoreCoordinator::finalize(b), "pre-selector rollback can finalize after initialization");
      check(authorityReady(b), "pre-selector rollback returns to ready old generation");
    } else {
      check(recovered == RestoreCoordinator::Result::Ready, "prepared but unactivated restore is canceled safely");
      check(b.activeSelector == 1 && authorityReady(b), "unactivated failure retains ready old generation");
    }
  }
}

void finalizeFaultMatrix() {
  for (FaultMoment moment : {FaultMoment::Before, FaultMoment::After}) {
    DurableFakeBackend b; seed(b);
    check(RestoreCoordinator::activate(b, 1, 2) == RestoreCoordinator::Result::RebootRequired,
          "successful activation requests reboot");
    check(RestoreCoordinator::recover(b) == RestoreCoordinator::Result::InitializationRequired,
          "candidate remains unavailable until boot initialization");
    b.arm(FaultOp::Finalize, moment);
    check(!RestoreCoordinator::finalize(b), "injected finalization interruption is reported");
    b.disarm();
    if (moment == FaultMoment::Before) {
      assertFailClosed(b, RestoreCoordinator::recover(b), "failed pre-write finalization remains gated");
      check(RestoreCoordinator::finalize(b), "retry finalization succeeds after validation");
    }
    check(authorityReady(b), "completed durable finalization allows selected validated state");
    check(b.activeSelector == 2, "finalization never changes the selected generation");
  }
}

void preparedDoesNotRevertLegitimateChange() {
  DurableFakeBackend b; seed(b, 1, 2); b.addComplete(3, 303);
  RestoreCoordinator::Journal prepared{RestoreCoordinator::Phase::Prepared, 1, 2};
  check(b.writeJournal(prepared), "persist prepared transaction fixture");
  check(b.select(3), "later legitimate current generation is selected");
  check(RestoreCoordinator::recover(b) == RestoreCoordinator::Result::Ready,
        "prepared transaction is canceled while retaining later current state");
  check(b.activeSelector == 3 && b.valid(3, true), "prepared recovery does not roll back later legitimate state");
}

void finalizedNeverFallsBack() {
  DurableFakeBackend b; seed(b);
  b.activeSelector = 2;
  b.journal = {RestoreCoordinator::Phase::Finalized, 1, 2};
  b.generations.at(2).corrupt = true;
  const auto result = RestoreCoordinator::recover(b);
  check(result == RestoreCoordinator::Result::Blocked, "corrupt finalized active generation blocks");
  check(b.activeSelector == 2, "finalized recovery does not silently select older generation");
  check(!authorityReady(b), "corrupt finalized generation grants no authority");
}

void transactionSpecificRollbackAndDoubleCorruption() {
  {
    DurableFakeBackend b; seed(b, 5, 9); b.addComplete(3, 303);
    b.journal = {RestoreCoordinator::Phase::Started, 5, 9};
    b.activeSelector = 9;
    b.generations.at(9).corrupt = true;
    check(RestoreCoordinator::recover(b) == RestoreCoordinator::Result::InitializationRequired,
          "bad selected candidate triggers specific transaction rollback");
    check(b.activeSelector == 5, "rollback uses journal previous ID, not arbitrary older generation");
    check(!authorityReady(b), "rolled-back previous state awaits initialization");
    check(RestoreCoordinator::finalize(b) && authorityReady(b), "transaction rollback can finish on validated previous state");
  }
  {
    DurableFakeBackend b; seed(b, 5, 9);
    b.journal = {RestoreCoordinator::Phase::Started, 5, 9};
    b.activeSelector = 9;
    b.generations.at(9).corrupt = true;
    b.generations.at(5).corrupt = true;
    check(RestoreCoordinator::recover(b) == RestoreCoordinator::Result::Blocked,
          "corrupt candidate and previous generation fail closed");
    check(!authorityReady(b), "double corruption never grants authority");
  }
}

void strictGenerationSemantics() {
  using Mutation = bool State::*;
  const Mutation invalidFields[] = {
      &State::exactlyOneOwner, &State::ownerIsD000001, &State::ownerActive,
      &State::ownerVerifier, &State::identityAuthOneToOne, &State::uniqueNames,
      &State::uniqueIds, &State::noOrphanVerifiers, &State::rolesValid,
      &State::statusesValid, &State::configValid, &State::wifiValid,
      &State::boardBound, &State::authenticated, &State::snapshotCrcValid,
  };
  for (Mutation field : invalidFields) {
    DurableFakeBackend b; seed(b);
    b.generations.at(2).state.*field = false;
    check(b.valid(2, false), "non-strict generation lookup only confirms physical fixture exists");
    check(!b.valid(2, true), "strict generation validation rejects a semantic/package invariant violation");
    check(RestoreCoordinator::activate(b, 1, 2) == RestoreCoordinator::Result::Rejected,
          "restore activation rejects a semantically invalid candidate");
    check(b.activeSelector == 1 && authorityReady(b), "invalid candidate cannot displace current authority");
  }
  DurableFakeBackend invalidOld; seed(invalidOld);
  invalidOld.generations.at(1).state.ownerVerifier = false;
  check(RestoreCoordinator::activate(invalidOld, 1, 2) == RestoreCoordinator::Result::Rejected,
        "activation also rejects a current generation that fails strict validation");
}

void malformedTransactionMetadata() {
  struct JournalCase { RestoreCoordinator::Phase phase; uint32_t previous; uint32_t candidate; };
  const JournalCase invalid[] = {
      {static_cast<RestoreCoordinator::Phase>(99), 1, 2},
      {RestoreCoordinator::Phase::Started, 0, 2},
      {RestoreCoordinator::Phase::Started, 1, 0},
      {RestoreCoordinator::Phase::Started, 1, 1},
  };
  for (const JournalCase& item : invalid) {
    DurableFakeBackend b; seed(b);
    b.journal = {item.phase, item.previous, item.candidate};
    check(RestoreCoordinator::recover(b) == RestoreCoordinator::Result::Blocked,
          "malformed restore transaction metadata blocks recovery");
    check(!authorityReady(b), "malformed transaction metadata grants no authority");
  }
}

void splitBundleAndReadFailuresFailClosed() {
  {
    DurableFakeBackend b; seed(b);
    Generation mixed;
    mixed.sdWritten = mixed.sdRenamed = mixed.nvsStaged = true;
    mixed.sdPayload = 700;
    mixed.nvsPayload = 701;
    b.generations[7] = mixed;
    b.activeSelector = 7;
    check(RestoreCoordinator::recover(b) == RestoreCoordinator::Result::Blocked,
          "selector to mismatched SD/NVS payloads is blocked");
    check(!authorityReady(b), "cross-store split bundle grants no authority");
  }
  {
    DurableFakeBackend b; seed(b); b.journalReadFails = true;
    check(RestoreCoordinator::recover(b) == RestoreCoordinator::Result::Blocked,
          "unreadable transaction journal blocks recovery");
    check(!resultAllowsAuthority(b, RestoreCoordinator::Result::Blocked), "unreadable journal grants no authority");
  }
  {
    DurableFakeBackend b; seed(b); b.selectionReadFails = true;
    check(RestoreCoordinator::recover(b) == RestoreCoordinator::Result::Blocked,
          "unreadable active selector blocks recovery");
    check(!resultAllowsAuthority(b, RestoreCoordinator::Result::Blocked), "unreadable selector grants no authority");
  }
}

void badInputRejectedWithoutMutation() {
  DurableFakeBackend b; seed(b);
  check(RestoreCoordinator::activate(b, 1, 1) == RestoreCoordinator::Result::Rejected,
        "same generation ID is rejected");
  check(RestoreCoordinator::activate(b, 0, 2) == RestoreCoordinator::Result::Rejected,
        "zero previous ID is rejected");
  check(RestoreCoordinator::activate(b, 1, 0) == RestoreCoordinator::Result::Rejected,
        "zero candidate ID is rejected");
  check(RestoreCoordinator::activate(b, 99, 2) == RestoreCoordinator::Result::Rejected,
        "unknown previous generation is rejected");
  check(RestoreCoordinator::activate(b, 1, 99) == RestoreCoordinator::Result::Rejected,
        "unknown candidate generation is rejected");
  check(b.activeSelector == 1 && b.journal.phase == RestoreCoordinator::Phase::None,
        "rejected activation leaves journal and current selector unchanged");
}
}

int main() {
  stageFailureMatrix();
  activationFaultMatrix();
  finalizeFaultMatrix();
  preparedDoesNotRevertLegitimateChange();
  finalizedNeverFallsBack();
  transactionSpecificRollbackAndDoubleCorruption();
  strictGenerationSemantics();
  malformedTransactionMetadata();
  splitBundleAndReadFailuresFailClosed();
  badInputRejectedWithoutMutation();
  std::cout << "PASS: " << assertions << " assertions across restore activation, recovery, and fault injection\n";
}
