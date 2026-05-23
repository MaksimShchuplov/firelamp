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
