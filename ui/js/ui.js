function dynDesc(sid,val){
  var r=DD[sid];if(!r)return;
  var a=r[ru?'ru':'en'],t=a[0][2];
  for(var i=0;i<a.length;i++){if(val>=a[i][0]&&val<=a[i][1]){t=a[i][2];break;}}
  document.getElementById(sid.replace('s','d')).textContent=t;
}
function dynAll(){dynDesc('sb',+sb.value);dynDesc('sc',+sc.value);dynDesc('sco',+sco.value);dynDesc('ssp',+ssp.value);dynDesc('sbl',+sbl.value);}
// The banner is hidden by sliding it off-screen (CSS transform), not display:none,
// so it stays in the accessibility tree; aria-hidden keeps a screen reader from
// announcing a stale "Lamp not responding" once the lamp is back. Clearing the
// text instead would show an empty red strip during the slide-out transition.
function showOffline(){var b=document.getElementById('offb');b.textContent=ru?'⚠ Лампа не отвечает':'⚠ Lamp not responding';b.setAttribute('aria-hidden','false');b.classList.add('show');}
function hideOffline(){var b=document.getElementById('offb');b.classList.remove('show');b.setAttribute('aria-hidden','true');}
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
