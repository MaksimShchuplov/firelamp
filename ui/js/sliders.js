function vib(v,min,max){if(navigator.vibrate&&(v==min||v==max))navigator.vibrate(15);}
sb.addEventListener('input',function(){pb(+sb.value);vib(+sb.value,0,100);var v=sb.value;lastIn=Date.now();clearTimeout(t1);t1=setTimeout(function(){xf('/setb?v='+v);},120);clearActive();});
sc.addEventListener('input',function(){pc(+sc.value);vib(+sc.value,0,100);var v=sc.value;lastIn=Date.now();clearTimeout(t2);t2=setTimeout(function(){xf('/setc?v='+v);},120);clearActive();});
sco.addEventListener('input',function(){pco(+sco.value);vib(+sco.value,20,150);var v=sco.value;lastIn=Date.now();clearTimeout(t3);t3=setTimeout(function(){xf('/setco?v='+v);},120);clearActive();});
ssp.addEventListener('input',function(){psp(+ssp.value);vib(+ssp.value,0,255);var v=ssp.value;lastIn=Date.now();clearTimeout(t4);t4=setTimeout(function(){xf('/setsp?v='+v);},120);clearActive();});
sbl.addEventListener('input',function(){pbl(+sbl.value);vib(+sbl.value,0,255);var v=sbl.value;lastIn=Date.now();clearTimeout(t5);t5=setTimeout(function(){xf('/setbl?v='+v);},120);clearActive();});
[0,1,2,3].forEach(function(t){document.getElementById('tb'+t).onclick=function(){if(navigator.vibrate)navigator.vibrate(10);xf('/settheme?v='+t).then(pull).catch(function(){});clearActive();};});
document.getElementById('rst').onclick=function(){xf('/reset').then(pull).catch(function(){});clearActive();};
