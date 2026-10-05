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
    this.classList = {toggle() {}};
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
}
class Input extends Element {}

function setup() {
  const elements = Object.fromEntries(['claim','release','stop','status','speedValue','theme']
    .map(id => [id, new Element()]));
  elements.speed = new Input();
  const directions = Object.fromEntries(['f','b','l','r'].map(dir => {
    const button = new Element(); button.dataset.dir = dir; return [dir,button];
  }));
  const document = new Element(), window = new Element();
  document.getElementById = id => elements[id];
  document.querySelectorAll = () => Object.values(directions);
  document.body = new Element();
  const requests = [], timers = new Map();
  let timerId = 0, heartbeat, nextReply;
  const context = vm.createContext({
    document,window,HTMLInputElement:Input,AbortController,
    setInterval(fn) { heartbeat = fn; },
    setTimeout(fn) { timers.set(++timerId,fn); return timerId; },
    clearTimeout(id) { timers.delete(id); },
    fetch(url, options) {
      requests.push({url, ...options, data: options.body && JSON.parse(options.body)});
      const reply = nextReply; nextReply = undefined;
      if (reply) return reply(options);
      return Promise.resolve({ok:true,json:async () => url.endsWith('claim') ? {session:1234} : {ok:true}});
    }
  });
  vm.runInContext(script,context);
  return {elements,directions,document,window,requests,timers,
    heartbeat:() => heartbeat(), replyNext:fn => { nextReply = fn; }};
}
const settle = () => new Promise(resolve => setImmediate(resolve));
async function claim(ui) { await ui.elements.claim.emit('click'); await settle(); }
const lastCommand = ui => ui.requests.filter(x => x.data).at(-1).data;

test('opening does not claim or move; four directions stop on release', async () => {
  const ui = setup();
  assert.equal(ui.requests.length,0);
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
  await ui.window.emit('blur');
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

test('expired or failed requests disable driving and never auto-reclaim', async () => {
  for (const response of ['conflict','network','timeout']) {
    const ui = setup(); await claim(ui);
    ui.replyNext(options => {
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
