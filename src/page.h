#pragma once
#include <pgmspace.h>

// Ember-theme control UI — self-contained, served from PROGMEM.
// EN/RU language toggle stored in localStorage.
// Sliders debounce at 120 ms before sending to the ESP.
// Initial values are injected by handleRoot() via a trailing <script> chunk
// so sliders show correct state immediately without waiting for pull().
static const char PAGE[] PROGMEM = R"HTML(<!doctype html><html lang=en><head>
<meta charset=utf-8>
<meta name=viewport content="width=device-width,initial-scale=1">
<meta name=theme-color content="#0a0503"><title>Ember</title><style>
:root{--b:60;--g:calc(var(--b)/100)}
*{margin:0;padding:0;box-sizing:border-box;-webkit-tap-highlight-color:transparent}
button,input[type=range]{touch-action:manipulation}
html,body{min-height:100%}
body{font-family:'Helvetica Neue',Helvetica,Arial,sans-serif;color:#f6d9b0;
 display:flex;align-items:center;justify-content:center;overflow-x:hidden;overflow-y:auto;padding:40px 0;
 background:radial-gradient(120% 90% at 50% 118%,
  rgba(255,120,20,calc(.55*var(--g))) 0%,rgba(180,40,8,calc(.32*var(--g))) 32%,
  rgba(20,6,2,0) 66%),#0a0503}
.amb{position:fixed;inset:0;z-index:1;pointer-events:none;animation:fl 3.2s ease-in-out infinite;
 background:radial-gradient(60% 50% at 50% 116%,rgba(255,110,25,calc(.40*var(--g))),transparent 70%)}
@keyframes fl{0%,100%{opacity:.85}45%{opacity:1}70%{opacity:.78}}
.wrap{width:min(92vw,420px);text-align:center;position:relative;z-index:2}
.kick{font-size:12px;letter-spacing:.42em;text-transform:uppercase;color:#c8743a;opacity:.85;margin-bottom:6px}
h1{font-size:13px;letter-spacing:.3em;text-transform:uppercase;font-weight:600;color:#8a4a22;margin-bottom:12px;margin-top:24px}
.val{font-size:72px;font-weight:200;line-height:1;letter-spacing:-.04em;
 background:linear-gradient(180deg,#fff3d6,#ffb14a 55%,#ff6a18);-webkit-background-clip:text;
 background-clip:text;color:transparent;transition:filter .25s;
 filter:drop-shadow(0 0 calc(6px + 20px*var(--g)) rgba(255,140,40,calc(.4 + .5*var(--g))))}
.bar{margin:24px 0;-webkit-appearance:none;appearance:none;width:100%;height:14px;border-radius:9px;
 outline:none;box-shadow:inset 0 1px 3px rgba(0,0,0,.7);
 background:linear-gradient(90deg,#2a0d04,#7a1f06 22%,#d6510c 55%,#ff8a1f 78%,#ffe7b8)}
.bar::-webkit-slider-thumb{-webkit-appearance:none;width:48px;height:48px;border-radius:50%;cursor:pointer;
 background:radial-gradient(circle at 38% 32%,#fff,#ffae45 40%,#ff5e10 75%,#7a1f00);
 box-shadow:0 0 16px rgba(255,140,40,.9),0 2px 6px rgba(0,0,0,.6)}
.bar::-moz-range-thumb{width:48px;height:48px;border:0;border-radius:50%;
 background:radial-gradient(circle at 38% 32%,#fff,#ffae45 40%,#ff5e10 75%,#7a1f00);
 box-shadow:0 0 16px rgba(255,140,40,.9)}
.desc{font-size:12px;color:#a85a22;margin-top:-14px;margin-bottom:24px;letter-spacing:0.05em;line-height:1.4;opacity:0.8;font-weight:400;}
.reset{margin-top:10px;padding:14px 24px;background:none;border:1px solid #7a3f16;color:#c8743a;border-radius:24px;cursor:pointer;font-size:13px;text-transform:uppercase;letter-spacing:0.1em;transition:all 0.2s;}
.reset:active{background:#7a3f16;color:#fff2dd;}
.lang{position:absolute;top:20px;right:20px;display:flex;gap:4px;z-index:3;}
.lbtn{background:none;border:1px solid #7a3f16;color:#c8743a;padding:0 12px;border-radius:4px;cursor:pointer;font-size:12px;font-weight:bold;transition:all 0.2s;display:inline-flex;align-items:center;min-height:44px;}
.lbtn.act{background:#7a3f16;color:#fff2dd;}
.stat{margin-top:20px;text-align:center;font-size:13px;color:#a85a22;letter-spacing:0.1em;opacity:0.8;font-weight:400;}
.stat span{color:#ffb14a;font-weight:bold;}
.ibtn{position:absolute;top:20px;left:20px;background:none;border:1px solid #7a3f16;color:#c8743a;padding:0 16px;border-radius:4px;cursor:pointer;font-size:14px;font-weight:bold;transition:all 0.2s;z-index:3;display:inline-flex;align-items:center;min-height:44px;min-width:44px;}
.ibtn:active{background:#7a3f16;color:#fff2dd;}
.mod{display:none;position:fixed;inset:0;background:rgba(10,5,3,0.95);z-index:10;flex-direction:column;padding:30px;overflow-y:auto;color:#f6d9b0;text-align:left;font-size:14px;line-height:1.5;}
.mod.show{display:flex;}
.mcls{align-self:flex-end;background:none;border:none;color:#ffb14a;font-size:28px;cursor:pointer;margin-bottom:10px;min-width:44px;min-height:44px;display:flex;align-items:center;justify-content:center;}
.mt{font-size:16px;font-weight:bold;color:#ff6a18;margin-top:15px;margin-bottom:5px;letter-spacing:.1em;text-transform:uppercase;}
.md{margin-bottom:15px;opacity:0.85;}
body.off .val,body.off .amb{filter:grayscale(.5);opacity:.4}
</style></head><body><div class=amb></div>
<button id=ibtn class=ibtn>?</button>
<div id=mod class=mod>
<button id=mcls class=mcls>&times;</button>
<div class=mt id=mt1>Brightness</div><div class=md id=md1></div>
<div class=mt id=mt2>Contrast</div><div class=md id=md2></div>
<div class=mt id=mt3>Cooling</div><div class=md id=md3></div>
<div class=mt id=mt4>Sparking</div><div class=md id=md4></div>
<div class=mt id=mt5>Reset to Default</div><div class=md id=md5></div>
<div class=mt id=mt6>Check for Update</div><div class=md id=md6></div>
<div class=mt id=mt7>Reset WiFi</div><div class=md id=md7></div>
<div class=mt id=mt8>Power</div><div class=md id=md8></div>
</div>
<div class=lang><button id=len class="lbtn act">EN</button><button id=lru class=lbtn>RU</button></div>
<div class=wrap>
<div class=kick>Fire Lamp</div>
<h1 id=lb>Brightness</h1><div class=val id=vb>--</div>
<input class=bar id=sb type=range min=0 max=100 value=100>
<div class=desc id=db>Controls the overall light output of the lamp.</div>
<h1 id=lc>Contrast</h1><div class=val id=vc>--</div>
<input class=bar id=sc type=range min=0 max=100 value=50>
<div class=desc id=dc>Adjusts the intensity of reds and oranges.</div>
<h1 id=lco>Cooling</h1><div class=val id=vco>--</div>
<input class=bar id=sco type=range min=20 max=150 value=70>
<div class=desc id=dco>Lower = taller flames.</div>
<h1 id=lsp>Sparking</h1><div class=val id=vsp>--</div>
<input class=bar id=ssp type=range min=0 max=255 value=95>
<div class=desc id=dsp>Higher = hotter base.</div>
<button class=reset id=rst>Reset to Default</button>
<button class=reset id=chk style="margin-top:10px;border-color:#1e3a8a;color:#60a5fa">Check for Update</button>
<div id=vinfo style="font-size:11px;color:#888;margin-top:6px;text-align:center"></div>
<button class=reset id=rwifi style="margin-top:10px;border-color:#7a1616;color:#f87171">Reset WiFi</button>
<div class=stat><span id=lw>Power:</span> <span id=vw>0.0</span> W</div>
</div><script>
var vb=document.getElementById('vb'),sb=document.getElementById('sb');
var vc=document.getElementById('vc'),sc=document.getElementById('sc');
var vco=document.getElementById('vco'),sco=document.getElementById('sco');
var vsp=document.getElementById('vsp'),ssp=document.getElementById('ssp');
var R=document.documentElement,t1,t2,t4,t5;
var xf=function(u){return fetch(u,{headers:{'X-Requested-With':'firelamp'}});};
var ru=(localStorage.getItem('lang')==='ru')||(!localStorage.getItem('lang')&&navigator.language.startsWith('ru'));
function ul(){
 document.getElementById('len').classList.toggle('act',!ru);
 document.getElementById('lru').classList.toggle('act',ru);
 document.getElementById('lb').textContent=ru?'Яркость':'Brightness';
 document.getElementById('db').textContent=ru?'Управляет общей яркостью лампы.':'Controls the overall light output of the lamp.';
 document.getElementById('lc').textContent=ru?'Контрастность':'Contrast';
 document.getElementById('dc').textContent=ru?'Насыщенность красных и оранжевых оттенков.':'Adjusts the intensity of reds and oranges.';
 document.getElementById('lco').textContent=ru?'Охлаждение':'Cooling';
 document.getElementById('dco').textContent=ru?'Меньше = выше пламя.':'Lower = taller flames.';
 document.getElementById('lsp').textContent=ru?'Искры':'Sparking';
 document.getElementById('dsp').textContent=ru?'Больше = горячее основание.':'Higher = hotter base.';
 document.getElementById('rst').textContent=ru?'По умолчанию':'Reset to Default';
 document.getElementById('chk').textContent=ru?'Проверить обновления':'Check for Update';
 document.getElementById('rwifi').textContent=ru?'Сменить сеть Wi-Fi':'Reset WiFi';
 document.getElementById('lw').textContent=ru?'Потребление:':'Power:';
 document.getElementById('mt1').textContent=ru?'Яркость (Масштаб)':'Brightness (Scale)';
 document.getElementById('md1').textContent=ru?'Управляет общей яркостью лампы. Гамма-коррекция 2.2 обеспечивает равномерное восприятие по всей шкале. Не меняет физику пламени.':'Controls the overall light output. A gamma-2.2 curve makes the slider feel perceptually even across its range. Does not change the flame physics.';
 document.getElementById('mt2').textContent=ru?'Контрастность (Цвет)':'Contrast (Palette)';
 document.getElementById('md2').textContent=ru?'Не влияет на физику. Сдвигает цвета: низкая контрастность дает больше желтого/белого, высокая оставляет только глубокий красный.':'Does not affect physics. Shifts the colors: low contrast allows more yellow/white, high contrast forces deep reds.';
 document.getElementById('mt3').textContent=ru?'Охлаждение (Высота)':'Cooling (Height)';
 document.getElementById('md3').textContent=ru?'Управляет скоростью затухания искр по мере подъема. Меньше значение = выше пламя. Больше значение = короткие искры.':'Dictates how quickly sparks die out as they travel up. Lower value = taller flames. Higher value = short embers.';
 document.getElementById('mt4').textContent=ru?'Искры (Бензин)':'Sparking (Ignition)';
 document.getElementById('md4').textContent=ru?'Управляет хаосом у основания. Высокое значение = сплошной, ревущий белый/желтый жар. Низкое = спокойное тление.':'Dictates chaos at the base. Higher value = a solid, roaring white/yellow inferno. Lower value = calm smoldering.';
 document.getElementById('mt5').textContent=ru?'По умолчанию':'Reset to Default';
 document.getElementById('md5').textContent=ru?'Восстанавливает все ползунки к заводским значениям: яркость 100, контрастность 50, охлаждение 45, искры 36.':'Restores all sliders to factory defaults: brightness 100, contrast 50, cooling 45, sparking 36.';
 document.getElementById('mt6').textContent=ru?'Проверить обновления':'Check for Update';
 document.getElementById('md6').textContent=ru?'Сравнивает текущую прошивку с последней сборкой на GitHub. При наличии обновления предложит установить его — лампа перезагрузится автоматически.':'Compares current firmware with the latest build on GitHub. If an update is available you can install it — the lamp reboots automatically.';
 document.getElementById('mt7').textContent=ru?'Сменить сеть Wi-Fi':'Reset WiFi';
 document.getElementById('md7').textContent=ru?'Удаляет сохранённые данные сети и перезагружает лампу в режим настройки. Подключитесь к точке доступа "FireLamp-Setup" и откройте 192.168.4.1 чтобы выбрать новую сеть.':'Clears saved Wi-Fi credentials and reboots into setup mode. Connect to the "FireLamp-Setup" hotspot and open 192.168.4.1 to choose a new network.';
 document.getElementById('mt8').textContent=ru?'Потребление':'Power';
 document.getElementById('md8').textContent=ru?'Расчётное потребление в ваттах на основе текущего цвета и яркости каждого светодиода. Обновляется каждые 4 секунды.':'Estimated power draw in watts based on the current colour and brightness of each LED. Updates every 4 seconds.';
}
ul();
document.getElementById('ibtn').onclick=function(){document.getElementById('mod').classList.add('show')};
document.getElementById('mcls').onclick=function(){document.getElementById('mod').classList.remove('show')};
document.getElementById('len').onclick=function(){ru=false;localStorage.setItem('lang','en');ul();};
document.getElementById('lru').onclick=function(){ru=true;localStorage.setItem('lang','ru');ul();};
function pb(n){n=Math.max(0,Math.min(100,n|0));vb.textContent=n;sb.value=n;R.style.setProperty('--b',n);document.body.classList.toggle('off',n===0)}
function pc(n){n=Math.max(0,Math.min(100,n|0));vc.textContent=n;sc.value=n;}
function pco(n){n=Math.max(20,Math.min(150,n|0));vco.textContent=n;sco.value=n;}
function psp(n){n=Math.max(0,Math.min(255,n|0));vsp.textContent=n;ssp.value=n;}
function pull(){fetch('/state').then(r=>r.json()).then(x=>{pb(x.b);pc(x.c);pco(x.co);psp(x.sp);if(x.w!==undefined)document.getElementById('vw').textContent=x.w.toFixed(1);}).catch(()=>{})}
sb.addEventListener('input',function(){pb(+sb.value);clearTimeout(t1);t1=setTimeout(function(){xf('/setb?v='+sb.value)},120)});
sc.addEventListener('input',function(){pc(+sc.value);clearTimeout(t2);t2=setTimeout(function(){xf('/setc?v='+sc.value)},120)});
sco.addEventListener('input',function(){pco(+sco.value);clearTimeout(t4);t4=setTimeout(function(){xf('/setco?v='+sco.value)},120)});
ssp.addEventListener('input',function(){psp(+ssp.value);clearTimeout(t5);t5=setTimeout(function(){xf('/setsp?v='+ssp.value)},120)});
document.getElementById('rst').onclick=function(){xf('/reset').then(pull)};
document.getElementById('rwifi').onclick=function(){
  if(confirm(ru?'Сбросить настройки Wi-Fi?\nЛампа перезагрузится в режим настройки.\nПодключитесь к сети "FireLamp-Setup" и откройте 192.168.4.1':'Reset WiFi credentials?\nThe lamp will reboot into setup mode.\nConnect to "FireLamp-Setup" and open 192.168.4.1'))
    {var b=document.getElementById('rwifi');b.textContent=ru?'Перезагрузка...':'Rebooting...';b.disabled=true;xf('/resetwifi').catch(function(){});}
};
function startOTA(){
  ['sb','sc','sco','ssp','rst','chk','rwifi'].forEach(function(id){var e=document.getElementById(id);if(e)e.disabled=true;});
  var btn=document.getElementById('chk'),info=document.getElementById('vinfo');
  btn.textContent=ru?'Прошивка...':'Flashing...';
  btn.style.borderColor='#f59e0b';btn.style.color='#fbbf24';
  var pb=document.createElement('div');
  pb.style.cssText='margin-top:8px;height:4px;border-radius:2px;background:#1a1a1a;overflow:hidden';
  var pf=document.createElement('div');
  pf.style.cssText='height:100%;width:0%;background:#fbbf24;border-radius:2px;transition:width 35s linear';
  pb.appendChild(pf);info.insertAdjacentElement('afterend',pb);
  info.textContent=ru?'Скачивание... Не закрывайте страницу.':'Downloading... Do not close this page.';
  setTimeout(function(){pf.style.width='88%';},50);
  function pollReboot(){
    var n=0,tid=setInterval(function(){n++;
      fetch('/info',{cache:'no-store'}).then(function(r){return r.json();}).then(function(d){
        clearInterval(tid);
        pf.style.transition='width .4s';pf.style.width='100%';pf.style.background='#4ade80';
        btn.textContent=ru?'Готово! ✓':'Done! ✓';btn.style.borderColor='#4ade80';btn.style.color='#4ade80';
        info.textContent=(ru?'Обновлено до ':'Updated to ')+d.version;
        setTimeout(function(){location.reload();},2000);
      }).catch(function(){if(n>20){clearInterval(tid);pf.style.background='#ef4444';
        info.textContent=ru?'Лампа не отвечает. Обновите страницу вручную.':'Lamp not responding. Refresh manually.';
        btn.textContent=ru?'Обновить страницу':'Refresh page';btn.style.borderColor='#ef4444';btn.style.color='#ef4444';btn.disabled=false;
        btn.onclick=function(){location.reload();};}});
    },3000);
  }
  var doAfter=function(){
    pf.style.transition='width 2s';pf.style.width='95%';
    btn.textContent=ru?'Перезагрузка...':'Rebooting...';
    info.textContent=ru?'Лампа перезагружается...':'Lamp rebooting...';
    setTimeout(pollReboot,5000);
  };
  xf('/update').then(doAfter,doAfter);
}
document.getElementById('chk').onclick=function(){
  var btn=document.getElementById('chk');
  btn.textContent=ru?'Проверка...':'Checking...';
  btn.disabled=true;
  fetch('/checkupdate').then(r=>r.json()).then(x=>{
    document.getElementById('vinfo').textContent=(ru?'Текущая: ':'Current: ')+x.current+' → GitHub: '+x.latest;
    if(x.update_available){
      btn.textContent=ru?'Установить обновление ↑':'Install Update ↑';
      btn.style.borderColor='#16a34a';btn.style.color='#4ade80';btn.disabled=false;
      btn.onclick=function(){if(confirm(ru?'Начать обновление? Лампа перезагрузится.':'Install update? The lamp will reboot.'))startOTA();};
    } else {
      btn.textContent=ru?'Версия актуальна ✓':'Up to date ✓';btn.style.color='#4ade80';
      setTimeout(function(){btn.textContent=ru?'Проверить обновления':'Check for Update';btn.style.color='#60a5fa';btn.disabled=false;},3000);
    }
  }).catch(function(){btn.textContent=ru?'Ошибка сети':'Network error';btn.disabled=false;});
};
pull();setInterval(function(){if(!document.hidden)pull()},4000);
</script></body></html>)HTML";
