#include "AppStateMachine.h"

namespace {
constexpr uint32_t kAccessTimeoutMs = 30000;
constexpr uint32_t kManagementTimeoutMs = 90000;
constexpr uint32_t kResetTimeoutMs = 90000;
constexpr uint32_t kSetupTimeoutMs = 300000;
}

void AppStateMachine::transition(AppState next, uint32_t nowMs) {
  state_ = next;
  shownAtMs_ = nowMs;
  changed_ = true;
}

void AppStateMachine::begin(uint32_t nowMs, bool configured) {
  configured_ = configured;
  transition(AppState::IdleScreenOff, nowMs);
}

void AppStateMachine::onSingleTap(uint32_t nowMs) {
  // TouchManager emits SingleTap on press, before it can distinguish a hold.
  // Preserve Management until release or Hold so a second hold reaches Reset.
  if (state_ == AppState::ManagementRequest || state_ == AppState::ResetRequest ||
      state_ == AppState::ResetConfirm || state_ == AppState::Resetting) return;
  transition(configured_ ? AppState::AccessRequest : AppState::SetupRequest, nowMs);
}

void AppStateMachine::onTapReleased(uint32_t nowMs) {
  // Holds suppress TapReleased. Only a completed short press leaves Management.
  if (state_ == AppState::ManagementRequest)
    transition(AppState::AccessRequest, nowMs);
}

void AppStateMachine::onTripleTap(uint32_t nowMs) {
  (void)nowMs;
}

void AppStateMachine::onHold(uint32_t nowMs) {
  if (!configured_ || state_ == AppState::ResetRequest ||
      state_ == AppState::ResetConfirm || state_ == AppState::Resetting) return;
  transition(state_ == AppState::ManagementRequest ? AppState::ResetRequest :
             AppState::ManagementRequest, nowMs);
}

void AppStateMachine::resetChoice(bool yes, uint32_t nowMs) {
  if (state_ == AppState::ResetRequest)
    transition(yes ? AppState::ResetConfirm : AppState::AccessRequest, nowMs);
}

void AppStateMachine::resetStarted(uint32_t nowMs) {
  if (state_ == AppState::ResetConfirm) transition(AppState::Resetting, nowMs);
}

void AppStateMachine::update(uint32_t nowMs) {
  uint32_t timeoutMs = kAccessTimeoutMs;
  if (state_ == AppState::ManagementRequest) timeoutMs = kManagementTimeoutMs;
  if (state_ == AppState::ResetRequest) timeoutMs = kResetTimeoutMs;
  if (state_ == AppState::ResetConfirm) timeoutMs = kResetTimeoutMs;
  if (state_ == AppState::Resetting) return;
  if (state_ == AppState::SetupRequest) timeoutMs = kSetupTimeoutMs;
  if (state_ != AppState::IdleScreenOff &&
      static_cast<uint32_t>(nowMs - shownAtMs_) >= timeoutMs) {
    transition(AppState::IdleScreenOff, nowMs);
  }
}
