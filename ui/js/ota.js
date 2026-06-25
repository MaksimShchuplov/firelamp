document.getElementById('rwifi').onclick=function(){
  showSheet(ru?'Сменить сеть Wi-Fi':'Reset WiFi',
    ru?'Лампа перезагрузится в режим настройки. Подключитесь к "FireLamp-Setup" и откройте 192.168.4.1':'Lamp will reboot into setup mode. Connect to "FireLamp-Setup" and open 192.168.4.1',
    [{label:ru?'Сбросить и перезагрузить':'Reset and Reboot',cls:'danger',fn:function(){
      var b=document.getElementById('rwifi');b.textContent=ru?'Перезагрузка...':'Rebooting...';b.disabled=true;xf('/resetwifi').catch(function(){});
    }}]);
};
var otaEls=['sb','sc','sco','ssp','sbl','tb0','tb1','tb2','tb3','rst','chk','rwifi','surprise','ibtn','len','lru'];
function enableOtaEls(){otaEls.forEach(function(id){var e=document.getElementById(id);if(e)e.disabled=false;});}
function startOTA(){
  pullSeq++;clearInterval(pollTid);
  otaEls.forEach(function(id){var e=document.getElementById(id);if(e)e.disabled=true;});
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
    var n=0,wentOffline=false,tid;
    function showOtaError(msg){clearInterval(tid);pfil.style.background='#ef4444';info.textContent=msg;enableOtaEls();pollTid=setInterval(function(){if(!document.hidden)pull();},5000);btn.textContent=ru?'Обновить страницу':'Refresh page';btn.style.borderColor='#ef4444';btn.style.color='#ef4444';btn.onclick=function(){location.reload();};}
    tid=setInterval(function(){n++;
      var ac=new AbortController(),to=setTimeout(function(){ac.abort();},2000);
      xf('/info',{cache:'no-store',signal:ac.signal}).then(function(r){clearTimeout(to);return r.json();}).then(function(d){
        if(!wentOffline){
          if(n>10){showOtaError(ru?'Обновление не удалось. Обновите страницу.':'Update failed. Refresh the page to try again.');}
          return;
        }
        // Lamp responded after going offline. Confirm it actually rebooted by
        // checking uptime — a freshly booted ESP has uptime_s < 120.
        // Without this check, a single transient /info failure followed by a
        // recovery (e.g. OTA flash failed on ESP side) would trigger a premature
        // page reload while the old firmware is still running.
        if(d.uptime_s!==undefined&&d.uptime_s>=120){return;}
        clearInterval(tid);
        pfil.style.transition='width .4s';pfil.style.width='100%';pfil.style.background='#4ade80';
        btn.textContent=ru?'Готово! ✓':'Done! ✓';btn.style.borderColor='#4ade80';btn.style.color='#4ade80';
        info.textContent=(ru?'Обновлено до ':'Updated to ')+d.version;
        setTimeout(function(){location.reload();},2000);
      }).catch(function(){clearTimeout(to);wentOffline=true;if(n>20){showOtaError(ru?'Лампа не отвечает. Обновите страницу вручную.':'Lamp not responding. Refresh manually.');}});
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
    if(x.error){btn.textContent=ru?'Ошибка проверки':'Check failed';btn.disabled=false;return;}
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
