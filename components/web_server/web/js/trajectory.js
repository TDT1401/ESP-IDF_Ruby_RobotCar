import {state} from './state.js';
import {$} from './ui.js';

const trajectoryLimit = 12000;

export function setPlannedDistance(distance, direction, pending = false) {
  if (!state.latestPose) {
    state.plannedPath = null;
    $('plannedPathState').textContent = 'Chưa có pose hợp lệ để vẽ đường yêu cầu.';
    drawTrajectory();
    return;
  }
  const signedDistance = distance / 100 * (direction === 'backward' ? -1 : 1);
  state.plannedPath = {startX:state.latestPose.x_m,startY:state.latestPose.y_m,
    endX:state.latestPose.x_m + signedDistance * Math.cos(state.latestPose.theta_rad),
    endY:state.latestPose.y_m + signedDistance * Math.sin(state.latestPose.theta_rad),
    distanceCm:distance,direction,pending,locked:false};
  $('plannedPathState').textContent = (pending ? 'Đang gửi lệnh' : 'Xem trước') + ' · ' +
    (direction === 'backward' ? 'lùi ' : 'tiến ') + distance + ' cm theo hướng hiện tại.';
  drawTrajectory();
}

export function previewPlannedDistance() {
  if (!state.plannedPreviewRequested || state.plannedPath?.pending || state.plannedPath?.locked) return;
  const distance = Number($('moveDistance').value);
  if ($('moveDistance').value.trim() === '' || !Number.isInteger(distance) ||
      distance < 5 || distance > 1000) {
    state.plannedPath = null;
    $('plannedPathState').textContent = 'Nhập quãng đường từ 5 đến 1000 cm để xem trước.';
    drawTrajectory();
    return;
  }
  setPlannedDistance(distance,state.plannedDirection);
}

export function drawTrajectory() {
  if (state.activePanel !== 'drive') return;
  const canvas = $('trajectoryCanvas');
  const width = canvas.clientWidth, height = canvas.clientHeight;
  if (!width || !height) return;
  const ctx = canvas.getContext('2d');
  if (!ctx) return;
  const ratio = Math.min(window.devicePixelRatio || 1,2);
  canvas.width = Math.round(width * ratio);
  canvas.height = Math.round(height * ratio);
  ctx.setTransform(ratio,0,0,ratio,0,0);
  ctx.clearRect(0,0,width,height);
  let minX = 0, maxX = 0, minY = 0, maxY = 0, latest = null;
  for (const point of state.trajectory) {
    if (!point.valid) continue;
    minX = Math.min(minX,point.x_m); maxX = Math.max(maxX,point.x_m);
    minY = Math.min(minY,point.y_m); maxY = Math.max(maxY,point.y_m);
    latest = point;
  }
  if (state.plannedPath) {
    minX = Math.min(minX,state.plannedPath.startX,state.plannedPath.endX);
    maxX = Math.max(maxX,state.plannedPath.startX,state.plannedPath.endX);
    minY = Math.min(minY,state.plannedPath.startY,state.plannedPath.endY);
    maxY = Math.max(maxY,state.plannedPath.startY,state.plannedPath.endY);
  }
  const centreX = (minX + maxX) / 2, centreY = (minY + maxY) / 2;
  const scale = Math.min(Math.max(1,width - 64) / Math.max(0.5,(maxX - minX) * 1.25),
    Math.max(1,height - 64) / Math.max(0.5,(maxY - minY) * 1.25));
  const px = x => width / 2 + (x - centreX) * scale;
  const py = y => height / 2 - (y - centreY) * scale;
  let grid = 10 ** Math.floor(Math.log10(70 / scale));
  if (grid * scale < 45) grid *= 2;
  if (grid * scale < 45) grid *= 2.5;
  ctx.lineWidth = 1;
  ctx.strokeStyle = '#28405d';
  for (let x = Math.ceil((centreX - width / 2 / scale) / grid) * grid;
    px(x) <= width; x += grid) {
    ctx.beginPath(); ctx.moveTo(px(x),0); ctx.lineTo(px(x),height); ctx.stroke();
  }
  for (let y = Math.ceil((centreY - height / 2 / scale) / grid) * grid;
    py(y) >= 0; y += grid) {
    ctx.beginPath(); ctx.moveTo(0,py(y)); ctx.lineTo(width,py(y)); ctx.stroke();
  }
  ctx.strokeStyle = '#6a86a8';
  ctx.beginPath(); ctx.moveTo(px(0),0); ctx.lineTo(px(0),height);
  ctx.moveTo(0,py(0)); ctx.lineTo(width,py(0)); ctx.stroke();
  ctx.fillStyle = '#9fb0cd';
  ctx.font = '11px system-ui, sans-serif';
  ctx.fillText('X →',width - 34,Math.min(height - 10,Math.max(16,py(0) - 7)));
  ctx.fillText('Y ↑',Math.min(width - 32,Math.max(9,px(0) + 7)),16);
  ctx.fillText(grid.toPrecision(2) + ' m / ô',10,height - 10);
  if (state.plannedPath) {
    ctx.strokeStyle = '#a9b0bd';
    ctx.lineWidth = 2.5;
    ctx.setLineDash([7,5]);
    ctx.beginPath();
    ctx.moveTo(px(state.plannedPath.startX),py(state.plannedPath.startY));
    ctx.lineTo(px(state.plannedPath.endX),py(state.plannedPath.endY));
    ctx.stroke();
    ctx.setLineDash([]);
    ctx.fillStyle = '#cbd0d9';
    ctx.beginPath();
    ctx.arc(px(state.plannedPath.endX),py(state.plannedPath.endY),4,0,Math.PI * 2);
    ctx.fill();
  }
  ctx.strokeStyle = '#28d7ff';
  ctx.lineWidth = 2.5;
  ctx.beginPath();
  let penDown = false, segment = -1;
  for (const point of state.trajectory) {
    if (!point.valid) { penDown = false; continue; }
    if (!penDown || segment !== point.segment) ctx.moveTo(px(point.x_m),py(point.y_m));
    else ctx.lineTo(px(point.x_m),py(point.y_m));
    penDown = true; segment = point.segment;
  }
  ctx.stroke();
  ctx.fillStyle = '#ffffff';
  ctx.beginPath(); ctx.arc(px(0),py(0),3,0,Math.PI * 2); ctx.fill();
  if (latest && state.trajectoryValid) {
    const x = px(latest.x_m), y = py(latest.y_m);
    ctx.strokeStyle = '#39e58c'; ctx.lineWidth = 3;
    ctx.beginPath(); ctx.moveTo(x,y);
    ctx.lineTo(x + 18 * Math.cos(latest.theta_rad),
      y - 18 * Math.sin(latest.theta_rad)); ctx.stroke();
    ctx.fillStyle = '#39e58c';
    ctx.beginPath(); ctx.arc(x,y,5,0,Math.PI * 2); ctx.fill();
  }
}

function appendTrajectory(point) {
  state.trajectory.push(point);
  if (point.valid) state.trajectoryCount++;
  if (state.trajectory.length > trajectoryLimit && state.trajectory.shift().valid) state.trajectoryCount--;
  $('trajectoryCount').textContent = state.trajectoryCount + ' mẫu (giữ tối đa ' +
    trajectoryLimit + ' bản ghi gần nhất)';
  $('downloadTrajectory').disabled = state.trajectoryCount === 0;
  drawTrajectory();
}

export function breakTrajectory(reason) {
  if (state.trajectoryValid) {
    state.trajectorySegment++;
    appendTrajectory({captured_at:new Date().toISOString(),valid:false,
      fault:reason,segment:state.trajectorySegment});
  }
  state.trajectoryValid = false;
  $('trajectoryState').textContent = reason;
  drawTrajectory();
}

export function updateTrajectory(data) {
  const pose = data.odometry;
  if (!pose?.valid || !Number.isFinite(pose.x_m) || !Number.isFinite(pose.y_m) ||
      !Number.isFinite(pose.theta_rad) || !Number.isFinite(pose.sampled_at_ms)) {
    state.latestPose = null;
    previewPlannedDistance();
    breakTrajectory('Pose chưa hợp lệ: ' + (pose?.fault || 'đang chờ cảm biến'));
    return;
  }
  if (pose.sampled_at_ms < state.lastTrajectorySampleMs) {
    breakTrajectory('Thiết bị đã khởi động lại; bắt đầu đoạn mới.');
    state.plannedPath = null;
    state.plannedPreviewRequested = false;
    $('plannedPathState').textContent = 'Thiết bị đã khởi động lại; nhập quãng đường mới.';
  }
  state.latestPose = {x_m:pose.x_m,y_m:pose.y_m,theta_rad:pose.theta_rad};
  previewPlannedDistance();
  if (pose.sampled_at_ms === state.lastTrajectorySampleMs) return;
  state.lastTrajectorySampleMs = pose.sampled_at_ms;
  state.trajectoryValid = true;
  const loggingPlan = state.plannedPath?.locked && state.operationKind === 'distance';
  appendTrajectory({captured_at:new Date().toISOString(),device_ms:pose.sampled_at_ms,
    segment:state.trajectorySegment,valid:true,fault:pose.fault,x_m:pose.x_m,y_m:pose.y_m,
    theta_rad:pose.theta_rad,velocity_mps:pose.velocity_mps,left_m:pose.left_m,
    right_m:pose.right_m,command_mode:data.command_mode,
    distance_state:data.distance_move_state,target_m:data.distance_drive?.target_m,
    plan_start_x_m:loggingPlan ? state.plannedPath.startX : null,
    plan_start_y_m:loggingPlan ? state.plannedPath.startY : null,
    plan_end_x_m:loggingPlan ? state.plannedPath.endX : null,
    plan_end_y_m:loggingPlan ? state.plannedPath.endY : null,
    left_rpm:data.encoder?.left?.rpm,right_rpm:data.encoder?.right?.rpm,
    left_pwm:data.left_pwm_percent,right_pwm:data.right_pwm_percent});
  $('trajectoryState').textContent = 'Đang nhận vị trí · x ' + pose.x_m.toFixed(3) +
    ' m · y ' + pose.y_m.toFixed(3) + ' m';
}

export function clearTrajectory() {
  state.trajectoryVersion++;
  state.trajectory.length = 0;
  state.trajectoryCount = 0;
  state.trajectorySegment = 0;
  state.trajectoryValid = false;
  state.lastTrajectorySampleMs = -1;
  state.latestPose = null;
  state.plannedPath = null;
  state.plannedPreviewRequested = false;
  $('plannedPathState').textContent = 'Nhập quãng đường để xem đường yêu cầu.';
  $('trajectoryState').textContent = 'Đang chờ vị trí hợp lệ…';
  $('trajectoryCount').textContent = '0 mẫu';
  $('downloadTrajectory').disabled = true;
  drawTrajectory();
}

export function downloadTrajectory() {
  if (!state.trajectoryCount) return;
  const columns = ['captured_at','device_ms','segment','valid','fault','x_m','y_m',
    'theta_rad','velocity_mps','left_m','right_m','command_mode','distance_state',
    'target_m','plan_start_x_m','plan_start_y_m','plan_end_x_m','plan_end_y_m',
    'left_rpm','right_rpm','left_pwm','right_pwm'];
  const cell = value => '"' + String(value ?? '').replaceAll('"','""') + '"';
  const csv = [columns.join(','),...state.trajectory.map(point =>
    columns.map(key => cell(point[key])).join(','))].join('\r\n');
  const url = URL.createObjectURL(new Blob([csv],{type:'text/csv;charset=utf-8'}));
  const link = document.createElement('a');
  link.href = url;
  link.download = 'ruby-trajectory-' + new Date().toISOString().replaceAll(':','-') + '.csv';
  link.click();
  setTimeout(() => URL.revokeObjectURL(url),1000);
}
