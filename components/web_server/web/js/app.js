
    'use strict';
    const $ = id => document.getElementById(id);
    const driveButtons = [...document.querySelectorAll('[data-dir]')];
    const speedValues = [50,75,100];
    const names = {s:'Đã dừng',f:'Tiến',b:'Lùi',l:'Xoay trái',r:'Xoay phải'};
    let session = 0, sequence = 0, epoch = 0, busy = false, pending = false;
    let direction = 's', releasing = false, heldPointer = null;
    let joystickX = 0, joystickY = 0;
    let joystickPointer = null;
    let telemetryBusy = false, homeBusy = false, stopVersion = 0;
    let recovering = false, wakeLock = null, wakePending = false, wakeUnavailable = false;
    let hudMode = false, warningEnabled = true, speed = 75;
    let warningDistanceCm = 30, obstacleDistanceCm = 20;
    let toastTimer, lastStatusTime = '';
    let operationActive = false, operationKind = '', operationText = '', operationStale = false;
    let distanceDriveOwner = '';
    let turnOwner = '';
    let navigationOwner = '';
    let activePanel = 'manual';
    const trajectory = [];
    const trajectoryLimit = 12000;
    let lastTrajectorySampleMs = -1, trajectoryValid = false, trajectorySegment = 0;
    let trajectoryCount = 0;
    let trajectoryVersion = 0;
    let latestPose = null, plannedPath = null;
    let plannedPreviewRequested = false, plannedDirection = 'forward';

    function dismissNotice() {
      $('actionToast').className = '';
      $('actionToastText').textContent = '';
      $('actionToastClose').hidden = true;
    }

    function notifyAction(message, successful) {
      const toast = $('actionToast');
      clearTimeout(toastTimer);
      toast.setAttribute('role',successful ? 'status' : 'alert');
      toast.setAttribute('aria-live',successful ? 'polite' : 'assertive');
      $('actionToastText').textContent = message;
      $('actionToastClose').hidden = successful;
      toast.className = 'show ' + (successful ? 'success' : 'error');
      if (successful) toastTimer = setTimeout(dismissNotice,5000);
    }

    function showOperation(text, kind) {
      operationActive = true;
      operationKind = kind;
      operationText = text;
      operationStale = false;
      render();
    }

    function operationFromStatus(data) {
      if (data.command_mode === 'navigate' && data.navigation) {
        const navigation = data.navigation;
        const state = navigation.state === 'turning' ? 'Đang xoay' : 'Đang đi';
        return ['navigate',state + ' tới (' + Math.round(navigation.goal_x_m * 100) + ', ' +
          Math.round(navigation.goal_y_m * 100) + ') cm · còn ' +
          (navigation.remaining_m * 100).toFixed(1) + ' cm'];
      }
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

    function finishedOperation(data) {
      if (operationKind === 'navigate') {
        if (data.navigation?.state === 'completed') return ['Đã hoàn thành lệnh đi tới tọa độ.',true];
        if (data.navigation?.state === 'fault')
          return ['Lệnh đi tới tọa độ dừng do lỗi: ' + data.navigation.fault,false];
        return ['Lệnh đi tới tọa độ đã bị ngắt.',false];
      }
      if (operationKind === 'turn') {
        if (data.angle_turn_state === 'completed') return ['Đã xoay đến góc đặt.',true];
        if (data.angle_turn_state === 'fault')
          return ['Xe dừng vì lỗi cảm biến, motor hoặc quá thời gian.',false];
        return ['Lệnh xoay đã bị ngắt.',false];
      }
      if (operationKind === 'distance') {
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

    function showPanel(name) {
      if (hudMode && name !== 'hud') {
        hudMode = false;
        park('Đã rời HUD và trả quyền điều khiển về giọng nói.');
        updateDistanceAlert(false,0);
        $('manualPanel').classList.toggle('hud-mode',false);
        setDriveFallback(false);
      }
      activePanel = name;
      for (const mode of ['manual','drive']) {
        $(mode + 'Panel').classList.toggle('active',mode === (name === 'hud' ? 'manual' : name));
        $(mode + 'Tab').classList.toggle('active',mode === name);
      }
      $('hudTab').classList.toggle('active',name === 'hud');
      if (name === 'drive') {
        drawTrajectory();
        void refreshTelemetry();
      }
      render();
    }

    function updateDistanceAlert(valid, distance) {
      const nearObstacle = hudMode && valid && distance < warningDistanceCm;
      $('hudDistance').classList.toggle('danger',nearObstacle);
      $('hudWarning').classList.toggle('active',nearObstacle);
      $('hudWarning').textContent = distance < obstacleDistanceCm
        ? '⚠ Nguy cơ va chạm · dưới ' + obstacleDistanceCm + ' cm'
        : '⚠ Cảnh báo sớm · dưới ' + warningDistanceCm + ' cm';
    }

    function renderEncoder(encoder, disconnected = false) {
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
      let state = disconnected ? 'Mất kết nối số đo' : !encoder?.enabled ? 'FC-03 chưa bật'
        : !ready ? 'Lỗi FC-03: ' + (encoder.error || 'không có số đo')
        : !calibrated ? 'Chưa hiệu chuẩn: xem tổng xung và log FC03 trong monitor'
        : encoder.distance_complete === false ? 'Ước lượng thiếu xung không xác định được chiều quay'
        : 'Chiều suy ra từ lệnh motor; có sai số khi bánh trượt';
      if (ready && (encoder.left?.no_pulses || encoder.right?.no_pulses)) {
        const sides = [encoder.left?.no_pulses ? 'trái' : '', encoder.right?.no_pulses ? 'phải' : ''].filter(Boolean);
        state += ' · Không có xung bánh ' + sides.join('/') + ' khi đang có lệnh chạy';
      }
      $('encoderState').textContent = state;
      $('hudEncoderState').textContent = ready && !calibrated
        ? 'Chưa hiệu chuẩn · xung ' + $('encoderPulses').textContent : state;
    }

    function setPlannedDistance(distance, direction, pending = false) {
      if (!latestPose) {
        plannedPath = null;
        $('plannedPathState').textContent = 'Chưa có pose hợp lệ để vẽ đường yêu cầu.';
        drawTrajectory();
        return;
      }
      const signedDistance = distance / 100 * (direction === 'backward' ? -1 : 1);
      plannedPath = {startX:latestPose.x_m,startY:latestPose.y_m,
        endX:latestPose.x_m + signedDistance * Math.cos(latestPose.theta_rad),
        endY:latestPose.y_m + signedDistance * Math.sin(latestPose.theta_rad),
        distanceCm:distance,direction,pending,locked:false};
      $('plannedPathState').textContent = (pending ? 'Đang gửi lệnh' : 'Xem trước') + ' · ' +
        (direction === 'backward' ? 'lùi ' : 'tiến ') + distance + ' cm theo hướng hiện tại.';
      drawTrajectory();
    }

    function previewPlannedDistance() {
      if (!plannedPreviewRequested || plannedPath?.pending || plannedPath?.locked) return;
      const distance = Number($('moveDistance').value);
      if ($('moveDistance').value.trim() === '' || !Number.isInteger(distance) ||
          distance < 5 || distance > 1000) {
        plannedPath = null;
        $('plannedPathState').textContent = 'Nhập quãng đường từ 5 đến 1000 cm để xem trước.';
        drawTrajectory();
        return;
      }
      setPlannedDistance(distance,plannedDirection);
    }

    function drawTrajectory() {
      if (activePanel !== 'drive') return;
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
      for (const point of trajectory) {
        if (!point.valid) continue;
        minX = Math.min(minX,point.x_m); maxX = Math.max(maxX,point.x_m);
        minY = Math.min(minY,point.y_m); maxY = Math.max(maxY,point.y_m);
        latest = point;
      }
      if (plannedPath) {
        minX = Math.min(minX,plannedPath.startX,plannedPath.endX);
        maxX = Math.max(maxX,plannedPath.startX,plannedPath.endX);
        minY = Math.min(minY,plannedPath.startY,plannedPath.endY);
        maxY = Math.max(maxY,plannedPath.startY,plannedPath.endY);
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
      if (plannedPath) {
        ctx.strokeStyle = '#a9b0bd';
        ctx.lineWidth = 2.5;
        ctx.setLineDash([7,5]);
        ctx.beginPath();
        ctx.moveTo(px(plannedPath.startX),py(plannedPath.startY));
        ctx.lineTo(px(plannedPath.endX),py(plannedPath.endY));
        ctx.stroke();
        ctx.setLineDash([]);
        ctx.fillStyle = '#cbd0d9';
        ctx.beginPath();
        ctx.arc(px(plannedPath.endX),py(plannedPath.endY),4,0,Math.PI * 2);
        ctx.fill();
      }
      ctx.strokeStyle = '#28d7ff';
      ctx.lineWidth = 2.5;
      ctx.beginPath();
      let penDown = false, segment = -1;
      for (const point of trajectory) {
        if (!point.valid) { penDown = false; continue; }
        if (!penDown || segment !== point.segment) ctx.moveTo(px(point.x_m),py(point.y_m));
        else ctx.lineTo(px(point.x_m),py(point.y_m));
        penDown = true; segment = point.segment;
      }
      ctx.stroke();
      ctx.fillStyle = '#ffffff';
      ctx.beginPath(); ctx.arc(px(0),py(0),3,0,Math.PI * 2); ctx.fill();
      if (latest && trajectoryValid) {
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
      trajectory.push(point);
      if (point.valid) trajectoryCount++;
      if (trajectory.length > trajectoryLimit && trajectory.shift().valid) trajectoryCount--;
      $('trajectoryCount').textContent = trajectoryCount + ' mẫu (giữ tối đa ' +
        trajectoryLimit + ' bản ghi gần nhất)';
      $('downloadTrajectory').disabled = trajectoryCount === 0;
      drawTrajectory();
    }

    function breakTrajectory(reason) {
      if (trajectoryValid) {
        trajectorySegment++;
        appendTrajectory({captured_at:new Date().toISOString(),valid:false,
          fault:reason,segment:trajectorySegment});
      }
      trajectoryValid = false;
      $('trajectoryState').textContent = reason;
      drawTrajectory();
    }

    function updateTrajectory(data) {
      const pose = data.odometry;
      if (!pose?.valid || !Number.isFinite(pose.x_m) || !Number.isFinite(pose.y_m) ||
          !Number.isFinite(pose.theta_rad) || !Number.isFinite(pose.sampled_at_ms)) {
        latestPose = null;
        previewPlannedDistance();
        breakTrajectory('Pose chưa hợp lệ: ' + (pose?.fault || 'đang chờ cảm biến'));
        return;
      }
      if (pose.sampled_at_ms < lastTrajectorySampleMs) {
        breakTrajectory('Thiết bị đã khởi động lại; bắt đầu đoạn mới.');
        plannedPath = null;
        plannedPreviewRequested = false;
        $('plannedPathState').textContent = 'Thiết bị đã khởi động lại; nhập quãng đường mới.';
      }
      latestPose = {x_m:pose.x_m,y_m:pose.y_m,theta_rad:pose.theta_rad};
      previewPlannedDistance();
      if (pose.sampled_at_ms === lastTrajectorySampleMs) return;
      lastTrajectorySampleMs = pose.sampled_at_ms;
      trajectoryValid = true;
      const loggingPlan = plannedPath?.locked && operationKind === 'distance';
      appendTrajectory({captured_at:new Date().toISOString(),device_ms:pose.sampled_at_ms,
        segment:trajectorySegment,valid:true,fault:pose.fault,x_m:pose.x_m,y_m:pose.y_m,
        theta_rad:pose.theta_rad,velocity_mps:pose.velocity_mps,left_m:pose.left_m,
        right_m:pose.right_m,command_mode:data.command_mode,
        distance_state:data.distance_move_state,target_m:data.distance_drive?.target_m,
        plan_start_x_m:loggingPlan ? plannedPath.startX : null,
        plan_start_y_m:loggingPlan ? plannedPath.startY : null,
        plan_end_x_m:loggingPlan ? plannedPath.endX : null,
        plan_end_y_m:loggingPlan ? plannedPath.endY : null,
        left_rpm:data.encoder?.left?.rpm,right_rpm:data.encoder?.right?.rpm,
        left_pwm:data.left_pwm_percent,right_pwm:data.right_pwm_percent});
      $('trajectoryState').textContent = 'Đang nhận vị trí · x ' + pose.x_m.toFixed(3) +
        ' m · y ' + pose.y_m.toFixed(3) + ' m';
    }

    function clearTrajectory() {
      trajectoryVersion++;
      trajectory.length = 0;
      trajectoryCount = 0;
      trajectorySegment = 0;
      trajectoryValid = false;
      lastTrajectorySampleMs = -1;
      latestPose = null;
      plannedPath = null;
      plannedPreviewRequested = false;
      $('plannedPathState').textContent = 'Nhập quãng đường để xem đường yêu cầu.';
      $('trajectoryState').textContent = 'Đang chờ vị trí hợp lệ…';
      $('trajectoryCount').textContent = '0 mẫu';
      $('downloadTrajectory').disabled = true;
      drawTrajectory();
    }

    function downloadTrajectory() {
      if (!trajectoryCount) return;
      const columns = ['captured_at','device_ms','segment','valid','fault','x_m','y_m',
        'theta_rad','velocity_mps','left_m','right_m','command_mode','distance_state',
        'target_m','plan_start_x_m','plan_start_y_m','plan_end_x_m','plan_end_y_m',
        'left_rpm','right_rpm','left_pwm','right_pwm'];
      const cell = value => '"' + String(value ?? '').replaceAll('"','""') + '"';
      const csv = [columns.join(','),...trajectory.map(point =>
        columns.map(key => cell(point[key])).join(','))].join('\r\n');
      const url = URL.createObjectURL(new Blob([csv],{type:'text/csv;charset=utf-8'}));
      const link = document.createElement('a');
      link.href = url;
      link.download = 'ruby-trajectory-' + new Date().toISOString().replaceAll(':','-') + '.csv';
      link.click();
      setTimeout(() => URL.revokeObjectURL(url),1000);
    }

    async function refreshTelemetry() {
      if (telemetryBusy) return false;
      telemetryBusy = true;
      const trackVersion = trajectoryVersion;
      const controller = new AbortController();
      const timeout = setTimeout(() => controller.abort(),500);
      try {
        const response = await fetch('/api/status',{cache:'no-store',signal:controller.signal});
        if (!response.ok) throw new Error(String(response.status));
        const data = await response.json();
        distanceDriveOwner = data.command_mode === 'distance_drive' ? data.distance_drive?.owner : '';
        turnOwner = data.command_mode === 'angle_turn' ? data.angle_turn_owner : '';
        navigationOwner = data.command_mode === 'navigate' ? data.navigation?.owner : '';
        lastStatusTime = new Date().toLocaleTimeString('vi-VN');
        $('homeUpdated').textContent = 'Cập nhật lúc ' + lastStatusTime;
        const operation = operationFromStatus(data);
        if (operation) {
          showOperation(operation[1],operation[0]);
        } else if (operationActive) {
          const result = finishedOperation(data);
          operationActive = false;
          operationKind = '';
          operationText = '';
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
          const state = ({turning:'Đang xoay',driving:'Đang đi',completed:'Đã hoàn thành',
            interrupted:'Đã ngắt',fault:'Lỗi: ' + navigation.fault})[navigation.state] || navigation.state;
          $('navigateProgress').textContent = state + ' · đích (' +
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
        if (trackVersion === trajectoryVersion) updateTrajectory(data);
        obstacleDistanceCm = Number.isFinite(data.obstacle_distance_cm)
          ? data.obstacle_distance_cm : 20;
        warningDistanceCm = Number.isFinite(data.warning_distance_cm)
          ? data.warning_distance_cm : obstacleDistanceCm;
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
        if (trackVersion === trajectoryVersion) {
          latestPose = null;
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
        $('homeUpdated').textContent = lastStatusTime
          ? 'Chưa lấy được trạng thái mới · lần cuối ' + lastStatusTime
          : 'Chưa lấy được trạng thái xe.';
        operationStale = true;
        render();
        return false;
      } finally { clearTimeout(timeout); telemetryBusy = false; }
    }

    function driveIsFullscreen() {
      return document.fullscreenElement === $('manualPanel') ||
        document.webkitFullscreenElement === $('manualPanel') ||
        $('manualPanel').classList.contains('drive-expanded');
    }

    function setDriveFallback(active) {
      $('manualPanel').classList.toggle('drive-expanded',active);
      document.body.classList.toggle('drive-expanded',active);
      render();
    }

    // Optional: ESP32 HTTP pages may not expose the secure-context Wake Lock API.
    async function syncWakeLock() {
      const wanted = Boolean(session) && driveIsFullscreen() && !document.hidden;
      if (!wanted) {
        const previous = wakeLock;
        wakeLock = null;
        if (previous) { try { await previous.release(); } catch (_) {} }
        return;
      }
      if (wakeLock || wakePending || wakeUnavailable) return;
      if (typeof navigator === 'undefined' || !navigator.wakeLock) {
        wakeUnavailable = true;
        renderHud();
        return;
      }
      wakePending = true;
      try {
        const lock = await navigator.wakeLock.request('screen');
        if (!session || !driveIsFullscreen() || document.hidden) {
          await lock.release();
          return;
        }
        wakeLock = lock;
        lock.addEventListener('release',() => {
          if (wakeLock === lock) {
            wakeLock = null;
            wakeUnavailable = true;
            renderHud();
          }
        });
      } catch (_) { wakeUnavailable = true; }
      finally { wakePending = false; renderHud(); }
    }

    function renderHud() {
      $('hudSound').disabled = !session || releasing || recovering;
      $('hudSound').setAttribute('aria-pressed',String(warningEnabled));
      $('hudSound').textContent = warningEnabled ? '🔊 Âm: Bật' : '🔇 Âm: Tắt';
      $('hudConnect').disabled = Boolean(session) || busy;
      $('hudRotateConnect').disabled = Boolean(session) || busy;
      $('hudConnect').textContent = recovering ? 'Đang nối lại…' : session ? '● Đã kết nối' : '↻ Kết nối lại';
      $('hudRotateStatus').textContent = session
        ? 'Đã kết nối. Xoay ngang để điều khiển xe.' : $('status').textContent;
      $('hudNotice').textContent = !session ? $('status').textContent : recovering
        ? 'Xe đã dừng · Đang kiểm tra kết nối…' : wakeUnavailable
        ? 'Hãy tăng thời gian tự tắt màn hình điện thoại khi lái.' : '';
    }

    function render() {
      $('hudStop').disabled = !session || releasing;
      const actionBusy = homeBusy || busy || releasing;
      $('wakeRobot').disabled = actionBusy;
      for (const id of ['moveForward','moveBackward','moveLeft','moveRight','resetPose','navigateStart'])
        if ($(id)) $(id).disabled = actionBusy || operationActive || Boolean(session);
      const showBar = operationActive && !hudMode;
      $('operationBar').hidden = !showBar;
      $('operationText').textContent = operationText + (operationStale ? ' · Chưa cập nhật mới' : '');
      document.body.classList.toggle('operation-active',showBar);
      driveButtons.forEach(button => {
        button.disabled = !hudMode || !session || releasing || recovering;
        button.classList.toggle('held', Boolean(session) && button.dataset.dir === direction);
      });
      if ($('hudJoystick')) {
        $('hudJoystick').setAttribute('aria-disabled',String(!hudMode || !session || releasing || recovering));
        $('hudJoystick').classList.toggle('disabled',!hudMode || !session || releasing || recovering);
      }
      renderHud();
      void syncWakeLock();
    }

    function setSpeed(value) {
      speed = value;
      $('hudSpeedValue').textContent = value + '%';
      speedValues.forEach(speed => $('speed' + speed).classList.toggle('active',speed === value));
      warningDistanceCm = Math.ceil(obstacleDistanceCm * Math.max(value,50) / 50);
      updateDistanceAlert($('manualDistance').textContent !== '-- cm',
        Number.parseInt($('manualDistance').textContent,10));
      if (hudMode && session && !releasing) { pending = true; void pump(); }
    }

    function enterHud() {
      if (hudMode) return;
      if (operationActive) {
        notifyAction('Dừng xe trước khi vào HUD để không ngắt lệnh đang chạy.',false);
        return;
      }
      hudMode = true;
      showPanel('hud');
      $('manualPanel').classList.toggle('hud-mode',true);
      setDriveFallback(true);
      void claimControl();
      void refreshTelemetry();
    }

    function exitHud() {
      if (!hudMode) return;
      showPanel('manual');
    }

    function commandBody(token, dir) {
      const command = {session:token,sequence:++sequence,direction:dir,
        speed,warning_enabled:warningEnabled};
      if (dir === 'j') { command.x = joystickX; command.y = joystickY; }
      return JSON.stringify(command);
    }

    async function post(path, body) {
      const controller = new AbortController();
      const timeout = setTimeout(() => controller.abort(), 350);
      try {
        const response = await fetch(path, {
          method:'POST',headers:{'X-Robot-Control':'1','Content-Type':'application/json'},
          body,signal:controller.signal,cache:'no-store'
        });
        if (!response.ok) {
          const error = new Error(String(response.status));
          error.status = response.status;
          throw error;
        }
        return await response.json();
      } finally { clearTimeout(timeout); }
    }

    // Best-effort release on HUD exit or page loss. The firmware's deadline remains authoritative.
    // A newer sequence prevents an older in-flight movement from restoring control.
    function park(message) {
      if (session) {
        fetch('/api/command', {
          method:'POST',headers:{'X-Robot-Control':'1','Content-Type':'application/json'},
          body:commandBody(session,'release'),keepalive:true
        }).catch(() => {});
      }
      ++epoch;
      session = 0; direction = 's'; pending = false; releasing = false;
      recovering = false;
      heldPointer = null;
      joystickPointer = null; joystickX = 0; joystickY = 0;
      if ($('hudJoystick')) $('hudJoystick').classList.toggle('active',false);
      if ($('hudJoystickHandle')) {
        $('hudJoystickHandle').style.left = '50%';
        $('hudJoystickHandle').style.top = '50%';
      }
      $('status').textContent = message;
      render();
    }

    async function pump() {
      if (!session || busy) return;
      busy = true; pending = false;
      const currentEpoch = epoch, sentDirection = direction;
      const sentSpeed = speed;
      render();
      try {
        await post('/api/command',commandBody(session,sentDirection));
        if (currentEpoch !== epoch) return;
        recovering = false;
        if (sentDirection === 'release') {
          session = 0; direction = 's'; releasing = false; pending = false;
          $('status').textContent = 'Đã trả quyền điều khiển motor về giọng nói.';
        } else if (sentDirection === direction) {
          $('status').textContent = (sentSpeed === 0 ? names.s : sentDirection === 'j'
            ? 'Joystick' : names[sentDirection]) + ' · Điều khiển thủ công';
        }
      } catch (error) {
        if (currentEpoch === epoch) {
          // Retry once with a newer STOP, never replay a held movement or claim a new session.
          if (!recovering && !releasing && !error.status && !document.hidden) {
            recovering = true;
            heldPointer = null; resetJoystick();
            direction = 's'; pending = true;
          } else {
            park(error.status === 409
              ? 'Phiên điều khiển đã kết thúc. Bấm kết nối lại để tiếp tục.'
              : 'Mất kết nối. Xe tự dừng; bấm kết nối lại để tiếp tục.');
          }
        }
      } finally {
        busy = false;
        render();
        if (pending && session) void pump();
      }
    }

    function requestDirection(next) {
      if (!hudMode || !session || releasing || (recovering && next !== 's')) return;
      if (next !== 'j') { joystickX = 0; joystickY = 0; }
      direction = next; pending = true;
      render();
      void pump();
    }

    function stop() {
      heldPointer = null; joystickPointer = null;
      resetJoystick();
      requestDirection('s');
    }

    function resetJoystick() {
      joystickX = 0; joystickY = 0; joystickPointer = null;
      if ($('hudJoystick')) $('hudJoystick').classList.toggle('active',false);
      if ($('hudJoystickHandle')) {
        $('hudJoystickHandle').style.left = '50%';
        $('hudJoystickHandle').style.top = '50%';
      }
    }

    function updateJoystick(event) {
      const surface = $('hudJoystick');
      const bounds = surface.getBoundingClientRect();
      const radius = Math.min(bounds.width,bounds.height) * 0.36;
      const dx = event.clientX - (bounds.left + bounds.width / 2);
      const dy = event.clientY - (bounds.top + bounds.height / 2);
      const length = Math.hypot(dx,dy);
      const scale = length > radius ? radius / length : 1;
      const nx = dx * scale / radius;
      const ny = dy * scale / radius;
      const deadzone = 0.12;
      joystickX = Math.abs(nx) < deadzone ? 0 : Math.round(nx * 1000);
      joystickY = Math.abs(ny) < deadzone ? 0 : Math.round(-ny * 1000);
      $('hudJoystickHandle').style.left = (50 + nx * 36) + '%';
      $('hudJoystickHandle').style.top = (50 + ny * 36) + '%';
      $('hudJoystick').classList.toggle('active',joystickX !== 0 || joystickY !== 0);
      if (joystickX === 0 && joystickY === 0) requestDirection('s');
      else requestDirection('j');
    }

    async function reassertStop(version) {
      if (version !== stopVersion)
        await post('/api/action',JSON.stringify({action:'stop'})).catch(() => {});
    }

    async function homeAction(action, fields, success, failure) {
      const stopping = action === 'stop';
      if (!stopping && (homeBusy || busy || releasing)) return;
      if (stopping) ++stopVersion;
      else { homeBusy = true; render(); }
      const version = stopVersion;
      try {
        const result = await post('/api/action',JSON.stringify({action,...fields}));
        if (version === stopVersion) {
          if (stopping && session) park('Đã dừng xe và kết thúc phiên HUD.');
          if (action === 'reset_odometry') clearTrajectory();
          $('status').textContent = result.already_listening
            ? 'Ruby đang lắng nghe. Hãy nói vào mic của Ruby.' : success;
          if (stopping) {
            operationActive = false;
            operationKind = '';
            render();
          } else if (action === 'forward' || action === 'backward') {
            if (plannedPath?.pending) {
              plannedPath.pending = false;
              plannedPath.locked = true;
              $('plannedPathState').textContent = 'Đường yêu cầu · ' +
                (action === 'backward' ? 'lùi ' : 'tiến ') + fields.distance_cm + ' cm.';
            }
            distanceDriveOwner = 'web';
            showOperation('Đã nhận lệnh ' + (action === 'forward' ? 'tiến ' : 'lùi ') +
              fields.distance_cm + ' cm','distance');
          } else if (action === 'turn_left' || action === 'turn_right') {
            turnOwner = 'web';
            showOperation('Đã nhận lệnh xoay ' + (action === 'turn_left' ? 'trái ' : 'phải ') +
              fields.angle_degrees + '°','turn');
          } else if (action === 'navigate_to') {
            navigationOwner = 'web';
            showOperation('Đang đi tới (' + fields.x_cm + ', ' + fields.y_cm + ') cm','navigate');
          }
          notifyAction($('status').textContent,true);
        }
        await refreshTelemetry();
      } catch (_) {
        if ((action === 'forward' || action === 'backward') && plannedPath?.pending) {
          plannedPath.pending = false;
          previewPlannedDistance();
        }
        if (version === stopVersion) {
          $('status').textContent = failure;
          notifyAction(failure,false);
        }
      } finally {
        if ((action === 'forward' || action === 'backward') && plannedPath?.pending) {
          plannedPath.pending = false;
          previewPlannedDistance();
        }
        if (!stopping) {
          await reassertStop(version);
          homeBusy = false;
          render();
        }
      }
    }

    function moveDistance(action) {
      if (homeBusy || busy || releasing) return;
      if (operationActive || session) {
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
      plannedDirection = action;
      plannedPreviewRequested = true;
      setPlannedDistance(distance,action,true);
      void homeAction(action,{distance_cm:distance},
        'Đã nhận lệnh đi ' + distance + ' cm.',
        'Chưa thể di chuyển. Kiểm tra FC-03, MPU6050, pose và chờ xe đứng yên.');
    }

    function navigateTo() {
      if (homeBusy || busy || releasing) return;
      if (operationActive || session) {
        notifyAction('Dừng xe và rời phiên HUD trước khi gửi tọa độ.',false);
        return;
      }
      const inputs = ['navigateX','navigateY'].map($);
      if (inputs.some(input => input.value.trim() === '' || !input.checkValidity())) {
        notifyAction('Nhập X/Y nguyên từ -1000 đến 1000 cm.',false);
        return;
      }
      const [x,y] = inputs.map(input => Number(input.value));
      void homeAction('navigate_to',{x_cm:x,y_cm:y},
        'Đã nhận đích (' + x + ', ' + y + ') cm.',
        'Chưa thể đi tới tọa độ. Kiểm tra pose, cảm biến, đích phải cách vị trí hiện tại ít nhất 3 cm và chờ xe đứng yên.');
    }

    function turnAngle(action) {
      if (operationActive || session) {
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

    async function claimControl() {
      if (!hudMode || session || busy || document.hidden) return;
      busy = true;
      const currentEpoch = ++epoch;
      render();
      $('status').textContent = 'Đang kết nối…';
      try {
        const result = await post('/api/claim');
        if (currentEpoch !== epoch) {
          void post('/api/command',JSON.stringify({session:result.session,sequence:1,
            direction:'release',speed:0,warning_enabled:true})).catch(() => {});
          return;
        }
        session = result.session; sequence = 0; direction = 's'; pending = true;
        joystickX = 0; joystickY = 0; joystickPointer = null;
        warningEnabled = true;
        recovering = false;
        wakeUnavailable = false;
        $('status').textContent = 'Đã kết nối HUD. Kéo joystick hoặc giữ nút hướng để lái.';
        notifyAction($('status').textContent,true);
      } catch (_) {
        if (currentEpoch === epoch) {
          $('status').textContent = 'Chưa nhận được quyền điều khiển. Kiểm tra Wi-Fi hoặc trang đang điều khiển khác, rồi thử lại.';
          notifyAction($('status').textContent,false);
        }
      } finally {
        busy = false;
        render();
        if (pending && session) void pump();
      }
    }
    $('hudConnect').addEventListener('click',claimControl);
    $('manualTab').addEventListener('click',() => showPanel('manual'));
    $('driveTab').addEventListener('click',() => showPanel('drive'));
    $('hudTab').addEventListener('click',enterHud);
    $('hudSound').addEventListener('click',() => {
      if (!session || releasing || recovering) return;
      warningEnabled = !warningEnabled;
      pending = true;
      render();
      void pump();
    });
    const stopRobot = () => {
      void homeAction('stop',{},'Đã dừng xe.',
        'Không gửi được lệnh dừng. Hãy kiểm tra xe ngay.');
    };
    $('emergencyStop').addEventListener('click',stopRobot);
    $('driveStop').addEventListener('click',stopRobot);
    $('navigateStop').addEventListener('click',stopRobot);
    $('navigateStart').addEventListener('click',navigateTo);
    $('operationStop').addEventListener('click',stopRobot);
    $('hudRotateStop').addEventListener('click',stopRobot);
    $('actionToastClose').addEventListener('click',() => {
      clearTimeout(toastTimer);
      dismissNotice();
    });
    $('wakeRobot').addEventListener('click',() => {
      void homeAction('wake',{},'Đã gửi yêu cầu đánh thức Ruby.',
        'Ruby chưa sẵn sàng để đánh thức. Hãy thử lại sau.');
    });
    $('moveForward').addEventListener('click',() => moveDistance('forward'));
    $('moveBackward').addEventListener('click',() => moveDistance('backward'));
    $('moveDistance').addEventListener('input',() => {
      if (plannedPath?.pending || operationKind === 'distance') return;
      plannedPreviewRequested = true;
      plannedPath = null;
      previewPlannedDistance();
    });
    $('moveLeft').addEventListener('click',() => turnAngle('turn_left'));
    $('moveRight').addEventListener('click',() => turnAngle('turn_right'));
    if ($('resetPose')) $('resetPose').addEventListener('click',() => {
      if (operationActive || session) return;
      void homeAction('reset_odometry',{},'Đã đặt gốc vị trí tương đối.',
        'Dừng xe, rời phiên HUD và chờ cảm biến ổn định trước khi đặt gốc.');
    });
    $('clearTrajectory').addEventListener('click',clearTrajectory);
    $('downloadTrajectory').addEventListener('click',downloadTrajectory);
    $('refreshHome').addEventListener('click',async () => {
      if (telemetryBusy) return;
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
    speedValues.forEach(speed =>
      $('speed' + speed).addEventListener('click',() => setSpeed(speed)));
    driveButtons.forEach(button => {
      button.addEventListener('contextmenu', event => event.preventDefault());
      button.addEventListener('pointerdown', event => {
        event.preventDefault();
        if (!hudMode || !session || releasing || recovering || heldPointer !== null ||
            joystickPointer !== null || event.button !== 0) return;
        heldPointer = event.pointerId;
        button.setPointerCapture(event.pointerId);
        requestDirection(button.dataset.dir);
      });
      for (const type of ['pointerup','pointercancel','lostpointercapture'])
        button.addEventListener(type, event => { if (event.pointerId === heldPointer) stop(); });
    });
    const joystick = $('hudJoystick');
    if (joystick) {
      joystick.addEventListener('contextmenu', event => event.preventDefault());
      joystick.addEventListener('pointerdown', event => {
        event.preventDefault();
        if (!hudMode || !session || releasing || recovering || heldPointer !== null ||
            joystickPointer !== null || event.button !== 0) return;
        joystickPointer = event.pointerId;
        joystick.setPointerCapture(event.pointerId);
        joystick.classList.add('active');
        updateJoystick(event);
      });
      joystick.addEventListener('pointermove', event => {
        if (event.pointerId === joystickPointer) updateJoystick(event);
      });
      for (const type of ['pointerup','pointercancel','lostpointercapture'])
        joystick.addEventListener(type, event => {
          if (event.pointerId === joystickPointer) stop();
        });
    }
    window.addEventListener('keydown', event => {
      if (event.code === 'Space' && hudMode && session) { event.preventDefault(); stop(); }
    });
    const pageLost = () => {
      if ((operationKind === 'distance' && distanceDriveOwner === 'web') ||
          (operationKind === 'turn' && turnOwner === 'web') ||
          (operationKind === 'navigate' && navigationOwner === 'web'))
        fetch('/api/action',{method:'POST',headers:{'X-Robot-Control':'1',
          'Content-Type':'application/json'},body:JSON.stringify({action:'stop'}),
          keepalive:true}).catch(() => {});
      park('Đã dừng xe và trả quyền điều khiển về giọng nói.');
    };
    window.addEventListener('blur',() => {
      // Browser chrome/fullscreen changes can blur a still-visible cockpit.
      if (document.hidden) pageLost();
      else stop();
    });
    window.addEventListener('focus',() => { wakeUnavailable = false; void syncWakeLock(); });
    window.addEventListener('resize',drawTrajectory);
    window.addEventListener('pagehide', pageLost);
    document.addEventListener('visibilitychange', () => { if (document.hidden) pageLost(); });
    void refreshTelemetry();
    setInterval(refreshTelemetry,2000);
    setInterval(() => { if ((operationKind === 'distance' || operationKind === 'turn') && !document.hidden) void refreshTelemetry(); },200);
    setInterval(() => { if (activePanel === 'drive' && operationKind !== 'distance' && operationKind !== 'turn' && !document.hidden) void refreshTelemetry(); },250);
    setInterval(() => { if (hudMode && !document.hidden) void refreshTelemetry(); },250);
    setInterval(() => { if (hudMode && session && !releasing) { pending = true; void pump(); } },150);
    $('theme').addEventListener('click', () => document.body.classList.toggle('ultra'));
    render();
  
