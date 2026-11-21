import {state} from './state.js';
import {post, postKeepalive} from './api.js';
import {$, speedValues, render, notifyAction, showPanel, setDriveFallback,
  updateDistanceAlert} from './ui.js';
import {refreshTelemetry} from './telemetry.js';

const names = {s:'Đã dừng',f:'Tiến',b:'Lùi',l:'Xoay trái',r:'Xoay phải'};

export function setSpeed(value) {
  state.speed = value;
  $('hudSpeedValue').textContent = value + '%';
  speedValues.forEach(option => $('speed' + option).classList.toggle('active',option === value));
  state.warningDistanceCm = Math.ceil(state.obstacleDistanceCm * Math.max(value,50) / 50);
  updateDistanceAlert($('manualDistance').textContent !== '-- cm',
    Number.parseInt($('manualDistance').textContent,10));
  if (state.hudMode && state.session && !state.releasing) { state.pending = true; void pump(); }
}

export function enterHud() {
  if (state.hudMode) return;
  if (state.operationActive) {
    notifyAction('Dừng xe trước khi vào HUD để không ngắt lệnh đang chạy.',false);
    return;
  }
  state.hudMode = true;
  showPanel('hud');
  $('manualPanel').classList.toggle('hud-mode',true);
  setDriveFallback(true);
  void claimControl();
  void refreshTelemetry();
}

export function exitHud() {
  if (!state.hudMode) return;
  showPanel('manual');
}

function commandBody(token, dir) {
  return JSON.stringify({session:token,sequence:++state.sequence,direction:dir,
    speed:state.speed,warning_enabled:state.warningEnabled});
}

// Best-effort release on HUD exit or page loss. The firmware's deadline remains authoritative.
// A newer sequence prevents an older in-flight movement from restoring control.
export function park(message) {
  if (state.session) {
    void postKeepalive('/api/command',commandBody(state.session,'release'));
  }
  ++state.epoch;
  state.session = 0; state.direction = 's'; state.pending = false; state.releasing = false;
  state.recovering = false;
  state.heldPointer = null;
  $('status').textContent = message;
  render();
}

export async function pump() {
  if (!state.session || state.busy) return;
  state.busy = true; state.pending = false;
  const currentEpoch = state.epoch, sentDirection = state.direction;
  const sentSpeed = state.speed;
  render();
  try {
    await post('/api/command',commandBody(state.session,sentDirection));
    if (currentEpoch !== state.epoch) return;
    state.recovering = false;
    if (sentDirection === 'release') {
      state.session = 0; state.direction = 's'; state.releasing = false; state.pending = false;
      $('status').textContent = 'Đã trả quyền điều khiển motor về giọng nói.';
    } else if (sentDirection === state.direction) {
      $('status').textContent = (sentSpeed === 0 ? names.s : names[sentDirection]) +
        ' · Điều khiển thủ công';
    }
  } catch (error) {
    if (currentEpoch === state.epoch) {
      // Retry once with a newer STOP, never replay a held movement or claim a new session.
      if (!state.recovering && !state.releasing && !error.status && !document.hidden) {
        state.recovering = true;
        state.heldPointer = null;
        state.direction = 's'; state.pending = true;
      } else {
        park(error.status === 409
          ? 'Phiên điều khiển đã kết thúc. Bấm kết nối lại để tiếp tục.'
          : 'Mất kết nối. Xe tự dừng; bấm kết nối lại để tiếp tục.');
      }
    }
  } finally {
    state.busy = false;
    render();
    if (state.pending && state.session) void pump();
  }
}

export function requestDirection(next) {
  if (!state.hudMode || !state.session || state.releasing || (state.recovering && next !== 's')) return;
  state.direction = next; state.pending = true;
  render();
  void pump();
}

export function stop() {
  state.heldPointer = null;
  requestDirection('s');
}

export async function claimControl() {
  if (!state.hudMode || state.session || state.busy || document.hidden) return;
  state.busy = true;
  const currentEpoch = ++state.epoch;
  render();
  $('status').textContent = 'Đang kết nối…';
  try {
    const result = await post('/api/claim');
    if (currentEpoch !== state.epoch) {
      void post('/api/command',JSON.stringify({session:result.session,sequence:1,
        direction:'release',speed:0,warning_enabled:true})).catch(() => {});
      return;
    }
    state.session = result.session; state.sequence = 0; state.direction = 's'; state.pending = true;
    state.warningEnabled = true;
    state.recovering = false;
    state.wakeUnavailable = false;
    $('status').textContent = 'Đã kết nối HUD. Giữ nút hướng để lái.';
    notifyAction($('status').textContent,true);
  } catch (_) {
    if (currentEpoch === state.epoch) {
      $('status').textContent = 'Chưa nhận được quyền điều khiển. Kiểm tra Wi-Fi hoặc trang đang điều khiển khác, rồi thử lại.';
      notifyAction($('status').textContent,false);
    }
  } finally {
    state.busy = false;
    render();
    if (state.pending && state.session) void pump();
  }
}
