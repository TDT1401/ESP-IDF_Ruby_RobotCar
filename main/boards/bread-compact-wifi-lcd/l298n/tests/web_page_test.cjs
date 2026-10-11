// Browser control regression tests; no firmware compilation or external packages.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const {test} = require('node:test');
const source = fs.readFileSync(path.join(__dirname, '../l298n_web_page.h'), 'utf8');
const script = source.match(/<script>([\s\S]*?)<\/script>/)[1];

class Element {
  constructor() {
    this.listeners = new Map();
    this.dataset = {};
    this.value = '75';
    this.disabled = false;
    this.attributes = new Map();
    this.classes = new Set();
    this.classList = {
      toggle: (name, enabled) => enabled ? this.classes.add(name) : this.classes.delete(name),
      contains: name => this.classes.has(name)
    };
  }
  addEventListener(type, fn) {
    if (!this.listeners.has(type)) this.listeners.set(type, []);
    this.listeners.get(type).push(fn);
  }
  emit(type, values = {}) {
    const event = {preventDefault() {}, target: this, button: 0, pointerId: 1, ...values};
    return Promise.all((this.listeners.get(type) || []).map(fn => fn(event)));
  }
  setPointerCapture() {}
  setAttribute(name, value) { this.attributes.set(name,value); }
}
class Input extends Element {}

function setup({fullscreen = 'native'} = {}) {
  const elements = Object.fromEntries([
    'claim','release','stop','hudStop','status','speedValue','hudSpeedValue','theme','distance',
    'manualDistance','sensorState',
    'modeNotice','manualTab','hudTab','avoidTab','followTab','manualPanel','avoidPanel','followPanel',
    'avoidStatus','followStatus','avoidDistance','followDistance','avoidOn','avoidOff','followOn',
    'followOff','expandDrive','hudExit','hudConnect','hudNotice','hudRotateConnect',
    'hudRotateExit','hudRotateStatus','hudDistance','hudWarning','hudSound',
    'speed50','speed75','speed100'
  ]
    .map(id => [id, new Element()]));
  elements.speed = new Input();
  const directions = Object.fromEntries(['f','b','l','r'].map(dir => {
    const button = new Element(); button.dataset.dir = dir; return [dir,button];
  }));
  const hudDirections = Object.fromEntries(['f','b','l','r'].map(dir => {
    const button = new Element(); button.dataset.dir = dir; return [dir,button];
  }));
  const document = new Element(), window = new Element();
  document.getElementById = id => elements[id];
  document.querySelectorAll = () => [...Object.values(directions),...Object.values(hudDirections)];
  document.body = new Element();
  document.fullscreenElement = null;
  if (fullscreen === 'native') {
    elements.manualPanel.requestFullscreen = async function() {
      document.fullscreenElement = this;
      await document.emit('fullscreenchange');
    };
  } else if (fullscreen === 'rejected') {
    elements.manualPanel.requestFullscreen = async () => { throw new Error('Not supported'); };
  } else if (fullscreen === 'ignored') {
    elements.manualPanel.requestFullscreen = async () => {};
  }
  document.exitFullscreen = async () => {
    document.fullscreenElement = null;
    await document.emit('fullscreenchange');
  };
  const requests = [], timers = new Map(), intervals = new Map();
  const orientationLog = [];
  const wakeLocks = [];
  const navigator = {wakeLock:{async request() {
    const lock = new Element();
    lock.released = false;
    lock.release = async () => { lock.released = true; await lock.emit('release'); };
    wakeLocks.push(lock);
    return lock;
  }}};
  const screen = {orientation:{
    async lock(mode) { orientationLog.push(['lock',mode]); },
    unlock() { orientationLog.push(['unlock']); }
  }};
  window.screen = screen;
  let timerId = 0, heartbeat, nextReply;
  let currentMode = 'manual';
  let statusDistance = 34, statusValid = true;
  let now = 0, serverDeadline = 0;
  const context = vm.createContext({
    document,window,screen,navigator,HTMLInputElement:Input,AbortController,
    setInterval(fn, delay) { intervals.set(delay,fn); heartbeat = fn; },
    setTimeout(fn) { timers.set(++timerId,fn); return timerId; },
    clearTimeout(id) { timers.delete(id); },
    fetch(url, options = {}) {
      const requestData = options.body && JSON.parse(options.body);
      requests.push({url, ...options, data: requestData});
      const reply = nextReply; nextReply = undefined;
      if (reply) return reply(options);
      if (url.endsWith('claim')) { currentMode = 'manual'; serverDeadline = now + 600; }
      if (url.endsWith('command')) {
        if (now >= serverDeadline) return Promise.resolve({ok:false,status:409});
        serverDeadline = requestData.direction === 'release' ? 0 : now + 600;
      }
      if (url.endsWith('autonomy'))
        currentMode = requestData.enabled ? requestData.mode : currentMode === requestData.mode ?
          'manual' : currentMode;
      const responseData = url.endsWith('claim') ? {session:1234} : url.endsWith('status') ? {
        distance_valid:statusValid,distance_cm:statusDistance,autonomy_mode:currentMode,follow_distance_cm:30,
        obstacle_distance_cm:20,moving:false
      } : {ok:true};
      return Promise.resolve({ok:true,json:async () => responseData});
    }
  });
  vm.runInContext(script,context);
  return {elements,directions,hudDirections,document,window,requests,timers,intervals,orientationLog,
    wakeLocks,navigator,advance:ms => { now += ms; },
    setDistance:(value, valid = true) => { statusDistance = value; statusValid = valid; },
    heartbeat:() => heartbeat(), replyNext:fn => { nextReply = fn; }};
}
const settle = () => new Promise(resolve => setImmediate(resolve));
async function claim(ui) { await ui.elements.claim.emit('click'); await settle(); }
const lastCommand = ui => ui.requests.filter(x => x.data).at(-1).data;

test('opening does not claim or move; four directions stop on release', async () => {
  const ui = setup();
  assert.equal(ui.requests.filter(request => request.data).length,0);
  assert.equal(ui.directions.f.disabled,true);
  await claim(ui);
  assert.equal(lastCommand(ui).direction,'s');
  for (const dir of ['f','b','l','r']) {
    await ui.directions[dir].emit('pointerdown'); await settle();
    assert.equal(lastCommand(ui).direction,dir);
    ui.heartbeat(); await settle();
    assert.equal(lastCommand(ui).direction,dir);
    await ui.directions[dir].emit('pointerup'); await settle();
    assert.equal(lastCommand(ui).direction,'s');
  }
});

test('status telemetry renders the measured SR04 distance in both compact displays', async () => {
  const ui = setup(); await settle();
  assert.equal(ui.elements.distance.textContent,'34 cm');
  assert.equal(ui.elements.sensorState.textContent,'SR04 hoạt động');
  assert.equal(ui.elements.manualDistance.textContent,'34 cm');
  assert.ok(ui.intervals.has(2000));
});

test('portrait keeps the classic D-pad and landscape HUD is orientation-gated', () => {
  assert.match(source,/class="portrait-manual"[\s\S]*class="drive-pad"/);
  assert.match(source,/\.landscape-hud\{display:none\}/);
  assert.match(source,/@media\(orientation:landscape\)[\s\S]*\.landscape-hud/);
  assert.match(source,/#manualPanel\.drive-expanded \.landscape-hud\{display:grid/);
  assert.match(source,/@media\(orientation:landscape\) and \(max-height:500px\)/);
});

test('speed presets send the selected PWM level and update their active state', async () => {
  const ui = setup(); await claim(ui);
  await ui.elements.speed50.emit('click'); await settle();
  assert.equal(lastCommand(ui).speed,50);
  assert.equal(ui.elements.speedValue.textContent,'50%');
  assert.equal(ui.elements.hudSpeedValue.textContent,'50%');
  assert.equal(ui.elements.speed50.classList.contains('active'),true);
  assert.equal(ui.elements.speed75.classList.contains('active'),false);
  await ui.elements.speed100.emit('click'); await settle();
  assert.equal(lastCommand(ui).speed,100);
});

test('expanded cockpit enters fullscreen, requests landscape, and can shrink again', async () => {
  const ui = setup();
  assert.equal(ui.elements.expandDrive.disabled,true);
  await claim(ui);
  assert.equal(ui.elements.expandDrive.disabled,false);
  await ui.elements.expandDrive.emit('click'); await settle();
  assert.equal(ui.document.fullscreenElement,ui.elements.manualPanel);
  assert.deepEqual(ui.orientationLog.at(-1),['lock','landscape']);
  assert.equal(ui.elements.expandDrive.textContent,'✕ Thu nhỏ');
  await ui.hudDirections.f.emit('pointerdown'); await settle();
  assert.equal(lastCommand(ui).direction,'f');
  await ui.hudDirections.f.emit('pointerup'); await settle();
  assert.equal(lastCommand(ui).direction,'s');
  await ui.elements.hudExit.emit('click'); await settle();
  assert.equal(ui.document.fullscreenElement,null);
  assert.deepEqual(ui.orientationLog.at(-1),['unlock']);
});

test('iPhone fallback expands and shrinks when element fullscreen is unavailable', async () => {
  for (const fullscreen of ['unsupported','rejected','ignored']) {
    const ui = setup({fullscreen}); await claim(ui);
    await ui.elements.expandDrive.emit('click'); await settle();
    assert.equal(ui.document.fullscreenElement,null);
    assert.equal(ui.elements.manualPanel.classList.contains('drive-expanded'),true);
    assert.equal(ui.document.body.classList.contains('drive-expanded'),true);
    assert.equal(ui.elements.expandDrive.textContent,'✕ Thu nhỏ');
    assert.equal(ui.wakeLocks.length,1);
    await ui.hudDirections.f.emit('pointerdown'); await settle();
    assert.equal(lastCommand(ui).direction,'f');
    await ui.hudDirections.f.emit('pointerup'); await settle();
    assert.equal(lastCommand(ui).direction,'s');
    await ui.elements.hudExit.emit('click'); await settle();
    assert.equal(ui.elements.manualPanel.classList.contains('drive-expanded'),false);
    assert.equal(ui.document.body.classList.contains('drive-expanded'),false);
    assert.equal(ui.wakeLocks[0].released,true);
  }
});

test('HUD tab claims control, shows the portrait rotation prompt, and exits safely', async () => {
  const ui = setup({fullscreen:'unsupported'});
  assert.match(source,/Xoay ngang màn hình để trải nghiệm\./);
  await ui.elements.hudTab.emit('click'); await settle();
  assert.equal(ui.elements.hudTab.classList.contains('active'),true);
  assert.equal(ui.elements.manualPanel.classList.contains('hud-mode'),true);
  assert.equal(ui.elements.manualPanel.classList.contains('drive-expanded'),true);
  assert.equal(ui.requests.filter(x => x.url.endsWith('claim')).length,1);
  assert.equal(ui.hudDirections.f.disabled,false);
  assert.equal(ui.elements.hudRotateStatus.textContent,'Đã kết nối. Xoay ngang để điều khiển xe.');
  await ui.hudDirections.f.emit('pointerdown'); await settle();
  assert.equal(lastCommand(ui).direction,'f');
  await ui.elements.hudRotateExit.emit('click'); await settle();
  assert.equal(lastCommand(ui).direction,'s');
  assert.equal(ui.elements.manualPanel.classList.contains('hud-mode'),false);
  assert.equal(ui.elements.manualPanel.classList.contains('drive-expanded'),false);
  assert.equal(ui.elements.manualTab.classList.contains('active'),true);
});

test('HUD refreshes SR04 distance and marks values below 20 cm', async () => {
  const ui = setup();
  await ui.elements.hudTab.emit('click'); await settle();
  assert.ok(ui.intervals.has(250));
  ui.setDistance(20);
  ui.intervals.get(250)(); await settle();
  assert.equal(ui.elements.manualDistance.textContent,'20 cm');
  assert.equal(ui.elements.hudDistance.classList.contains('danger'),false);
  ui.setDistance(19);
  ui.intervals.get(250)(); await settle();
  assert.equal(ui.elements.hudDistance.classList.contains('danger'),true);
  ui.setDistance(10,false);
  ui.intervals.get(250)(); await settle();
  assert.equal(ui.elements.manualDistance.textContent,'-- cm');
  assert.equal(ui.elements.hudDistance.classList.contains('danger'),false);
  await ui.elements.hudExit.emit('click'); await settle();
  ui.setDistance(10);
  ui.intervals.get(250)(); await settle();
  assert.equal(ui.elements.hudDistance.classList.contains('danger'),false);
  assert.doesNotMatch(source,/AudioContext|beepIfNeeded/);
});

test('HUD sound switch changes the ESP warning flag without changing motion', async () => {
  const ui = setup();
  assert.equal(ui.elements.hudSound.disabled,true);
  await ui.elements.hudTab.emit('click'); await settle();
  assert.equal(ui.elements.hudSound.disabled,false);
  assert.equal(ui.elements.hudSound.attributes.get('aria-pressed'),'true');
  assert.equal(lastCommand(ui).warning_enabled,true);
  await ui.elements.hudSound.emit('click'); await settle();
  assert.equal(ui.elements.hudSound.attributes.get('aria-pressed'),'false');
  assert.match(ui.elements.hudSound.textContent,/Tắt/);
  assert.equal(lastCommand(ui).direction,'s');
  assert.equal(lastCommand(ui).warning_enabled,false);
  ui.heartbeat(); await settle();
  assert.equal(lastCommand(ui).warning_enabled,false);
  await ui.elements.hudSound.emit('click'); await settle();
  assert.equal(ui.elements.hudSound.attributes.get('aria-pressed'),'true');
  assert.equal(lastCommand(ui).warning_enabled,true);
  await ui.hudDirections.f.emit('pointerdown'); await settle();
  assert.equal(lastCommand(ui).direction,'f');
  assert.equal(lastCommand(ui).warning_enabled,true);
  await ui.hudDirections.f.emit('pointerup'); await settle();
  await ui.elements.hudSound.emit('click'); await settle();
  assert.equal(lastCommand(ui).warning_enabled,false);
  await ui.elements.release.emit('click'); await settle();
  assert.equal(ui.elements.hudSound.disabled,true);
  await ui.elements.hudConnect.emit('click'); await settle();
  assert.equal(ui.elements.hudSound.attributes.get('aria-pressed'),'true');
  assert.equal(lastCommand(ui).warning_enabled,true);
});

test('autonomy controls toggle modes and release manual control first', async () => {
  const ui = setup(); await settle();
  await ui.elements.followOn.emit('click'); await settle();
  assert.deepEqual(ui.requests.filter(x => x.url.endsWith('autonomy')).at(-1).data,
    {mode:'follow',enabled:true});
  assert.equal(ui.elements.followStatus.textContent,'ON');

  await claim(ui);
  await ui.elements.avoidOn.emit('click'); await settle();
  const releaseIndex = ui.requests.findLastIndex(x => x.data?.direction === 'release');
  const autonomyIndex = ui.requests.findLastIndex(x => x.url.endsWith('autonomy'));
  assert.ok(releaseIndex >= 0 && releaseIndex < autonomyIndex);
  assert.deepEqual(ui.requests[autonomyIndex].data,{mode:'avoid',enabled:true});
  assert.equal(ui.elements.avoidStatus.textContent,'ON');
  assert.equal(ui.elements.followStatus.textContent,'OFF');
});

test('one pointer owns motion; pointer cancellation and lost capture stop', async () => {
  const ui = setup(); await claim(ui);
  await ui.directions.f.emit('pointerdown'); await settle();
  await ui.directions.r.emit('pointerdown',{pointerId:2}); await settle();
  assert.equal(lastCommand(ui).direction,'f');
  await ui.directions.f.emit('pointercancel'); await settle();
  assert.equal(lastCommand(ui).direction,'s');
  await ui.directions.l.emit('pointerdown'); await settle();
  await ui.directions.l.emit('lostpointercapture'); await settle();
  assert.equal(lastCommand(ui).direction,'s');
});

test('slow requests coalesce heartbeats; release is sent next', async () => {
  const ui = setup(); await claim(ui);
  let finish;
  ui.replyNext(() => new Promise(resolve => { finish = resolve; }));
  await ui.directions.f.emit('pointerdown');
  const count = ui.requests.length;
  for (let i=0;i<20;++i) ui.heartbeat();
  await ui.directions.f.emit('pointerup');
  assert.equal(ui.requests.length,count);
  finish({ok:true,json:async () => ({ok:true})}); await settle();
  assert.equal(lastCommand(ui).direction,'s');
  assert.equal(ui.requests.length,count+1);
});

test('backgrounding sends a newer release; late response cannot resume control', async () => {
  const ui = setup(); await claim(ui);
  let finish;
  ui.replyNext(() => new Promise(resolve => { finish = resolve; }));
  await ui.directions.f.emit('pointerdown');
  const oldSequence = lastCommand(ui).sequence;
  ui.document.hidden = true;
  await ui.document.emit('visibilitychange');
  assert.equal(lastCommand(ui).direction,'release');
  assert.ok(lastCommand(ui).sequence > oldSequence);
  assert.equal(ui.directions.f.disabled,true);
  finish({ok:true,json:async () => ({ok:true})}); await settle();
  const count = ui.requests.length;
  ui.heartbeat(); await settle();
  assert.equal(ui.requests.length,count);
  assert.equal(ui.elements.claim.disabled,false);
});

test('zero speed releases direction; increasing it cannot restart', async () => {
  const ui = setup(); await claim(ui);
  await ui.directions.f.emit('pointerdown'); await settle();
  ui.elements.speed.value = '0';
  await ui.elements.speed.emit('input'); await settle();
  assert.equal(lastCommand(ui).direction,'s');
  ui.elements.speed.value = '80';
  await ui.elements.speed.emit('input'); await settle();
  assert.equal(lastCommand(ui).direction,'s');
  assert.equal(lastCommand(ui).speed,80);
});

test('keyboard release and space stop; returning control ends heartbeats', async () => {
  const ui = setup(); await claim(ui);
  await ui.window.emit('keydown',{key:'W'}); await settle();
  assert.equal(lastCommand(ui).direction,'f');
  await ui.window.emit('keyup',{key:'W'}); await settle();
  assert.equal(lastCommand(ui).direction,'s');
  await ui.window.emit('keydown',{key:'ArrowLeft'}); await settle();
  await ui.window.emit('keydown',{key:' ',code:'Space'}); await settle();
  assert.equal(lastCommand(ui).direction,'s');
  await ui.elements.release.emit('click'); await settle();
  assert.equal(lastCommand(ui).direction,'release');
  const count = ui.requests.length;
  ui.heartbeat(); await settle();
  assert.equal(ui.requests.length,count);
  assert.equal(ui.directions.f.disabled,true);
});

test('expired or repeated failed requests disable driving and never auto-reclaim', async () => {
  for (const response of ['conflict','network','timeout']) {
    const ui = setup(); await claim(ui);
    ui.replyNext(options => {
      ui.replyNext(() => Promise.reject(new Error('still offline')));
      if (response === 'network') return Promise.reject(new Error('offline'));
      if (response === 'conflict') return Promise.resolve({ok:false,status:409});
      return new Promise((_,reject) => options.signal.addEventListener('abort',() => reject(new Error('timeout'))));
    });
    await ui.directions.f.emit('pointerdown');
    if (response === 'timeout') for (const timer of ui.timers.values()) timer();
    await settle();
    assert.equal(ui.directions.f.disabled,true);
    assert.equal(lastCommand(ui).direction,'release');
    const count = ui.requests.length;
    ui.heartbeat(); await settle();
    assert.equal(ui.requests.length,count);
  }
});

test('idle fullscreen keeps the 600 ms lease alive for 30 seconds without touches', async () => {
  const ui = setup(); await claim(ui);
  await ui.elements.expandDrive.emit('click'); await settle();
  for (let i=0;i<200;++i) {
    ui.advance(150); ui.heartbeat(); await settle();
  }
  assert.equal(ui.hudDirections.f.disabled,false);
  assert.equal(ui.requests.filter(x => x.url.endsWith('claim')).length,1);
  assert.equal(lastCommand(ui).direction,'s');
  await ui.hudDirections.f.emit('pointerdown'); await settle();
  assert.equal(lastCommand(ui).direction,'f');
});

test('visible blur stops movement but retains control; hidden page releases it', async () => {
  const ui = setup(); await claim(ui);
  await ui.hudDirections.f.emit('pointerdown'); await settle();
  await ui.window.emit('blur'); await settle();
  assert.equal(lastCommand(ui).direction,'s');
  assert.equal(ui.hudDirections.f.disabled,false);
  ui.document.hidden = true;
  await ui.document.emit('visibilitychange'); await settle();
  assert.equal(lastCommand(ui).direction,'release');
  assert.equal(ui.hudDirections.f.disabled,true);
});

test('one transport failure recovers with STOP and never resumes the held direction', async () => {
  for (const timeout of [false,true]) {
    const ui = setup(); await claim(ui);
    ui.replyNext(options => timeout
      ? new Promise((_,reject) => options.signal.addEventListener('abort',() => reject(new Error('timeout'))))
      : Promise.reject(new Error('temporary network failure')));
    await ui.hudDirections.f.emit('pointerdown');
    if (timeout) {
      ui.advance(350);
      for (const timer of ui.timers.values()) timer();
    }
    await settle();
    assert.equal(lastCommand(ui).direction,'s');
    assert.equal(ui.hudDirections.f.disabled,false);
    ui.heartbeat(); await settle();
    assert.equal(lastCommand(ui).direction,'s');
    assert.equal(ui.requests.filter(x => x.url.endsWith('claim')).length,1);
  }
});

test('expired session can be explicitly reconnected within the HUD while stationary', async () => {
  const ui = setup(); await claim(ui);
  await ui.elements.expandDrive.emit('click'); await settle();
  ui.advance(700); ui.heartbeat(); await settle();
  assert.equal(ui.hudDirections.f.disabled,true);
  assert.equal(ui.elements.hudConnect.disabled,false);
  assert.match(ui.elements.hudNotice.textContent,/Phiên điều khiển/);
  await ui.elements.hudConnect.emit('click'); await settle();
  assert.equal(ui.document.fullscreenElement,ui.elements.manualPanel);
  assert.equal(ui.hudDirections.f.disabled,false);
  assert.equal(lastCommand(ui).direction,'s');
});

test('screen wake lock follows fullscreen and session lifetime', async () => {
  const ui = setup(); await claim(ui);
  assert.equal(ui.wakeLocks.length,0);
  await ui.elements.expandDrive.emit('click'); await settle();
  assert.equal(ui.wakeLocks.length,1);
  ui.heartbeat(); await settle();
  assert.equal(ui.wakeLocks.length,1);
  await ui.elements.hudExit.emit('click'); await settle();
  assert.equal(ui.wakeLocks[0].released,true);
  await ui.elements.expandDrive.emit('click'); await settle();
  ui.document.hidden = true;
  await ui.document.emit('visibilitychange'); await settle();
  assert.equal(ui.wakeLocks[1].released,true);
});

test('HTTP without wake lock keeps controls usable and shows the screen-timeout hint', async () => {
  const ui = setup(); delete ui.navigator.wakeLock;
  await claim(ui);
  await ui.elements.expandDrive.emit('click'); await settle();
  assert.equal(ui.hudDirections.f.disabled,false);
  assert.match(ui.elements.hudNotice.textContent,/tự tắt màn hình/);
});
