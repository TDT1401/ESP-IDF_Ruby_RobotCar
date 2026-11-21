import {state} from './state.js';
import {getStatus} from './api.js';
import {$, operationFromStatus, finishedOperation, showOperation, render,
  notifyAction, renderEncoder, updateDistanceAlert} from './ui.js';
import {updateTrajectory, previewPlannedDistance, breakTrajectory} from './trajectory.js';

export async function refreshTelemetry() {
  if (state.telemetryBusy) return false;
  state.telemetryBusy = true;
  const trackVersion = state.trajectoryVersion;
  try {
    const data = await getStatus();
    state.distanceDriveOwner = data.command_mode === 'distance_drive' ? data.distance_drive?.owner : '';
    state.turnOwner = data.command_mode === 'angle_turn' ? data.angle_turn_owner : '';
    state.navigationOwner = data.command_mode === 'navigate' ? data.navigation?.owner : '';
    state.lastStatusTime = new Date().toLocaleTimeString('vi-VN');
    $('homeUpdated').textContent = 'Cập nhật lúc ' + state.lastStatusTime;
    const operation = operationFromStatus(data);
    if (operation) {
      showOperation(operation[1],operation[0]);
    } else if (state.operationActive) {
      const result = finishedOperation(data);
      state.operationActive = false;
      state.operationKind = '';
      state.operationText = '';
      render();
      if (result && !$('actionToast').classList.contains('error'))
        notifyAction(result[0],result[1]);
    }
    renderEncoder(data.encoder);
    let piLine = $('speedPiState');
    if (!piLine && typeof document.createElement === 'function') {
      piLine = document.createElement('p');
      piLine.id = 'speedPiState';
      piLine.className = 'encoder-note';
      $('encoderState').after(piLine);
    }
    const piNumber = value => Number.isFinite(value) ? value.toFixed(1) : '--';
    if (data.navigation?.state && data.navigation.state !== 'idle') {
      const navigation = data.navigation;
      const navigationState = ({turning:'Đang xoay',driving:'Đang đi',completed:'Đã hoàn thành',
        interrupted:'Đã ngắt',fault:'Lỗi: ' + navigation.fault})[navigation.state] || navigation.state;
      $('navigateProgress').textContent = navigationState + ' · đích (' +
        piNumber(navigation.goal_x_m * 100) + ', ' + piNumber(navigation.goal_y_m * 100) +
        ') cm · khoảng cách ước lượng còn ' + piNumber(navigation.remaining_m * 100) + ' cm';
    }
    if (piLine) piLine.textContent = data.speed_pi_fault && data.speed_pi_fault !== 'none'
      ? 'PI tốc độ đã dừng: ' + data.speed_pi_fault
      : data.speed_pi_active
        ? 'PI L/R: mục tiêu ' + piNumber(data.speed_pi_left_target_rpm) + ' / ' +
          piNumber(data.speed_pi_right_target_rpm) + ' RPM; đo ' +
          piNumber(data.encoder?.left?.rpm) + ' / ' + piNumber(data.encoder?.right?.rpm) +
          ' RPM; PWM ' + data.left_pwm_percent + ' / ' + data.right_pwm_percent + '%'
        : data.speed_pi_calibrated ? 'PI tốc độ sẵn sàng' :
          'PI tốc độ chờ RPM tham chiếu từng bánh trong config/motion_control_config.h';
    if (typeof document.querySelector === 'function') {
      const speedLabel = document.querySelector('.hud-speed span');
      if (speedLabel) speedLabel.textContent =
        data.speed_pi_calibrated ? 'SPEED TARGET' : 'POWER OUTPUT';
    }
    $('homeNetwork').textContent = data.wifi_connected === false ? 'Wi-Fi đã ngắt' :
      Number.isFinite(data.wifi_rssi) ? 'Wi-Fi ' + data.wifi_rssi + ' dBm' : 'Wi-Fi đã kết nối';
    $('homeUptime').textContent = Number.isFinite(data.uptime_seconds)
      ? Math.floor(data.uptime_seconds / 3600) + ' giờ ' +
        Math.floor(data.uptime_seconds / 60 % 60) + ' phút' : '--';
    $('homeIp').textContent = data.ip_address || '--';
    $('homeRam').textContent = Number.isFinite(data.free_heap_kb) ? data.free_heap_kb : '--';
    const moveState = ({pending:'Đang chuẩn bị',moving:'Đang di chuyển',
      completed:'Đã hoàn thành',interrupted:'Đã ngắt',timeout:'Hết thời gian',
      no_pulses:'Không có xung FC-03',encoder_error:'Lỗi FC-03',
      obstacle:'Đã dừng vì vật cản',fault:'Lỗi: ' + (data.distance_drive?.fault || 'control_error')})[data.distance_move_state] || '';
    if (data.command_mode === 'angle_turn' && data.angle_turn_degrees !== 0) {
      $('moveProgress').textContent = operationFromStatus(data)?.[1] || '';
    } else {
      $('moveProgress').textContent = moveState && data.distance_move_target_cm > 0
        ? moveState + ' · ' + Math.round(data.distance_move_progress_cm || 0) +
          '/' + data.distance_move_target_cm + ' cm' : moveState;
      if (data.distance_drive && data.distance_move_target_cm > 0)
        $('moveProgress').textContent += ' · sai số quãng đường ' +
          piNumber(data.distance_drive.error_m * 100) + ' cm · sai số vị trí ước lượng ' +
          piNumber(data.distance_drive.position_error_m * 100) + ' cm';
    }
    if ($('poseState')) {
      const pose = data.odometry;
      $('poseState').textContent = pose?.valid
        ? 'Vị trí tương đối: x ' + pose.x_m.toFixed(3) + ' m · y ' + pose.y_m.toFixed(3) +
          ' m · θ ' + (pose.theta_rad * 180 / Math.PI).toFixed(1) + '°'
        : 'Pose chưa hợp lệ: ' + (pose?.fault || 'đang chờ cảm biến') +
          '. Dừng xe, chờ đứng yên rồi đặt lại gốc.';
    }
    if (trackVersion === state.trajectoryVersion) updateTrajectory(data);
    state.obstacleDistanceCm = Number.isFinite(data.obstacle_distance_cm)
      ? data.obstacle_distance_cm : 20;
    state.warningDistanceCm = Number.isFinite(data.warning_distance_cm)
      ? data.warning_distance_cm : state.obstacleDistanceCm;
    const distanceOk = data.distance_valid && Number.isFinite(data.distance_cm) &&
      data.distance_cm >= 0;
    const distanceText = distanceOk ? data.distance_cm + ' cm' : '-- cm';
    $('distance').textContent = distanceText;
    $('manualDistance').textContent = distanceText;
    $('sensorState').textContent = distanceOk ? 'SR04 hoạt động' : 'Lỗi SR04: không có Echo';
    $('sensorState').classList.toggle('ok',distanceOk);
    $('sensorState').classList.toggle('error',!distanceOk);
    updateDistanceAlert(distanceOk,data.distance_cm);
    render();
    return true;
  } catch (_) {
    if (trackVersion === state.trajectoryVersion) {
      state.latestPose = null;
      previewPlannedDistance();
      breakTrajectory('Mất kết nối; quỹ đạo sẽ nối lại ở đoạn mới.');
    }
    renderEncoder(null,true);
    if ($('poseState')) $('poseState').textContent = 'Chưa lấy được dữ liệu vị trí mới';
    if ($('speedPiState')) $('speedPiState').textContent = 'Không có dữ liệu PI mới';
    $('distance').textContent = '-- cm';
    $('manualDistance').textContent = '-- cm';
    $('sensorState').textContent = 'Chưa lấy được dữ liệu SR04 mới';
    $('sensorState').classList.toggle('ok',false);
    $('sensorState').classList.toggle('error',true);
    updateDistanceAlert(false,0);
    $('homeUpdated').textContent = state.lastStatusTime
      ? 'Chưa lấy được trạng thái mới · lần cuối ' + state.lastStatusTime
      : 'Chưa lấy được trạng thái xe.';
    state.operationStale = true;
    render();
    return false;
  } finally { state.telemetryBusy = false; }
}
