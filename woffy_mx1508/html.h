/*
 * Woffy Control UI — D-pad + Test Buttons
 */

#ifndef HTML_H
#define HTML_H

const char CONTROL_HTML[] PROGMEM = R"raw(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1, user-scalable=no">
  <title>Woffy Control</title>
  <style>
    *{box-sizing:border-box;margin:0;padding:0}
    body{font-family:system-ui,sans-serif;background:#1a1a2e;color:#eee;
      min-height:100vh;display:flex;flex-direction:column;align-items:center;
      justify-content:center;padding:1rem;gap:1.2rem}
    h1{font-size:1.4rem;color:#00d9ff}
    .pad{display:grid;grid-template-columns:repeat(3,1fr);gap:0.5rem;width:240px}
    .pad button{width:100%;aspect-ratio:1;border:none;border-radius:12px;
      font-size:1.4rem;font-weight:bold;background:#16213e;color:#fff;
      touch-action:manipulation;-webkit-tap-highlight-color:transparent}
    .pad button:active{background:#0f3460;transform:scale(0.95)}
    .pad .empty{background:transparent;pointer-events:none}
    .pad .stop{background:#e94560;font-size:0.9rem}
    .tests{display:flex;flex-direction:column;gap:0.4rem;width:240px}
    .tests button{border:none;border-radius:10px;padding:0.7rem;
      font-size:0.85rem;font-weight:600;color:#fff;
      touch-action:manipulation;-webkit-tap-highlight-color:transparent}
    .tests button:active{transform:scale(0.97);filter:brightness(0.85)}
    .tests button:disabled{opacity:0.5}
    .t-all{background:#e94560}
    .t1{background:#0f3460}
    .t2{background:#0f3460}
    .t3{background:#0f3460}
    .t4{background:#533483}
    .t5{background:#0f3460}
    .status{font-size:0.8rem;color:#888;min-height:1.2rem}
    .slider-box{width:240px;text-align:center}
    .slider-box label{font-size:0.85rem;color:#aaa}
    .slider-box span{color:#00d9ff;font-weight:bold;font-size:1rem}
    .slider-box input[type=range]{-webkit-appearance:none;width:100%;height:8px;
      border-radius:4px;background:#16213e;outline:none;margin:0.5rem 0}
    .slider-box input[type=range]::-webkit-slider-thumb{-webkit-appearance:none;
      width:28px;height:28px;border-radius:50%;background:#00d9ff;cursor:pointer}
  </style>
</head>
<body>
  <h1>Woffy Control</h1>

  <div class="slider-box">
    <label>Speed: PWM <span id="sv">180</span> | ~<span id="rpm">212</span> RPM</label>
    <input type="range" id="spd" min="60" max="255" value="180">
  </div>

  <div class="pad">
    <div class="empty"></div>
    <button data-cmd="fwd">&#9650;</button>
    <div class="empty"></div>
    <button data-cmd="left">&#9664;</button>
    <button class="stop" data-cmd="stop">STOP</button>
    <button data-cmd="right">&#9654;</button>
    <div class="empty"></div>
    <button data-cmd="bwd">&#9660;</button>
    <div class="empty"></div>
  </div>

  <div class="tests">
    <button class="t-all" onclick="runTest('test')">Run All Tests</button>
    <button class="t4" onclick="runTest('test4')">Test 4: Motor Check</button>
    <button class="t1" onclick="runTest('test1')">Test 1: Individual Spin</button>
    <button class="t2" onclick="runTest('test2')">Test 2: Directions</button>
    <button class="t3" onclick="runTest('test3')">Test 3: Speed Sweep</button>
    <button class="t5" onclick="runTest('test5')">Test 5: Figure-8</button>
  </div>

  <div class="status" id="st"></div>

  <script>
    var st=document.getElementById('st');
    var slider=document.getElementById('spd');
    var svEl=document.getElementById('sv');
    var rpmEl=document.getElementById('rpm');
    var pressed={};
    var lastCmd='';

    slider.addEventListener('input',function(){
      svEl.textContent=slider.value;
      rpmEl.textContent=Math.round(slider.value*300/255);
    });

    function send(cmd){
      if(cmd===lastCmd) return;
      lastCmd=cmd;
      fetch('/cmd?cmd='+cmd+'&speed='+slider.value);
    }

    function update(){
      var f=pressed.fwd||false;
      var b=pressed.bwd||false;
      var l=pressed.left||false;
      var r=pressed.right||false;

      if(f&&l)      send('cl');
      else if(f&&r) send('cr');
      else if(b&&l) send('bkl');
      else if(b&&r) send('bkr');
      else if(f)    send('fwd');
      else if(b)    send('bwd');
      else if(l)    send('left');
      else if(r)    send('right');
      else          send('stop');
    }

    document.querySelectorAll('.pad button[data-cmd]').forEach(function(b){
      var cmd=b.dataset.cmd;
      if(cmd==='stop'){
        b.addEventListener('touchstart',function(e){e.preventDefault();pressed={};update();});
        b.addEventListener('mousedown',function(){pressed={};update();});
        return;
      }
      b.addEventListener('touchstart',function(e){e.preventDefault();pressed[cmd]=true;update();});
      b.addEventListener('touchend',function(e){e.preventDefault();delete pressed[cmd];update();});
      b.addEventListener('touchcancel',function(e){e.preventDefault();delete pressed[cmd];update();});
      b.addEventListener('mousedown',function(){pressed[cmd]=true;update();});
      b.addEventListener('mouseup',function(){delete pressed[cmd];update();});
      b.addEventListener('mouseleave',function(){delete pressed[cmd];update();});
    });

    function runTest(t){
      var btns=document.querySelectorAll('.tests button');
      btns.forEach(function(b){b.disabled=true;});
      st.textContent='Running '+t+'...';
      fetch('/cmd?cmd='+t).then(function(r){return r.text();}).then(function(txt){
        st.textContent=t+': '+txt;
        btns.forEach(function(b){b.disabled=false;});
      }).catch(function(){
        st.textContent=t+' failed';
        btns.forEach(function(b){b.disabled=false;});
      });
    }
  </script>
</body>
</html>
)raw";

#endif
