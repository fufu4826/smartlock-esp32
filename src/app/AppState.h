#pragma once

enum class AppState {
  Boot,
  IdleScreenOff,
  SetupRequest,
  AccessRequest,
  ManagementRequest,
  ResetRequest,
  ResetConfirm,
  Resetting,
  HardwareTest,
  ErrorStatus,
};
