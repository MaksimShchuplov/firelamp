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
