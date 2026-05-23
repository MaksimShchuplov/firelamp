#pragma once
#include <pgmspace.h>

// Fire Lamp control UI — self-contained, served from PROGMEM.
// EN/RU language toggle stored in localStorage.
// Sliders debounce at 120 ms before sending to the ESP.
// Initial values are injected by handleRoot() via a trailing <script> chunk
// so sliders show correct state immediately without waiting for pull().
static const char PAGE[] PROGMEM = R"HTML(<!doctype html><html lang=en><head>
<meta charset=utf-8>
<meta name=viewport content="width=device-width,initial-scale=1">
<meta name=theme-color content="#0a0503">
<meta name=referrer content=no-referrer>
<link rel=icon href="data:image/svg+xml,<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 100 100'><text y='.9em' font-size='90'>🔥</text></svg>">
<title>Fire Lamp</title><style>
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
.label{font-size:13px;letter-spacing:.3em;text-transform:uppercase;font-weight:600;color:#b06030;margin-bottom:12px;margin-top:24px}
.bar:focus-visible{outline:2px solid #ff8a1f;outline-offset:3px}
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
.desc{font-size:12px;color:#a85a22;margin-top:-14px;margin-bottom:24px;letter-spacing:.05em;line-height:1.4;opacity:.8;font-weight:400}
.reset{margin-top:10px;padding:14px 24px;background:none;border:1px solid #7a3f16;color:#c8743a;border-radius:24px;cursor:pointer;font-size:13px;text-transform:uppercase;letter-spacing:.1em;transition:all .2s}
.reset:active{background:#7a3f16;color:#fff2dd}
.lang{position:absolute;top:20px;right:20px;display:flex;gap:4px;z-index:3}
.lbtn{background:none;border:1px solid #7a3f16;color:#c8743a;padding:0 12px;border-radius:4px;cursor:pointer;font-size:12px;font-weight:bold;transition:all .2s;display:inline-flex;align-items:center;min-height:44px}
.lbtn.act{background:#7a3f16;color:#fff2dd}
.stat{margin-top:20px;text-align:center;font-size:13px;color:#a85a22;letter-spacing:.1em;opacity:.8;font-weight:400}
.stat span{color:#ffb14a;font-weight:bold}
.ibtn{position:absolute;top:20px;left:20px;background:none;border:1px solid #7a3f16;color:#c8743a;padding:0 16px;border-radius:4px;cursor:pointer;font-size:14px;font-weight:bold;transition:all .2s;z-index:3;display:inline-flex;align-items:center;min-height:44px;min-width:44px}
.ibtn:active{background:#7a3f16;color:#fff2dd}
.mod{display:none;position:fixed;inset:0;background:rgba(10,5,3,.95);z-index:10;flex-direction:column;padding:30px;overflow-y:auto;color:#f6d9b0;text-align:left;font-size:14px;line-height:1.5}
.mod.show{display:flex}
.mcls{align-self:flex-end;background:none;border:none;color:#ffb14a;font-size:28px;cursor:pointer;margin-bottom:10px;min-width:44px;min-height:44px;display:flex;align-items:center;justify-content:center}
.mt{font-size:16px;font-weight:bold;color:#ff6a18;margin-top:15px;margin-bottom:5px;letter-spacing:.1em;text-transform:uppercase}
.md{margin-bottom:15px;opacity:.85}
body.off .val,body.off .amb{filter:grayscale(.5);opacity:.4}
.themes{display:flex;gap:8px;margin:8px 0 24px}
.tbtn{flex:1;background:none;border:1px solid #7a3f16;color:#c8743a;border-radius:8px;cursor:pointer;font-size:11px;text-transform:uppercase;letter-spacing:.05em;transition:all .2s;min-height:44px;touch-action:manipulation}
.tbtn[data-t="0"].act{background:#5a1a03;border-color:#e05020;color:#ffd8a0}
.tbtn[data-t="1"].act{background:#200803;border-color:#602010;color:#e08060}
.tbtn[data-t="2"].act{background:#2a0840;border-color:#8830c0;color:#d898ff}
.tbtn[data-t="3"].act{background:#04152a;border-color:#2060a0;color:#80c8ff}
.tbtn[data-t="0"]:before{content:'';display:inline-block;width:7px;height:7px;border-radius:50%;background:#ff6a18;margin-right:5px;vertical-align:middle}
.tbtn[data-t="1"]:before{content:'';display:inline-block;width:7px;height:7px;border-radius:50%;background:#8b1a0a;margin-right:5px;vertical-align:middle}
.tbtn[data-t="2"]:before{content:'';display:inline-block;width:7px;height:7px;border-radius:50%;background:#9933cc;margin-right:5px;vertical-align:middle}
.tbtn[data-t="3"]:before{content:'';display:inline-block;width:7px;height:7px;border-radius:50%;background:#4499dd;margin-right:5px;vertical-align:middle}
.presets{display:flex;flex-wrap:wrap;gap:8px;margin:8px 0 24px}
.prbtn{flex:0 0 calc(25% - 6px);background:none;border:1px dashed #4a2010;color:#5a3018;border-radius:8px;cursor:pointer;font-size:10px;text-transform:uppercase;letter-spacing:.05em;transition:all .2s;min-height:44px;padding:0 4px;overflow:hidden;text-overflow:ellipsis;white-space:nowrap;touch-action:manipulation}
.prbtn.filled{border-style:solid;border-color:#7a3f16;color:#c8743a}
.prbtn.act{background:#3a1a06;border-color:#d6510c;color:#ffd8a0}
@keyframes shake{0%,100%{transform:translateX(0)}25%{transform:translateX(-3px)}75%{transform:translateX(3px)}}
.shk{animation:shake .25s ease}
.prein{overflow:hidden;max-height:0;transition:max-height .25s ease;display:flex;gap:8px;align-items:center;margin-bottom:0}
.prein.show{max-height:52px;margin-bottom:16px}
.prein input{flex:1;background:rgba(120,40,10,.15);border:1px solid #7a3f16;border-radius:8px;color:#f6d9b0;padding:0 12px;height:44px;font-size:13px;font-family:inherit;-webkit-appearance:none}
.prein input:focus{outline:none;border-color:#d6510c}
.pric{background:none;border:1px solid #7a3f16;color:#c8743a;border-radius:8px;cursor:pointer;min-width:44px;height:44px;font-size:16px;transition:all .2s;flex-shrink:0;touch-action:manipulation}
.pric:active{background:#7a3f16;color:#fff2dd}
.offb{position:fixed;top:0;left:0;right:0;background:#7a1616;color:#fca5a5;text-align:center;font-size:12px;padding:10px;z-index:15;transform:translateY(-100%);transition:transform .3s;letter-spacing:.05em}
.offb.show{transform:translateY(0)}
.shdim{position:fixed;inset:0;background:rgba(0,0,0,.55);z-index:19;opacity:0;pointer-events:none;transition:opacity .25s}
.shdim.show{opacity:1;pointer-events:auto}
.sheet{position:fixed;bottom:0;left:0;right:0;background:#150803;border-top:1px solid #7a3f16;border-radius:16px 16px 0 0;z-index:20;padding:20px 20px 36px;transform:translateY(100%);transition:transform .25s ease}
.sheet.show{transform:translateY(0)}
.shtit{font-size:14px;font-weight:bold;color:#c8743a;text-transform:uppercase;letter-spacing:.1em;margin-bottom:6px;text-align:center}
.shmsg{font-size:13px;color:#a85a22;text-align:center;margin-bottom:16px;line-height:1.5;opacity:.9}
.shbtn{width:100%;padding:14px;border-radius:8px;border:1px solid #7a3f16;background:none;color:#c8743a;font-size:14px;cursor:pointer;letter-spacing:.05em;transition:all .2s;touch-action:manipulation;margin-top:8px;display:block}
.shbtn:active{background:#7a3f16}
.shbtn.danger{border-color:#7a1616;color:#f87171}
.shbtn.danger:active{background:#4a1010}
.shbtn.primary{border-color:#16a34a;color:#4ade80}
.shbtn.primary:active{background:#0d2a18}
.surprise{margin-top:10px;padding:14px 24px;background:none;border:1px solid #7a5f00;color:#fbbf24;border-radius:24px;cursor:pointer;font-size:13px;text-transform:uppercase;letter-spacing:.1em;transition:all .2s;width:100%}
.surprise:active{background:#3a2f00;color:#fff2a0}
.surprise:disabled{opacity:.5;cursor:default}
.ainame{font-size:11px;color:#fbbf24;margin-top:6px;text-align:center;min-height:16px;opacity:.85;letter-spacing:.05em}
.aikey{width:100%;background:rgba(120,40,10,.15);border:1px solid #7a3f16;border-radius:8px;color:#f6d9b0;padding:0 12px;height:44px;font-size:13px;font-family:inherit;-webkit-appearance:none;margin-top:8px;box-sizing:border-box}
.aikey:focus{outline:none;border-color:#d6510c}
.aiksave{background:none;border:1px solid #7a3f16;color:#c8743a;border-radius:8px;cursor:pointer;padding:0 16px;height:44px;font-size:13px;transition:all .2s;margin-top:8px;touch-action:manipulation}
.aikeyst{font-size:11px;color:#4ade80;margin-top:4px;min-height:16px;letter-spacing:.05em}
.aiksave:active{background:#7a3f16;color:#fff2dd}
</style></head><body>
<div id=offb class=offb></div>
<div id=shdim class=shdim></div>
<div id=sheet class=sheet><div id=shtit class=shtit></div><div id=shmsg class=shmsg></div><div id=shbtns></div></div>
<div class=amb></div>
<button id=ibtn class=ibtn aria-label="Help">?</button>
<div id=mod class=mod role=dialog aria-modal=true aria-label="Help">
<button id=mcls class=mcls aria-label="Close">&times;</button>
<div class=mt id=mt1>Brightness</div><div class=md id=md1></div>
<div class=mt id=mt2>Contrast</div><div class=md id=md2></div>
<div class=mt id=mt3>Cooling</div><div class=md id=md3></div>
<div class=mt id=mt4>Sparking</div><div class=md id=md4></div>
<div class=mt id=mt5>Reset to Default</div><div class=md id=md5></div>
<div class=mt id=mt6>Check for Update</div><div class=md id=md6></div>
<div class=mt id=mt7>Reset WiFi</div><div class=md id=md7></div>
<div class=mt id=mt8>Power</div><div class=md id=md8></div>
<div class=mt id=mt9>Blend</div><div class=md id=md9></div>
<div class=mt id=mt10>Theme</div><div class=md id=md10></div>
<div class=mt id=mt11>Presets</div><div class=md id=md11></div>
<div class=mt id=mt12>✨ Surprise Me (AI)</div><div class=md id=md12></div>
<input class=aikey id=aikey type=password placeholder="Gemini API key" autocomplete=off spellcheck=false>
<button class=aiksave id=aiksave>Save key</button>
<div id=aikeystatus class=aikeyst></div>
</div>
<div class=lang><button id=len class="lbtn act">EN</button><button id=lru class=lbtn>RU</button></div>
<div class=wrap>
<div class=kick>Fire Lamp</div>
<p class=label id=lb>Brightness</p><div class=val id=vb>--</div>
<input class=bar id=sb type=range min=0 max=100 value=100 aria-labelledby=lb>
<div class=desc id=db>Full brightness</div>
<p class=label id=lc>Contrast</p><div class=val id=vc>--</div>
<input class=bar id=sc type=range min=0 max=100 value=50 aria-labelledby=lc>
<div class=desc id=dc>Warm balanced</div>
<p class=label id=lco>Cooling</p><div class=val id=vco>--</div>
<input class=bar id=sco type=range min=20 max=150 value=45 aria-labelledby=lco>
<div class=desc id=dco>Tall flames</div>
<p class=label id=lsp>Sparking</p><div class=val id=vsp>--</div>
<input class=bar id=ssp type=range min=0 max=255 value=36 aria-labelledby=lsp>
<div class=desc id=dsp>Calm smoldering</div>
<p class=label id=lbl>Blend</p><div class=val id=vbl>--</div>
<input class=bar id=sbl type=range min=0 max=255 value=50 aria-labelledby=lbl>
<div class=desc id=dbl>Natural fire</div>
<p class=label id=lth>Theme</p>
<div class=themes><button class="tbtn act" id=tb0 data-t=0>Fire</button><button class=tbtn id=tb1 data-t=1>Ember</button><button class=tbtn id=tb2 data-t=2>Plasma</button><button class=tbtn id=tb3 data-t=3>Ice</button></div>
<p class=label id=lpr>Presets</p>
<div class=presets><button class=prbtn id=pr0 data-slot=0>+</button><button class=prbtn id=pr1 data-slot=1>+</button><button class=prbtn id=pr2 data-slot=2>+</button><button class=prbtn id=pr3 data-slot=3>+</button><button class=prbtn id=pr4 data-slot=4>+</button><button class=prbtn id=pr5 data-slot=5>+</button><button class=prbtn id=pr6 data-slot=6>+</button><button class=prbtn id=pr7 data-slot=7>+</button></div>
<div class=prein id=prein><input id=prename type=text maxlength=15 autocomplete=off spellcheck=false><button class=pric id=presave>✓</button><button class=pric id=precancel>✗</button></div>
<button class=reset id=rst>Reset to Default</button>
<button class=reset id=chk style="margin-top:10px;border-color:#1e3a8a;color:#60a5fa">Check for Update</button>
<div id=vinfo style="font-size:11px;color:#888;margin-top:6px;text-align:center"></div>
<button class=reset id=rwifi style="margin-top:10px;border-color:#7a1616;color:#f87171">Reset WiFi</button>
<button class=surprise id=surprise>✨ Surprise Me</button>
<div class=ainame id=ainame></div>
<div class=stat><span id=lw>Power:</span> <span id=vw>0.0</span> W</div>
</div><script>
var DD={
 sb:{en:[[0,0,'Off'],[1,25,'Very dim'],[26,50,'Dim'],[51,75,'Medium'],[76,95,'Bright'],[96,100,'Full brightness']],
     ru:[[0,0,'Выключено'],[1,25,'Очень тускло'],[26,50,'Тускло'],[51,75,'Средняя'],[76,95,'Ярко'],[96,100,'Максимум яркости']]},
 sc:{en:[[0,25,'Yellows and whites'],[26,55,'Warm balanced'],[56,80,'Saturated oranges'],[81,100,'Deep reds only']],
     ru:[[0,25,'Жёлтые и белые тона'],[26,55,'Тёплые сбалансированные'],[56,80,'Насыщенные оранжевые'],[81,100,'Только глубокий красный']]},
 sco:{en:[[20,40,'Very tall flames'],[41,70,'Tall flames'],[71,105,'Medium flames'],[106,135,'Short sparks'],[136,150,'Quick embers']],
      ru:[[20,40,'Очень высокое пламя'],[41,70,'Высокое пламя'],[71,105,'Среднее пламя'],[106,135,'Короткие искры'],[136,150,'Быстрое тление']]},
 ssp:{en:[[0,40,'Calm smoldering'],[41,90,'Steady flame'],[91,160,'Active fire'],[161,220,'Hot inferno'],[221,255,'Raging maximum']],
      ru:[[0,40,'Тихое тление'],[41,90,'Ровное пламя'],[91,160,'Активный огонь'],[161,220,'Жаркое пламя'],[221,255,'Бушующий максимум']]},
 sbl:{en:[[0,20,'Sharp flicker'],[21,60,'Natural fire'],[61,120,'Soft glow'],[121,200,'Slow motion'],[201,255,'Frozen glow']],
      ru:[[0,20,'Резкое мерцание'],[21,60,'Естественный огонь'],[61,120,'Мягкое свечение'],[121,200,'Замедленное движение'],[201,255,'Застывшее свечение']]}
};
var vb=document.getElementById('vb'),sb=document.getElementById('sb');
var vc=document.getElementById('vc'),sc=document.getElementById('sc');
var vco=document.getElementById('vco'),sco=document.getElementById('sco');
var vsp=document.getElementById('vsp'),ssp=document.getElementById('ssp');
var vbl=document.getElementById('vbl'),sbl=document.getElementById('sbl');
var R=document.documentElement,t1,t2,t3,t4,t5;
var xf=function(u,o){return fetch(u,Object.assign({headers:{'X-Requested-With':'firelamp'}},o));};
var ru=(localStorage.getItem('lang')==='ru')||(!localStorage.getItem('lang')&&navigator.language.startsWith('ru'));
var pullFails=0;
function dynDesc(sid,val){
  var r=DD[sid];if(!r)return;
  var a=r[ru?'ru':'en'],t=a[0][2];
  for(var i=0;i<a.length;i++){if(val>=a[i][0]&&val<=a[i][1]){t=a[i][2];break;}}
  document.getElementById(sid.replace('s','d')).textContent=t;
}
function dynAll(){dynDesc('sb',+sb.value);dynDesc('sc',+sc.value);dynDesc('sco',+sco.value);dynDesc('ssp',+ssp.value);dynDesc('sbl',+sbl.value);}
function showOffline(){var b=document.getElementById('offb');b.textContent=ru?'⚠ Лампа не отвечает':'⚠ Lamp not responding';b.classList.add('show');}
function hideOffline(){document.getElementById('offb').classList.remove('show');}
function showSheet(title,msg,btns){
  document.getElementById('shtit').textContent=title;
  document.getElementById('shmsg').textContent=msg;
  var c=document.getElementById('shbtns');c.innerHTML='';
  btns.forEach(function(b){var e=document.createElement('button');e.className='shbtn'+(b.cls?' '+b.cls:'');e.textContent=b.label;e.onclick=function(){hideSheet();b.fn();};c.appendChild(e);});
  var cl=document.createElement('button');cl.className='shbtn';cl.textContent=ru?'Отмена':'Cancel';cl.onclick=hideSheet;c.appendChild(cl);
  document.getElementById('shdim').classList.add('show');
  document.getElementById('sheet').classList.add('show');
}
function hideSheet(){document.getElementById('shdim').classList.remove('show');document.getElementById('sheet').classList.remove('show');}
document.getElementById('shdim').onclick=hideSheet;
function ul(){
 document.documentElement.lang=ru?'ru':'en';
 document.getElementById('len').classList.toggle('act',!ru);
 document.getElementById('lru').classList.toggle('act',ru);
 document.getElementById('lb').textContent=ru?'Яркость':'Brightness';
 document.getElementById('lc').textContent=ru?'Контрастность':'Contrast';
 document.getElementById('lco').textContent=ru?'Охлаждение':'Cooling';
 document.getElementById('lsp').textContent=ru?'Искры':'Sparking';
 document.getElementById('rst').textContent=ru?'По умолчанию':'Reset to Default';
 var ck=document.getElementById('chk');if(!ck.disabled){ck.textContent=ru?'Проверить обновления':'Check for Update';ck.style.borderColor='#1e3a8a';ck.style.color='#60a5fa';}
 document.getElementById('rwifi').textContent=ru?'Сменить сеть Wi-Fi':'Reset WiFi';
 document.getElementById('lbl').textContent=ru?'Плавность':'Blend';
 document.getElementById('lth').textContent=ru?'Тема':'Theme';
 document.getElementById('lpr').textContent=ru?'Пресеты':'Presets';
 document.getElementById('tb0').textContent=ru?'Огонь':'Fire';
 document.getElementById('tb1').textContent=ru?'Тление':'Ember';
 document.getElementById('tb2').textContent=ru?'Плазма':'Plasma';
 document.getElementById('tb3').textContent=ru?'Лёд':'Ice';
 document.getElementById('lw').textContent=ru?'Потребление:':'Power:';
 document.getElementById('mt1').textContent=ru?'Яркость (Масштаб)':'Brightness (Scale)';
 document.getElementById('md1').textContent=ru?'Управляет общей яркостью лампы. Гамма-коррекция 2.2 обеспечивает равномерное восприятие по всей шкале. Не меняет физику пламени.':'Controls the overall light output. A gamma-2.2 curve makes the slider feel perceptually even across its range. Does not change the flame physics.';
 document.getElementById('mt2').textContent=ru?'Контрастность (Цвет)':'Contrast (Palette)';
 document.getElementById('md2').textContent=ru?'Не влияет на физику. Сдвигает цвета: низкая контрастность даёт больше жёлтого/белого, высокая оставляет только глубокий красный.':'Does not affect physics. Shifts the colors: low contrast allows more yellow/white, high contrast forces deep reds.';
 document.getElementById('mt3').textContent=ru?'Охлаждение (Высота)':'Cooling (Height)';
 document.getElementById('md3').textContent=ru?'Управляет скоростью затухания искр по мере подъёма. Меньше = выше пламя. Больше = короткие искры.':'Dictates how quickly sparks die out as they travel up. Lower = taller flames. Higher = short embers.';
 document.getElementById('mt4').textContent=ru?'Искры (Зажигание)':'Sparking (Ignition)';
 document.getElementById('md4').textContent=ru?'Управляет хаосом у основания. Высокое = сплошной ревущий жар. Низкое = спокойное тление.':'Dictates chaos at the base. Higher = a solid roaring inferno. Lower = calm smoldering.';
 document.getElementById('mt5').textContent=ru?'По умолчанию':'Reset to Default';
 document.getElementById('md5').textContent=ru?'Восстанавливает все параметры к заводским значениям: яркость 100, контрастность 50, охлаждение 45, искры 36, плавность 50, тема Огонь.':'Restores all parameters to factory defaults: brightness 100, contrast 50, cooling 45, sparking 36, blend 50, theme Fire.';
 document.getElementById('mt6').textContent=ru?'Проверить обновления':'Check for Update';
 document.getElementById('md6').textContent=ru?'Сравнивает текущую прошивку с последней сборкой на GitHub. При наличии обновления предложит установить его — лампа перезагрузится автоматически.':'Compares current firmware with the latest build on GitHub. If an update is available you can install it — the lamp reboots automatically.';
 document.getElementById('mt7').textContent=ru?'Сменить сеть Wi-Fi':'Reset WiFi';
 document.getElementById('md7').textContent=ru?'Удаляет сохранённые данные сети и перезагружает лампу в режим настройки. Подключитесь к "FireLamp-Setup" и откройте 192.168.4.1 чтобы выбрать новую сеть.':'Clears saved Wi-Fi credentials and reboots into setup mode. Connect to "FireLamp-Setup" and open 192.168.4.1 to choose a new network.';
 document.getElementById('mt8').textContent=ru?'Потребление':'Power';
 document.getElementById('md8').textContent=ru?'Расчётное потребление в ваттах на основе текущего цвета и яркости каждого светодиода. Обновляется каждые 8 секунд.':'Estimated power draw in watts based on the current colour and brightness of each LED. Updates every 8 seconds.';
 document.getElementById('mt9').textContent=ru?'Плавность':'Blend';
 document.getElementById('md9').textContent=ru?'Временное сглаживание кадров. 0 = резкое мерцание, 255 = медленное свечение. Оптимальный диапазон 30–80.':'Temporal blend per frame. 0 = sharp flicker, 255 = slow soft glow. Sweet spot 30–80.';
 document.getElementById('mt10').textContent=ru?'Тема цвета':'Color Theme';
 document.getElementById('md10').textContent=ru?'Цветовая палитра пламени: Огонь (красно-оранжевый), Тление (тёмно-красный), Плазма (пурпурный), Лёд (синий).':'Color palette: Fire (red/orange/white), Ember (deep dark red), Plasma (purple/magenta/white), Ice (blue/cyan/white).';
 document.getElementById('mt11').textContent=ru?'Пресеты':'Presets';
 document.getElementById('md11').textContent=ru?'До 8 наборов параметров. Нажмите + чтобы сохранить. Нажмите на заполненный слот чтобы загрузить. Удерживайте чтобы сохранить в слот или удалить.':'Up to 8 parameter sets. Tap + to save. Tap a filled slot to load. Long-press to save or delete.';
 document.getElementById('mt12').textContent=ru?'✨ Удиви меня (ИИ)':'✨ Surprise Me (AI)';
 document.getElementById('md12').textContent=ru?'Gemini AI придумает уникальный эффект пламени. Вставьте API-ключ Gemini ниже — он сохранится в памяти лампы и будет работать с любого устройства.':'Gemini AI designs a unique flame effect each time. Paste your Gemini API key below — it is stored on the lamp and works from any device.';
 document.getElementById('aiksave').textContent=ru?'Сохранить ключ':'Save key';
 document.getElementById('aikey').placeholder=ru?'Ключ Gemini API':'Gemini API key';
 var ks=document.getElementById('aikeystatus');
 if(ks.textContent){var isOk=ks.style.color==='rgb(74, 222, 128)';ks.textContent=isOk?(ru?'Ключ сохранён ✓':'Key saved ✓'):(ru?'Ключ не задан':'No key set');}
 var sp2=document.getElementById('surprise');if(sp2&&!sp2.disabled)sp2.textContent=ru?'✨ Удиви меня':'✨ Surprise Me';
 var ob=document.getElementById('offb');if(ob.classList.contains('show'))ob.textContent=ru?'⚠ Лампа не отвечает':'⚠ Lamp not responding';
 dynAll();
}
ul();
document.addEventListener('keydown',function(e){if(e.key==='Escape'&&document.getElementById('mod').classList.contains('show')){document.getElementById('mcls').click();}});
document.addEventListener('visibilitychange',function(){if(!document.hidden)pull();});
document.getElementById('ibtn').onclick=function(){
  document.getElementById('mod').classList.add('show');
  document.getElementById('mcls').focus();
  fetch('/geminikey').then(r=>r.json()).then(function(x){
    var s=document.getElementById('aikeystatus');
    s.textContent=x.set?(ru?'Ключ сохранён ✓':'Key saved ✓'):(ru?'Ключ не задан':'No key set');
    s.style.color=x.set?'#4ade80':'#f87171';
  }).catch(function(){});
};
document.getElementById('mcls').onclick=function(){document.getElementById('mod').classList.remove('show')};
document.getElementById('len').onclick=function(){ru=false;localStorage.setItem('lang','en');ul();};
document.getElementById('lru').onclick=function(){ru=true;localStorage.setItem('lang','ru');ul();};
function pb(n){n=Math.max(0,Math.min(100,n|0));vb.textContent=n;sb.value=n;R.style.setProperty('--b',n);document.body.classList.toggle('off',n===0);dynDesc('sb',n);}
function pc(n){n=Math.max(0,Math.min(100,n|0));vc.textContent=n;sc.value=n;dynDesc('sc',n);}
function pco(n){n=Math.max(20,Math.min(150,n|0));vco.textContent=n;sco.value=n;dynDesc('sco',n);}
function psp(n){n=Math.max(0,Math.min(255,n|0));vsp.textContent=n;ssp.value=n;dynDesc('ssp',n);}
function pbl(n){n=Math.max(0,Math.min(255,n|0));vbl.textContent=n;sbl.value=n;dynDesc('sbl',n);}
function pth(n){for(var i=0;i<4;i++)document.getElementById('tb'+i).classList.toggle('act',i===n);}
function pull(){fetch('/state').then(r=>r.json()).then(x=>{
  pullFails=0;hideOffline();
  pb(x.b);pc(x.c);pco(x.co);psp(x.sp);
  if(x.bl!==undefined)pbl(x.bl);
  if(x.th!==undefined)pth(x.th);
  if(x.w!==undefined)document.getElementById('vw').textContent=x.w.toFixed(1);
  if(x.upd&&!document.getElementById('chk').disabled){var vi=document.getElementById('vinfo');if(!vi.textContent){vi.style.color='#fbbf24';vi.textContent=ru?'● Доступно обновление':'● Update available';}}
}).catch(()=>{pullFails++;if(pullFails>=3)showOffline();});}
function xfc(u){return xf(u).then(function(r){pullFails=0;hideOffline();return r;},function(){pullFails++;if(pullFails>=3)showOffline();});}
sb.addEventListener('input',function(){pb(+sb.value);clearTimeout(t1);t1=setTimeout(function(){xfc('/setb?v='+sb.value);},120);clearActive();});
sc.addEventListener('input',function(){pc(+sc.value);clearTimeout(t2);t2=setTimeout(function(){xfc('/setc?v='+sc.value);},120);clearActive();});
sco.addEventListener('input',function(){pco(+sco.value);clearTimeout(t3);t3=setTimeout(function(){xfc('/setco?v='+sco.value);},120);clearActive();});
ssp.addEventListener('input',function(){psp(+ssp.value);clearTimeout(t4);t4=setTimeout(function(){xfc('/setsp?v='+ssp.value);},120);clearActive();});
sbl.addEventListener('input',function(){pbl(+sbl.value);clearTimeout(t5);t5=setTimeout(function(){xfc('/setbl?v='+sbl.value);},120);clearActive();});
[0,1,2,3].forEach(function(t){document.getElementById('tb'+t).onclick=function(){xf('/settheme?v='+t).then(pull);clearActive();};});
document.getElementById('rst').onclick=function(){xf('/reset').then(pull);clearActive();};
var presets=[{},{},{},{},{},{},{},{}],activePreset=-1;
function updPresetBtns(){presets.forEach(function(pr,i){var b=document.getElementById('pr'+i);if(pr.name){b.textContent=pr.name;b.classList.add('filled');}else{b.textContent='+';b.classList.remove('filled');}b.classList.toggle('act',i===activePreset);});}
function clearActive(){activePreset=-1;lastAiName='';document.getElementById('ainame').textContent='';updPresetBtns();}
function fetchPresets(){fetch('/getpresets').then(r=>r.json()).then(function(d){presets=d;updPresetBtns();}).catch(function(){});}
function deletePreset(s){xf('/deletepreset?slot='+s).then(function(){if(activePreset===s)activePreset=-1;fetchPresets();}).catch(function(){});}
var pendingSlot=-1,lastAiName='',aiPending=false,aiTimeout=null;
function saveSlot(s){
  pendingSlot=s;
  var inp=document.getElementById('prename');
  inp.value=presets[s]&&presets[s].name?presets[s].name:lastAiName||(ru?'Пресет ':'Preset ')+(s+1);
  inp.placeholder=ru?'Название...':'Name...';
  document.getElementById('prein').classList.add('show');
  setTimeout(function(){inp.focus();inp.select();},200);
}
function doSave(){
  var nm=document.getElementById('prename').value.trim();
  if(!nm){document.getElementById('prename').focus();return;}
  document.getElementById('prein').classList.remove('show');
  xf('/savepreset?slot='+pendingSlot+'&name='+encodeURIComponent(nm)).then(function(){activePreset=pendingSlot;pendingSlot=-1;fetchPresets();}).catch(function(){pendingSlot=-1;});
}
function cancelSave(){pendingSlot=-1;document.getElementById('prein').classList.remove('show');}
document.getElementById('presave').onclick=doSave;
document.getElementById('precancel').onclick=cancelSave;
document.getElementById('prename').addEventListener('keydown',function(e){if(e.key==='Enter'){e.preventDefault();doSave();}if(e.key==='Escape')cancelSave();});
[0,1,2,3,4,5,6,7].forEach(function(s){
  var b=document.getElementById('pr'+s),pt=null;
  function onStart(e){if(e.cancelable)e.preventDefault();pt=setTimeout(function(){pt=null;if(navigator.vibrate)navigator.vibrate(40);b.classList.add('shk');setTimeout(function(){b.classList.remove('shk');},250);
    if(presets[s]&&presets[s].name){
      showSheet(presets[s].name,ru?'Выберите действие:':'Choose an action:',
        [{label:ru?'Сохранить в этот слот':'Save to this slot',fn:function(){saveSlot(s);}},
         {label:ru?'Удалить пресет':'Delete preset',cls:'danger',fn:function(){deletePreset(s);}}]);
    }else saveSlot(s);
  },600);}
  function onEnd(e){if(e.cancelable)e.preventDefault();if(pt){clearTimeout(pt);pt=null;if(presets[s]&&presets[s].name){xf('/loadpreset?slot='+s).then(function(r){if(!r.ok)throw new Error('http_'+r.status);return r.json();}).then(function(x){pb(x.b);pc(x.c);pco(x.co);psp(x.sp);pbl(x.bl);pth(x.th);if(x.w!==undefined)document.getElementById('vw').textContent=x.w.toFixed(1);activePreset=s;updPresetBtns();}).catch(function(){fetchPresets();});}else saveSlot(s);}}
  b.addEventListener('touchstart',onStart,{passive:false});
  b.addEventListener('touchend',onEnd,{passive:false});
  b.addEventListener('touchmove',function(){if(pt){clearTimeout(pt);pt=null;}},{passive:true});
  b.addEventListener('mousedown',onStart);
  b.addEventListener('mouseup',onEnd);
  b.addEventListener('mouseleave',function(){if(pt){clearTimeout(pt);pt=null;}});
});
fetchPresets();
document.getElementById('rwifi').onclick=function(){
  showSheet(ru?'Сменить сеть Wi-Fi':'Reset WiFi',
    ru?'Лампа перезагрузится в режим настройки. Подключитесь к "FireLamp-Setup" и откройте 192.168.4.1':'Lamp will reboot into setup mode. Connect to "FireLamp-Setup" and open 192.168.4.1',
    [{label:ru?'Сбросить и перезагрузить':'Reset and Reboot',cls:'danger',fn:function(){
      var b=document.getElementById('rwifi');b.textContent=ru?'Перезагрузка...':'Rebooting...';b.disabled=true;xf('/resetwifi').catch(function(){});
    }}]);
};
function startOTA(){
  clearInterval(pollTid);
  ['sb','sc','sco','ssp','sbl','tb0','tb1','tb2','tb3','rst','chk','rwifi','surprise','len','lru'].forEach(function(id){var e=document.getElementById(id);if(e)e.disabled=true;});
  var btn=document.getElementById('chk'),info=document.getElementById('vinfo');
  btn.textContent=ru?'Прошивка...':'Flashing...';
  btn.style.borderColor='#f59e0b';btn.style.color='#fbbf24';
  var pbar=document.createElement('div');
  pbar.style.cssText='margin-top:8px;height:4px;border-radius:2px;background:#1a1a1a;overflow:hidden';
  var pfil=document.createElement('div');
  pfil.style.cssText='height:100%;width:0%;background:#fbbf24;border-radius:2px;transition:width 35s linear';
  pbar.appendChild(pfil);info.insertAdjacentElement('afterend',pbar);
  info.textContent=ru?'Скачивание... Не закрывайте страницу.':'Downloading... Do not close this page.';
  setTimeout(function(){pfil.style.width='88%';},50);
  function pollReboot(){
    var n=0,tid=setInterval(function(){n++;
      xf('/info',{cache:'no-store'}).then(function(r){return r.json();}).then(function(d){
        clearInterval(tid);
        pfil.style.transition='width .4s';pfil.style.width='100%';pfil.style.background='#4ade80';
        btn.textContent=ru?'Готово! ✓':'Done! ✓';btn.style.borderColor='#4ade80';btn.style.color='#4ade80';
        info.textContent=(ru?'Обновлено до ':'Updated to ')+d.version;
        setTimeout(function(){location.reload();},2000);
      }).catch(function(){if(n>20){clearInterval(tid);pfil.style.background='#ef4444';
        info.textContent=ru?'Лампа не отвечает. Обновите страницу вручную.':'Lamp not responding. Refresh manually.';
        btn.textContent=ru?'Обновить страницу':'Refresh page';btn.style.borderColor='#ef4444';btn.style.color='#ef4444';btn.disabled=false;
        btn.onclick=function(){location.reload();};}});
    },3000);
  }
  var doAfter=function(){
    pfil.style.transition='width 2s';pfil.style.width='95%';
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
  xf('/checkupdate').then(r=>r.json()).then(x=>{
    document.getElementById('vinfo').textContent=(ru?'Текущая: ':'Current: ')+x.current+' → GitHub: '+x.latest;
    if(x.update_available){
      btn.textContent=ru?'Установить обновление ↑':'Install Update ↑';
      btn.style.borderColor='#16a34a';btn.style.color='#4ade80';btn.disabled=false;
      btn.onclick=function(){showSheet(ru?'Установить обновление?':'Install Update?',
        ru?'Лампа перезагрузится автоматически после прошивки.':'The lamp will reboot automatically after flashing.',
        [{label:ru?'Установить':'Install',cls:'primary',fn:startOTA}]);};
    }else{
      btn.textContent=ru?'Версия актуальна ✓':'Up to date ✓';btn.style.color='#4ade80';
      setTimeout(function(){btn.textContent=ru?'Проверить обновления':'Check for Update';btn.style.color='#60a5fa';btn.disabled=false;},3000);
    }
  }).catch(function(){btn.textContent=ru?'Ошибка сети':'Network error';btn.disabled=false;});
};
document.getElementById('aiksave').onclick=function(){
  var v=document.getElementById('aikey').value.trim();
  if(!v)return;
  var b=document.getElementById('aiksave');
  var s=document.getElementById('aikeystatus');
  b.disabled=true;
  fetch('/setgeminikey',{method:'POST',headers:{'X-Requested-With':'firelamp','Content-Type':'application/x-www-form-urlencoded'},body:'key='+encodeURIComponent(v)}).then(r=>r.json()).then(function(x){
    if(x.ok){document.getElementById('aikey').value='';s.textContent=ru?'Ключ сохранён ✓':'Key saved ✓';s.style.color='#4ade80';}
    else{s.textContent=ru?'Ошибка сохранения':'Save failed';s.style.color='#f87171';}
    b.textContent=x.ok?'✓':'✗';
    setTimeout(function(){b.textContent=ru?'Сохранить ключ':'Save key';b.disabled=false;},1500);
  }).catch(function(){
    s.textContent=ru?'Ошибка сети':'Network error';s.style.color='#f87171';
    b.textContent='✗';
    setTimeout(function(){b.textContent=ru?'Сохранить ключ':'Save key';b.disabled=false;},1500);
  });
};
function askAI(){
  var btn=document.getElementById('surprise'),nm=document.getElementById('ainame');
  btn.disabled=true;btn.textContent=ru?'✨ Думаю...':'✨ Thinking...';nm.style.color='#fbbf24';nm.textContent='';
  xf('/surprise').then(function(r){
    if(r.status===429)throw new Error('rate_limit');
    if(r.status===403)throw new Error('bad_key');
    return r.json();
  }).then(function(x){
    if(x.error)throw new Error(x.error);
    pullFails=0;hideOffline();
    if(x.status==='pending'){
      aiPending=true;
      aiTimeout=setTimeout(function(){
        if(!aiPending)return;
        aiPending=false;aiTimeout=null;
        btn.disabled=false;btn.textContent=ru?'✨ Удиви меня':'✨ Surprise Me';
        nm.style.color='#ef4444';nm.textContent=ru?'⚠ Нет ответа AI':'⚠ AI no response';
        setTimeout(function(){nm.textContent='';nm.style.color='#fbbf24';},4000);
      },25000);
      return;
    }
    pb(x.b);pc(x.c);pco(x.co);psp(x.sp);pbl(x.bl);pth(x.th);
    if(x.w!==undefined)document.getElementById('vw').textContent=x.w.toFixed(1);
    clearActive();lastAiName=(x.name||'').substring(0,15);nm.textContent=(lastAiName||'AI Effect')+' ✨';
    btn.disabled=false;btn.textContent=ru?'✨ Удиви меня':'✨ Surprise Me';
  }).catch(function(e){
    btn.disabled=false;btn.textContent=ru?'✨ Удиви меня':'✨ Surprise Me';
    var msg=e.message==='no_key'?(ru?'⚠ Укажите ключ в настройках (?)':'⚠ Set API key in settings (?)')
      :e.message==='rate_limit'?(ru?'⚠ Лимит — подождите немного':'⚠ Rate limit — wait a moment')
      :e.message==='bad_key'?(ru?'⚠ Неверный ключ API':'⚠ Invalid API key')
      :e.message==='parse_failed'?(ru?'⚠ Ошибка ответа AI':'⚠ AI response error')
      :e.message==='fetch_failed'?(ru?'⚠ Нет связи':'⚠ Connection failed')
      :(ru?'⚠ Ошибка':'⚠ Error');
    nm.style.color='#ef4444';nm.textContent=msg;
    setTimeout(function(){nm.textContent='';nm.style.color='#fbbf24';},4000);
  });
}
document.getElementById('surprise').onclick=askAI;
pull();var pollTid=setInterval(function(){if(!document.hidden)pull()},5000);
(function(){
  var ws,wst,wsDelay=3000;
  function applyState(x){
    pullFails=0;hideOffline();
    // Skip updating a slider that is currently being dragged — avoids the thumb
    // snapping back to the server-confirmed value while the user is still moving it.
    var ae=document.activeElement;
    if(ae!==sb)pb(x.b);
    if(ae!==sc)pc(x.c);
    if(ae!==sco)pco(x.co);
    if(ae!==ssp)psp(x.sp);
    if(x.bl!==undefined&&ae!==sbl)pbl(x.bl);
    if(x.th!==undefined)pth(x.th);
    if(x.w!==undefined)document.getElementById('vw').textContent=x.w.toFixed(1);
    if(x.upd&&!document.getElementById('chk').disabled){var vi=document.getElementById('vinfo');if(!vi.textContent){vi.style.color='#fbbf24';vi.textContent=ru?'● Доступно обновление':'● Update available';}}
    if(x.name!==undefined&&aiPending){
      aiPending=false;clearTimeout(aiTimeout);aiTimeout=null;
      var sbtn=document.getElementById('surprise'),snm=document.getElementById('ainame');
      sbtn.disabled=false;sbtn.textContent=ru?'✨ Удиви меня':'✨ Surprise Me';
      if(x.name){clearActive();lastAiName=x.name.substring(0,15);snm.style.color='#fbbf24';snm.textContent=lastAiName+' ✨';}
      else{snm.style.color='#ef4444';snm.textContent=ru?'⚠ Ошибка AI':'⚠ AI error';setTimeout(function(){snm.textContent='';snm.style.color='#fbbf24';},4000);}
    }
  }
  function connect(){
    try{ws=new WebSocket('ws://'+location.hostname+':81/');}catch(e){wst=setTimeout(connect,wsDelay);wsDelay=Math.min(wsDelay*2,30000);return;}
    ws.onopen=function(){wsDelay=3000;pullFails=0;hideOffline();};
    ws.onmessage=function(e){try{applyState(JSON.parse(e.data));}catch(err){}};
    ws.onclose=function(){clearTimeout(wst);wst=setTimeout(connect,wsDelay);wsDelay=Math.min(wsDelay*2,30000);if(wsDelay>3000){pullFails++;if(pullFails>=3)showOffline();}};
    ws.onerror=function(){ws.close();};
  }
  connect();
})();
</script></body></html>)HTML";
