var presets=[{},{},{},{},{},{},{},{}],activePreset=-1;
function updPresetBtns(){presets.forEach(function(pr,i){var b=document.getElementById('pr'+i);if(pr.name){b.textContent=pr.name;b.classList.add('filled');}else{b.textContent='+';b.classList.remove('filled');}b.classList.toggle('act',i===activePreset);});}
function clearActive(){activePreset=-1;lastAiName='';document.getElementById('ainame').textContent='';updPresetBtns();}
function fetchPresets(){fetch('/getpresets').then(r=>r.json()).then(function(d){presets=d;updPresetBtns();}).catch(function(){});}
function deletePreset(s){xf('/deletepreset?slot='+s).then(function(){if(activePreset===s)activePreset=-1;fetchPresets();}).catch(function(){});}
function saveSlot(s){
  pendingSlot=s;
  var inp=document.getElementById('prename');
  inp.value=presets[s]&&presets[s].name?presets[s].name:lastAiName||(ru?'Пресет ':'Preset ')+(s+1);
  inp.placeholder=ru?'Название...':'Name...';
  document.getElementById('prein').classList.add('show');
  setTimeout(function(){inp.focus();inp.select();},200);
}
function doSave(){
  if(pendingSlot<0)return;
  var nm=document.getElementById('prename').value.trim();
  if(!nm){document.getElementById('prename').focus();return;}
  document.getElementById('prein').classList.remove('show');
  var ac=new AbortController(),to=setTimeout(function(){ac.abort();pendingSlot=-1;},8000);
  xf('/savepreset?slot='+pendingSlot+'&name='+encodeURIComponent(nm),{signal:ac.signal}).then(function(){clearTimeout(to);activePreset=pendingSlot;pendingSlot=-1;fetchPresets();}).catch(function(){clearTimeout(to);pendingSlot=-1;});
}
function cancelSave(){pendingSlot=-1;document.getElementById('prein').classList.remove('show');}
document.getElementById('presave').onclick=doSave;
document.getElementById('precancel').onclick=cancelSave;
document.getElementById('prename').addEventListener('keydown',function(e){if(e.key==='Enter'){e.preventDefault();doSave();}if(e.key==='Escape')cancelSave();});
[0,1,2,3,4,5,6,7].forEach(function(s){
  var b=document.getElementById('pr'+s),pt=null;
  function onStart(e){if(e.cancelable)e.preventDefault();pt=setTimeout(function(){pt=null;if(navigator.vibrate)navigator.vibrate(40);b.classList.add('shk');setTimeout(function(){b.classList.remove('shk');},250);
    if(presets[s]&&presets[s].name){
      showSheet(presets[s].name,ru?'Выберите действие:':'Choose an action:',
        [{label:ru?'Сохранить в этот слот':'Save to this slot',fn:function(){saveSlot(s);}},
         {label:ru?'Удалить пресет':'Delete preset',cls:'danger',fn:function(){deletePreset(s);}}]);
    }else saveSlot(s);
  },600);}
  function onEnd(e){if(e.cancelable)e.preventDefault();if(pt){clearTimeout(pt);pt=null;if(presets[s]&&presets[s].name){xf('/loadpreset?slot='+s).then(function(r){if(!r.ok)throw new Error('http_'+r.status);return r.json();}).then(function(x){var ae=document.activeElement;if(ae!==sb)pb(x.b);if(ae!==sc)pc(x.c);if(ae!==sco)pco(x.co);if(ae!==ssp)psp(x.sp);if(ae!==sbl)pbl(x.bl);pth(x.th);if(x.w!=null)document.getElementById('vw').textContent=x.w.toFixed(1);activePreset=s;updPresetBtns();}).catch(function(){fetchPresets();});}else saveSlot(s);}}
  b.addEventListener('touchstart',onStart,{passive:false});
  b.addEventListener('touchend',onEnd,{passive:false});
  b.addEventListener('touchmove',function(){if(pt){clearTimeout(pt);pt=null;}},{passive:true});
  b.addEventListener('mousedown',onStart);
  b.addEventListener('mouseup',onEnd);
  b.addEventListener('mouseleave',function(){if(pt){clearTimeout(pt);pt=null;}});
});
fetchPresets();
