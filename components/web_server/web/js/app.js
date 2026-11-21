import {state} from './state.js';
import {postKeepalive} from './api.js';
import {$, driveButtons, speedValues, configureUi, showPanel, dismissNotice,
  notifyAction, driveIsFullscreen, syncWakeLock, render} from './ui.js';
import {homeAction, moveDistance, turnAngle} from './actions.js';
import {claimControl, enterHud, exitHud, setSpeed, requestDirection,
  stop, park, pump} from './hud.js';
import {refreshTelemetry} from './telemetry.js';
import {previewPlannedDistance, clearTrajectory, downloadTrajectory,
  drawTrajectory} from './trajectory.js';

configureUi({
  releaseControl: park,
  refreshDrivePanel() {
    drawTrajectory();
    void refreshTelemetry();
  }
});

$('hudConnect').addEventListener('click',claimControl);
$('manualTab').addEventListener('click',() => showPanel('manual'));
$('driveTab').addEventListener('click',() => showPanel('drive'));
$('hudTab').addEventListener('click',enterHud);
$('hudSound').addEventListener('click',() => {
  if (!state.session || state.releasing || state.recovering) return;
  state.warningEnabled = !state.warningEnabled;
  state.pending = true;
  render();
  void pump();
});
const stopRobot = () => {
  void homeAction('stop',{},'Đã dừng xe.',
    'Không gửi được lệnh dừng. Hãy kiểm tra xe ngay.');
};
$('emergencyStop').addEventListener('click',stopRobot);
$('driveStop').addEventListener('click',stopRobot);
$('operationStop').addEventListener('click',stopRobot);
$('hudRotateStop').addEventListener('click',stopRobot);
$('actionToastClose').addEventListener('click',() => {
  clearTimeout(state.toastTimer);
  dismissNotice();
});
$('wakeRobot').addEventListener('click',() => {
  void homeAction('wake',{},'Đã gửi yêu cầu đánh thức Ruby.',
    'Ruby chưa sẵn sàng để đánh thức. Hãy thử lại sau.');
});
$('moveForward').addEventListener('click',() => moveDistance('forward'));
$('moveBackward').addEventListener('click',() => moveDistance('backward'));
$('moveDistance').addEventListener('input',() => {
  if (state.plannedPath?.pending || state.operationKind === 'distance') return;
  state.plannedPreviewRequested = true;
  state.plannedPath = null;
  previewPlannedDistance();
});
$('moveLeft').addEventListener('click',() => turnAngle('turn_left'));
$('moveRight').addEventListener('click',() => turnAngle('turn_right'));
if ($('resetPose')) $('resetPose').addEventListener('click',() => {
  if (state.operationActive || state.session) return;
  void homeAction('reset_odometry',{},'Đã đặt gốc vị trí tương đối.',
    'Dừng xe, rời phiên HUD và chờ cảm biến ổn định trước khi đặt gốc.');
});
$('clearTrajectory').addEventListener('click',clearTrajectory);
$('downloadTrajectory').addEventListener('click',downloadTrajectory);
$('refreshHome').addEventListener('click',async () => {
  if (state.telemetryBusy) return;
  $('refreshHome').disabled = true;
  const refreshed = await refreshTelemetry();
  notifyAction(refreshed ? 'Đã cập nhật trạng thái xe.' :
    'Chưa lấy được trạng thái mới. Hãy thử lại sau.',refreshed);
  $('refreshHome').disabled = false;
});
$('hudExit').addEventListener('click',exitHud);
$('hudRotateExit').addEventListener('click',exitHud);
$('hudRotateConnect').addEventListener('click',() => { void claimControl(); });
document.addEventListener('fullscreenchange',() => {
  if (!driveIsFullscreen()) {
    try { if (screen.orientation && screen.orientation.unlock) screen.orientation.unlock(); }
    catch (_) {}
  }
  render();
});
document.addEventListener('webkitfullscreenchange',() => render());
$('hudStop').addEventListener('pointerdown', event => { event.preventDefault(); stop(); });
$('hudStop').addEventListener('click', stop);
speedValues.forEach(option =>
  $('speed' + option).addEventListener('click',() => setSpeed(option)));
driveButtons.forEach(button => {
  button.addEventListener('contextmenu', event => event.preventDefault());
  button.addEventListener('pointerdown', event => {
    event.preventDefault();
    if (!state.hudMode || !state.session || state.releasing || state.recovering || state.heldPointer !== null ||
        event.button !== 0) return;
    state.heldPointer = event.pointerId;
    button.setPointerCapture(event.pointerId);
    requestDirection(button.dataset.dir);
  });
  for (const type of ['pointerup','pointercancel','lostpointercapture'])
    button.addEventListener(type, event => { if (event.pointerId === state.heldPointer) stop(); });
});
window.addEventListener('keydown', event => {
  if (event.code === 'Space' && state.hudMode && state.session) { event.preventDefault(); stop(); }
});
const pageLost = () => {
  if ((state.operationKind === 'distance' && state.distanceDriveOwner === 'web') ||
      (state.operationKind === 'turn' && state.turnOwner === 'web'))
    void postKeepalive('/api/action',JSON.stringify({action:'stop'}));
  park('Đã dừng xe và trả quyền điều khiển về giọng nói.');
};
window.addEventListener('blur',() => {
  // Browser chrome/fullscreen changes can blur a still-visible cockpit.
  if (document.hidden) pageLost();
  else stop();
});
window.addEventListener('focus',() => { state.wakeUnavailable = false; void syncWakeLock(); });
window.addEventListener('resize',drawTrajectory);
window.addEventListener('pagehide', pageLost);
document.addEventListener('visibilitychange', () => { if (document.hidden) pageLost(); });
void refreshTelemetry();
setInterval(refreshTelemetry,2000);
setInterval(() => { if ((state.operationKind === 'distance' || state.operationKind === 'turn') && !document.hidden) void refreshTelemetry(); },200);
setInterval(() => { if (state.activePanel === 'drive' && state.operationKind !== 'distance' && state.operationKind !== 'turn' && !document.hidden) void refreshTelemetry(); },250);
setInterval(() => { if (state.hudMode && !document.hidden) void refreshTelemetry(); },250);
setInterval(() => { if (state.hudMode && state.session && !state.releasing) { state.pending = true; void pump(); } },150);
$('theme').addEventListener('click', () => document.body.classList.toggle('ultra'));
render();
