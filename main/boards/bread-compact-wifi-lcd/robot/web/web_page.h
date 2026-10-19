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
    .scan-map{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:8px;margin-top:10px}.scan-sector{padding:12px 6px;border:1px solid var(--line);border-radius:12px;text-align:center;background:var(--panel2)}.scan-sector strong{display:block;color:var(--cyan);font-size:20px}.feature-card{margin-top:18px;padding:16px;border:1px solid var(--line);border-radius:16px;background:var(--panel2)}.feature-card p{color:var(--muted)}.feature-card button{margin-right:8px}.feature-state{color:var(--cyan);font-weight:700}
    .feature-card input[type=text]{width:100%;margin:6px 0 12px;padding:11px;border:1px solid var(--line);border-radius:10px;background:#050a14;color:var(--text);font:inherit}
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
    #status{margin:12px 0 0;color:#b8cae4;font-size:13px;line-height:1.5}#status:empty{display:none}
    #actionToast{position:fixed;z-index:50;top:max(12px,env(safe-area-inset-top));right:12px;display:flex;align-items:flex-start;gap:10px;width:min(380px,calc(100vw - 24px));padding:12px 15px;border:1px solid transparent;border-radius:13px;background:#102019;color:#dcffeb;box-shadow:0 12px 32px #0009;font-size:13px;line-height:1.5;opacity:0;transform:translateY(-8px);pointer-events:none;transition:opacity .18s,transform .18s}#actionToast.show{opacity:1;transform:none}#actionToast.success{border-color:#39e58c80}#actionToast.error{border-color:#ff4f7080;background:#2b101b;color:#ffe3e8;pointer-events:auto}#actionToastText{flex:1}#actionToastClose{padding:0 5px;border:0;background:transparent;color:inherit;font-size:19px;line-height:1}#actionToastClose[hidden]{display:none}
    #operationBar{position:fixed;z-index:30;right:14px;bottom:14px;display:flex;align-items:center;gap:12px;width:min(520px,calc(100vw - 28px));padding:12px 14px;border:1px solid #28d7ff75;border-radius:16px;background:#09162cf5;box-shadow:0 14px 38px #000b;color:#e7f8ff}#operationBar[hidden]{display:none}.operation-copy{flex:1;font-size:13px;line-height:1.4}.operation-copy small{display:block;margin-top:3px;color:#9fb0cd;font-size:11px}#operationStop{flex:none;border-color:#ff4f70a0;background:#8d2540;color:#fff;font-weight:800}
    .home-overview{display:grid;grid-template-columns:1fr 1fr auto;align-items:center;gap:9px;margin:18px 0 4px}.home-overview span{padding:11px 13px;border:1px solid var(--line);border-radius:13px;background:#080e1c;color:var(--muted);font-size:12px}.home-overview strong{color:var(--text)}.home-overview button{padding:11px 13px;font-size:12px}.home-updated{margin:0 0 16px;color:var(--muted);font-size:11px}body.operation-active main{padding-bottom:110px}
    .home-section{margin-top:16px;padding:18px;border:1px solid var(--line);border-radius:16px;background:var(--panel2)}.home-section h3{margin:0 0 7px}.home-section p{margin:6px 0 12px;color:var(--muted);font-size:12px}.home-actions{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:9px}.home-actions .danger{border-color:#ff4f70a0;background:linear-gradient(135deg,#9d2944,#5f1830);color:#fff;font-weight:850}.home-row{display:flex;flex-wrap:wrap;align-items:center;gap:9px;margin-top:11px}.home-row label{color:var(--muted);font-size:12px}.home-row input{width:105px;padding:10px;border:1px solid var(--line);border-radius:10px;background:#050a14;color:var(--text);font:inherit}.home-row button{flex:1;min-width:100px}.home-detail{color:var(--muted);font-size:12px}.home-detail summary{cursor:pointer;color:var(--text);font-size:15px;font-weight:750}.home-detail p{margin-top:12px}
    .drive{position:relative;border:1px solid #28d7ff47;border-radius:18px;background:linear-gradient(145deg,#183558,#101b32);box-shadow:0 10px 20px #0005,inset 0 1px #ffffff18;color:#dff8ff;font-size:25px;font-weight:850;touch-action:none;user-select:none;-webkit-user-select:none}.drive:hover:not(:disabled){box-shadow:0 0 20px #28d7ff25,inset 0 1px #ffffff22}.drive small{display:block;margin-top:3px;color:#8eaac8;font-size:10px;font-weight:700;letter-spacing:.5px}.drive.held{transform:scale(.94);border-color:var(--cyan);background:#0a7998;box-shadow:0 0 26px #28d7ff55}.drive.held small{color:#fff}.hint{margin-top:14px;text-align:center}.status-grid{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:10px;margin:18px 0}.status-card,.config-card{padding:15px;border:1px solid var(--line);border-radius:15px;background:#080e1c}.status-card span,.config-card label{display:block;color:var(--muted);font-size:11px;font-weight:750}.status-card strong{display:block;margin-top:7px;color:#dff8ff;font-size:15px}.config-grid{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:12px;margin-top:14px}.config-card input{width:100%;margin:10px 0 0;padding:11px;border:1px solid var(--line);border-radius:10px;background:#050a14;color:var(--text);font:inherit}.config-actions{display:flex;align-items:center;gap:12px;margin-top:14px}.config-actions button{min-width:150px}.config-hint{margin:0;color:var(--muted);font-size:11px}
    .landscape-hud{display:none}.hud-topbar{grid-column:1/-1;display:grid;grid-template-columns:1fr auto 1fr;align-items:center;padding:0 clamp(10px,2vw,26px);border-bottom:1px solid #28d7ff54;background:linear-gradient(90deg,#06132d,#10264a 50%,#06132d);box-shadow:0 8px 28px #00d9ff20}.hud-signal{display:flex;align-items:center;gap:7px;color:#70f6ff;font-size:10px;font-weight:850;letter-spacing:1.2px}.hud-signal:before{width:8px;height:8px;border-radius:50%;background:var(--success);box-shadow:0 0 13px var(--success);content:""}.hud-title{color:#34e8ff;font-size:clamp(14px,2.2vw,23px);font-weight:900;letter-spacing:1.6px;text-align:center;text-shadow:0 0 18px #00d9ffaa}.hud-exit{justify-self:end;padding:7px 11px;border-color:#28d7ff66;background:#0a1c37;color:#79f4ff;font-size:11px;font-weight:850}.hud-bank{position:relative;display:grid;grid-template-rows:auto 1fr;align-items:center;overflow:hidden;padding:clamp(11px,2vw,25px)}.hud-bank:before{position:absolute;inset:8% 3%;border:clamp(8px,1.1vw,15px) solid #253c65;border-top-color:#355787;border-bottom-color:#182b50;border-radius:42% 42% 27% 27%;box-shadow:inset 0 0 35px #0bdcff0c,0 0 34px #0bdcff0b;content:""}.hud-bank:after{position:absolute;inset:13% 7%;border:1px solid #28d7ff2c;border-radius:40% 40% 25% 25%;content:""}.hud-bank-label{position:relative;z-index:1;color:#6fefff;font-size:10px;font-weight:900;letter-spacing:2px;text-align:center;text-shadow:0 0 12px #00d9ff}.hud-controls{position:relative;z-index:1;display:grid;grid-template-columns:1fr 1fr;place-items:center;gap:clamp(8px,2vw,28px)}.hud-drive{width:clamp(68px,12vw,136px);height:clamp(68px,12vw,136px);padding:0;border:2px solid #36dfff7a;border-radius:50%;background:radial-gradient(circle at 40% 32%,#244d79,#0b1c38 58%,#050d1e 70%);box-shadow:inset 0 0 18px #55eaff38,inset 0 -12px 24px #0007,0 0 26px #00d9ff20;color:#9cf7ff;font-size:clamp(26px,4.5vw,49px);text-shadow:0 0 13px #00d9ff}.hud-drive small{font-size:9px}.hud-drive.held{transform:scale(.92);background:radial-gradient(circle,#15a6cb,#07516d 65%,#032335);box-shadow:inset 0 0 20px #fff4,0 0 32px #00d9ffaa}.hud-center{position:relative;display:flex;flex-direction:column;align-items:center;justify-content:center;gap:clamp(5px,1.2vh,12px);padding:8px;text-align:center}.hud-center:before{position:absolute;inset:12% -5% 3%;z-index:0;border:1px solid #28d7ff22;clip-path:polygon(22% 0,78% 0,100% 100%,0 100%);background:linear-gradient(180deg,#10284e45,transparent);content:""}.hud-center>*{position:relative;z-index:1}.hud-kicker{color:#667b9f;font-size:8px;font-weight:900;letter-spacing:2.3px}.hud-heading{color:#20e7ff;font-size:clamp(11px,1.6vw,17px);font-weight:900;letter-spacing:1.8px;text-shadow:0 0 13px #00d9ff}.hud-speed{display:grid;line-height:.9}.hud-speed output{color:#f4fbff;font-size:clamp(40px,8vw,92px);font-weight:900;letter-spacing:-4px;text-shadow:0 0 25px #247dff8a}.hud-speed span{margin-top:7px;color:#5b6e90;font-size:8px;font-weight:850;letter-spacing:2px}.speed-presets{display:grid;grid-template-columns:repeat(3,1fr);gap:7px;width:min(100%,260px)}.speed-preset{padding:8px 6px;border-radius:999px;color:#8397b8;font-size:11px;font-weight:850}.speed-preset.active{border-color:#28d7ff9c;background:linear-gradient(135deg,#087fa4,#315bd8);box-shadow:0 0 18px #28d7ff4a;color:#fff}.hud-distance{display:flex;align-items:baseline;gap:8px;padding:7px 13px;border:1px solid #28d7ff3c;border-radius:999px;background:#031024c7}.hud-distance span{color:#637b9e;font-size:8px;font-weight:900;letter-spacing:1.2px}.hud-distance strong{color:#61f0ff;font-size:clamp(15px,2.4vw,25px);font-variant-numeric:tabular-nums}.hud-stop{min-width:110px;padding:8px 18px;border-color:#ff4f7080;border-radius:999px;background:linear-gradient(145deg,#7c1731,#3a1020);color:#ffdce4;font-size:12px;font-weight:850}.hud-stop small{display:inline;margin-left:5px}
    .hud-controls{width:min(100%,350px);margin:auto;grid-template-columns:repeat(3,minmax(0,1fr));grid-template-rows:repeat(3,auto);gap:clamp(4px,1vw,12px)}
    .hud-controls .hud-drive{width:min(100%,clamp(44px,min(8vw,18vh),96px));height:auto;aspect-ratio:1}
    .hud-controls [data-dir="f"]{grid-column:2;grid-row:1}.hud-controls [data-dir="l"]{grid-column:1;grid-row:2}.hud-controls [data-dir="r"]{grid-column:3;grid-row:2}.hud-controls [data-dir="b"]{grid-column:2;grid-row:3}
    body.drive-expanded{overflow:hidden}#manualPanel.drive-expanded{position:fixed;inset:0;z-index:20}#manualPanel:fullscreen,#manualPanel:-webkit-full-screen,#manualPanel.drive-expanded{width:100%;height:100vh;height:100dvh;min-height:0;padding:20px;border:0;border-radius:0;background:radial-gradient(circle at 50% 30%,#13284a,#050711 72%);overflow:auto}
    .mode-card{position:relative;display:grid;min-height:330px;margin-top:25px;padding:30px;place-items:center;overflow:hidden;border:1px solid var(--line);border-radius:22px;background:radial-gradient(circle at 50% 25%,#243b6638,transparent 45%),#070c18;text-align:center}.mode-card:before{position:absolute;inset:14px;border:1px solid #ffffff08;border-radius:16px;content:""}.mode-icon{display:grid;width:88px;height:88px;place-items:center;border:1px solid #28d7ff3b;border-radius:27px;background:linear-gradient(145deg,#172f51,#18152d);box-shadow:0 0 32px #28d7ff1f;font-size:44px}.mode-card strong{font-size:19px;letter-spacing:.1px}.mode-actions{position:relative;z-index:1;display:grid;grid-template-columns:1fr 1fr;gap:12px;width:min(100%,410px);margin-top:22px}.mode-on,.mode-off{min-height:50px;color:white;font-size:12px;font-weight:850;letter-spacing:1px}.mode-on{border-color:#39e58c55;background:linear-gradient(135deg,#128351,#16a86b);box-shadow:0 8px 24px #16a86b25}.mode-off{border-color:#ff4f7055;background:linear-gradient(135deg,#9d2944,#c53755);box-shadow:0 8px 24px #c5375525}footer{padding:22px 8px;color:#576784;font-size:11px;line-height:1.75;text-align:center}
    @media(max-width:720px){main{padding:18px 13px 100px}body.operation-active main{padding-bottom:190px}#operationBar{bottom:82px}.brand-copy{display:block}.online{margin-top:5px;width:max-content}.dashboard{display:block}.menu{position:fixed;z-index:10;right:10px;bottom:10px;left:10px;display:grid;grid-template-columns:repeat(4,minmax(0,1fr));padding:7px;border-color:#314462;background:#080d18f2;box-shadow:0 14px 45px #000c}.menu:before{display:none}.menu button{padding:10px 4px;text-align:center;font-size:11px}.panel{min-height:520px}}
    @media(max-width:470px){header{align-items:flex-start}.brand-mark{width:44px;height:44px}.online{display:none}.telemetry{width:100%;gap:10px;padding:10px 12px}.sensor-state{font-size:10px}.panel{padding:22px 14px}.home-overview{grid-template-columns:1fr auto}.home-overview span:nth-child(2){grid-column:1/-1}.home-overview button{grid-column:2;grid-row:1}.home-actions{grid-template-columns:1fr 1fr}.home-actions .danger{grid-column:1/-1}.mode-actions{grid-template-columns:1fr}.status-grid{grid-template-columns:1fr}.config-grid{grid-template-columns:1fr}.config-actions{align-items:stretch;flex-direction:column}.config-actions button{width:100%}}
    @media(orientation:landscape){#manualPanel:fullscreen,#manualPanel:-webkit-full-screen,#manualPanel.drive-expanded{display:block;padding:0;overflow:hidden;background:radial-gradient(circle at 50% 45%,#0d2a51,#030816 62%,#080922);box-shadow:inset 0 0 90px #000}#manualPanel:fullscreen .panel-head,#manualPanel:-webkit-full-screen .panel-head,#manualPanel.drive-expanded .panel-head,#manualPanel:fullscreen .portrait-manual,#manualPanel:-webkit-full-screen .portrait-manual,#manualPanel.drive-expanded .portrait-manual{display:none}#manualPanel:fullscreen .landscape-hud,#manualPanel:-webkit-full-screen .landscape-hud,#manualPanel.drive-expanded .landscape-hud{display:grid;width:100%;height:100%;min-height:0;grid-template-columns:minmax(0,1fr) minmax(0,.82fr) minmax(0,1fr);grid-template-rows:48px minmax(0,1fr);padding-right:env(safe-area-inset-right);padding-left:env(safe-area-inset-left)}}
    @media(orientation:landscape) and (max-height:500px){.landscape-hud{padding-bottom:16px}.hud-topbar{min-height:40px}.hud-bank{min-width:0;padding:6px}.hud-bank:before{inset:6% 1%;border-width:6px}.hud-bank:after{inset:11% 5%}.hud-controls{gap:clamp(4px,1vw,12px)}.hud-drive{width:clamp(44px,min(10vw,23vh),88px);height:clamp(44px,min(10vw,23vh),88px);font-size:clamp(20px,5vh,32px)}.hud-center{min-width:0;gap:3px;padding:3px}.hud-speed output{font-size:clamp(32px,15vh,58px)}.hud-speed span{margin-top:3px}.speed-presets{gap:3px}.speed-preset{padding:5px 2px}.hud-distance{padding:4px 8px}.hud-stop{min-width:0;padding:5px 10px}#manualPanel:fullscreen .landscape-hud,#manualPanel:-webkit-full-screen .landscape-hud,#manualPanel.drive-expanded .landscape-hud{grid-template-rows:40px minmax(0,1fr)}}
    @media(orientation:landscape) and (max-height:500px){.hud-controls .hud-drive{width:min(100%,clamp(42px,min(7vw,17vh),72px));height:auto}}
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
  <div id="actionToast" role="status" aria-live="polite" aria-atomic="true"><span id="actionToastText"></span><button id="actionToastClose" type="button" aria-label="Đóng thông báo" hidden>×</button></div>
  <div id="operationBar" role="status" aria-live="polite" hidden><div class="operation-copy"><strong id="operationText"></strong><small>Dừng xe trước khi gửi lệnh di chuyển khác.</small></div><button id="operationStop" type="button">⏹ Dừng xe</button></div>
  <main>
    <header><div class="brand"><div class="brand-mark">🤖</div><div class="brand-copy"><div><div class="eyebrow">Robot control system</div><h1>Ruby <span>Drive</span></h1><p class="subtitle">Điều khiển xe qua Wi-Fi</p></div><span class="online">SYSTEM ONLINE</span></div></div><button id="theme" class="theme" aria-label="Đổi độ tương phản">◐</button></header>
    <div class="telemetry" aria-live="polite">
      <div class="telemetry-icon">◉</div>
      <div class="telemetry-copy"><span class="distance-label">KHOẢNG CÁCH VẬT CẢN</span><strong id="distance" class="distance">-- cm</strong></div><span id="sensorState" class="sensor-state error">Đang chờ SR04…</span>
    </div>
    <div id="modeNotice" class="notice" role="status" aria-live="polite"></div>
    <div class="dashboard">
      <nav class="menu" aria-label="Chế độ điều khiển">
        <button id="manualTab" class="active">🏠 Home</button>
        <button id="hudTab">🚗 HUD</button>
        <button id="avoidTab">🛡️ Né vật cản</button>
        <button id="followTab">👣 Follow</button>
      </nav>
      <section class="panel-shell">
        <article id="manualPanel" class="panel active">
          <div class="panel-head"><div><h2>Home</h2><p class="subtitle">Theo dõi xe và dùng các chức năng quen thuộc từ Telegram.</p></div></div>
          <div class="portrait-manual">
            <div class="home-overview" aria-label="Thông tin thiết bị"><span>📶 <strong id="homeNetwork">Đang kiểm tra Wi-Fi…</strong></span><span>⏱ <strong id="homeUptime">--</strong></span><button id="refreshHome" type="button">Làm mới</button></div>
            <p id="homeUpdated" class="home-updated">Chưa có dữ liệu trạng thái.</p>
            <section class="home-section"><h3>Thao tác nhanh</h3><p>Dừng khẩn cấp có hiệu lực cả khi xe đang tự hành hoặc lái trong HUD.</p><div class="home-actions"><button id="emergencyStop" class="danger" type="button">⏹ Dừng xe</button><button id="wakeRobot" type="button">🎙 Đánh thức Ruby</button><button id="findRobot" type="button">📍 Tìm xe</button></div><p id="status" role="status" aria-live="polite"></p></section>
            <section class="home-section"><h3>Đi theo quãng đường</h3><p>Cần FC-03 đã hiệu chuẩn. Xe tự dừng ở đích; phía sau không có cảm biến vật cản.</p><div class="home-row"><label for="moveDistance">Quãng đường</label><input id="moveDistance" type="number" min="1" max="1000" step="1" value="30" inputmode="numeric"><span>cm</span></div><div class="home-row"><button id="moveForward" type="button">⬆ Tiến</button><button id="moveBackward" type="button">⬇ Lùi</button></div><p id="moveProgress" class="feature-state" aria-live="polite"></p></section>
            <section class="home-section"><h3>Chế độ tự hành</h3><p>Bật một chế độ tại một thời điểm; chỉnh khoảng cách ở phần cấu hình bên dưới.</p><div class="home-row"><button id="homeFollowToggle" type="button" aria-pressed="false">Bật Follow</button><button id="homeAvoidToggle" type="button" aria-pressed="false">Bật né vật cản</button></div></section>
            <section id="encoderPanel" hidden aria-live="polite">
              <div class="status-grid">
                <div class="status-card"><span>Xe đã đi · ước lượng từ lúc bật</span><strong id="encoderTravel">-- m</strong></div>
                <div class="status-card"><span>RPM đo được · trái / phải</span><strong id="encoderRpm">-- / --</strong></div>
                <div class="status-card"><span>Tổng xung · trái / phải</span><strong id="encoderPulses">-- / --</strong></div>
              </div>
              <p id="encoderWheels" class="encoder-note">Quãng đường bánh trái/phải: -- / -- m</p>
              <p id="encoderState" class="encoder-note">Đang chờ FC-03…</p>
            </section>
            <details class="home-section home-detail"><summary>Cấu hình &amp; công cụ nâng cao</summary>
            <section class="feature-card"><h3>Tốc độ dùng chung</h3><p>Áp dụng cho giọng nói, Telegram, tự hành và lệnh theo cm; không đổi tốc độ HUD. Đặt 0 để dừng motor.</p><div class="home-row"><label for="sharedSpeed">PWM</label><input id="sharedSpeed" type="number" min="0" max="100" step="1" value="75" inputmode="numeric"><span>%</span><button id="saveSharedSpeed" type="button">Áp dụng</button></div><p>Tốc độ đang đặt: <strong id="sharedSpeedState">--%</strong></p></section>
            <h3>Thông số tự hành</h3>
            <div class="config-grid">
              <div class="config-card"><label for="followDistanceSetting">Khoảng cách Follow (cm)</label><input id="followDistanceSetting" type="number" min="10" max="200" step="1" value="30"></div>
              <div class="config-card"><label for="obstacleDistanceSetting">Ngưỡng né vật cản (cm)</label><input id="obstacleDistanceSetting" type="number" min="10" max="100" step="1" value="20"></div>
            </div>
            <div class="config-actions"><button id="saveConfig" type="button">Lưu thông số</button><p class="config-hint">Áp dụng ngay trong firmware; giá trị sẽ về mặc định sau khi khởi động lại.</p></div>
            <section class="feature-card"><h3>Canh gác khi đứng yên</h3><p>SR04 theo dõi thay đổi khoảng cách. Khi phát hiện thay đổi, mắt robot, loa và Telegram cảnh báo; mỗi lần cách nhau ít nhất 30 giây.</p><button id="guardOn" type="button">Bật canh gác</button><button id="guardOff" type="button">Tắt</button> <span id="guardState" class="feature-state">Đang tắt</span></section>
            <section class="feature-card"><h3>Quét khoảng trống trước xe</h3><p>Xe quay từng bước để đo trái, giữa, phải, rồi quay về hướng ban đầu theo thời gian. Sơ đồ chỉ là ước lượng, không phải bản đồ chính xác.</p><button id="scanStart" type="button">Bắt đầu quét</button><button id="scanCancel" type="button">Dừng quét</button> <span id="scanState" class="feature-state">Chưa quét</span><div class="scan-map" aria-label="Sơ đồ khoảng cách ước lượng"><div class="scan-sector">Trái<strong id="scanLeft">--</strong></div><div class="scan-sector">Giữa<strong id="scanCenter">--</strong></div><div class="scan-sector">Phải<strong id="scanRight">--</strong></div></div><p id="scanSuggestion">Chỉ gợi ý hướng đi khi cả ba số đo hợp lệ.</p></section>
            <section class="feature-card"><h3>Lệnh nhiều bước</h3><p>Nhập F30,L90,S = tiến 30 cm, xoay trái 90° rồi dừng. B = lùi, R = xoay phải. Tối đa 5 bước; F/B dùng 1–50 cm, L/R dùng 1–180°. Góc được tính từ FC-03 và còn sai số do trượt bánh. Phía sau không có cảm biến vật cản. Kịch bản lưu trong RAM và mất khi khởi động lại.</p><input id="routeScript" type="text" maxlength="63" value="F30,L90,S" aria-label="Kịch bản di chuyển"><div class="home-row"><button id="routeSave" type="button">Lưu</button><button id="routeRun" type="button">Lưu &amp; chạy</button><button id="routeReplay" type="button">Chạy đã lưu</button><button id="routeStop" type="button">Dừng kịch bản</button></div><p id="routeState" class="feature-state">Chưa lưu</p></section>
            </details>
            <details class="home-section home-detail"><summary>Thông tin thiết bị &amp; trợ giúp</summary><p>Địa chỉ web: <strong id="homeIp">--</strong> · RAM trống: <strong id="homeRam">--</strong> KB.</p><p>Follow và né vật cản có tab riêng để xem trạng thái. Dùng HUD để lái liên tục; lệnh tiến/lùi theo cm và lộ trình tự dừng khi hoàn thành. Nút Dừng xe ngắt mọi chuyển động.</p></details>
          </div>
          <div class="hud-rotate" aria-live="polite"><div class="hud-rotate-icon">↻</div><strong>Xoay ngang màn hình để trải nghiệm.</strong><p id="hudRotateStatus">Đang kết nối điều khiển xe…</p><div class="hud-rotate-actions"><button id="hudRotateStop" class="mode-off" type="button">⏹ Dừng xe</button><button id="hudRotateConnect" type="button">Kết nối lại</button><button id="hudRotateExit" type="button">Quay lại</button></div></div>
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
    const speedValues = [50,75,100];
    const names = {s:'Đã dừng',f:'Tiến',b:'Lùi',l:'Xoay trái',r:'Xoay phải'};
    let session = 0, sequence = 0, epoch = 0, busy = false, pending = false;
    let direction = 's', releasing = false, heldPointer = null;
    let joystickX = 0, joystickY = 0;
    let joystickPointer = null;
    let telemetryBusy = false, autonomyBusy = false, homeBusy = false, stopVersion = 0;
    let currentMode = 'manual', autonomyVersion = 0;
    let recovering = false, wakeLock = null, wakePending = false, wakeUnavailable = false;
    let hudMode = false, warningEnabled = true, speed = 75;
    let warningDistanceCm = 30, obstacleDistanceCm = 20;
    let toastTimer, lastStatusTime = '';
    let operationActive = false, operationKind = '', operationText = '', operationStale = false;

    const modeNames = {manual:'Thủ công',follow:'Follow',avoid:'Né vật cản'};

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
      if (data.route_state === 'running') {
        const progress = data.distance_move_state === 'moving' &&
          data.distance_move_target_cm > 0 ? ' · ' +
          Math.round(data.distance_move_progress_cm || 0) + '/' +
          data.distance_move_target_cm + ' cm' : '';
        return ['route','Lộ trình · bước ' + data.route_step + '/' + data.route_steps + progress];
      }
      if (data.scan_state === 'scanning') return ['scan','Đang quét khoảng trống trước xe'];
      if (data.autonomy_mode === 'follow') return ['autonomy','Follow đang bật'];
      if (data.autonomy_mode === 'obstacle_avoidance')
        return ['autonomy','Né vật cản đang bật'];
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
          (({web:'web',telegram:'Telegram',voice:'giọng nói'})[data.control_owner] || 'nguồn khác')];
      }
      return null;
    }

    function finishedOperation(data) {
      if (operationKind === 'distance') {
        if (data.distance_move_state === 'completed') return ['Đã hoàn thành quãng đường.',true];
        if (data.distance_move_state === 'obstacle') return ['Xe đã dừng vì vật cản.',false];
        if (data.distance_move_state === 'no_pulses' ||
            data.distance_move_state === 'encoder_error') return ['Xe đã dừng vì lỗi FC-03.',false];
        if (data.distance_move_state === 'timeout') return ['Xe đã dừng vì hết thời gian.',false];
        return ['Lệnh di chuyển đã bị ngắt.',false];
      }
      if (operationKind === 'route')
        return data.route_state === 'complete' ? ['Lộ trình đã hoàn thành.',true] :
          ['Lộ trình đã bị ngắt.',false];
      if (operationKind === 'scan')
        return data.scan_state === 'complete' ? ['Đã quét xong.',true] : ['Quét đã bị ngắt.',false];
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
      for (const mode of ['manual','avoid','follow']) {
        $(mode + 'Panel').classList.toggle('active',mode === (name === 'hud' ? 'manual' : name));
        $(mode + 'Tab').classList.toggle('active',mode === name);
      }
      $('hudTab').classList.toggle('active',name === 'hud');
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

    function applyMode(mode) {
      // Status uses the firmware enum name; control requests use the shorter "avoid".
      if (mode === 'obstacle_avoidance') mode = 'avoid';
      currentMode = mode;
      const follow = mode === 'follow', avoid = mode === 'avoid';
      $('followStatus').textContent = follow ? 'ON' : 'OFF';
      $('followStatus').classList.toggle('on',follow);
      $('avoidStatus').textContent = avoid ? 'ON' : 'OFF';
      $('avoidStatus').classList.toggle('on',avoid);
      $('homeFollowToggle').textContent = follow ? 'Tắt Follow' : 'Bật Follow';
      $('homeFollowToggle').setAttribute('aria-pressed',String(follow));
      $('homeAvoidToggle').textContent = avoid ? 'Tắt né vật cản' : 'Bật né vật cản';
      $('homeAvoidToggle').setAttribute('aria-pressed',String(avoid));
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
      if (telemetryBusy) return false;
      telemetryBusy = true;
      const modeVersion = autonomyVersion;
      const controller = new AbortController();
      const timeout = setTimeout(() => controller.abort(),500);
      try {
        const response = await fetch('/api/status',{cache:'no-store',signal:controller.signal});
        if (!response.ok) throw new Error(String(response.status));
        const data = await response.json();
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
        $('homeNetwork').textContent = data.wifi_connected === false ? 'Wi-Fi đã ngắt' :
          Number.isFinite(data.wifi_rssi) ? 'Wi-Fi ' + data.wifi_rssi + ' dBm' : 'Wi-Fi đã kết nối';
        $('homeUptime').textContent = Number.isFinite(data.uptime_seconds)
          ? Math.floor(data.uptime_seconds / 3600) + ' giờ ' +
            Math.floor(data.uptime_seconds / 60 % 60) + ' phút' : '--';
        $('homeIp').textContent = data.ip_address || '--';
        $('homeRam').textContent = Number.isFinite(data.free_heap_kb) ? data.free_heap_kb : '--';
        $('sharedSpeedState').textContent = Number.isFinite(data.saved_speed_percent)
          ? data.saved_speed_percent + '%' : '--%';
        if (document.activeElement !== $('sharedSpeed') &&
            Number.isFinite(data.saved_speed_percent))
          $('sharedSpeed').value = String(data.saved_speed_percent);
        const moveState = ({pending:'Đang chuẩn bị',moving:'Đang di chuyển',
          completed:'Đã hoàn thành',interrupted:'Đã ngắt',timeout:'Hết thời gian',
          no_pulses:'Không có xung FC-03',encoder_error:'Lỗi FC-03',
          obstacle:'Đã dừng vì vật cản'})[data.distance_move_state] || '';
        $('moveProgress').textContent = moveState && data.distance_move_target_cm > 0
          ? moveState + ' · ' + Math.round(data.distance_move_progress_cm || 0) +
            '/' + data.distance_move_target_cm + ' cm' : moveState;
        $('guardState').textContent = data.guard_enabled ? 'Đang bật' : 'Đang tắt';
        const routeLabel = ({empty:'Chưa lưu',saved:'Đã lưu',running:'Đang chạy',
          complete:'Đã hoàn thành',interrupted:'Đã ngắt'})[data.route_state] || 'Chưa lưu';
        $('routeState').textContent = routeLabel + (data.route_state === 'running'
          ? ' · bước ' + data.route_step + '/' + data.route_steps : '');
        if (document.activeElement !== $('routeScript') && data.route_script)
          $('routeScript').value = data.route_script;
        $('scanState').textContent = ({idle:'Chưa quét',scanning:'Đang quét…',
          complete:'Đã quét xong',interrupted:'Đã dừng'})[data.scan_state] || 'Chưa quét';
        const scanValues = [data.scan_left_cm,data.scan_center_cm,data.scan_right_cm];
        ['scanLeft','scanCenter','scanRight'].forEach((id,index) => {
          $(id).textContent = Number.isFinite(scanValues[index]) && scanValues[index] > 0
            ? scanValues[index] + ' cm' : '--';
        });
        const clear = scanValues.map(value => Number.isFinite(value) && value >= 30 ? value : -1);
        const widest = Math.max(...clear);
        const suggested = clear[1] === widest ? 1 : clear.indexOf(widest);
        $('scanSuggestion').textContent = data.scan_state !== 'complete'
          ? 'Chờ quét xong để gợi ý hướng.'
          : scanValues.some(value => !Number.isFinite(value) || value <= 0)
            ? 'Có hướng không đo được SR04; chưa thể gợi ý đáng tin cậy.'
          : widest < 0 ? 'Chưa thấy hướng trống đủ 30 cm; không nên tiến.'
          : 'Gợi ý hướng ' + ['trái','thẳng','phải'][suggested] +
            ' (ước lượng theo SR04 và thời gian quay).';
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
        if ($('followDistanceSetting') && document.activeElement !== $('followDistanceSetting'))
          $('followDistanceSetting').value = String(data.follow_distance_cm);
        if ($('obstacleDistanceSetting') && document.activeElement !== $('obstacleDistanceSetting'))
          $('obstacleDistanceSetting').value = String(data.obstacle_distance_cm);
        render();
        return true;
      } catch (_) {
        renderEncoder(null,true);
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
      $('hudStop').disabled = !session || releasing;
      const actionBusy = homeBusy || autonomyBusy || busy || releasing;
      for (const id of ['followOff','avoidOff','wakeRobot','findRobot','saveSharedSpeed',
                        'guardOff','scanCancel'])
        $(id).disabled = actionBusy;
      for (const id of ['followOn','avoidOn','moveForward','moveBackward','guardOn',
                        'scanStart','routeSave','routeRun','routeReplay'])
        $(id).disabled = actionBusy || operationActive;
      $('homeFollowToggle').disabled = actionBusy || (operationActive && currentMode !== 'follow');
      $('homeAvoidToggle').disabled = actionBusy || (operationActive && currentMode !== 'avoid');
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

    async function releaseForAction() {
      if (!session) return;
      heldPointer = null; resetJoystick();
      releasing = true; direction = 'release'; pending = true;
      await pump();
      if (session) throw new Error('release failed');
    }

    async function reassertStop(version) {
      if (version !== stopVersion)
        await post('/api/action',JSON.stringify({action:'stop'})).catch(() => {});
    }

    async function homeAction(action, fields, success, failure) {
      const stopping = action === 'stop';
      if (!stopping && (homeBusy || autonomyBusy || busy || releasing)) return;
      if (stopping) ++stopVersion;
      else { homeBusy = true; render(); }
      const version = stopVersion;
      try {
        const result = await post('/api/action',JSON.stringify({action,...fields}));
        if (version === stopVersion) {
          if (stopping && session) park('Đã dừng xe và kết thúc phiên HUD.');
          $('status').textContent = result.already_listening
            ? 'Ruby đang lắng nghe. Hãy nói vào mic của Ruby.' : success;
          if (stopping || (action === 'speed' && fields.speed === 0)) {
            operationActive = false;
            operationKind = '';
            render();
          } else if (action === 'forward' || action === 'backward') {
            showOperation('Đã nhận lệnh ' + (action === 'forward' ? 'tiến ' : 'lùi ') +
              fields.distance_cm + ' cm','distance');
          }
          notifyAction($('status').textContent,true);
        }
        await refreshTelemetry();
      } catch (_) {
        if (version === stopVersion) {
          $('status').textContent = failure;
          notifyAction(failure,false);
        }
      } finally {
        if (!stopping) {
          await reassertStop(version);
          homeBusy = false;
          render();
        }
      }
    }

    function moveDistance(action) {
      if (operationActive) {
        notifyAction('Dừng xe trước khi gửi lệnh di chuyển khác.',false);
        return;
      }
      const distance = Number($('moveDistance').value);
      if (!Number.isInteger(distance) || distance < 1 || distance > 1000) {
        $('status').textContent = 'Nhập quãng đường nguyên từ 1 đến 1000 cm.';
        notifyAction($('status').textContent,false);
        return;
      }
      void homeAction(action,{distance_cm:distance},'Đã nhận lệnh đi ' + distance + ' cm.',
        'Chưa thể di chuyển. Kiểm tra FC-03, tốc độ PWM và trạng thái xe.');
    }

    function saveSharedSpeed() {
      const value = Number($('sharedSpeed').value);
      if ($('sharedSpeed').value.trim() === '' || !Number.isInteger(value) ||
          value < 0 || value > 100) {
        $('status').textContent = 'Nhập tốc độ PWM nguyên từ 0 đến 100%.';
        notifyAction($('status').textContent,false);
        return;
      }
      void homeAction('speed',{speed:value},'Đã đặt tốc độ dùng chung ' + value + '%.',
        'Chưa đặt được tốc độ. Hãy rời HUD và kiểm tra motor.');
    }

    async function setAutonomy(mode, enabled) {
      if (homeBusy || autonomyBusy || busy || releasing) return;
      if (enabled && operationActive) {
        notifyAction('Dừng xe trước khi bật chế độ tự hành khác.',false);
        return;
      }
      const version = stopVersion;
      autonomyBusy = true;
      render();
      $('modeNotice').textContent = 'Đang chuyển chế độ…';
      try {
        if (session) await releaseForAction();
        await post('/api/autonomy',JSON.stringify({mode,enabled}));
        const wasMode = currentMode === mode;
        ++autonomyVersion;
        applyMode(enabled ? mode : currentMode === mode ? 'manual' : currentMode);
        if (enabled) showOperation(mode === 'follow' ? 'Follow đang bật' :
          'Né vật cản đang bật','autonomy');
        else if (wasMode && operationKind === 'autonomy') {
          operationActive = false;
          render();
        }
        $('modeNotice').textContent = enabled
          ? 'Đã bật ' + modeNames[mode] + '.' : 'Đã tắt ' + modeNames[mode] + '.';
        if (version === stopVersion) notifyAction($('modeNotice').textContent,true);
        await refreshTelemetry();
      } catch (_) {
        $('modeNotice').textContent =
          'Không thể đổi chế độ. Kiểm tra tốc độ PWM, motor và kết nối rồi thử lại.';
        if (version === stopVersion) notifyAction($('modeNotice').textContent,false);
      } finally {
        await reassertStop(version);
        autonomyBusy = false;
        render();
      }
    }

    async function setFeature(mode, enabled) {
      if (homeBusy || autonomyBusy || busy || releasing) return;
      if (enabled && operationActive) {
        notifyAction('Dừng xe trước khi bắt đầu thao tác mới.',false);
        return;
      }
      const version = stopVersion;
      autonomyBusy = true;
      render();
      $('modeNotice').textContent = 'Đang cập nhật chế độ…';
      try {
        if (session) await releaseForAction();
        await post('/api/autonomy',JSON.stringify({mode,enabled}));
        if (mode === 'scan' && enabled) showOperation('Đã nhận lệnh quét','scan');
        else if (mode === 'scan' && operationKind === 'scan') {
          operationActive = false;
          render();
        }
        $('modeNotice').textContent = mode === 'guard'
          ? (enabled ? 'Đã bật canh gác.' : 'Đã tắt canh gác.')
          : (enabled ? 'Đã bắt đầu quét.' : 'Đã dừng quét.');
        if (version === stopVersion) notifyAction($('modeNotice').textContent,true);
        await refreshTelemetry();
      } catch (_) {
        $('modeNotice').textContent = 'Không thể đổi chế độ. Xe phải đứng yên, không ở phiên lái web hoặc chế độ tự hành.';
        if (version === stopVersion) notifyAction($('modeNotice').textContent,false);
      } finally { await reassertStop(version); autonomyBusy = false; render(); }
    }

    async function findRobot() {
      if (homeBusy || busy || releasing || autonomyBusy) return;
      homeBusy = true;
      render();
      try {
        if (session) await releaseForAction();
        await post('/api/find');
        $('status').textContent = 'Ruby đang phát âm báo và nhấp nháy màn hình hiển thị IP.';
        notifyAction($('status').textContent,true);
      } catch (_) {
        $('status').textContent = 'Chưa thể tìm xe: xe đang hoạt động hoặc vừa được gọi. Hãy thử lại sau vài giây.';
        notifyAction($('status').textContent,false);
      } finally { homeBusy = false; render(); }
    }

    async function routeAction(action) {
      if (action !== 'stop' && (homeBusy || autonomyBusy || busy || releasing)) return;
      if (action !== 'stop' && operationActive) {
        notifyAction('Dừng xe trước khi gửi lộ trình khác.',false);
        return;
      }
      const version = stopVersion;
      if (action !== 'stop') { autonomyBusy = true; render(); }
      try {
        if ((action === 'run' || action === 'runSaved') && session)
          await releaseForAction();
        const body = action === 'runSaved' ? {action:'run'} :
          action === 'save' || action === 'run'
            ? {action,script:$('routeScript').value.trim()} : {action};
        await post('/api/route',JSON.stringify(body));
        if (action === 'run' || action === 'runSaved') showOperation('Đã nhận lộ trình','route');
        else if (action === 'stop' && operationKind === 'route') {
          operationActive = false;
          render();
        }
        $('modeNotice').textContent = action === 'save' ? 'Đã lưu kịch bản trong RAM.' :
          action === 'stop' ? 'Đã dừng kịch bản.' : 'Đang chạy kịch bản.';
        if (version === stopVersion) notifyAction($('modeNotice').textContent,true);
        await refreshTelemetry();
      } catch (_) {
        $('modeNotice').textContent = action === 'save'
          ? 'Kịch bản sai. Dùng F/B/L/R với 1–50 cm, tối đa 5 bước.'
          : action === 'stop' ? 'Không thể dừng kịch bản. Hãy dùng nút Dừng xe.'
          : 'Không thể chạy kịch bản. Xe cần đứng yên và FC-03 đã hiệu chuẩn.';
        if (version === stopVersion) notifyAction($('modeNotice').textContent,false);
      } finally {
        await reassertStop(version);
        if (action !== 'stop') autonomyBusy = false;
        render();
      }
    }

    async function saveConfiguration() {
      if (homeBusy || autonomyBusy || busy || releasing) return;
      const followDistance = Number($('followDistanceSetting').value);
      const obstacleDistance = Number($('obstacleDistanceSetting').value);
      if (!Number.isInteger(followDistance) || followDistance < 10 || followDistance > 200 ||
          !Number.isInteger(obstacleDistance) || obstacleDistance < 10 || obstacleDistance > 100) {
        $('status').textContent = 'Khoảng cách Follow phải từ 10–200 cm; ngưỡng né vật cản từ 10–100 cm.';
        notifyAction($('status').textContent,false);
        return;
      }
      $('saveConfig').disabled = true;
      $('status').textContent = 'Đang lưu thông số…';
      try {
        await post('/api/config',JSON.stringify({
          follow_distance_cm:followDistance,obstacle_distance_cm:obstacleDistance
        }));
        obstacleDistanceCm = obstacleDistance;
        warningDistanceCm = Math.ceil(obstacleDistance * Math.max(speed,50) / 50);
        $('status').textContent = 'Đã áp dụng thông số trong firmware.';
        notifyAction($('status').textContent,true);
        await refreshTelemetry();
      } catch (_) {
        $('status').textContent = 'Không lưu được thông số. Kiểm tra kết nối rồi thử lại.';
        notifyAction($('status').textContent,false);
      } finally { $('saveConfig').disabled = false; }
    }

    async function claimControl() {
      if (!hudMode || session || busy || autonomyBusy || document.hidden) return;
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
        ++autonomyVersion;
        applyMode('manual');
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
    $('homeFollowToggle').addEventListener('click',() => {
      void setAutonomy('follow',currentMode !== 'follow');
    });
    $('homeAvoidToggle').addEventListener('click',() => {
      void setAutonomy('avoid',currentMode !== 'avoid');
    });
    const stopRobot = () => {
      void homeAction('stop',{},'Đã dừng xe và tắt tự hành.',
        'Không gửi được lệnh dừng. Hãy kiểm tra xe ngay.');
    };
    $('emergencyStop').addEventListener('click',stopRobot);
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
    $('saveSharedSpeed').addEventListener('click',saveSharedSpeed);
    $('sharedSpeed').addEventListener('keydown',event => {
      if (event.key === 'Enter') saveSharedSpeed();
    });
    $('refreshHome').addEventListener('click',async () => {
      if (telemetryBusy) return;
      $('refreshHome').disabled = true;
      const refreshed = await refreshTelemetry();
      notifyAction(refreshed ? 'Đã cập nhật trạng thái xe.' :
        'Chưa lấy được trạng thái mới. Hãy thử lại sau.',refreshed);
      $('refreshHome').disabled = false;
    });
    $('guardOn').addEventListener('click',() => { void setFeature('guard',true); });
    $('guardOff').addEventListener('click',() => { void setFeature('guard',false); });
    $('findRobot').addEventListener('click',() => { void findRobot(); });
    $('scanStart').addEventListener('click',() => { void setFeature('scan',true); });
    $('scanCancel').addEventListener('click',() => { void setFeature('scan',false); });
    $('routeSave').addEventListener('click',() => { void routeAction('save'); });
    $('routeRun').addEventListener('click',() => { void routeAction('run'); });
    $('routeReplay').addEventListener('click',() => { void routeAction('runSaved'); });
    $('routeStop').addEventListener('click',() => { void routeAction('stop'); });
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
    setInterval(() => { if (hudMode && session && !releasing) { pending = true; void pump(); } },150);
    $('theme').addEventListener('click', () => document.body.classList.toggle('ultra'));
    render();
  </script>
</body>
</html>
)html";
