#pragma once

// Three-mode adaptation of the supplied Kylio Robot Car dashboard.
// Self-contained: no CDN, filesystem, Arduino runtime or Internet connection required.
inline constexpr char kL298nWebPage[] = R"html(
<!doctype html>
<html lang="vi">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
  <meta name="theme-color" content="#060914">
  <title>Ruby · Điều khiển xe</title>
  <style>
    :root{color-scheme:dark;--bg:#050711;--panel:#0d1324dd;--panel2:#101a30;--text:#f3f7ff;--muted:#8291ae;--line:#21304d;--cyan:#28d7ff;--blue:#4776ff;--purple:#985cff;--success:#39e58c;--danger:#ff4f70;--shadow:0 24px 70px #0009}
    body.ultra{--bg:#000;--panel:#070a12f2;--panel2:#090e18;--line:#192235}
    *{box-sizing:border-box;-webkit-tap-highlight-color:transparent}
    body{margin:0;min-height:100vh;overflow-x:hidden;background:radial-gradient(circle at 12% 4%,#1b4fff33,transparent 30%),radial-gradient(circle at 88% 22%,#8d3cff28,transparent 28%),linear-gradient(145deg,#060914,var(--bg) 48%,#070a16);color:var(--text);font-family:Inter,ui-sans-serif,system-ui,sans-serif}
    body:before{position:fixed;inset:0;z-index:-1;background-image:linear-gradient(#5f7ba20c 1px,transparent 1px),linear-gradient(90deg,#5f7ba20c 1px,transparent 1px);background-size:32px 32px;content:"";mask-image:linear-gradient(to bottom,#000,transparent 80%)}
    main{max-width:980px;margin:auto;padding:26px 18px 42px}
    header{display:flex;align-items:center;justify-content:space-between;gap:16px;margin-bottom:22px}.brand{display:flex;align-items:center;gap:13px}.brand-mark{display:grid;width:50px;height:50px;place-items:center;border:1px solid #43dfff55;border-radius:16px;background:linear-gradient(145deg,#172b52,#11162b);box-shadow:0 0 28px #28d7ff30,inset 0 1px #ffffff1c;font-size:25px}.brand-copy{display:flex;align-items:center;gap:12px}
    h1{margin:0 0 3px;font-size:clamp(22px,4vw,29px);letter-spacing:-.8px}h1 span{color:var(--cyan)}h2{margin:0 0 7px;font-size:clamp(21px,4vw,27px);letter-spacing:-.5px}.eyebrow{color:var(--cyan);font-size:10px;font-weight:800;letter-spacing:2px;text-transform:uppercase}
    p{line-height:1.6}.subtitle,.hint{margin:0;color:var(--muted);font-size:13px}.online{display:flex;align-items:center;gap:7px;padding:6px 9px;border:1px solid #39e58c35;border-radius:999px;background:#39e58c0d;color:var(--success);font-size:10px;font-weight:800;letter-spacing:.8px}.online:before{width:7px;height:7px;border-radius:50%;background:var(--success);box-shadow:0 0 11px var(--success);content:""}
    button{font:inherit;cursor:pointer;border:1px solid var(--line);border-radius:14px;padding:12px;background:#111a2d;color:var(--text);transition:transform .15s,border-color .15s,filter .15s,box-shadow .15s}button:hover:not(:disabled){border-color:#28d7ff66;filter:brightness(1.13)}button:active:not(:disabled){transform:translateY(1px) scale(.98)}button:focus-visible,input:focus-visible{outline:2px solid var(--cyan);outline-offset:3px}button:disabled{cursor:not-allowed;filter:grayscale(.65);opacity:.36}.theme{display:grid;width:46px;height:46px;place-items:center;padding:0;border-color:#31425f;background:#0d1425;box-shadow:inset 0 1px #ffffff10;color:var(--cyan);font-size:19px}
    .telemetry{display:flex;align-items:center;gap:14px;width:min(100%,480px);margin-bottom:18px;padding:12px 15px;border:1px solid #28d7ff3b;border-radius:17px;background:linear-gradient(120deg,#0e1930f2,#0b1020e8 70%,#17102be8);box-shadow:0 14px 42px #0007,inset 0 1px #ffffff13}.telemetry-icon{display:grid;width:38px;height:38px;flex:0 0 38px;place-items:center;border:1px solid #28d7ff4f;border-radius:12px;background:#10263e;color:var(--cyan);box-shadow:0 0 18px #28d7ff1d}.telemetry-copy{display:grid;gap:1px}.distance-label{color:var(--muted);font-size:9px;font-weight:850;letter-spacing:1.4px}.distance{color:var(--cyan);font-size:22px;font-weight:850;font-variant-numeric:tabular-nums;letter-spacing:-.6px;text-shadow:0 0 16px #28d7ff55}.sensor-state{margin-left:auto;font-size:11px;font-weight:800;text-align:right}.sensor-state.ok{color:var(--success);text-shadow:0 0 14px #39e58c55}.sensor-state.error{color:var(--danger)}
    .notice{min-height:22px;margin:9px 0 13px;color:#9fb0cd;font-size:12px;text-align:center}.dashboard{display:grid;grid-template-columns:195px 1fr;gap:18px}.menu{display:flex;flex-direction:column;gap:8px;align-self:start;padding:9px;border:1px solid var(--line);border-radius:21px;background:linear-gradient(160deg,#10182bdd,#090f1ddd);box-shadow:var(--shadow),inset 0 1px #ffffff0d;backdrop-filter:blur(18px)}.menu:before{padding:8px 10px 4px;color:#596985;font-size:9px;font-weight:850;letter-spacing:1.8px;content:"CONTROL MODE"}.menu button{border:1px solid transparent;text-align:left;font-size:13px;font-weight:750;color:#8f9db7}.menu button.active{border-color:#28d7ff55;background:linear-gradient(105deg,#164262aa,#25245aaa);box-shadow:0 0 22px #28d7ff19,inset 3px 0 var(--cyan);color:#fff}
    .panel-shell{padding:0;border:0;background:transparent}.panel{display:none;min-height:510px;padding:26px 24px;border:1px solid var(--line);border-radius:25px;background:linear-gradient(145deg,var(--panel),#090f1de8);box-shadow:var(--shadow),inset 0 1px #ffffff0d;backdrop-filter:blur(18px)}.panel.active{display:block;animation:panelIn .22s ease}@keyframes panelIn{from{opacity:0;transform:translateY(7px)}to{opacity:1;transform:none}}.panel-head{display:flex;align-items:flex-start;justify-content:space-between;gap:12px}.mode-status{padding:7px 11px;border:1px solid #ff4f7038;border-radius:999px;background:#ff4f7011;color:var(--danger);font-size:10px;font-weight:850;letter-spacing:1px}.mode-status.on{border-color:#39e58c45;background:#39e58c12;color:var(--success);box-shadow:0 0 18px #39e58c16}
    #status{min-height:50px;margin:19px 0;padding:13px 15px;border:1px solid var(--line);border-radius:13px;background:#060b16a8;color:#adbad0;font-size:13px;line-height:1.55}.actions{display:flex;gap:10px}.actions button{flex:1;font-size:13px;font-weight:750}.primary{border-color:#28d7ff66;background:linear-gradient(135deg,#087fa4,#315bd8);box-shadow:0 8px 24px #197acb35;color:white}
    .manual-head-actions{display:flex;align-items:center;gap:8px}.drive{position:relative;border:1px solid #28d7ff47;border-radius:18px;background:linear-gradient(145deg,#183558,#101b32);box-shadow:0 10px 20px #0005,inset 0 1px #ffffff18;color:#dff8ff;font-size:25px;font-weight:850;touch-action:none;user-select:none;-webkit-user-select:none}.drive:hover:not(:disabled){box-shadow:0 0 20px #28d7ff25,inset 0 1px #ffffff22}.drive small{display:block;margin-top:3px;color:#8eaac8;font-size:10px;font-weight:700;letter-spacing:.5px}.drive.held{transform:scale(.94);border-color:var(--cyan);background:#0a7998;box-shadow:0 0 26px #28d7ff55}.drive.held small{color:#fff}.stop-drive{border-color:#ff4f7066!important;background:linear-gradient(145deg,#7c1731,#3a1020)!important;color:#ffdce4!important}.stop-drive:hover:not(:disabled){box-shadow:0 0 22px #ff4f7038}.speed-label{display:flex;justify-content:space-between;color:#aab8cf;font-size:12px;font-weight:750;letter-spacing:.3px}.speed-label output{color:var(--cyan);font-variant-numeric:tabular-nums}input[type=range]{width:100%;height:5px;margin:18px 0;appearance:none;border-radius:999px;background:linear-gradient(90deg,var(--cyan),var(--purple));accent-color:var(--cyan)}input[type=range]::-webkit-slider-thumb{width:19px;height:19px;appearance:none;border:3px solid #dffaff;border-radius:50%;background:var(--cyan);box-shadow:0 0 15px #28d7ffaa}.hint{margin-top:14px;text-align:center}.status-grid{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:10px;margin:18px 0}.status-card,.config-card{padding:15px;border:1px solid var(--line);border-radius:15px;background:#080e1c}.status-card span,.config-card label{display:block;color:var(--muted);font-size:11px;font-weight:750}.status-card strong{display:block;margin-top:7px;color:#dff8ff;font-size:15px}.config-grid{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:12px;margin-top:14px}.config-card input{width:100%;margin:10px 0 0;padding:11px;border:1px solid var(--line);border-radius:10px;background:#050a14;color:var(--text);font:inherit}.config-actions{display:flex;align-items:center;gap:12px;margin-top:14px}.config-actions button{min-width:150px}.config-hint{margin:0;color:var(--muted);font-size:11px}
    .landscape-hud{display:none}.hud-topbar{grid-column:1/-1;display:grid;grid-template-columns:1fr auto 1fr;align-items:center;padding:0 clamp(10px,2vw,26px);border-bottom:1px solid #28d7ff54;background:linear-gradient(90deg,#06132d,#10264a 50%,#06132d);box-shadow:0 8px 28px #00d9ff20}.hud-signal{display:flex;align-items:center;gap:7px;color:#70f6ff;font-size:10px;font-weight:850;letter-spacing:1.2px}.hud-signal:before{width:8px;height:8px;border-radius:50%;background:var(--success);box-shadow:0 0 13px var(--success);content:""}.hud-title{color:#34e8ff;font-size:clamp(14px,2.2vw,23px);font-weight:900;letter-spacing:1.6px;text-align:center;text-shadow:0 0 18px #00d9ffaa}.hud-exit{justify-self:end;padding:7px 11px;border-color:#28d7ff66;background:#0a1c37;color:#79f4ff;font-size:11px;font-weight:850}.hud-bank{position:relative;display:grid;grid-template-rows:auto 1fr;align-items:center;overflow:hidden;padding:clamp(11px,2vw,25px)}.hud-bank:before{position:absolute;inset:8% 3%;border:clamp(8px,1.1vw,15px) solid #253c65;border-top-color:#355787;border-bottom-color:#182b50;border-radius:42% 42% 27% 27%;box-shadow:inset 0 0 35px #0bdcff0c,0 0 34px #0bdcff0b;content:""}.hud-bank:after{position:absolute;inset:13% 7%;border:1px solid #28d7ff2c;border-radius:40% 40% 25% 25%;content:""}.hud-bank-label{position:relative;z-index:1;color:#6fefff;font-size:10px;font-weight:900;letter-spacing:2px;text-align:center;text-shadow:0 0 12px #00d9ff}.hud-controls{position:relative;z-index:1;display:grid;grid-template-columns:1fr 1fr;place-items:center;gap:clamp(8px,2vw,28px)}.hud-drive{width:clamp(68px,12vw,136px);height:clamp(68px,12vw,136px);padding:0;border:2px solid #36dfff7a;border-radius:50%;background:radial-gradient(circle at 40% 32%,#244d79,#0b1c38 58%,#050d1e 70%);box-shadow:inset 0 0 18px #55eaff38,inset 0 -12px 24px #0007,0 0 26px #00d9ff20;color:#9cf7ff;font-size:clamp(26px,4.5vw,49px);text-shadow:0 0 13px #00d9ff}.hud-drive small{font-size:9px}.hud-drive.held{transform:scale(.92);background:radial-gradient(circle,#15a6cb,#07516d 65%,#032335);box-shadow:inset 0 0 20px #fff4,0 0 32px #00d9ffaa}.hud-center{position:relative;display:flex;flex-direction:column;align-items:center;justify-content:center;gap:clamp(5px,1.2vh,12px);padding:8px;text-align:center}.hud-center:before{position:absolute;inset:12% -5% 3%;z-index:0;border:1px solid #28d7ff22;clip-path:polygon(22% 0,78% 0,100% 100%,0 100%);background:linear-gradient(180deg,#10284e45,transparent);content:""}.hud-center>*{position:relative;z-index:1}.hud-kicker{color:#667b9f;font-size:8px;font-weight:900;letter-spacing:2.3px}.hud-heading{color:#20e7ff;font-size:clamp(11px,1.6vw,17px);font-weight:900;letter-spacing:1.8px;text-shadow:0 0 13px #00d9ff}.hud-speed{display:grid;line-height:.9}.hud-speed output{color:#f4fbff;font-size:clamp(40px,8vw,92px);font-weight:900;letter-spacing:-4px;text-shadow:0 0 25px #247dff8a}.hud-speed span{margin-top:7px;color:#5b6e90;font-size:8px;font-weight:850;letter-spacing:2px}.speed-presets{display:grid;grid-template-columns:repeat(3,1fr);gap:7px;width:min(100%,260px)}.speed-preset{padding:8px 6px;border-radius:999px;color:#8397b8;font-size:11px;font-weight:850}.speed-preset.active{border-color:#28d7ff9c;background:linear-gradient(135deg,#087fa4,#315bd8);box-shadow:0 0 18px #28d7ff4a;color:#fff}.hud-distance{display:flex;align-items:baseline;gap:8px;padding:7px 13px;border:1px solid #28d7ff3c;border-radius:999px;background:#031024c7}.hud-distance span{color:#637b9e;font-size:8px;font-weight:900;letter-spacing:1.2px}.hud-distance strong{color:#61f0ff;font-size:clamp(15px,2.4vw,25px);font-variant-numeric:tabular-nums}.hud-stop{min-width:110px;padding:8px 18px;border-color:#ff4f7080;border-radius:999px;background:linear-gradient(145deg,#7c1731,#3a1020);color:#ffdce4;font-size:12px;font-weight:850}.hud-stop small{display:inline;margin-left:5px}
    body.drive-expanded{overflow:hidden}#manualPanel.drive-expanded{position:fixed;inset:0;z-index:20}#manualPanel:fullscreen,#manualPanel:-webkit-full-screen,#manualPanel.drive-expanded{width:100%;height:100vh;height:100dvh;min-height:0;padding:20px;border:0;border-radius:0;background:radial-gradient(circle at 50% 30%,#13284a,#050711 72%);overflow:auto}
    .mode-card{position:relative;display:grid;min-height:330px;margin-top:25px;padding:30px;place-items:center;overflow:hidden;border:1px solid var(--line);border-radius:22px;background:radial-gradient(circle at 50% 25%,#243b6638,transparent 45%),#070c18;text-align:center}.mode-card:before{position:absolute;inset:14px;border:1px solid #ffffff08;border-radius:16px;content:""}.mode-icon{display:grid;width:88px;height:88px;place-items:center;border:1px solid #28d7ff3b;border-radius:27px;background:linear-gradient(145deg,#172f51,#18152d);box-shadow:0 0 32px #28d7ff1f;font-size:44px}.mode-card strong{font-size:19px;letter-spacing:.1px}.mode-actions{position:relative;z-index:1;display:grid;grid-template-columns:1fr 1fr;gap:12px;width:min(100%,410px);margin-top:22px}.mode-on,.mode-off{min-height:50px;color:white;font-size:12px;font-weight:850;letter-spacing:1px}.mode-on{border-color:#39e58c55;background:linear-gradient(135deg,#128351,#16a86b);box-shadow:0 8px 24px #16a86b25}.mode-off{border-color:#ff4f7055;background:linear-gradient(135deg,#9d2944,#c53755);box-shadow:0 8px 24px #c5375525}footer{padding:22px 8px;color:#576784;font-size:11px;line-height:1.75;text-align:center}
    @media(max-width:720px){main{padding:18px 13px 100px}.brand-copy{display:block}.online{margin-top:5px;width:max-content}.dashboard{display:block}.menu{position:fixed;z-index:10;right:10px;bottom:10px;left:10px;display:grid;grid-template-columns:repeat(3,1fr);padding:7px;border-color:#314462;background:#080d18f2;box-shadow:0 14px 45px #000c}.menu:before{display:none}.menu button{text-align:center}.panel{min-height:520px}}
    @media(max-width:470px){header{align-items:flex-start}.brand-mark{width:44px;height:44px}.online{display:none}.telemetry{width:100%;gap:10px;padding:10px 12px}.sensor-state{font-size:10px}.panel{padding:22px 14px}.manual-head-actions .mode-status{display:none}.mode-actions{grid-template-columns:1fr}.actions{flex-direction:column}.status-grid{grid-template-columns:1fr}.config-grid{grid-template-columns:1fr}.config-actions{align-items:stretch;flex-direction:column}.config-actions button{width:100%}}
    @media(orientation:landscape){#manualPanel:fullscreen,#manualPanel:-webkit-full-screen,#manualPanel.drive-expanded{display:block;padding:0;overflow:hidden;background:radial-gradient(circle at 50% 45%,#0d2a51,#030816 62%,#080922);box-shadow:inset 0 0 90px #000}#manualPanel:fullscreen .panel-head,#manualPanel:-webkit-full-screen .panel-head,#manualPanel.drive-expanded .panel-head,#manualPanel:fullscreen .portrait-manual,#manualPanel:-webkit-full-screen .portrait-manual,#manualPanel.drive-expanded .portrait-manual{display:none}#manualPanel:fullscreen .landscape-hud,#manualPanel:-webkit-full-screen .landscape-hud,#manualPanel.drive-expanded .landscape-hud{display:grid;width:100%;height:100%;min-height:0;grid-template-columns:minmax(0,1fr) minmax(0,.82fr) minmax(0,1fr);grid-template-rows:48px minmax(0,1fr);padding-right:env(safe-area-inset-right);padding-left:env(safe-area-inset-left)}}
    @media(orientation:landscape) and (max-height:500px){.landscape-hud{padding-bottom:16px}.hud-topbar{min-height:40px}.hud-bank{min-width:0;padding:6px}.hud-bank:before{inset:6% 1%;border-width:6px}.hud-bank:after{inset:11% 5%}.hud-controls{gap:clamp(4px,1vw,12px)}.hud-drive{width:clamp(44px,min(10vw,23vh),88px);height:clamp(44px,min(10vw,23vh),88px);font-size:clamp(20px,5vh,32px)}.hud-center{min-width:0;gap:3px;padding:3px}.hud-speed output{font-size:clamp(32px,15vh,58px)}.hud-speed span{margin-top:3px}.speed-presets{gap:3px}.speed-preset{padding:5px 2px}.hud-distance{padding:4px 8px}.hud-stop{min-width:0;padding:5px 10px}#manualPanel:fullscreen .landscape-hud,#manualPanel:-webkit-full-screen .landscape-hud,#manualPanel.drive-expanded .landscape-hud{grid-template-rows:40px minmax(0,1fr)}}
    .landscape-hud{position:relative;padding-bottom:max(24px,env(safe-area-inset-bottom))}.hud-notice{position:absolute;z-index:2;right:10px;bottom:max(5px,env(safe-area-inset-bottom));left:10px;color:#b8cae4;font-size:11px;text-align:center;pointer-events:none}#hudConnect{justify-self:start}#hudConnect:disabled{opacity:1;filter:none;color:var(--success);cursor:default}
    .hud-joystick-bank{grid-template-rows:auto 1fr;min-width:0}.joystick-surface{position:relative;z-index:1;align-self:center;justify-self:center;width:min(38vw,34vh,290px);aspect-ratio:1;border:2px solid #36dfff70;border-radius:50%;background:radial-gradient(circle,#19355a 0 5%,#102343 6% 48%,#0a1830 49% 68%,#12284a 69% 71%,#071126 72%);box-shadow:inset 0 0 35px #38ddff25,0 0 35px #00d9ff20;touch-action:none;user-select:none;-webkit-user-select:none}.joystick-surface:before,.joystick-surface:after{position:absolute;content:"";pointer-events:none}.joystick-surface:before{top:50%;right:9%;left:9%;height:1px;background:#52dfff40}.joystick-surface:after{top:9%;bottom:9%;left:50%;width:1px;background:#52dfff40}.joystick-surface.disabled{opacity:.38;filter:grayscale(.6)}.joystick-handle{position:absolute;z-index:1;top:50%;left:50%;display:grid;width:31%;height:31%;place-items:center;border:2px solid #9bf5ff;border-radius:50%;background:radial-gradient(circle at 35% 28%,#35ddf5,#08759d 62%,#063552);box-shadow:inset 0 2px #fff8,0 0 26px #00d9ffaa;color:#effcff;font-size:clamp(21px,4vh,38px);transform:translate(-50%,-50%);pointer-events:none}.joystick-surface.active .joystick-handle{background:radial-gradient(circle at 35% 28%,#64edff,#087fa4 62%,#063552);box-shadow:inset 0 2px #fff9,0 0 34px #00d9ffff}.joystick-label{position:absolute;z-index:1;color:#78dff3;font-size:9px;font-weight:850;letter-spacing:1px;pointer-events:none}.joystick-label.forward{top:7%;left:50%;transform:translateX(-50%)}.joystick-label.reverse{bottom:7%;left:50%;transform:translateX(-50%)}.joystick-label.left{top:50%;left:5%;transform:translateY(-50%)}.joystick-label.right{top:50%;right:5%;transform:translateY(-50%)}
    @media(orientation:landscape) and (max-height:500px){.joystick-surface{width:min(34vw,43vh,220px)}.joystick-handle{width:30%;height:30%}}
    .hud-top-actions{display:flex;align-items:center;justify-self:end;gap:6px}.hud-sound[aria-pressed="false"]{border-color:#ff4f7059;color:#ffb8c5}
    @media(orientation:landscape) and (max-height:500px){.landscape-hud{padding-bottom:max(16px,env(safe-area-inset-bottom))}}
    .hud-rotate{display:none}#manualPanel.hud-mode .panel-head,#manualPanel.hud-mode .portrait-manual{display:none}#manualPanel.hud-mode .hud-rotate{display:grid;min-height:100%;padding:30px;place-content:center;justify-items:center;gap:18px;text-align:center}.hud-rotate-icon{font-size:clamp(56px,15vw,100px)}.hud-rotate strong{font-size:clamp(22px,6vw,36px)}.hud-rotate p{max-width:28em;color:var(--muted)}.hud-rotate-actions{display:flex;flex-wrap:wrap;justify-content:center;gap:10px}.hud-warning{display:none;color:var(--danger);font-size:11px;font-weight:900;text-align:center}.hud-warning.active{display:block}.hud-distance.danger{border-color:var(--danger);box-shadow:0 0 18px #ff4f7055}.hud-distance.danger strong{color:var(--danger)}
    @media(max-width:720px){.menu{grid-template-columns:repeat(4,minmax(0,1fr))}.menu button{padding:10px 3px;font-size:11px}}
    @media(orientation:landscape){#manualPanel.hud-mode .hud-rotate{display:none}}
    .hud-encoder{color:#b8cae4;font-size:11px;line-height:1.3}.hud-encoder strong{color:#61f0ff;font-variant-numeric:tabular-nums}.hud-encoder small{display:block;max-width:220px;font-size:9px;color:#a5b4cb}.encoder-note{color:var(--muted);font-size:12px;line-height:1.5;overflow-wrap:anywhere}
  </style>
</head>
<body>
  <main>
    <header><div class="brand"><div class="brand-mark">🤖</div><div class="brand-copy"><div><div class="eyebrow">Robot control system</div><h1>Ruby <span>Drive</span></h1><p class="subtitle">Điều khiển xe qua Wi-Fi</p></div><span class="online">SYSTEM ONLINE</span></div></div><button id="theme" class="theme" aria-label="Đổi độ tương phản">◐</button></header>
    <div class="telemetry" aria-live="polite">
      <div class="telemetry-icon">◉</div>
      <div class="telemetry-copy"><span class="distance-label">KHOẢNG CÁCH VẬT CẢN</span><strong id="distance" class="distance">-- cm</strong></div><span id="sensorState" class="sensor-state error">Đang chờ SR04…</span>
    </div>
    <div id="modeNotice" class="notice" role="status" aria-live="polite"></div>
    <div class="dashboard">
      <nav class="menu" aria-label="Chế độ điều khiển">
        <button id="manualTab" class="active">📊 Trạng thái</button>
        <button id="hudTab">🚗 HUD</button>
        <button id="avoidTab">🛡️ Né vật cản</button>
        <button id="followTab">👣 Follow</button>
      </nav>
      <section class="panel-shell">
        <article id="manualPanel" class="panel active">
          <div class="panel-head"><div><h2>Trạng thái &amp; cấu hình</h2><p class="subtitle">Thông tin cảm biến, chế độ lái và thông số hoạt động.</p></div><div class="manual-head-actions"><span id="controlStatus" class="mode-status">CHƯA KẾT NỐI</span></div></div>
          <div class="portrait-manual">
            <div id="status" role="status" aria-live="polite">Bấm “Bắt đầu” để nhận quyền điều khiển.</div>
            <div class="status-grid">
              <div class="status-card"><span>Chế độ</span><strong id="statusMode">Điều khiển web</strong></div>
              <div class="status-card"><span>Motor</span><strong id="statusMotion">Đang dừng</strong></div>
              <div class="status-card"><span>Mức PWM lệnh</span><strong id="statusSpeed">0%</strong></div>
            </div>
            <section id="encoderPanel" hidden aria-live="polite">
              <div class="status-grid">
                <div class="status-card"><span>Xe đã đi · ước lượng từ lúc bật</span><strong id="encoderTravel">-- m</strong></div>
                <div class="status-card"><span>RPM đo được · trái / phải</span><strong id="encoderRpm">-- / --</strong></div>
                <div class="status-card"><span>Tổng xung · trái / phải</span><strong id="encoderPulses">-- / --</strong></div>
              </div>
              <p id="encoderWheels" class="encoder-note">Quãng đường bánh trái/phải: -- / -- m</p>
              <p id="encoderState" class="encoder-note">Đang chờ FC-03…</p>
            </section>
            <div class="actions"><button id="claim" class="primary">Nhận quyền lái</button><button id="release" disabled>Trả quyền về giọng nói</button><button class="stop-drive" id="stop" disabled>Dừng xe</button></div>
            <label class="speed-label" for="speed">Tốc độ tối đa trên HUD <output id="speedValue">75%</output></label>
            <input id="speed" type="range" min="0" max="100" value="75" step="5">
            <h3>Thông số tự hành</h3>
            <div class="config-grid">
              <div class="config-card"><label for="followDistanceSetting">Khoảng cách Follow (cm)</label><input id="followDistanceSetting" type="number" min="10" max="200" step="1" value="30"></div>
              <div class="config-card"><label for="obstacleDistanceSetting">Ngưỡng né vật cản (cm)</label><input id="obstacleDistanceSetting" type="number" min="10" max="100" step="1" value="20"></div>
            </div>
            <div class="config-actions"><button id="saveConfig" type="button">Lưu thông số</button><p class="config-hint">Áp dụng ngay trong firmware; giá trị sẽ về mặc định sau khi khởi động lại.</p></div>
          </div>
          <div class="hud-rotate" aria-live="polite"><div class="hud-rotate-icon">↻</div><strong>Xoay ngang màn hình để trải nghiệm.</strong><p id="hudRotateStatus">Đang kết nối điều khiển xe…</p><div class="hud-rotate-actions"><button id="hudRotateConnect" type="button">Kết nối lại</button><button id="hudRotateExit" type="button">Quay lại</button></div></div>
          <div class="landscape-hud" aria-label="Buồng lái ngang">
            <div class="hud-topbar"><button id="hudConnect" class="hud-exit" type="button" aria-live="polite">Kết nối lại</button><strong class="hud-title">RUBY · REMOTE CONTROL</strong><div class="hud-top-actions"><button id="hudSound" class="hud-exit hud-sound" type="button" aria-label="Âm cảnh báo" aria-pressed="true">🔊 Âm: Bật</button><button id="hudExit" class="hud-exit" type="button">✕ Thu nhỏ</button></div></div>
            <div id="hudNotice" class="hud-notice" role="status" aria-live="polite"></div>
            <section class="hud-bank hud-joystick-bank"><span class="hud-bank-label">JOYSTICK · LÁI MỀM</span><div id="hudJoystick" class="joystick-surface" role="application" aria-label="Kéo joystick để tiến, lùi và rẽ"><span class="joystick-label forward">TIẾN</span><span class="joystick-label reverse">LÙI</span><span class="joystick-label left">TRÁI</span><span class="joystick-label right">PHẢI</span><span id="hudJoystickHandle" class="joystick-handle" aria-hidden="true">✣</span></div></section>
            <section class="hud-center"><span class="hud-kicker">MANUAL DRIVE</span><strong class="hud-heading">REMOTE CONTROL</strong><div class="hud-speed"><output id="hudSpeedValue">75%</output><span>POWER OUTPUT</span></div><div class="speed-presets" aria-label="Mức tốc độ"><button id="speed50" class="speed-preset" type="button">50</button><button id="speed75" class="speed-preset active" type="button">75</button><button id="speed100" class="speed-preset" type="button">100</button></div><div id="hudEncoder" class="hud-encoder" hidden>ĐÃ ĐI <strong id="hudTravel">-- m</strong><small id="hudEncoderState">Đang chờ FC-03…</small></div><div id="hudDistance" class="hud-distance"><span>SR04</span><strong id="manualDistance">-- cm</strong></div><div id="hudWarning" class="hud-warning" role="alert">⚠ Cảnh báo sớm · dưới 30 cm</div><button class="hud-stop" id="hudStop" disabled aria-label="Dừng xe">■<small>DỪNG</small></button></section>
            <section class="hud-bank"><span class="hud-bank-label">ĐIỀU HƯỚNG DỰ PHÒNG</span><div class="hud-controls"><button class="drive hud-drive" data-dir="f" disabled aria-label="Tiến">▲<small>Tiến</small></button><button class="drive hud-drive" data-dir="l" disabled aria-label="Xoay trái">◀<small>Trái</small></button><button class="drive hud-drive" data-dir="r" disabled aria-label="Xoay phải">▶<small>Phải</small></button><button class="drive hud-drive" data-dir="b" disabled aria-label="Lùi">▼<small>Lùi</small></button></div></section>
          </div>
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
    <footer>Trong phiên HUD, motor chỉ nhận lệnh từ trang này; lệnh dừng bằng giọng nói vẫn có hiệu lực.<br>Mất kết nối sẽ dừng xe. Các thông số tự hành áp dụng trong phiên hoạt động hiện tại.</footer>
  </main>
  <script>
    'use strict';
    const $ = id => document.getElementById(id);
    const driveButtons = [...document.querySelectorAll('[data-dir]')];
    const stopButtons = [$('stop'),$('hudStop')];
    const speedValues = [50,75,100];
    const names = {s:'Đã dừng',f:'Tiến',b:'Lùi',l:'Xoay trái',r:'Xoay phải'};
    let session = 0, sequence = 0, epoch = 0, busy = false, pending = false;
    let direction = 's', releasing = false, heldPointer = null;
    let joystickX = 0, joystickY = 0;
    let joystickPointer = null;
    let telemetryBusy = false, autonomyBusy = false, currentMode = 'manual', autonomyVersion = 0;
    let recovering = false, wakeLock = null, wakePending = false, wakeUnavailable = false;
    let hudMode = false, warningEnabled = true;
    let warningDistanceCm = 30, obstacleDistanceCm = 20;

    const modeNames = {manual:'Điều khiển web',follow:'Follow',avoid:'Né vật cản'};

    function showPanel(name) {
      for (const mode of ['manual','avoid','follow']) {
        $(mode + 'Panel').classList.toggle('active',mode === (name === 'hud' ? 'manual' : name));
        $(mode + 'Tab').classList.toggle('active',mode === name);
      }
      $('hudTab').classList.toggle('active',name === 'hud');
    }

    function updateDistanceAlert(valid, distance) {
      const nearObstacle = hudMode && valid && distance < warningDistanceCm;
      $('hudDistance').classList.toggle('danger',nearObstacle);
      $('hudWarning').classList.toggle('active',nearObstacle);
      $('hudWarning').textContent = distance < obstacleDistanceCm
        ? '⚠ Nguy cơ va chạm · dưới ' + obstacleDistanceCm + ' cm'
        : '⚠ Cảnh báo sớm · dưới ' + warningDistanceCm + ' cm';
    }

    function applyMode(mode) {
      // Status uses the firmware enum name; control requests use the shorter "avoid".
      if (mode === 'obstacle_avoidance') mode = 'avoid';
      currentMode = mode;
      const follow = mode === 'follow', avoid = mode === 'avoid';
      if ($('statusMode')) $('statusMode').textContent = modeNames[mode] || modeNames.manual;
      $('followStatus').textContent = follow ? 'ON' : 'OFF';
      $('followStatus').classList.toggle('on',follow);
      $('avoidStatus').textContent = avoid ? 'ON' : 'OFF';
      $('avoidStatus').classList.toggle('on',avoid);
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

    async function refreshTelemetry() {
      if (telemetryBusy) return;
      telemetryBusy = true;
      const modeVersion = autonomyVersion;
      const controller = new AbortController();
      const timeout = setTimeout(() => controller.abort(),500);
      try {
        const response = await fetch('/api/status',{cache:'no-store',signal:controller.signal});
        if (!response.ok) throw new Error(String(response.status));
        const data = await response.json();
        renderEncoder(data.encoder);
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
        $('followDistance').textContent = 'Mục tiêu ' +
          (Number.isFinite(data.follow_target_cm) ? data.follow_target_cm : data.follow_distance_cm) + ' cm';
        $('avoidDistance').textContent = 'Ngưỡng ' +
          (Number.isFinite(data.obstacle_stop_cm) ? data.obstacle_stop_cm : data.obstacle_distance_cm) + ' cm';
        if (modeVersion === autonomyVersion) applyMode(data.autonomy_mode);
        if ($('statusMotion')) {
          const motionNames = {forward:'Tiến',backward:'Lùi',turn_left:'Xoay trái',
            turn_right:'Xoay phải',stopped:'Đang dừng'};
          $('statusMotion').textContent = data.moving
            ? motionNames[data.motion] || 'Đang chạy' : 'Đang dừng';
        }
        if ($('statusSpeed')) $('statusSpeed').textContent =
          (data.moving && Number.isFinite(data.speed_percent) ? data.speed_percent : 0) + '%';
        if ($('followDistanceSetting') && document.activeElement !== $('followDistanceSetting'))
          $('followDistanceSetting').value = String(data.follow_distance_cm);
        if ($('obstacleDistanceSetting') && document.activeElement !== $('obstacleDistanceSetting'))
          $('obstacleDistanceSetting').value = String(data.obstacle_distance_cm);
      } catch (_) {
        renderEncoder(null,true);
        $('distance').textContent = '-- cm';
        $('manualDistance').textContent = '-- cm';
        $('sensorState').textContent = 'Lỗi: mất kết nối trạng thái';
        $('sensorState').classList.toggle('ok',false);
        $('sensorState').classList.toggle('error',true);
        updateDistanceAlert(false,0);
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
      $('hudConnect').disabled = Boolean(session) || busy || autonomyBusy;
      $('hudRotateConnect').disabled = Boolean(session) || busy || autonomyBusy;
      $('hudConnect').textContent = recovering ? 'Đang nối lại…' : session ? '● Đã kết nối' : '↻ Kết nối lại';
      $('hudRotateStatus').textContent = session
        ? 'Đã kết nối. Xoay ngang để điều khiển xe.' : $('status').textContent;
      $('hudNotice').textContent = !session ? $('status').textContent : recovering
        ? 'Xe đã dừng · Đang kiểm tra kết nối…' : wakeUnavailable
        ? 'Hãy tăng thời gian tự tắt màn hình điện thoại khi lái.' : '';
    }

    function render() {
      $('claim').disabled = Boolean(session) || busy || autonomyBusy;
      $('release').disabled = !session || releasing;
      stopButtons.forEach(button => { button.disabled = !session || releasing; });
      if ($('controlStatus')) {
        $('controlStatus').textContent = session ? 'ĐÃ KẾT NỐI' : 'CHƯA KẾT NỐI';
        $('controlStatus').classList.toggle('on',Boolean(session));
      }
      for (const id of ['followOn','followOff','avoidOn','avoidOff'])
        $(id).disabled = autonomyBusy || busy || releasing;
      driveButtons.forEach(button => {
        button.disabled = !session || releasing || recovering;
        button.classList.toggle('held', Boolean(session) && button.dataset.dir === direction);
      });
      if ($('hudJoystick')) {
        $('hudJoystick').setAttribute('aria-disabled',String(!session || releasing || recovering));
        $('hudJoystick').classList.toggle('disabled',!session || releasing || recovering);
      }
      renderHud();
      void syncWakeLock();
    }

    function setSpeed(value) {
      $('speed').value = String(value);
      $('speedValue').textContent = value + '%';
      $('hudSpeedValue').textContent = value + '%';
      speedValues.forEach(speed => $('speed' + speed).classList.toggle('active',speed === value));
      warningDistanceCm = Math.ceil(obstacleDistanceCm * Math.max(value,50) / 50);
      updateDistanceAlert($('manualDistance').textContent !== '-- cm',
        Number.parseInt($('manualDistance').textContent,10));
      if (value === 0) stop();
      else if (session && !releasing) { pending = true; void pump(); }
    }

    function enterHud() {
      if (hudMode) return;
      hudMode = true;
      showPanel('hud');
      $('manualPanel').classList.toggle('hud-mode',true);
      setDriveFallback(true);
      void claimControl();
      void refreshTelemetry();
    }

    function exitHud() {
      if (!hudMode) return;
      if (direction !== 's') stop();
      hudMode = false;
      updateDistanceAlert(false,0);
      $('manualPanel').classList.toggle('hud-mode',false);
      setDriveFallback(false);
      showPanel('manual');
    }

    function commandBody(token, dir) {
      const command = {session:token,sequence:++sequence,direction:dir,
        speed:Number($('speed').value),warning_enabled:warningEnabled};
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
      const sentSpeed = Number($('speed').value);
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
      if (!session || releasing || (recovering && next !== 's')) return;
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

    async function setAutonomy(mode, enabled) {
      if (autonomyBusy || busy || releasing) return;
      autonomyBusy = true;
      render();
      $('modeNotice').textContent = 'Đang chuyển chế độ…';
      try {
        if (session) {
          heldPointer = null; resetJoystick();
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

    async function saveConfiguration() {
      const followDistance = Number($('followDistanceSetting').value);
      const obstacleDistance = Number($('obstacleDistanceSetting').value);
      if (!Number.isInteger(followDistance) || followDistance < 10 || followDistance > 200 ||
          !Number.isInteger(obstacleDistance) || obstacleDistance < 10 || obstacleDistance > 100) {
        $('status').textContent = 'Khoảng cách Follow phải từ 10–200 cm; ngưỡng né vật cản từ 10–100 cm.';
        return;
      }
      $('saveConfig').disabled = true;
      $('status').textContent = 'Đang lưu thông số…';
      try {
        await post('/api/config',JSON.stringify({
          follow_distance_cm:followDistance,obstacle_distance_cm:obstacleDistance
        }));
        obstacleDistanceCm = obstacleDistance;
        warningDistanceCm = Math.ceil(obstacleDistance * Math.max(Number($('speed').value),50) / 50);
        $('status').textContent = 'Đã áp dụng thông số trong firmware.';
        await refreshTelemetry();
      } catch (_) {
        $('status').textContent = 'Không lưu được thông số. Kiểm tra kết nối rồi thử lại.';
      } finally { $('saveConfig').disabled = false; }
    }

    async function claimControl() {
      if (session || busy || autonomyBusy || document.hidden) return;
      busy = true;
      const currentEpoch = ++epoch;
      render();
      $('status').textContent = 'Đang kết nối…';
      try {
        const result = await post('/api/claim');
        if (currentEpoch !== epoch) return;
        session = result.session; sequence = 0; direction = 's'; pending = true;
        joystickX = 0; joystickY = 0; joystickPointer = null;
        warningEnabled = true;
        recovering = false;
        wakeUnavailable = false;
        ++autonomyVersion;
        applyMode('manual');
        $('status').textContent = 'Đã kết nối HUD. Kéo joystick hoặc giữ nút hướng để lái.';
      } catch (_) {
        if (currentEpoch === epoch)
          $('status').textContent = 'Chưa nhận được quyền điều khiển. Kiểm tra Wi-Fi hoặc trang đang điều khiển khác, rồi thử lại.';
      } finally {
        busy = false;
        render();
        if (pending && session) void pump();
      }
    }
    $('claim').addEventListener('click',claimControl);
    $('hudConnect').addEventListener('click',claimControl);

    $('release').addEventListener('click', () => {
      if (!session || releasing) return;
      heldPointer = null; resetJoystick();
      releasing = true; direction = 'release'; pending = true;
      render();
      void pump();
    });
    $('manualTab').addEventListener('click',() => showPanel('manual'));
    $('hudTab').addEventListener('click',enterHud);
    $('hudSound').addEventListener('click',() => {
      if (!session || releasing || recovering) return;
      warningEnabled = !warningEnabled;
      pending = true;
      render();
      void pump();
    });
    $('avoidTab').addEventListener('click',() => showPanel('avoid'));
    $('followTab').addEventListener('click',() => showPanel('follow'));
    $('avoidOn').addEventListener('click',() => { void setAutonomy('avoid',true); });
    $('avoidOff').addEventListener('click',() => { void setAutonomy('avoid',false); });
    $('followOn').addEventListener('click',() => { void setAutonomy('follow',true); });
    $('followOff').addEventListener('click',() => { void setAutonomy('follow',false); });
    if ($('saveConfig')) $('saveConfig').addEventListener('click',() => { void saveConfiguration(); });
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
    stopButtons.forEach(button => {
      button.addEventListener('pointerdown', event => { event.preventDefault(); stop(); });
      button.addEventListener('click', stop);
    });
    // Setting zero releases the held direction; raising it never restarts motion.
    $('speed').addEventListener('input',() => setSpeed(Number($('speed').value)));
    speedValues.forEach(speed =>
      $('speed' + speed).addEventListener('click',() => setSpeed(speed)));
    driveButtons.forEach(button => {
      button.addEventListener('contextmenu', event => event.preventDefault());
      button.addEventListener('pointerdown', event => {
        event.preventDefault();
        if (!session || releasing || recovering || heldPointer !== null ||
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
        if (!session || releasing || recovering || heldPointer !== null ||
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
      if (event.code === 'Space' && session) { event.preventDefault(); stop(); }
    });
    const pageLost = () => park('Đã dừng xe và trả quyền điều khiển về giọng nói.');
    window.addEventListener('blur',() => {
      // Browser chrome/fullscreen changes can blur a still-visible cockpit.
      if (document.hidden) pageLost();
      else stop();
    });
    window.addEventListener('focus',() => { wakeUnavailable = false; void syncWakeLock(); });
    window.addEventListener('pagehide', pageLost);
    document.addEventListener('visibilitychange', () => { if (document.hidden) pageLost(); });
    void refreshTelemetry();
    setInterval(refreshTelemetry,2000);
    setInterval(() => { if (hudMode && !document.hidden) void refreshTelemetry(); },250);
    setInterval(() => { if (session && !releasing) { pending = true; void pump(); } },150);
    $('theme').addEventListener('click', () => document.body.classList.toggle('ultra'));
    render();
  </script>
</body>
</html>
)html";
