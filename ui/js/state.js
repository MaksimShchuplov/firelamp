function ps(lo,hi,ve,se,n){n=Math.max(lo,Math.min(hi,n|0));ve.textContent=n;se.value=n;dynDesc(se.id,n);return n;}
function pb(n){n=ps(0,100,vb,sb,n);R.style.setProperty('--b',n);document.body.classList.toggle('off',n===0);}
function pc(n){ps(0,100,vc,sc,n);}
function pco(n){ps(20,150,vco,sco,n);}
function psp(n){ps(0,255,vsp,ssp,n);}
function pbl(n){ps(0,255,vbl,sbl,n);}
// per theme: ambient[r,g,b, r2,g2,b2], track[tc1..tc5], thumb[th2,th3,th4]
var kAmb=[
  [255,120,20, 180,40,8,  '#2a0d04','#7a1f06','#d6510c','#ff8a1f','#ffe7b8', '#ffae45','#ff5e10','#7a1f00'],
  [180,30,10,  80,10,3,   '#1a0402','#4a0d04','#8a2010','#c04030','#e8a090', '#e08060','#c03020','#5a1008'],
  [160,20,240, 80,8,140,  '#180220','#4a0870','#8a20c0','#c040e0','#e8b0ff', '#d080ff','#9020d0','#3a0860'],
  [20,100,230, 8,50,170,  '#020810','#083060','#1060b0','#20a0e0','#b0e0ff', '#80c8ff','#1080d0','#043060']
];
function pth(n){
  for(var i=0;i<4;i++)document.getElementById('tb'+i).classList.toggle('act',i===n);
  var t=kAmb[n]||kAmb[0],s=R.style;
  s.setProperty('--ar',t[0]);s.setProperty('--ag',t[1]);s.setProperty('--ab',t[2]);
  s.setProperty('--ar2',t[3]);s.setProperty('--ag2',t[4]);s.setProperty('--ab2',t[5]);
  s.setProperty('--tc1',t[6]);s.setProperty('--tc2',t[7]);s.setProperty('--tc3',t[8]);
  s.setProperty('--tc4',t[9]);s.setProperty('--tc5',t[10]);
  s.setProperty('--th2',t[11]);s.setProperty('--th3',t[12]);s.setProperty('--th4',t[13]);
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
