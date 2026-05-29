function ps(lo,hi,ve,se,n){n=Math.max(lo,Math.min(hi,n|0));ve.textContent=n;se.value=n;dynDesc(se.id,n);return n;}
function pb(n){n=ps(0,100,vb,sb,n);R.style.setProperty('--b',n);document.body.classList.toggle('off',n===0);}
function pc(n){ps(0,100,vc,sc,n);}
function pco(n){ps(20,150,vco,sco,n);}
function psp(n){ps(0,255,vsp,ssp,n);}
function pbl(n){ps(0,255,vbl,sbl,n);}
var kAmb=[[255,120,20,180,40,8],[180,30,10,80,10,3],[160,20,240,80,8,140],[20,100,230,8,50,170]];
function pth(n){
  for(var i=0;i<4;i++)document.getElementById('tb'+i).classList.toggle('act',i===n);
  var t=kAmb[n]||kAmb[0];
  R.style.setProperty('--ar',t[0]);R.style.setProperty('--ag',t[1]);R.style.setProperty('--ab',t[2]);
  R.style.setProperty('--ar2',t[3]);R.style.setProperty('--ag2',t[4]);R.style.setProperty('--ab2',t[5]);
}
function applyState(x){
  pullFails=0;hideOffline();
  var ae=document.activeElement;
  if(ae!==sb)pb(x.b);
  if(ae!==sc)pc(x.c);
  if(ae!==sco)pco(x.co);
  if(ae!==ssp)psp(x.sp);
  if(x.bl!==undefined&&ae!==sbl)pbl(x.bl);
  if(x.th!==undefined)pth(x.th);
  if(x.w!=null)document.getElementById('vw').textContent=x.w.toFixed(1);
  if(x.upd&&!document.getElementById('chk').disabled){var vi=document.getElementById('vinfo');if(!vi.textContent){vi.style.color='#fbbf24';vi.textContent=ru?'● Доступно обновление':'● Update available';}}
}
function pull(){fetch('/state').then(r=>r.json()).then(x=>{applyState(x);}).catch(()=>{pullFails++;if(pullFails>=3)showOffline();});}
document.addEventListener('visibilitychange',function(){if(!document.hidden)pull();});
