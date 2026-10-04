#pragma once

// Four-direction adaptation of the supplied Kylio Robot Car manual panel.
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
    :root{color-scheme:light;--bg:#eef4ff;--surface:#fff;--text:#172033;--muted:#68758d;--border:#dce5f2;--primary:#356df3;--danger:#e84253}
    body.dark{color-scheme:dark;--bg:#0c1220;--surface:#151f34;--text:#edf3ff;--muted:#a7b6cc;--border:#334461;--primary:#507df0;--danger:#dc4051}
    *{box-sizing:border-box;-webkit-tap-highlight-color:transparent}
    body{margin:0;min-height:100vh;background:radial-gradient(circle at 15% 5%,#356df322,transparent 40%),var(--bg);color:var(--text);font-family:system-ui,sans-serif}
    main{max-width:570px;margin:auto;padding:24px 16px 36px}
    header{display:flex;align-items:center;justify-content:space-between;gap:12px;margin-bottom:24px}
    h1{font-size:25px;letter-spacing:-.5px;margin:0 0 4px}h2{font-size:22px;margin:0 0 8px}
    p{line-height:1.6}.subtitle,.hint{color:var(--muted);font-size:13px;margin:0}
    button{font:inherit;cursor:pointer;border:1px solid var(--border);border-radius:14px;padding:12px;background:var(--surface);color:var(--text)}
    button:focus-visible,input:focus-visible{outline:3px solid var(--primary);outline-offset:4px}
    button:disabled{opacity:.4;cursor:not-allowed}.theme{font-size:20px}
    section{padding:24px 20px;border:1px solid var(--border);border-radius:26px;background:var(--surface);box-shadow:0 18px 50px #25438220}
    #status{min-height:48px;padding:12px;border-radius:12px;background:var(--bg);font-size:14px;line-height:1.5;margin:18px 0}
    .actions{display:flex;gap:10px}.actions button{flex:1;font-size:14px;font-weight:650}.primary{background:var(--primary);border-color:var(--primary);color:white}
    .drive-pad{display:grid;grid-template-columns:repeat(3,minmax(65px,86px));grid-template-rows:repeat(3,80px);justify-content:center;gap:12px;margin:26px auto}
    .drive{background:var(--primary);border:0;color:white;font-size:27px;font-weight:800;border-radius:19px;box-shadow:0 8px 18px #356df32b;touch-action:none;user-select:none;-webkit-user-select:none}
    .drive small{display:block;font-size:12px;font-weight:600}.drive.held{transform:scale(.94);filter:brightness(.85)}
    [data-dir=f]{grid-column:2;grid-row:1}[data-dir=l]{grid-column:1;grid-row:2}[data-dir=r]{grid-column:3;grid-row:2}[data-dir=b]{grid-column:2;grid-row:3}
    #stop{grid-column:2;grid-row:2;background:var(--danger)}
    label{display:flex;justify-content:space-between;font-size:14px;font-weight:650}input{width:100%;margin:18px 0;accent-color:var(--primary)}
    .hint{text-align:center;margin-top:14px}footer{color:var(--muted);font-size:12px;line-height:1.7;text-align:center;padding:18px 8px}
    @media(max-width:360px){section{padding:20px 12px}.drive-pad{gap:8px}.actions{flex-direction:column}}
  </style>
</head>
<body>
  <main>
    <header><div><h1>🚗 XiaoZhi Robot</h1><p class="subtitle">Điều khiển xe qua Wi-Fi</p></div><button id="theme" class="theme" aria-label="Đổi giao diện sáng tối">◐</button></header>
    <section>
      <h2>Cầm lái thủ công</h2>
      <p class="subtitle">Giữ nút để chạy, thả tay để dừng. Trái/phải xoay xe tại chỗ.</p>
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
    </section>
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

    function render() {
      $('claim').disabled = Boolean(session) || busy;
      $('release').disabled = !session || releasing;
      $('stop').disabled = !session || releasing;
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

    // Best-effort stop on page loss. The firmware's deadline remains authoritative.
    // A newer sequence prevents an older in-flight movement from undoing this stop.
    function park(message) {
      if (session) {
        fetch('/api/command', {
          method:'POST',headers:{'X-Robot-Control':'1','Content-Type':'application/json'},
          body:commandBody(session,'s'),keepalive:true
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
    const pageLost = () => park('Đã ngắt điều khiển và yêu cầu dừng. Bấm “Bắt đầu” để kết nối lại.');
    window.addEventListener('blur', pageLost);
    window.addEventListener('pagehide', pageLost);
    document.addEventListener('visibilitychange', () => { if (document.hidden) pageLost(); });
    setInterval(() => { if (session && !releasing) { pending = true; void pump(); } },150);
    $('theme').addEventListener('click', () => document.body.classList.toggle('dark'));
    render();
  </script>
</body>
</html>
)html";
