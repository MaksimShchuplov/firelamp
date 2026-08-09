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
  // Clamp by codepoint here rather than with maxlength: the attribute counts
  // UTF-16 units, so it capped at 7 emoji while the firmware stores 15
  // codepoints — and once saveSlot() prefilled an over-length value the field
  // became uneditable.
  var nm=Array.from(document.getElementById('prename').value.trim()).slice(0,15).join('');
  if(!nm){document.getElementById('prename').focus();return;}
  document.getElementById('prein').classList.remove('show');
  var slot=pendingSlot;
  var ac=new AbortController(),to=setTimeout(function(){ac.abort();pendingSlot=-1;},8000);
  xf('/savepreset?slot='+slot+'&name='+encodeURIComponent(nm),{signal:ac.signal}).then(function(){clearTimeout(to);activePreset=slot;pendingSlot=-1;fetchPresets();}).catch(function(){clearTimeout(to);pendingSlot=-1;});
}
function cancelSave(){pendingSlot=-1;document.getElementById('prein').classList.remove('show');}
document.getElementById('presave').onclick=doSave;
document.getElementById('precancel').onclick=cancelSave;
document.getElementById('prename').addEventListener('keydown',function(e){if(e.key==='Enter'){e.preventDefault();doSave();}if(e.key==='Escape')cancelSave();});
[0,1,2,3,4,5,6,7].forEach(function(s){
  var b=document.getElementById('pr'+s),pt=null,skipClick=false;
  // No preventDefault here: cancelling the first touchstart of a touch point
  // suppresses panning for the whole gesture, so a swipe starting on the
  // preset grid froze the page. touch-action:manipulation does not affect
  // cancelability. The callout/selection this used to suppress is handled in
  // CSS instead. Dropping it also restores focus-on-mousedown for the keyboard
  // path below.
  function onStart(e){pt=setTimeout(function(){pt=null;if(navigator.vibrate)navigator.vibrate(40);b.classList.add('shk');setTimeout(function(){b.classList.remove('shk');},250);
    if(presets[s]&&presets[s].name){
      showSheet(presets[s].name,ru?'Выберите действие:':'Choose an action:',
        [{label:ru?'Сохранить в этот слот':'Save to this slot',fn:function(){saveSlot(s);}},
         {label:ru?'Удалить пресет':'Delete preset',cls:'danger',fn:function(){deletePreset(s);}}]);
    }else saveSlot(s);
  },600);}
  function activate(){if(presets[s]&&presets[s].name){xf('/loadpreset?slot='+s).then(function(r){if(!r.ok)throw new Error('http_'+r.status);return r.json();}).then(function(x){applyState(x);activePreset=s;updPresetBtns();}).catch(function(){fetchPresets();});}else saveSlot(s);}
  // skipClick covers the long-press release too, so the sheet is not shadowed
  // by a load. Keyboard activation dispatches click with no pointer events, so
  // it falls through and is the only path that reaches activate() directly.
  function onEnd(e){if(e.cancelable)e.preventDefault();skipClick=true;setTimeout(function(){skipClick=false;},400);if(pt){clearTimeout(pt);pt=null;activate();}}
  b.addEventListener('click',function(){if(skipClick)return;activate();});
  b.addEventListener('touchstart',onStart,{passive:true});
  b.addEventListener('touchend',onEnd,{passive:false});
  b.addEventListener('touchmove',function(){if(pt){clearTimeout(pt);pt=null;}},{passive:true});
  b.addEventListener('mousedown',onStart);
  b.addEventListener('mouseup',onEnd);
  b.addEventListener('mouseleave',function(){if(pt){clearTimeout(pt);pt=null;}});
});
document.getElementById('prexp').onclick=function(){
  fetch('/getpresets').then(r=>r.json()).then(function(d){
    var a=document.createElement('a');
    a.href=URL.createObjectURL(new Blob([JSON.stringify(d,null,1)],{type:'application/json'}));
    a.download='firelamp-presets.json';a.click();setTimeout(function(){URL.revokeObjectURL(a.href);},2000);
  }).catch(function(){});
};
document.getElementById('primp').onclick=function(){document.getElementById('prfile').click();};
document.getElementById('prfile').addEventListener('change',function(){
  var f=this.files[0];this.value='';
  if(!f)return;
  f.text().then(function(t){
    var d=JSON.parse(t);if(!Array.isArray(d))throw 0;
    var q=Promise.resolve();
    d.forEach(function(pr){
      if(!pr||typeof pr.slot!=='number'||pr.slot<0||pr.slot>7||!pr.name)return;
      // Truncate by codepoints, not UTF-16 units: substring(0,15) can split an
      // emoji surrogate pair, and encodeURIComponent throws URIError on lone
      // surrogates — which killed the whole import of a legitimately exported file.
      var u='/savepreset?slot='+pr.slot+'&name='+encodeURIComponent(Array.from(String(pr.name)).slice(0,15).join(''));
      ['b','c','co','sp','bl','th'].forEach(function(k){if(typeof pr[k]==='number')u+='&'+k+'='+pr[k];});
      q=q.then(function(){return xf(u).catch(function(){});});
    });
    q.then(fetchPresets,fetchPresets);
  }).catch(function(){alert(ru?'Неверный файл пресетов':'Invalid presets file');});
});
fetchPresets();
