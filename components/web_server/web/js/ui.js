import {state} from './state.js';

export const $ = id => document.getElementById(id);
export const driveButtons = [...document.querySelectorAll('[data-dir]')];
export const speedValues = [50,75,100];

// The entry point connects panel changes to control and telemetry.
let callbacks;
export function configureUi(handlers) {
  callbacks = handlers;
}

export function dismissNotice() {
  $('actionToast').className = '';
  $('actionToastText').textContent = '';
  $('actionToastClose').hidden = true;
}

export function notifyAction(message, successful) {
  const toast = $('actionToast');
  clearTimeout(state.toastTimer);
  toast.setAttribute('role',successful ? 'status' : 'alert');
  toast.setAttribute('aria-live',successful ? 'polite' : 'assertive');
  $('actionToastText').textContent = message;
  $('actionToastClose').hidden = successful;
  toast.className = 'show ' + (successful ? 'success' : 'error');
  if (successful) state.toastTimer = setTimeout(dismissNotice,5000);
}

export function showOperation(text, kind) {
  state.operationActive = true;
  state.operationKind = kind;
  state.operationText = text;
  state.operationStale = false;
  render();
}

export function operationFromStatus(data) {
  if (data.command_mode === 'angle_turn' && data.angle_turn_degrees !== 0) {
    const direction = data.angle_turn_degrees > 0 ? 'Xoay trái' : 'Xoay phải';
    return ['turn',direction + ' ' + Math.round(Math.abs(data.turn_progress_degrees || 0)) +
      '/' + Math.abs(data.angle_turn_degrees) + '°'];
  }
  if (data.distance_move_state === 'pending' || data.distance_move_state === 'moving') {
    const direction = data.motion === 'backward' ? 'Lùi' : data.motion === 'forward'
      ? 'Tiến' : 'Đang xoay';
    const target = data.turn_target_degrees > 0 ? data.turn_target_degrees + '°' :
      data.distance_move_target_cm + ' cm';
    const progress = data.turn_target_degrees > 0 ? data.turn_progress_degrees :
      data.distance_move_progress_cm;
    return ['distance',(data.distance_move_state === 'pending' ? 'Chuẩn bị · ' : '') +
      direction + ' ' + Math.round(progress || 0) + '/' + target];
  }
  if (data.moving) {
    const motion = ({forward:'Tiến',backward:'Lùi',turn_left:'Xoay trái',
      turn_right:'Xoay phải'})[data.motion] || 'Đang di chuyển';
    return ['motor',motion + ' · điều khiển bằng ' +
      (({web:'web',voice:'giọng nói'})[data.control_owner] || 'nguồn khác')];
  }
  return null;
}

export function finishedOperation(data) {
  if (state.operationKind === 'turn') {
    if (data.angle_turn_state === 'completed') return ['Đã xoay đến góc đặt.',true];
    if (data.angle_turn_state === 'fault')
      return ['Xe dừng vì lỗi cảm biến, motor hoặc quá thời gian.',false];
    return ['Lệnh xoay đã bị ngắt.',false];
  }
  if (state.operationKind === 'distance') {
    if (data.distance_move_state === 'completed') return ['Đã hoàn thành quãng đường.',true];
    if (data.distance_move_state === 'fault')
      return ['Lệnh quãng đường dừng do lỗi: ' + (data.distance_drive?.fault || 'control_error'),false];
    if (data.distance_move_state === 'obstacle') return ['Xe đã dừng vì vật cản.',false];
    if (data.distance_move_state === 'no_pulses' ||
        data.distance_move_state === 'encoder_error') return ['Xe đã dừng vì lỗi FC-03.',false];
    if (data.distance_move_state === 'timeout') return ['Xe đã dừng vì hết thời gian.',false];
    return ['Lệnh di chuyển đã bị ngắt.',false];
  }
  return null;
}

export function showPanel(name) {
  if (state.hudMode && name !== 'hud') {
    state.hudMode = false;
    callbacks.releaseControl('Đã rời HUD và trả quyền điều khiển về giọng nói.');
    updateDistanceAlert(false,0);
    $('manualPanel').classList.toggle('hud-mode',false);
    setDriveFallback(false);
  }
  state.activePanel = name;
  for (const mode of ['manual','drive']) {
    $(mode + 'Panel').classList.toggle('active',mode === (name === 'hud' ? 'manual' : name));
    $(mode + 'Tab').classList.toggle('active',mode === name);
  }
  $('hudTab').classList.toggle('active',name === 'hud');
  if (name === 'drive') {
    callbacks.refreshDrivePanel();
  }
  render();
}

export function updateDistanceAlert(valid, distance) {
  const nearObstacle = state.hudMode && valid && distance < state.warningDistanceCm;
  $('hudDistance').classList.toggle('danger',nearObstacle);
  $('hudWarning').classList.toggle('active',nearObstacle);
  $('hudWarning').textContent = distance < state.obstacleDistanceCm
    ? '⚠ Nguy cơ va chạm · dưới ' + state.obstacleDistanceCm + ' cm'
    : '⚠ Cảnh báo sớm · dưới ' + state.warningDistanceCm + ' cm';
}

export function renderEncoder(encoder, disconnected = false) {
  if (!disconnected) {
    $('encoderPanel').hidden = !encoder?.enabled;
    $('hudEncoder').hidden = !encoder?.enabled;
  }
  const ready = encoder?.enabled && encoder.ready;
  const numeric = value => Number.isFinite(value) && value >= 0;
  const calibrated = ready && encoder.calibrated;
  const travel = calibrated && numeric(encoder.travel_m)
    ? '≈ ' + encoder.travel_m.toFixed(3) + ' m' : '-- m';
  $('encoderTravel').textContent = travel;
  $('hudTravel').textContent = travel;
  $('encoderPulses').textContent = ready && numeric(encoder.left?.pulses) && numeric(encoder.right?.pulses)
    ? encoder.left.pulses + ' / ' + encoder.right.pulses : '-- / --';
  $('encoderRpm').textContent = calibrated && numeric(encoder.left?.rpm) && numeric(encoder.right?.rpm)
    ? encoder.left.rpm.toFixed(1) + ' / ' + encoder.right.rpm.toFixed(1) : '-- / --';
  const wheels = calibrated && numeric(encoder.left?.distance_m) && numeric(encoder.right?.distance_m)
    ? encoder.left.distance_m.toFixed(3) + ' / ' + encoder.right.distance_m.toFixed(3) : '-- / --';
  $('encoderWheels').textContent = 'Quãng đường bánh trái/phải: ' + wheels + ' m';
  let encoderState = disconnected ? 'Mất kết nối số đo' : !encoder?.enabled ? 'FC-03 chưa bật'
    : !ready ? 'Lỗi FC-03: ' + (encoder.error || 'không có số đo')
    : !calibrated ? 'Chưa hiệu chuẩn: xem tổng xung và log FC03 trong monitor'
    : encoder.distance_complete === false ? 'Ước lượng thiếu xung không xác định được chiều quay'
    : 'Chiều suy ra từ lệnh motor; có sai số khi bánh trượt';
  if (ready && (encoder.left?.no_pulses || encoder.right?.no_pulses)) {
    const sides = [encoder.left?.no_pulses ? 'trái' : '', encoder.right?.no_pulses ? 'phải' : ''].filter(Boolean);
    encoderState += ' · Không có xung bánh ' + sides.join('/') + ' khi đang có lệnh chạy';
  }
  $('encoderState').textContent = encoderState;
  $('hudEncoderState').textContent = ready && !calibrated
    ? 'Chưa hiệu chuẩn · xung ' + $('encoderPulses').textContent : encoderState;
}

export function driveIsFullscreen() {
  return document.fullscreenElement === $('manualPanel') ||
    document.webkitFullscreenElement === $('manualPanel') ||
    $('manualPanel').classList.contains('drive-expanded');
}

export function setDriveFallback(active) {
  $('manualPanel').classList.toggle('drive-expanded',active);
  document.body.classList.toggle('drive-expanded',active);
  render();
}

// Optional: ESP32 HTTP pages may not expose the secure-context Wake Lock API.
export async function syncWakeLock() {
  const wanted = Boolean(state.session) && driveIsFullscreen() && !document.hidden;
  if (!wanted) {
    const previous = state.wakeLock;
    state.wakeLock = null;
    if (previous) { try { await previous.release(); } catch (_) {} }
    return;
  }
  if (state.wakeLock || state.wakePending || state.wakeUnavailable) return;
  if (typeof navigator === 'undefined' || !navigator.wakeLock) {
    state.wakeUnavailable = true;
    renderHud();
    return;
  }
  state.wakePending = true;
  try {
    const lock = await navigator.wakeLock.request('screen');
    if (!state.session || !driveIsFullscreen() || document.hidden) {
      await lock.release();
      return;
    }
    state.wakeLock = lock;
    lock.addEventListener('release',() => {
      if (state.wakeLock === lock) {
        state.wakeLock = null;
        state.wakeUnavailable = true;
        renderHud();
      }
    });
  } catch (_) { state.wakeUnavailable = true; }
  finally { state.wakePending = false; renderHud(); }
}

function renderHud() {
  $('hudSound').disabled = !state.session || state.releasing || state.recovering;
  $('hudSound').setAttribute('aria-pressed',String(state.warningEnabled));
  $('hudSound').textContent = state.warningEnabled ? '🔊 Âm: Bật' : '🔇 Âm: Tắt';
  $('hudConnect').disabled = Boolean(state.session) || state.busy;
  $('hudRotateConnect').disabled = Boolean(state.session) || state.busy;
  $('hudConnect').textContent = state.recovering ? 'Đang nối lại…' : state.session ? '● Đã kết nối' : '↻ Kết nối lại';
  $('hudRotateStatus').textContent = state.session
    ? 'Đã kết nối. Xoay ngang để điều khiển xe.' : $('status').textContent;
  $('hudNotice').textContent = !state.session ? $('status').textContent : state.recovering
    ? 'Xe đã dừng · Đang kiểm tra kết nối…' : state.wakeUnavailable
    ? 'Hãy tăng thời gian tự tắt màn hình điện thoại khi lái.' : '';
}

export function render() {
  $('hudStop').disabled = !state.session || state.releasing;
  const actionBusy = state.homeBusy || state.busy || state.releasing;
  $('wakeRobot').disabled = actionBusy;
  for (const id of ['moveForward','moveBackward','moveLeft','moveRight','resetPose'])
    if ($(id)) $(id).disabled = actionBusy || state.operationActive || Boolean(state.session);
  const showBar = state.operationActive && !state.hudMode;
  $('operationBar').hidden = !showBar;
  $('operationText').textContent = state.operationText + (state.operationStale ? ' · Chưa cập nhật mới' : '');
  document.body.classList.toggle('operation-active',showBar);
  driveButtons.forEach(button => {
    button.disabled = !state.hudMode || !state.session || state.releasing || state.recovering;
    button.classList.toggle('held', Boolean(state.session) && button.dataset.dir === state.direction);
  });
  renderHud();
  void syncWakeLock();
}
