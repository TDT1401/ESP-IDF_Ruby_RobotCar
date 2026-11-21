import {state} from './state.js';
import {post} from './api.js';
import {$, render, notifyAction, showOperation} from './ui.js';
import {clearTrajectory, previewPlannedDistance, setPlannedDistance} from './trajectory.js';
import {park} from './hud.js';
import {refreshTelemetry} from './telemetry.js';

async function reassertStop(version) {
  if (version !== state.stopVersion)
    await post('/api/action',JSON.stringify({action:'stop'})).catch(() => {});
}

export async function homeAction(action, fields, success, failure) {
  const stopping = action === 'stop';
  if (!stopping && (state.homeBusy || state.busy || state.releasing)) return;
  if (stopping) ++state.stopVersion;
  else { state.homeBusy = true; render(); }
  const version = state.stopVersion;
  try {
    const result = await post('/api/action',JSON.stringify({action,...fields}));
    if (version === state.stopVersion) {
      if (stopping && state.session) park('Đã dừng xe và kết thúc phiên HUD.');
      if (action === 'reset_odometry') clearTrajectory();
      $('status').textContent = result.already_listening
        ? 'Ruby đang lắng nghe. Hãy nói vào mic của Ruby.' : success;
      if (stopping) {
        state.operationActive = false;
        state.operationKind = '';
        render();
      } else if (action === 'forward' || action === 'backward') {
        if (state.plannedPath?.pending) {
          state.plannedPath.pending = false;
          state.plannedPath.locked = true;
          $('plannedPathState').textContent = 'Đường yêu cầu · ' +
            (action === 'backward' ? 'lùi ' : 'tiến ') + fields.distance_cm + ' cm.';
        }
        state.distanceDriveOwner = 'web';
        showOperation('Đã nhận lệnh ' + (action === 'forward' ? 'tiến ' : 'lùi ') +
          fields.distance_cm + ' cm','distance');
      } else if (action === 'turn_left' || action === 'turn_right') {
        state.turnOwner = 'web';
        showOperation('Đã nhận lệnh xoay ' + (action === 'turn_left' ? 'trái ' : 'phải ') +
          fields.angle_degrees + '°','turn');
      }
      notifyAction($('status').textContent,true);
    }
    await refreshTelemetry();
  } catch (_) {
    if ((action === 'forward' || action === 'backward') && state.plannedPath?.pending) {
      state.plannedPath.pending = false;
      previewPlannedDistance();
    }
    if (version === state.stopVersion) {
      $('status').textContent = failure;
      notifyAction(failure,false);
    }
  } finally {
    if ((action === 'forward' || action === 'backward') && state.plannedPath?.pending) {
      state.plannedPath.pending = false;
      previewPlannedDistance();
    }
    if (!stopping) {
      await reassertStop(version);
      state.homeBusy = false;
      render();
    }
  }
}

export function moveDistance(action) {
  if (state.homeBusy || state.busy || state.releasing) return;
  if (state.operationActive || state.session) {
    notifyAction('Dừng xe và rời phiên HUD trước khi gửi lệnh quãng đường.',false);
    return;
  }
  const distance = Number($('moveDistance').value);
  if ($('moveDistance').value.trim() === '' || !Number.isInteger(distance) ||
      distance < 5 || distance > 1000) {
    $('status').textContent = 'Nhập quãng đường từ 5 đến 1000 cm.';
    notifyAction($('status').textContent,false);
    return;
  }
  state.plannedDirection = action;
  state.plannedPreviewRequested = true;
  setPlannedDistance(distance,action,true);
  void homeAction(action,{distance_cm:distance},
    'Đã nhận lệnh đi ' + distance + ' cm.',
    'Chưa thể di chuyển. Kiểm tra FC-03, MPU6050, pose và chờ xe đứng yên.');
}

export function turnAngle(action) {
  if (state.operationActive || state.session) {
    notifyAction('Dừng xe và rời phiên HUD trước khi gửi lệnh xoay.',false);
    return;
  }
  const angle = Number($('moveAngle').value);
  if (!Number.isInteger(angle) || angle < 5 || angle > 180) {
    notifyAction('Nhập góc xoay từ 5 đến 180°.',false);
    return;
  }
  void homeAction(action,{angle_degrees:angle},'Đã nhận lệnh xoay ' + angle + '°.',
    'Chưa thể xoay. Kiểm tra MPU6050, FC-03 và chờ xe đứng yên.');
}
