function pb(n){n=Math.max(0,Math.min(100,n|0));vb.textContent=n;sb.value=n;R.style.setProperty('--b',n);document.body.classList.toggle('off',n===0);dynDesc('sb',n);}
function pc(n){n=Math.max(0,Math.min(100,n|0));vc.textContent=n;sc.value=n;dynDesc('sc',n);}
function pco(n){n=Math.max(20,Math.min(150,n|0));vco.textContent=n;sco.value=n;dynDesc('sco',n);}
function psp(n){n=Math.max(0,Math.min(255,n|0));vsp.textContent=n;ssp.value=n;dynDesc('ssp',n);}
function pbl(n){n=Math.max(0,Math.min(255,n|0));vbl.textContent=n;sbl.value=n;dynDesc('sbl',n);}
function pth(n){for(var i=0;i<4;i++)document.getElementById('tb'+i).classList.toggle('act',i===n);}
function applyState(x){
  pullFails=0;hideOffline();
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
    var btn=document.getElementById('surprise'),nm=document.getElementById('ainame');
    btn.disabled=false;btn.textContent=ru?'✨ Удиви меня':'✨ Surprise Me';
    var n=x.name||'';
    if(n.substring(0,6)==='__err_'){
      var code=n.substring(6);
      var msg=code==='rate_limit'?(ru?'⚠ Лимит запросов':'⚠ Rate limit')
        :code==='auth_error'?(ru?'⚠ Неверный ключ API':'⚠ Invalid API key')
        :code==='no_key'?(ru?'⚠ Укажите ключ Gemini (?)':'⚠ Set Gemini key (?)')
        :code==='timeout'?(ru?'⚠ Нет ответа AI (таймаут)':'⚠ AI timeout')
        :code==='parse_failed'?(ru?'⚠ Ошибка ответа AI':'⚠ AI response error')
        :code==='http_error'?(ru?'⚠ Ошибка сервера Gemini':'⚠ Gemini server error')
        :(ru?'⚠ Ошибка AI':'⚠ AI error');
      nm.style.color='#ef4444';nm.textContent=msg;
      setTimeout(function(){nm.textContent='';nm.style.color='#fbbf24';},5000);
    } else if(n){
      clearActive();lastAiName=n.substring(0,15);nm.style.color='#fbbf24';nm.textContent=lastAiName+' ✨';
    } else {
      nm.style.color='#ef4444';nm.textContent=ru?'⚠ Ошибка AI':'⚠ AI error';
      setTimeout(function(){nm.textContent='';nm.style.color='#fbbf24';},4000);
    }
  }
}
function pull(){fetch('/state').then(r=>r.json()).then(x=>{applyState(x);}).catch(()=>{pullFails++;if(pullFails>=3)showOffline();});}
function xfc(u){return xf(u).then(function(r){pullFails=0;hideOffline();return r;},function(){pullFails++;if(pullFails>=3)showOffline();});}
document.addEventListener('visibilitychange',function(){if(!document.hidden)pull();});
