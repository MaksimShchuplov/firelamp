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
 var vi=document.getElementById('vinfo');if(vi.textContent==='● Доступно обновление'||vi.textContent==='● Update available'){vi.textContent=ru?'● Доступно обновление':'● Update available';}
 var ob=document.getElementById('offb');if(ob.classList.contains('show'))ob.textContent=ru?'⚠ Лампа не отвечает':'⚠ Lamp not responding';
 dynAll();
}
ul();
document.addEventListener('keydown',function(e){if(e.key==='Escape'&&document.getElementById('mod').classList.contains('show')){document.getElementById('mcls').click();}});
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
