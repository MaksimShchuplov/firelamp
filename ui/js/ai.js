document.getElementById('aiksave').onclick=function(){
  var v=document.getElementById('aikey').value.trim();
  if(!v)return;
  var b=document.getElementById('aiksave');
  var s=document.getElementById('aikeystatus');
  b.disabled=true;
  fetch('/setgeminikey',{method:'POST',headers:{'X-Requested-With':'firelamp','Content-Type':'application/x-www-form-urlencoded'},body:'key='+encodeURIComponent(v)}).then(r=>r.json()).then(function(x){
    if(x.ok){document.getElementById('aikey').value='';s.textContent=ru?'Ключ сохранён ✓':'Key saved ✓';s.style.color='#4ade80';s.dataset.ks='ok';}
    else{s.textContent=ru?'Ошибка сохранения':'Save failed';s.style.color='#f87171';s.dataset.ks='error';}
    b.textContent=x.ok?'✓':'✗';
    setTimeout(function(){b.textContent=ru?'Сохранить ключ':'Save key';b.disabled=false;},1500);
  }).catch(function(){
    s.textContent=ru?'Ошибка сети':'Network error';s.style.color='#f87171';s.dataset.ks='error';
    b.textContent='✗';
    setTimeout(function(){b.textContent=ru?'Сохранить ключ':'Save key';b.disabled=false;},1500);
  });
};
function askAI(){
  var btn=document.getElementById('surprise'),nm=document.getElementById('ainame');
  btn.disabled=true;nm.style.color='#fbbf24';nm.textContent='';
  var elapsed=0,etid=setInterval(function(){elapsed++;btn.textContent=(ru?'✨ Думаю ':'✨ Thinking ')+elapsed+'s…';},1000);
  btn.textContent=ru?'✨ Думаю 0s…':'✨ Thinking 0s…';
  var ac=new AbortController(),to=setTimeout(function(){ac.abort();},30000);
  xf('/surprise',{signal:ac.signal}).then(function(r){
    clearTimeout(to);clearInterval(etid);
    if(r.status===429)throw new Error('rate_limit');
    return r.json();
  }).then(function(x){
    if(x.error)throw new Error(x.error);
    applyState(x);
    if(aiNmTid){clearTimeout(aiNmTid);aiNmTid=null;}clearActive();lastAiName=Array.from(x.name||'').slice(0,15).join('');nm.textContent=(lastAiName||'AI Effect')+' ✨';
    btn.disabled=false;btn.textContent=ru?'✨ Удиви меня':'✨ Surprise Me';
  }).catch(function(e){
    clearTimeout(to);clearInterval(etid);
    btn.disabled=false;btn.textContent=ru?'✨ Удиви меня':'✨ Surprise Me';
    var msg=(e.name==='AbortError'||e.message==='timeout')?(ru?'⚠ AI не ответил (таймаут)':'⚠ AI timed out')
      :e.message==='no_key'?(ru?'⚠ Укажите ключ в настройках (?)':'⚠ Set API key in settings (?)')
      :e.message==='rate_limit'?(ru?'⚠ Лимит — подождите немного':'⚠ Rate limit — wait a moment')
      :e.message==='auth_error'?(ru?'⚠ Неверный ключ API':'⚠ Invalid API key')
      :e.message==='parse_failed'?(ru?'⚠ Ошибка ответа AI':'⚠ AI response error')
      :e.message==='http_error'?(ru?'⚠ Ошибка сервера Gemini':'⚠ Gemini server error')
      :(ru?'⚠ Ошибка':'⚠ Error');
    nm.style.color='#ef4444';nm.textContent=msg;
    if(aiNmTid)clearTimeout(aiNmTid);
    aiNmTid=setTimeout(function(){nm.textContent='';nm.style.color='#fbbf24';aiNmTid=null;},4000);
  });
}
document.getElementById('surprise').onclick=askAI;
