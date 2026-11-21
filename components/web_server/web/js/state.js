// Mutable state shared by the web UI modules.
export const state = {
  // Manual control session.
  session: 0,
  sequence: 0,
  epoch: 0,
  busy: false,
  pending: false,
  direction: 's',
  releasing: false,
  heldPointer: null,
  recovering: false,
  hudMode: false,
  warningEnabled: true,
  speed: 75,

  // Screen and notifications.
  wakeLock: null,
  wakePending: false,
  wakeUnavailable: false,
  toastTimer: undefined,
  activePanel: 'manual',

  // Requests, sensor status and active operation.
  telemetryBusy: false,
  homeBusy: false,
  stopVersion: 0,
  lastStatusTime: '',
  warningDistanceCm: 30,
  obstacleDistanceCm: 20,
  operationActive: false,
  operationKind: '',
  operationText: '',
  operationStale: false,
  distanceDriveOwner: '',
  turnOwner: '',
  navigationOwner: '',

  // Trajectory and requested path.
  trajectory: [],
  lastTrajectorySampleMs: -1,
  trajectoryValid: false,
  trajectorySegment: 0,
  trajectoryCount: 0,
  trajectoryVersion: 0,
  latestPose: null,
  plannedPath: null,
  plannedPreviewRequested: false,
  plannedDirection: 'forward'
};
