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
  if(x.name){var nm=document.getElementById('ainame');clearActive();lastAiName=x.name.substring(0,15);nm.style.color='#fbbf24';nm.textContent=lastAiName+' ✨';}
  if(x.upd&&!document.getElementById('chk').disabled){var vi=document.getElementById('vinfo');if(!vi.textContent){vi.style.color='#fbbf24';vi.textContent=ru?'● Доступно обновление':'● Update available';}}
}
function pull(){fetch('/state').then(r=>r.json()).then(x=>{applyState(x);}).catch(()=>{pullFails++;if(pullFails>=3)showOffline();});}
function xfc(u){return xf(u).then(function(r){pullFails=0;hideOffline();return r;},function(){pullFails++;if(pullFails>=3)showOffline();});}
document.addEventListener('visibilitychange',function(){if(!document.hidden)pull();});
