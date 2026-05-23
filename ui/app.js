
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
