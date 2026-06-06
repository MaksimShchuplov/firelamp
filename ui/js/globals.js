var DD={
 sb:{en:[[0,0,'Off'],[1,25,'Very dim'],[26,50,'Dim'],[51,75,'Medium'],[76,95,'Bright'],[96,100,'Full brightness']],
     ru:[[0,0,'Выключено'],[1,25,'Очень тускло'],[26,50,'Тускло'],[51,75,'Средняя'],[76,95,'Ярко'],[96,100,'Максимум яркости']]},
 sc:{en:[[0,25,'Yellows and whites'],[26,55,'Warm balanced'],[56,80,'Saturated oranges'],[81,100,'Deep reds only']],
     ru:[[0,25,'Жёлтые и белые тона'],[26,55,'Тёплые сбалансированные'],[56,80,'Насыщенные оранжевые'],[81,100,'Только глубокий красный']]},
 sco:{en:[[20,40,'Very tall flames'],[41,70,'Tall flames'],[71,105,'Medium flames'],[106,135,'Short sparks'],[136,150,'Quick embers']],
      ru:[[20,40,'Очень высокое пламя'],[41,70,'Высокое пламя'],[71,105,'Среднее пламя'],[106,135,'Короткие искры'],[136,150,'Быстрое тление']]},
 ssp:{en:[[0,40,'Calm smoldering'],[41,90,'Steady flame'],[91,160,'Active fire'],[161,220,'Hot inferno'],[221,255,'Raging maximum']],
      ru:[[0,40,'Тихое тление'],[41,90,'Ровное пламя'],[91,160,'Активный огонь'],[161,220,'Жаркое пламя'],[221,255,'Бушующий максимум']]},
 sbl:{en:[[0,20,'Frozen glow'],[21,60,'Slow motion'],[61,120,'Soft glow'],[121,200,'Natural fire'],[201,255,'Sharp flicker']],
      ru:[[0,20,'Застывшее свечение'],[21,60,'Замедленное движение'],[61,120,'Мягкое свечение'],[121,200,'Естественный огонь'],[201,255,'Резкое мерцание']]}
};
var vb=document.getElementById('vb'),sb=document.getElementById('sb');
var vc=document.getElementById('vc'),sc=document.getElementById('sc');
var vco=document.getElementById('vco'),sco=document.getElementById('sco');
var vsp=document.getElementById('vsp'),ssp=document.getElementById('ssp');
var vbl=document.getElementById('vbl'),sbl=document.getElementById('sbl');
var R=document.documentElement,t1,t2,t3,t4,t5;
var xf=function(u,o){var h=Object.assign({'X-Requested-With':'firelamp'},o&&o.headers);return fetch(u,Object.assign({},o,{headers:h}));};
var ru=(localStorage.getItem('lang')==='ru')||(!localStorage.getItem('lang')&&navigator.language.startsWith('ru'));
var pullFails=0;
var pendingSlot=-1,lastAiName='',aiNmTid=null;
