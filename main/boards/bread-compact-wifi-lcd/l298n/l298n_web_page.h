#pragma once

// Three-mode adaptation of the supplied Kylio Robot Car dashboard.
// Self-contained: no CDN, filesystem, Arduino runtime or Internet connection required.
inline constexpr char kL298nWebPage[] = R"html(
<!doctype html>
<html lang="vi">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <meta name="theme-color" content="#eef4ff">
  <title>XiaoZhi · Điều khiển xe</title>
  <style>
    :root{color-scheme:light;--bg:#eef4ff;--surface:#fff;--text:#172033;--muted:#68758d;--border:#dce5f2;--primary:#356df3;--success:#16834b;--danger:#e84253}
    body.dark{color-scheme:dark;--bg:#0c1220;--surface:#151f34;--text:#edf3ff;--muted:#a7b6cc;--border:#334461;--primary:#507df0;--success:#35c77b;--danger:#dc4051}
    *{box-sizing:border-box;-webkit-tap-highlight-color:transparent}
    body{margin:0;min-height:100vh;background:radial-gradient(circle at 15% 5%,#356df322,transparent 40%),var(--bg);color:var(--text);font-family:system-ui,sans-serif}
    main{max-width:900px;margin:auto;padding:24px 16px 36px}
    header{display:flex;align-items:center;justify-content:space-between;gap:12px;margin-bottom:24px}
    h1{font-size:25px;letter-spacing:-.5px;margin:0 0 4px}h2{font-size:22px;margin:0 0 8px}
    p{line-height:1.6}.subtitle,.hint{color:var(--muted);font-size:13px;margin:0}
    button{font:inherit;cursor:pointer;border:1px solid var(--border);border-radius:14px;padding:12px;background:var(--surface);color:var(--text)}
    button:focus-visible,input:focus-visible{outline:3px solid var(--primary);outline-offset:4px}
    button:disabled{opacity:.4;cursor:not-allowed}.theme{font-size:20px}
    .telemetry{display:grid;grid-template-columns:auto 1fr;align-items:center;gap:8px 22px;margin-bottom:20px;padding:20px 24px;border:1px solid var(--border);border-radius:22px;background:var(--surface);box-shadow:0 18px 50px #25438220}
    .distance-label{color:var(--muted);font-size:14px;font-weight:700}.distance{grid-row:span 3;min-width:145px;color:var(--primary);font-size:42px;font-variant-numeric:tabular-nums;letter-spacing:-1px}
    .sensor-state,.navigation-state{font-size:13px}.sensor-state{font-weight:750}.sensor-state.ok{color:var(--success)}.sensor-state.error{color:var(--danger)}
    .navigation-state{color:var(--muted);line-height:1.5}
    .dashboard{display:grid;grid-template-columns:180px 1fr;gap:18px}.menu{display:flex;flex-direction:column;gap:8px;align-self:start;padding:10px;border:1px solid var(--border);border-radius:22px;background:var(--surface);box-shadow:0 18px 50px #25438220}
    .menu button{border:0;text-align:left;font-weight:700;color:var(--muted)}.menu button.active{background:var(--primary);color:white}.panel-shell{padding:0;border:0;background:transparent}.panel{display:none;min-height:470px;padding:24px 20px;border:1px solid var(--border);border-radius:26px;background:var(--surface);box-shadow:0 18px 50px #25438220}.panel.active{display:block}
    .panel-head{display:flex;align-items:flex-start;justify-content:space-between;gap:12px}.mode-status{padding:7px 11px;border-radius:999px;background:#e8425318;color:var(--danger);font-size:11px;font-weight:800}.mode-status.on{background:#16834b20;color:var(--success)}
    .mode-card{display:grid;place-items:center;min-height:290px;margin-top:24px;padding:26px;border:1px solid var(--border);border-radius:22px;background:var(--bg);text-align:center}.mode-icon{font-size:48px}.mode-card strong{font-size:19px}.mode-actions{display:grid;grid-template-columns:1fr 1fr;gap:12px;width:min(100%,410px);margin-top:22px}.mode-on{background:var(--success);border-color:var(--success);color:white;font-weight:750}.mode-off{background:var(--danger);border-color:var(--danger);color:white;font-weight:750}.notice{min-height:22px;margin:12px 0;color:var(--muted);font-size:13px;text-align:center}
    #status{min-height:48px;padding:12px;border-radius:12px;background:var(--bg);font-size:14px;line-height:1.5;margin:18px 0}
    .actions{display:flex;gap:10px}.actions button{flex:1;font-size:14px;font-weight:650}.primary{background:var(--primary);border-color:var(--primary);color:white}
    .drive-pad{display:grid;grid-template-columns:repeat(3,minmax(65px,86px));grid-template-rows:repeat(3,80px);justify-content:center;gap:12px;margin:26px auto}
    .drive{background:var(--primary);border:0;color:white;font-size:27px;font-weight:800;border-radius:19px;box-shadow:0 8px 18px #356df32b;touch-action:none;user-select:none;-webkit-user-select:none}
    .drive small{display:block;font-size:12px;font-weight:600}.drive.held{transform:scale(.94);filter:brightness(.85)}
    [data-dir=f]{grid-column:2;grid-row:1}[data-dir=l]{grid-column:1;grid-row:2}[data-dir=r]{grid-column:3;grid-row:2}[data-dir=b]{grid-column:2;grid-row:3}
    #stop{grid-column:2;grid-row:2;background:var(--danger)}
    label{display:flex;justify-content:space-between;font-size:14px;font-weight:650}input{width:100%;margin:18px 0;accent-color:var(--primary)}
    .hint{text-align:center;margin-top:14px}footer{color:var(--muted);font-size:12px;line-height:1.7;text-align:center;padding:18px 8px}
    @media(max-width:680px){main{padding-bottom:100px}.dashboard{display:block}.menu{position:fixed;z-index:10;right:12px;bottom:12px;left:12px;display:grid;grid-template-columns:repeat(3,1fr)}.menu button{text-align:center}.panel{min-height:500px}.telemetry{grid-template-columns:1fr;text-align:center}.distance{grid-row:auto}.distance-label{grid-row:1}}
    @media(max-width:360px){.panel{padding:20px 12px}.drive-pad{gap:8px}.actions,.mode-actions{grid-template-columns:1fr;flex-direction:column}}
  </style>
</head>
<body>
  <main>
    <header><div><h1>🚗 XiaoZhi Robot</h1><p class="subtitle">Điều khiển xe qua Wi-Fi</p></div><button id="theme" class="theme" aria-label="Đổi giao diện sáng tối">◐</button></header>
    <div class="telemetry" aria-live="polite">
      <strong id="distance" class="distance">-- cm</strong>
      <span class="distance-label">📡 Khoảng cách HC-SR04 · cập nhật mỗi 2 giây</span>
      <span id="sensorState" class="sensor-state error">Đang chờ số đo đầu tiên…</span>
      <span id="navigationState" class="navigation-state">Đang đọc chế độ tự hành…</span>
    </div>
    <div id="modeNotice" class="notice" role="status" aria-live="polite"></div>
    <div class="dashboard">
      <nav class="menu" aria-label="Chế độ điều khiển">
        <button id="manualTab" class="active">🎮 Thủ công</button>
        <button id="avoidTab">🛡️ Né vật cản</button>
        <button id="followTab">👣 Follow</button>
      </nav>
      <section class="panel-shell">
        <article id="manualPanel" class="panel active">
          <div class="panel-head"><div><h2>Cầm lái thủ công</h2><p class="subtitle">Giữ nút để chạy, thả tay để dừng.</p></div><span class="mode-status on">READY</span></div>
          <div id="status" role="status" aria-live="polite">Bấm “Bắt đầu” để nhận quyền điều khiển.</div>
          <div class="actions"><button id="claim" class="primary">Bắt đầu</button><button id="release" disabled>Trả về giọng nói</button></div>
          <div class="drive-pad" aria-label="Bốn hướng điều khiển">
            <button class="drive" data-dir="f" disabled aria-label="Tiến">▲<small>Tiến</small></button>
            <button class="drive" data-dir="l" disabled aria-label="Xoay trái">◀<small>Trái</small></button>
            <button class="drive" id="stop" disabled aria-label="Dừng xe">■<small>Dừng</small></button>
            <button class="drive" data-dir="r" disabled aria-label="Xoay phải">▶<small>Phải</small></button>
            <button class="drive" data-dir="b" disabled aria-label="Lùi">▼<small>Lùi</small></button>
          </div>
          <label for="speed">Tốc độ PWM <output id="speedValue">75%</output></label>
          <input id="speed" type="range" min="0" max="100" value="75" step="5">
          <p class="hint">Bàn phím: giữ phím mũi tên hoặc W / A / S / D. Phím cách để dừng.</p>
        </article>
        <article id="avoidPanel" class="panel">
          <div class="panel-head"><div><h2>Né vật cản</h2><p class="subtitle">Xe tự tiến và xoay khi gặp vật cản.</p></div><span id="avoidStatus" class="mode-status">OFF</span></div>
          <div class="mode-card"><div class="mode-icon">🛡️</div><strong>Tự hành né vật cản</strong><p id="avoidDistance" class="subtitle">Ngưỡng 20 cm</p><div class="mode-actions"><button id="avoidOn" class="mode-on">BẬT</button><button id="avoidOff" class="mode-off">TẮT</button></div></div>
        </article>
        <article id="followPanel" class="panel">
          <div class="panel-head"><div><h2>Follow mode</h2><p class="subtitle">Xe bám mục tiêu ở khoảng cách đã đặt.</p></div><span id="followStatus" class="mode-status">OFF</span></div>
          <div class="mode-card"><div class="mode-icon">👣</div><strong>Bám khoảng cách mục tiêu</strong><p id="followDistance" class="subtitle">Mục tiêu 30 cm</p><div class="mode-actions"><button id="followOn" class="mode-on">BẬT</button><button id="followOff" class="mode-off">TẮT</button></div></div>
        </article>
      </section>
    </div>
    <footer>Trong chế độ thủ công, motor chỉ nhận lệnh từ trang này; lệnh dừng bằng giọng nói vẫn có hiệu lực.<br>Mất kết nối sẽ dừng xe. Bấm “Bắt đầu”, rồi “Trả về giọng nói” để thoát chế độ thủ công.</footer>
  </main>
  <script>
    'use strict';
    const $ = id => document.getElementById(id);
    const driveButtons = [...document.querySelectorAll('[data-dir]')];
    const names = {s:'Đã dừng',f:'Tiến',b:'Lùi',l:'Xoay trái',r:'Xoay phải'};
    const keys = {ArrowUp:'f',ArrowDown:'b',ArrowLeft:'l',ArrowRight:'r',w:'f',s:'b',a:'l',d:'r'};
    let session = 0, sequence = 0, epoch = 0, busy = false, pending = false;
    let direction = 's', releasing = false, heldPointer = null, heldKey = null;
    let telemetryBusy = false, autonomyBusy = false, currentMode = 'manual', autonomyVersion = 0;

    const modeNames = {manual:'Thủ công',follow:'Follow',avoid:'Né vật cản'};

    function showPanel(name) {
      for (const mode of ['manual','avoid','follow']) {
        $(mode + 'Panel').classList.toggle('active',mode === name);
        $(mode + 'Tab').classList.toggle('active',mode === name);
      }
    }

    function applyMode(mode) {
      currentMode = mode;
      const follow = mode === 'follow', avoid = mode === 'avoid';
      $('followStatus').textContent = follow ? 'ON' : 'OFF';
      $('followStatus').classList.toggle('on',follow);
      $('avoidStatus').textContent = avoid ? 'ON' : 'OFF';
      $('avoidStatus').classList.toggle('on',avoid);
    }

    async function refreshTelemetry() {
      if (telemetryBusy) return;
      telemetryBusy = true;
      const modeVersion = autonomyVersion;
      try {
        const response = await fetch('/api/status',{cache:'no-store'});
        if (!response.ok) throw new Error(String(response.status));
        const data = await response.json();
        const distanceOk = data.distance_valid && Number.isFinite(data.distance_cm) &&
          data.distance_cm >= 0;
        $('distance').textContent = distanceOk ? data.distance_cm + ' cm' : '-- cm';
        $('sensorState').textContent = distanceOk ? 'SR04 hoạt động' : 'Lỗi SR04: không có Echo';
        $('sensorState').classList.toggle('ok',distanceOk);
        $('sensorState').classList.toggle('error',!distanceOk);
        const telemetryMode = modeVersion === autonomyVersion ? data.autonomy_mode : currentMode;
        const mode = modeNames[telemetryMode] || 'Không xác định';
        $('navigationState').textContent = mode + ' · Follow ' + data.follow_distance_cm +
          ' cm · Né ' + data.obstacle_distance_cm + ' cm';
        $('followDistance').textContent = 'Mục tiêu ' + data.follow_distance_cm + ' cm';
        $('avoidDistance').textContent = 'Ngưỡng ' + data.obstacle_distance_cm + ' cm';
        if (modeVersion === autonomyVersion) applyMode(data.autonomy_mode);
      } catch (_) {
        $('distance').textContent = '-- cm';
        $('sensorState').textContent = 'Lỗi: mất kết nối trạng thái';
        $('sensorState').classList.toggle('ok',false);
        $('sensorState').classList.toggle('error',true);
        $('navigationState').textContent = 'Không đọc được thông số xe';
      } finally { telemetryBusy = false; }
    }

    function render() {
      $('claim').disabled = Boolean(session) || busy || autonomyBusy;
      $('release').disabled = !session || releasing;
      $('stop').disabled = !session || releasing;
      for (const id of ['followOn','followOff','avoidOn','avoidOff'])
        $(id).disabled = autonomyBusy || busy || releasing;
      driveButtons.forEach(button => {
        button.disabled = !session || releasing;
        button.classList.toggle('held', Boolean(session) && button.dataset.dir === direction);
      });
    }

    function commandBody(token, dir) {
      return JSON.stringify({session:token,sequence:++sequence,direction:dir,speed:Number($('speed').value)});
    }

    async function post(path, body) {
      const controller = new AbortController();
      const timeout = setTimeout(() => controller.abort(), 350);
      try {
        const response = await fetch(path, {
          method:'POST',headers:{'X-Robot-Control':'1','Content-Type':'application/json'},
          body,signal:controller.signal,cache:'no-store'
        });
        if (!response.ok) throw new Error(String(response.status));
        return await response.json();
      } finally { clearTimeout(timeout); }
    }

    // Best-effort stop and release on page loss. The firmware's deadline remains authoritative.
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
      heldPointer = null; heldKey = null;
      $('status').textContent = message;
      render();
    }

    async function pump() {
      if (!session || busy) return;
      busy = true; pending = false;
      const currentEpoch = epoch, sentDirection = direction;
      const sentSpeed = Number($('speed').value);
      render();
      try {
        await post('/api/command',commandBody(session,sentDirection));
        if (currentEpoch !== epoch) return;
        if (sentDirection === 'release') {
          session = 0; direction = 's'; releasing = false; pending = false;
          $('status').textContent = 'Đã trả quyền điều khiển motor về giọng nói.';
        } else if (sentDirection === direction) {
          $('status').textContent = (sentSpeed === 0 ? names.s : names[sentDirection]) + ' · Điều khiển thủ công';
        }
      } catch (_) {
        if (currentEpoch === epoch)
          park('Mất kết nối hoặc phiên đã hết hạn. Xe tự dừng; bấm “Bắt đầu” để kết nối lại.');
      } finally {
        busy = false;
        render();
        if (pending && session) void pump();
      }
    }

    function requestDirection(next) {
      if (!session || releasing) return;
      direction = next; pending = true;
      render();
      void pump();
    }

    function stop() {
      heldPointer = null; heldKey = null;
      requestDirection('s');
    }

    async function setAutonomy(mode, enabled) {
      if (autonomyBusy || busy || releasing) return;
      autonomyBusy = true;
      render();
      $('modeNotice').textContent = 'Đang chuyển chế độ…';
      try {
        if (session) {
          heldPointer = null; heldKey = null;
          releasing = true; direction = 'release'; pending = true;
          await pump();
          if (session) throw new Error('release failed');
        }
        await post('/api/autonomy',JSON.stringify({mode,enabled}));
        ++autonomyVersion;
        applyMode(enabled ? mode : currentMode === mode ? 'manual' : currentMode);
        $('modeNotice').textContent = enabled
          ? 'Đã bật ' + modeNames[mode] + '.' : 'Đã tắt ' + modeNames[mode] + '.';
        await refreshTelemetry();
      } catch (_) {
        $('modeNotice').textContent =
          'Không thể đổi chế độ. Kiểm tra tốc độ PWM, motor và kết nối rồi thử lại.';
      } finally {
        autonomyBusy = false;
        render();
      }
    }

    $('claim').addEventListener('click', async () => {
      if (session || busy) return;
      busy = true;
      const currentEpoch = ++epoch;
      render();
      $('status').textContent = 'Đang kết nối…';
      try {
        const result = await post('/api/claim');
        if (currentEpoch !== epoch) return;
        session = result.session; sequence = 0; direction = 's'; pending = true;
        ++autonomyVersion;
        applyMode('manual');
        $('status').textContent = 'Đã kết nối. Giữ một nút hướng để chạy.';
      } catch (_) {
        if (currentEpoch === epoch)
          $('status').textContent = 'Chưa nhận được quyền điều khiển. Kiểm tra Wi-Fi hoặc trang đang điều khiển khác, rồi thử lại.';
      } finally {
        busy = false;
        render();
        if (pending && session) void pump();
      }
    });

    $('release').addEventListener('click', () => {
      if (!session || releasing) return;
      heldPointer = null; heldKey = null;
      releasing = true; direction = 'release'; pending = true;
      render();
      void pump();
    });
    $('manualTab').addEventListener('click',() => showPanel('manual'));
    $('avoidTab').addEventListener('click',() => showPanel('avoid'));
    $('followTab').addEventListener('click',() => showPanel('follow'));
    $('avoidOn').addEventListener('click',() => { void setAutonomy('avoid',true); });
    $('avoidOff').addEventListener('click',() => { void setAutonomy('avoid',false); });
    $('followOn').addEventListener('click',() => { void setAutonomy('follow',true); });
    $('followOff').addEventListener('click',() => { void setAutonomy('follow',false); });
    $('stop').addEventListener('pointerdown', event => { event.preventDefault(); stop(); });
    $('stop').addEventListener('click', stop);
    $('speed').addEventListener('input', () => {
      $('speedValue').textContent = $('speed').value + '%';
      // Setting zero releases the held direction; raising it never restarts motion.
      if (Number($('speed').value) === 0) stop();
      else if (session && !releasing) { pending = true; void pump(); }
    });
    driveButtons.forEach(button => {
      button.addEventListener('contextmenu', event => event.preventDefault());
      button.addEventListener('pointerdown', event => {
        event.preventDefault();
        if (!session || releasing || heldPointer !== null || heldKey !== null || event.button !== 0) return;
        heldPointer = event.pointerId;
        button.setPointerCapture(event.pointerId);
        requestDirection(button.dataset.dir);
      });
      for (const type of ['pointerup','pointercancel','lostpointercapture'])
        button.addEventListener(type, event => { if (event.pointerId === heldPointer) stop(); });
    });
    window.addEventListener('keydown', event => {
      if (event.target instanceof HTMLInputElement || event.ctrlKey || event.altKey || event.metaKey) return;
      if (event.code === 'Space' && session) { event.preventDefault(); stop(); return; }
      const key = event.key.length === 1 ? event.key.toLowerCase() : event.key;
      if (!keys[key] || !session || releasing) return;
      event.preventDefault();
      if (event.repeat || heldKey !== null || heldPointer !== null) return;
      heldKey = key;
      requestDirection(keys[key]);
    });
    window.addEventListener('keyup', event => {
      const key = event.key.length === 1 ? event.key.toLowerCase() : event.key;
      if (key === heldKey) stop();
    });
    const pageLost = () => park('Đã dừng xe và trả quyền điều khiển về giọng nói.');
    window.addEventListener('blur', pageLost);
    window.addEventListener('pagehide', pageLost);
    document.addEventListener('visibilitychange', () => { if (document.hidden) pageLost(); });
    void refreshTelemetry();
    setInterval(refreshTelemetry,2000);
    setInterval(() => { if (session && !releasing) { pending = true; void pump(); } },150);
    $('theme').addEventListener('click', () => document.body.classList.toggle('dark'));
    render();
  </script>
</body>
</html>
)html";
