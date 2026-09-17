(() => {
'use strict';

const $ = (s) => document.querySelector(s);
const canvas = $('#game');
const ctx = canvas.getContext('2d');
const W = canvas.width;
const H = canvas.height;
const FOV = Math.PI / 3;
const RAYS = 460;
const MAX_DEPTH = 24;
const TAU = Math.PI * 2;

const BASE_MAP = [
  '11111111111111111111','10000000000000000051','10111101111101111001','10100001000301000001','10102221011101011101',
  '10100001010001010001','10101111010001010101','10001000010000010101','11101011111111010101','10001010000001000101',
  '10111010111001111101','10100000101000000001','10101111101111101101','10100004000000100001','10101111111110101101',
  '10100000000000100001','10111101111111111001','10000300000000000001','10000000000000000001','11111111111111111111'
];
let map = [];

const roles = {
  Breacher: { code:'BR', gadget:'BREACH KIT', desc:'Reinforced entry and close-quarters access.', specialty:'FORCED ENTRY', utility:'BREACH CHARGE', mobility:'STANDARD', accuracy:1 },
  Recon: { code:'RC', gadget:'RECON DRONE', desc:'Surveillance, route intelligence and threat marking.', specialty:'SURVEILLANCE', utility:'MICRO DRONE', mobility:'FAST', accuracy:1 },
  Tech: { code:'TX', gadget:'SIGNAL OVERRIDE', desc:'Electronic intrusion and security-system control.', specialty:'ELECTRONIC WARFARE', utility:'SIGNAL TAP', mobility:'STANDARD', accuracy:.95 },
  Support: { code:'SP', gadget:'MEDICAL KIT', desc:'Field stabilization and sustained squad capability.', specialty:'SUSTAINMENT', utility:'TRAUMA KIT', mobility:'STANDARD', accuracy:1 },
  Marksman: { code:'MR', gadget:'RECON OPTICS', desc:'Precision overwatch and long-lane reconnaissance.', specialty:'OVERWATCH', utility:'RECON OPTICS', mobility:'LIGHT', accuracy:.58 }
};

let selectedRole = 'Breacher';
let keys = {};
let mouseDown = false;
let deployed = false;
let paused = false;
let showMap = false;
let last = performance.now();
let markerIndex = 0;
let markerType = 'ENTRY';
const markerTypes = ['ENTRY','ROUTE','BREACH','SNIPER','RALLY'];
let planMarkers = [{x:1.8,y:1.7,label:'ALPHA // MAIN APPROACH',type:'ENTRY'}];
let game;
let audioCtx = null;
let hudAccumulator = 0;
let lastCompassBin = -1;
let once = {};
let matchSession = {round:1,d9:0,helix:0,maxRounds:9,roundsToWin:5,connectionState:'CONNECTED',sessionId:`SP-${Math.random().toString(36).slice(2,10).toUpperCase()}`,serverId:'ACC-01',tickRate:60,ping:34,loss:.1,reconnectGrace:90};
let localReady=false;
let selectedSpawn='ALPHA';
const SPAWN_GROUPS={
  ATTACK:[
    {id:'ALPHA',name:'WEST SERVICE',detail:'Low-visibility service gate',x:1.7,y:1.7,a:.15},
    {id:'BRAVO',name:'SOUTH MOTOR POOL',detail:'Fast access to utility corridors',x:2.2,y:17.4,a:-.35},
    {id:'CHARLIE',name:'EAST ANNEX',detail:'Longer route with alternate breach',x:17.6,y:17.2,a:-2.7}
  ],
  DEFENSE:[
    {id:'ALPHA',name:'ARCHIVE CORE',detail:'Close to primary intelligence vault',x:7.4,y:14.4,a:-1.45},
    {id:'BRAVO',name:'SECURITY HUB',detail:'Camera-network control corridor',x:15.4,y:9.6,a:Math.PI},
    {id:'CHARLIE',name:'DIPLOMATIC WING',detail:'Flexible rotation to west approaches',x:12.7,y:5.5,a:2.2}
  ]
};
const COMPETITIVE_ROSTERS={
  D9:[['YOU','BREACHER'],['VANTA','RECON'],['ROOK','TECH'],['MERCY','SUPPORT'],['LONGSHOT','MARKSMAN']],
  HELIX:[['CIPHER','TECH'],['RAPTOR','BREACHER'],['MOTH','RECON'],['WARDEN','SUPPORT'],['SHADE','MARKSMAN']]
};
const OBJECTIVE_SITES=[
  {id:'ARCHIVE-A',name:'ARCHIVE CORE',x:7.5,y:13.5},
  {id:'VAULT-B',name:'SECURITY VAULT',x:15.5,y:9.5},
  {id:'SAFE-C',name:'DIPLOMATIC SAFE',x:12.5,y:5.5}
];

const EMBASSY_ZONES=[
  {name:'WEST SERVICE',x1:0,y1:0,x2:7,y2:8,surface:'CONCRETE',light:'COOL'},
  {name:'DIPLOMATIC WING',x1:7,y1:0,x2:19,y2:8,surface:'MARBLE',light:'WARM'},
  {name:'SECURITY HUB',x1:10,y1:7,x2:19,y2:13,surface:'METAL',light:'LOW'},
  {name:'ARCHIVE CORE',x1:5,y1:11,x2:14,y2:18,surface:'CARPET',light:'LOW'},
  {name:'MOTOR POOL',x1:0,y1:8,x2:7,y2:19,surface:'GRAVEL',light:'COOL'},
  {name:'EAST ANNEX',x1:14,y1:12,x2:19,y2:19,surface:'CONCRETE',light:'WARM'}
];
const SURFACE_AUDIO={CONCRETE:[104,.016],MARBLE:[148,.014],METAL:[208,.014],CARPET:[72,.007],GRAVEL:[92,.018]};
function environmentAt(x,y){return EMBASSY_ZONES.find(z=>x>=z.x1&&x<z.x2&&y>=z.y1&&y<z.y2)||{name:'EMBASSY TRANSIT',surface:'CONCRETE',light:'COOL'};}
function currentFov(p){return p&&p.ads?(p.optic==='MAGNIFIER'?Math.PI/5.35:Math.PI/3.75):FOV;}
function partialDoorBlocks(x,y){const tx=Math.floor(x),ty=Math.floor(y),fx=x-tx,fy=y-ty;return ((tx+ty)&1)?fx<.30:fy<.30;}
function sightBlocked(x,y){const v=map[Math.floor(y)]?.[Math.floor(x)]??'1';if(v==='7')return partialDoorBlocks(x,y);return v==='1'||v==='2'||v==='6';}


const menu = $('#menu');
const rolePanel = $('#rolePanel');
const planning = $('#planning');
const readyRoom = $('#readyRoom');
const hud = $('#hud');
const endScreen = $('#endScreen');
const resumeOverlay = $('#resumeOverlay');
const planCanvas = $('#planMap');
const planCtx = planCanvas.getContext('2d');
const miniMap = $('#miniMap');
const miniCtx = miniMap.getContext('2d');

function cloneMap(){ return BASE_MAP.map(row => row.split('')); }
function clamp(v,min,max){ return Math.max(min,Math.min(max,v)); }
function dist(a,b){ return Math.hypot(a.x-b.x,a.y-b.y); }
function angleDelta(a,b){ return Math.atan2(Math.sin(a-b),Math.cos(a-b)); }
function rand(min,max){ return min + Math.random()*(max-min); }

function resetGame(){
  map = cloneMap();
  const side = matchSession.round % 2 === 1 ? 'ATTACK' : 'DEFENSE';
  const defending = side === 'DEFENSE';
  const sg=currentSpawnGroup();
  const pStart = {x:sg.x,y:sg.y,a:sg.a};
  const objectiveSite=objectiveSiteForRound();
  const enemySeeds = defending ? [
    {x:17.2,y:17.2,id:'D9-01'},{x:17.2,y:2.4,id:'D9-02'},{x:2.5,y:16.8,id:'D9-03'},{x:2.6,y:2.5,id:'D9-04'},{x:10.5,y:18.1,id:'D9-05'}
  ] : [
    {x:13.4,y:3.5,id:'H-01'},{x:16.3,y:6.4,id:'H-02'},{x:11.4,y:12.6,id:'H-03'},{x:7.7,y:15.7,id:'H-04'},{x:15.8,y:17.5,id:'H-05'}
  ];
  game = {
    phase:'PREPARATION', time:defending?360:900, prepTime:8, actionStarted:false, objectiveRevealed:defending, secured:false, camerasDisabled:false,
    win:false, matchComplete:false, spectating:false, spectatorTime:0, spectatorMode:'FOLLOW', spectatorIndex:0, score:0, intel:0, alert:0, contact:false, alarmLevel:0, exposure:0, alertHold:0, reinforcements:0, overtimeUsed:false, missionStarted:performance.now(), timeline:[],
    match:{round:matchSession.round,attackScore:matchSession.d9,defendScore:matchSession.helix,side,state:'PREPARATION',spawnGroup:sg.id},
    playerSlots:[...COMPETITIVE_ROSTERS.D9.map((x,i)=>({team:'D9',slot:i,callsign:x[0],ready:i?true:localReady,connection:'connected'})),...COMPETITIVE_ROSTERS.HELIX.map((x,i)=>({team:'HELIX',slot:i+5,callsign:x[0],ready:true,connection:'connected'}))],
    stats:{shots:0,hits:0,headshots:0,assists:0,breaches:0,hacks:0,doors:0,partialDoors:0,damageTaken:0,neutralized:0,teamDamage:0,friendlyIncidents:0,validatedShots:0,flashes:0,smokes:0,squadCommands:0,barricades:0,penetrations:0,suppressionEvents:0,camerasDestroyed:0,lightsDestroyed:0,vaults:0,verticalRoutes:0,inspections:0},
    player:{x:pStart.x,y:pStart.y,a:pStart.a,hp:100,ammo:30,reserve:120,reloading:0,reloadTotal:1.55,crouch:false,ads:false,sprinting:false,stamina:100,footstepCd:0,armInjury:false,legInjury:false,shotCd:0,drone:0,recoil:0,bob:0,moveBlend:0,suppressed:0,throwable:'FLASH',flash:2,smoke:2,throwCd:0,lean:0,leanTarget:0,vault:0,inspect:0,optic:'REFLEX',floor:'L1'},
    enemies:enemySeeds.map((q,i)=>({x:q.x,y:q.y,hp:100,state:defending?'assault':(i===2?'guard':'patrol'),a:Math.PI,tx:defending?11.5:(i===0?13:16),ty:defending?3.5:(i===0?7:11),cd:0,id:q.id,pathCd:0,capture:0,carrier:false})),
    terminals:[
      {x:11.5,y:3.5,used:false,type:'COMMS TERMINAL',id:'D9-COMM-7'},
      {x:4.5,y:17.5,used:false,type:'SECURITY NODE',id:'SEC-NODE-2'}
    ],
    cameras:[{x:8.5,y:3.5,a:2.65,base:2.65,phase:0,destroyed:false,id:'CAM-W1'},{x:15.5,y:9.5,a:Math.PI,base:Math.PI,phase:2.1,destroyed:false,id:'CAM-S2'},{x:8.5,y:15.5,a:-1.2,base:-1.2,phase:4.2,destroyed:false,id:'CAM-A3'}],
    props:[{x:2.8,y:4.8,kind:'crate'},{x:3.4,y:4.8,kind:'crate'},{x:9.3,y:11.5,kind:'server'},{x:9.8,y:11.5,kind:'server'},{x:14.5,y:14.5,kind:'desk'},{x:6.4,y:7.4,kind:'lamp',destroyed:false,id:'LGT-D1'},{x:16.5,y:4.5,kind:'lamp',destroyed:false,id:'LGT-D2'},{x:5.5,y:15.5,kind:'crate'},{x:12.2,y:6.4,kind:'cover'},{x:15.7,y:12.5,kind:'cover'},{x:6.8,y:14.1,kind:'cover'},{x:10.4,y:4.5,kind:'column'},{x:13.6,y:5.3,kind:'planter'},{x:16.2,y:8.4,kind:'locker'},{x:7.2,y:12.7,kind:'archive'},{x:3.4,y:10.4,kind:'bollard'},{x:12.1,y:15.2,kind:'display'},{x:17.1,y:11.4,kind:'stairs',id:'STAIR-A'},{x:9.2,y:17.2,kind:'stairs',id:'STAIR-B'},{x:11.9,y:5.2,kind:'sofa'},{x:13.1,y:5.2,kind:'sofa'},{x:10.6,y:6.3,kind:'sculpture'}],
    squad:defending ? [
      {id:'RAPTOR',x:pStart.x-.5,y:pStart.y+.5,hp:100,state:'follow',cd:0},{id:'MOTH',x:pStart.x+.5,y:pStart.y+.5,hp:100,state:'follow',cd:0},{id:'WARDEN',x:pStart.x-.7,y:pStart.y-.4,hp:100,state:'follow',cd:0},{id:'SHADE',x:pStart.x+.7,y:pStart.y-.4,hp:100,state:'follow',cd:0}
    ] : [
      {id:'VANTA',x:pStart.x-.5,y:pStart.y+.5,hp:100,state:'follow',cd:0},{id:'ROOK',x:pStart.x+.5,y:pStart.y+.5,hp:100,state:'follow',cd:0},{id:'MERCY',x:pStart.x-.7,y:pStart.y-.4,hp:100,state:'follow',cd:0},{id:'LONGSHOT',x:pStart.x+.7,y:pStart.y-.4,hp:100,state:'follow',cd:0}
    ],
    grenades:[], smokes:[], barricades:{}, barricadeStock:defending?3:0,
    defense:{enemyIntelRevealed:false,enemySecured:false,carrierId:null},
    objective:{x:objectiveSite.x,y:objectiveSite.y,id:objectiveSite.id,name:objectiveSite.name}, extraction:defending?{x:18.2,y:1.7}:{x:18.5,y:1.5}, noises:[],
    network:{authenticated:true,sessionId:matchSession.sessionId,serverId:matchSession.serverId,tickRate:matchSession.tickRate,ping:matchSession.ping,loss:matchSession.loss,jitter:3,reconnectRemaining:matchSession.reconnectGrace,validatedShots:0,lastAck:performance.now()},
    reverseFriendlyFire:false,
    particles:[],screenParticles:[],lightingPulse:0,sector:null,lastSurface:null,
    ambience:{flicker:0,breachShock:0},
    freeCam:{x:pStart.x,y:pStart.y,a:pStart.a},
    verticalLinks:[{a:{x:17.1,y:11.4,floor:'L1',label:'SECURITY STAIR'},b:{x:9.2,y:17.2,floor:'L2',label:'ARCHIVE UPPER'}}]
  };
  keys = {};
  mouseDown = false;
  showMap = false;
  paused = false;
  once = {};
  hudAccumulator=0;lastCompassBin=-1;game.sector=environmentAt(pStart.x,pStart.y);game.lastSector=game.sector.name;game.lastSurface=game.sector.surface;updateHud();
}
resetGame();

function panel(p){
  document.querySelectorAll('.panel').forEach(el => el.classList.remove('show'));
  if(p) p.classList.add('show');
}

function initAudio(){
  if(audioCtx) return;
  const AC = window.AudioContext || window.webkitAudioContext;
  if(AC) audioCtx = new AC();
}
function tone(freq=440,dur=.05,type='sine',gain=.025){
  if(!audioCtx) return;
  const o=audioCtx.createOscillator(), g=audioCtx.createGain();
  o.type=type;o.frequency.value=freq;g.gain.setValueAtTime(gain,audioCtx.currentTime);g.gain.exponentialRampToValueAtTime(.0001,audioCtx.currentTime+dur);
  o.connect(g);g.connect(audioCtx.destination);o.start();o.stop(audioCtx.currentTime+dur);
}
function noiseBurst(dur=.06,gain=.035){
  if(!audioCtx) return;
  const len=Math.max(1,Math.floor(audioCtx.sampleRate*dur)),buffer=audioCtx.createBuffer(1,len,audioCtx.sampleRate),data=buffer.getChannelData(0);
  for(let i=0;i<len;i++)data[i]=(Math.random()*2-1)*(1-i/len);
  const src=audioCtx.createBufferSource(),g=audioCtx.createGain(),f=audioCtx.createBiquadFilter();
  f.type='lowpass';f.frequency.value=1600;g.gain.value=gain;src.buffer=buffer;src.connect(f);f.connect(g);g.connect(audioCtx.destination);src.start();
}
function uiBeep(ok=true){tone(ok?720:180,.055,'square',.018);setTimeout(()=>tone(ok?940:145,.045,'square',.012),55)}
function footstep(surface='CONCRETE',intensity=1){
  if(!audioCtx)return;const spec=SURFACE_AUDIO[surface]||SURFACE_AUDIO.CONCRETE;
  tone(spec[0]*(.94+Math.random()*.12),.035,'triangle',spec[1]*intensity);
  if(surface==='GRAVEL')noiseBurst(.025,.008*intensity);
  if(surface==='METAL')setTimeout(()=>tone(spec[0]*1.7,.025,'square',.006*intensity),20);
}
function spawnWorldBurst(x,y,type='dust',count=10){
  if(!game)return;for(let i=0;i<count;i++)game.particles.push({x:x+rand(-.12,.12),y:y+rand(-.12,.12),z:rand(.05,.75),vx:rand(-.75,.75),vy:rand(-.75,.75),vz:rand(.3,1.5),life:rand(.35,.95),max:1,type});
}
function spawnCasing(){
  if(!game)return;game.screenParticles.push({x:W*.62,y:H*.64,vx:rand(90,180),vy:rand(-160,-80),g:470,life:.6,max:.6,type:'casing',rot:rand(0,TAU),vr:rand(-12,12)});
}
function updateFeedback(dt){
  if(!game)return;const p=game.player,env=environmentAt(p.x,p.y);game.sector=env;
  if(game.lastSurface!==env.surface||game.lastSector!==env.name){game.lastSurface=env.surface;game.lastSector=env.name;const ribbon=$('#locationRibbon');if(ribbon){$('#locationName').textContent=env.name;$('#surfaceName').textContent=`${env.surface} // ${p.floor}`;ribbon.classList.remove('pulse');void ribbon.offsetWidth;ribbon.classList.add('pulse')}const top=$('#envTop');if(top)top.textContent=`EMBASSY // ${env.name}`;}
  game.lightingPulse=Math.max(0,game.lightingPulse-dt*4.5);game.ambience.breachShock=Math.max(0,game.ambience.breachShock-dt*2.8);
  game.particles.forEach(q=>{q.life-=dt;q.x+=q.vx*dt;q.y+=q.vy*dt;q.z+=q.vz*dt;q.vz-=2.4*dt;q.vx*=.98;q.vy*=.98});game.particles=game.particles.filter(q=>q.life>0&&q.z>-0.15);
  game.screenParticles.forEach(q=>{q.life-=dt;q.x+=q.vx*dt;q.y+=q.vy*dt;q.vy+=q.g*dt;q.rot+=q.vr*dt});game.screenParticles=game.screenParticles.filter(q=>q.life>0);
  const sup=$('#suppressionFX');if(sup)sup.classList.toggle('active',p.suppressed>.12);
  const light=$('#lightFX');if(light)light.classList.toggle('active',game.lightingPulse>.05||game.ambience.breachShock>.1);
  document.body.classList.toggle('alert3',game.alarmLevel>=3);
}
function drawWorldParticles(p,depth,pitch){
  for(const q of game.particles){const proj=spriteProjection(q,p,depth,pitch);if(!proj)continue;const a=clamp(q.life/q.max,0,1),r=Math.max(1,proj.size*.018);ctx.save();ctx.globalAlpha=a*(q.type==='spark'?1:.52);ctx.fillStyle=q.type==='spark'?'#efb463':'#9aa0a0';ctx.beginPath();ctx.arc(proj.sx,proj.y+proj.size*.58-q.z*proj.size*.18,r*(q.type==='spark'?.55:1.5),0,TAU);ctx.fill();ctx.restore()}
}
function drawScreenParticles(){
  for(const q of game.screenParticles){ctx.save();ctx.globalAlpha=clamp(q.life/q.max,0,1);ctx.translate(q.x,q.y);ctx.rotate(q.rot);ctx.fillStyle=q.type==='casing'?'#8d7443':'#bbc3c6';ctx.fillRect(-5,-1.5,10,3);ctx.restore()}
}


function currentSpawnGroup(){
  const side=(matchSession.round%2===1)?'ATTACK':'DEFENSE';
  const groups=SPAWN_GROUPS[side]||SPAWN_GROUPS.ATTACK;
  const requested=groups.find(g=>g.id===selectedSpawn)||groups[0];
  const valid=!solidForSpawn(requested.x,requested.y);
  return valid?requested:groups.find(g=>!solidForSpawn(g.x,g.y))||groups[0];
}
function solidForSpawn(x,y){const v=BASE_MAP[Math.floor(y)]?.[Math.floor(x)]??'1';return v==='1'||v==='2';}
function objectiveSiteForRound(){return OBJECTIVE_SITES[(matchSession.round-1)%OBJECTIVE_SITES.length];}
function serverValidateShot(clientTime){
  if(matchSession.connectionState!=='CONNECTED')return {accepted:false,reason:'NETWORK UNSYNCED'};
  const now=performance.now(),age=now-clientTime;
  if(age>250||age<-25)return {accepted:false,reason:'SHOT TIMESTAMP REJECTED'};
  game.network.validatedShots++;game.network.lastAck=now;return {accepted:true,rewindMs:Math.min(120,matchSession.ping/2)};
}
function renderReadyRoom(){
  const make=(team)=>COMPETITIVE_ROSTERS[team].map((slot,i)=>{
    const local=team==='D9'&&i===0,ready=!local||localReady;
    const role=local?selectedRole.toUpperCase():slot[1];
    return `<div class="rosterSlot ${local?'local ':''}${ready?'':'waiting'}"><span class="slotNo">${String(i+1).padStart(2,'0')}</span><b class="callsign">${slot[0]}</b><span class="roleTag">${role}</span><span class="readyTag">${ready?'READY':'WAITING'}</span><i class="sessionPip" title="Authenticated session"></i></div>`;
  }).join('');
  $('#rosterD9').innerHTML=make('D9');$('#rosterHelix').innerHTML=make('HELIX');
  $('#spawnOptions').innerHTML=SPAWN_GROUPS.ATTACK.map(g=>`<button class="spawnOption ${selectedSpawn===g.id?'selected':''}" data-spawn="${g.id}"><b>${g.id} // ${g.name}</b><span>${g.detail}</span></button>`).join('');
  document.querySelectorAll('.spawnOption').forEach(b=>b.onclick=()=>{selectedSpawn=b.dataset.spawn;uiBeep();renderReadyRoom()});
  $('#readyBtn').classList.toggle('ready',localReady);$('#readyBtn').textContent=localReady?'READY // LOCKED':'MARK READY';
  $('#planningContinue').disabled=!localReady;
  const ns=$('#networkState');if(ns){ns.className=`networkState ${matchSession.connectionState.toLowerCase()}`;ns.querySelector('b').textContent=matchSession.connectionState}
  const auth=$('#authState');if(auth){auth.className=`networkState ${matchSession.connectionState==='CONNECTED'?'connected':'reconnecting'}`;auth.querySelector('b').textContent=matchSession.connectionState==='CONNECTED'?'VERIFIED':'RESERVED'}
}
function setConnectionState(state){
  matchSession.connectionState=state;renderReadyRoom();
  if(!game)return;
  if(state==='RECONNECTING'){game.network.reconnectRemaining=matchSession.reconnectGrace;game.network.authenticated=true;$('#reconnectBanner').classList.remove('hidden');feed('NETWORK DEGRADED // RECONNECT TOKEN RESERVED // 90S GRACE');}
  if(state==='CONNECTED'){game.network.authenticated=true;$('#reconnectBanner').classList.add('hidden');feedOnce('reconnected','NETWORK LINK RESTORED // AUTHENTICATED SLOT RECLAIMED');}
  if(state==='DISCONNECTED'){game.network.authenticated=false;$('#reconnectBanner').classList.remove('hidden');}
}
function addKillFeed(killer,victim,weapon='AR-7C',friendly=true,flags={}){
  const root=$('#killFeed');if(!root)return;const d=document.createElement('div');
  d.className=`killLine ${friendly?'team':'enemy'}${flags.headshot?' headshot':''}${flags.assist?' assist':''}${flags.friendlyFire?' friendlyfire':''}`;
  d.innerHTML=`<span class="killer">${killer}</span><span class="weapon">${weapon}</span><span class="victim">${victim}</span>`;root.prepend(d);while(root.children.length>6)root.lastElementChild.remove();setTimeout(()=>d.remove(),5600);
}
function matchPointState(){
  if(matchSession.d9===matchSession.roundsToWin-1&&matchSession.helix===matchSession.roundsToWin-1)return 'DECIDING ROUND';
  if(matchSession.d9===matchSession.roundsToWin-1)return 'D9 MATCH POINT';
  if(matchSession.helix===matchSession.roundsToWin-1)return 'HELIX MATCH POINT';
  return '';
}

function selectRole(name){
  selectedRole=name;
  document.querySelectorAll('.role').forEach(el => el.classList.toggle('selected',el.dataset.role===name));
  const r=roles[name];
  $('#roleDetail').innerHTML=`
    <div><small>ROLE PROFILE</small><b>${r.desc}</b></div>
    <div><small>SPECIALTY</small><b>${r.specialty}</b></div>
    <div><small>FIELD UTILITY</small><b>${r.utility}</b></div>
    <div><small>MOBILITY</small><b>${r.mobility}</b></div>`;
  $('#mapRole').textContent=name.toUpperCase();
  uiBeep(true);
}

const rolesEl=$('#roles');
Object.keys(roles).forEach(name=>{
  const r=roles[name], b=document.createElement('button');
  b.className='role'+(name===selectedRole?' selected':'');
  b.dataset.role=name;b.dataset.code=r.code;
  b.innerHTML=`<b>${name.toUpperCase()}</b><small>${r.desc}</small>`;
  b.onclick=()=>selectRole(name);
  rolesEl.appendChild(b);
});
selectRole(selectedRole);

$('#playBtn').onclick=()=>{initAudio();uiBeep();panel(rolePanel)};
$('#planningBtn').onclick=()=>{uiBeep();renderReadyRoom();panel(readyRoom)};
$('#readyBtn').onclick=()=>{localReady=!localReady;uiBeep(localReady);renderReadyRoom()};
$('#planningContinue').onclick=()=>{if(!localReady){uiBeep(false);return}uiBeep();panel(planning);drawPlan(planCtx,planCanvas.width,planCanvas.height,false)};
$('#deployBtn').onclick=()=>{
  if(!localReady){panel(readyRoom);renderReadyRoom();uiBeep(false);return}
  resetGame();
  initAudio();uiBeep();panel(null);hud.classList.remove('hidden');deployed=true;paused=false;showMap=false;
  resumeOverlay.classList.add('hidden');const ds=$('#deploySplash');ds.classList.remove('hidden');void ds.offsetWidth;setTimeout(()=>ds.classList.add('hidden'),3450);canvas.requestPointerLock?.();
  game.timeline.push({t:'00:00',e:'DEPLOYMENT'});game.squadCommand='FOLLOW';showPhaseBanner('PREPARATION PHASE',game.match.side==='DEFENSE'?'FORTIFY // POSITION // HOLD':'CHECK LOADOUT // ISSUE SQUAD ORDERS');feed(`DEPLOYMENT AUTHORIZED // ${game.match.side} PREPARATION ACTIVE`);
  setTimeout(()=>feed('D9 CONTROL // INTELLIGENCE HAS PRIORITY OVER CONTACT'),650);
};
$('#restartBtn').onclick=()=>{
  document.exitPointerLock?.();deployed=false;matchSession={round:1,d9:0,helix:0,maxRounds:9,roundsToWin:5,connectionState:'CONNECTED',sessionId:`SP-${Math.random().toString(36).slice(2,10).toUpperCase()}`,serverId:'ACC-01',tickRate:60,ping:34,loss:.1,reconnectGrace:90};localReady=false;selectedSpawn='ALPHA';planMarkers=[{x:1.8,y:1.7,label:'ALPHA // MAIN APPROACH',type:'ENTRY'}];markerIndex=0;markerType='ENTRY';$('#markerType').textContent='ENTRY';resetGame();hud.classList.add('hidden');endScreen.classList.remove('show');resumeOverlay.classList.add('hidden');$('#mapOverlay').classList.add('hidden');$('#flashFX').classList.remove('active');$('#smokeFX').classList.remove('active');$('#phaseBanner').classList.add('hidden');panel(menu);
};
$('#nextRoundBtn').onclick=()=>{
  if(game.matchComplete)return;matchSession.round++;selectedSpawn='ALPHA';endScreen.classList.remove('show');resetGame();deployed=true;paused=false;hud.classList.remove('hidden');showMap=false;$('#mapOverlay').classList.add('hidden');resumeOverlay.classList.add('hidden');const ds=$('#deploySplash');ds.classList.remove('hidden');void ds.offsetWidth;setTimeout(()=>ds.classList.add('hidden'),2600);showPhaseBanner(`ROUND ${String(matchSession.round).padStart(2,'0')} // ${game.match.side}`,game.match.side==='DEFENSE'?'FORTIFY // DENY INTELLIGENCE':'LOCATE // SECURE // EXTRACT');feed(`SIDE ROTATION // YOU ARE NOW ${game.match.side}`);canvas.requestPointerLock?.();
};
$('#closeInfo').onclick=()=>{uiBeep();panel(menu)};

const info = {
  operators:`<div class="eyebrow">OPERATORS // SPECTRE PERSONNEL</div><h2>TACTICAL SPECIALIZATIONS</h2><p>Shadow Protocol avoids fixed superhero characters. Operators remain customizable while equipment and training define battlefield capability.</p><div class="infoGrid"><div><small>ASSAULT</small><b>BREACHER</b></div><div><small>INTELLIGENCE</small><b>RECON</b></div><div><small>SYSTEMS</small><b>TECH</b></div><div><small>SUSTAINMENT</small><b>SUPPORT</b></div><div><small>OVERWATCH</small><b>MARKSMAN</b></div><div><small>DOCTRINE</small><b>TEAM FIRST</b></div></div>`,
  armory:`<div class="eyebrow">ARMORY // CONTROLLED ISSUE</div><h2>FICTIONAL MODERN PLATFORMS</h2><p>The training build uses the AR-7C Spectre Carbine. The production weapon system is designed around recoil, weight, handling, ammunition capacity, penetration, sound, reload speed and accuracy—without pay-to-win statistics.</p><div class="infoGrid"><div><small>PRIMARY</small><b>AR-7C</b></div><div><small>CALIBER</small><b>5.56 SIM</b></div><div><small>CAPACITY</small><b>30</b></div><div><small>MODE</small><b>FULL</b></div><div><small>OPTIC</small><b>1× REFLEX</b></div><div><small>STATUS</small><b>FIELD READY</b></div></div>`,
  intel:`<div class="eyebrow">INTELLIGENCE // SP-01 DOSSIER</div><h2>INFORMATION WINS WARS</h2><p>Communications terminals reveal the true objective. Security nodes suppress surveillance. Reconnaissance exposes movement. Intelligence changes tactical opportunity rather than acting as a decorative collectible.</p><div class="infoGrid"><div><small>COMMS</small><b>OBJECTIVE REVEAL</b></div><div><small>SECURITY</small><b>CAMERA DISABLE</b></div><div><small>RECOVERY</small><b>SP DATA</b></div><div><small>EXTRACTION</small><b>MISSION EXIT</b></div><div><small>PRIORITY</small><b>CLASSIFIED</b></div><div><small>RULE</small><b>KNOW FIRST</b></div></div>`
};
document.querySelectorAll('[data-info]').forEach(b=>b.onclick=()=>{initAudio();uiBeep();$('#infoContent').innerHTML=info[b.dataset.info];panel($('#infoPanel'))});

function drawPlan(target,cw,ch,includePlayer=false){
  target.save();target.clearRect(0,0,cw,ch);
  const sx=cw/20,sy=ch/20;
  target.fillStyle='#080c0e';target.fillRect(0,0,cw,ch);
  target.strokeStyle='rgba(98,137,150,.11)';target.lineWidth=1;
  for(let i=0;i<=20;i++){target.beginPath();target.moveTo(i*sx,0);target.lineTo(i*sx,ch);target.stroke();target.beginPath();target.moveTo(0,i*sy);target.lineTo(cw,i*sy);target.stroke()}
  for(let y=0;y<20;y++)for(let x=0;x<20;x++){
    const v=map[y][x];
    if(v==='0')continue;
    target.fillStyle=v==='1'?'#263038':v==='2'?'#6e202a':v==='3'?'#6a5226':v==='4'?'#264457':'#285740';
    target.fillRect(x*sx+1,y*sy+1,sx-2,sy-2);
    if(v==='1'){target.fillStyle='rgba(255,255,255,.025)';target.fillRect(x*sx+3,y*sy+3,sx-6,2)}
  }
  target.font=`${Math.max(9,cw/62)}px Consolas, monospace`;
  planMarkers.forEach((m,i)=>{
    const colors={ENTRY:'#e5e9eb',ROUTE:'#6fa5bd',BREACH:'#dc4652',SNIPER:'#d2a650',RALLY:'#70b18a'};
    target.strokeStyle=colors[m.type]||'#fff';target.fillStyle=colors[m.type]||'#fff';target.lineWidth=2;
    target.beginPath();target.arc(m.x*sx,m.y*sy,6,0,TAU);target.stroke();target.beginPath();target.arc(m.x*sx,m.y*sy,2,0,TAU);target.fill();
    target.fillStyle='#cbd1d5';target.fillText(`${String(i+1).padStart(2,'0')} // ${m.label}`,m.x*sx+10,m.y*sy-7);
  });
  // terminals / intel / exfil indicators
  target.strokeStyle='#b68a35';target.strokeRect(11.5*sx-5,3.5*sy-5,10,10);target.strokeRect(4.5*sx-5,17.5*sy-5,10,10);
  OBJECTIVE_SITES.forEach(site=>{target.strokeStyle=game?.objectiveRevealed&&game.objective.id===site.id?'#d7aa4b':'rgba(111,126,134,.35)';target.setLineDash(game?.objectiveRevealed&&game.objective.id===site.id?[]:[3,3]);target.strokeRect(site.x*sx-6,site.y*sy-6,12,12);target.setLineDash([]);if(!includePlayer){target.fillStyle='rgba(125,137,144,.45)';target.fillText(game?.objectiveRevealed&&game.objective.id===site.id?site.id:'?',site.x*sx+8,site.y*sy+3)}});
  if(includePlayer&&game){
    const p=game.player;
    target.save();target.translate(p.x*sx,p.y*sy);target.rotate(p.a);target.fillStyle='#eef2f3';target.beginPath();target.moveTo(9,0);target.lineTo(-5,-5);target.lineTo(-3,0);target.lineTo(-5,5);target.closePath();target.fill();target.restore();
    if(p.drone>0){game.enemies.filter(e=>e.hp>0).forEach(e=>{target.fillStyle='#d13d49';target.beginPath();target.arc(e.x*sx,e.y*sy,4,0,TAU);target.fill()})}
    if(game.objectiveRevealed&&!game.secured){target.strokeStyle='#d7aa4b';target.lineWidth=2;target.strokeRect(game.objective.x*sx-7,game.objective.y*sy-7,14,14)}
    if(game.secured){target.strokeStyle='#69b087';target.lineWidth=2;target.beginPath();target.arc(game.extraction.x*sx,game.extraction.y*sy,8,0,TAU);target.stroke()}
    if(!game.camerasDisabled){game.cameras.forEach(c=>{target.fillStyle='#8b5156';target.fillRect(c.x*sx-2,c.y*sy-2,4,4)})}
  }

  // Embassy competitive callouts used for planning and squad communication.
  const zones=[['WEST SERVICE',2.2,6.2],['DIPLOMATIC WING',13.1,5.6],['SECURITY HUB',14.8,10.2],['ARCHIVE CORE',7.2,13.8],['MOTOR POOL',3.5,17.2],['EAST ANNEX',16.4,16.6]];
  target.font=`${Math.max(7,cw/82)}px Consolas, monospace`;target.fillStyle='rgba(155,169,176,.5)';zones.forEach(z=>target.fillText(z[0],z[1]*sx,z[2]*sy));
  target.restore();
}

planCanvas.onclick=e=>{
  const r=planCanvas.getBoundingClientRect(),x=(e.clientX-r.left)/r.width*20,y=(e.clientY-r.top)/r.height*20;
  if(map[Math.floor(y)]?.[Math.floor(x)]==='0'){
    planMarkers.push({x,y,label:`${markerType} // ${planMarkers.length+1}`,type:markerType});
    drawPlan(planCtx,planCanvas.width,planCanvas.height,false);tone(620,.04,'square',.012);
  }
};
planCanvas.oncontextmenu=e=>{e.preventDefault();if(planMarkers.length>1){planMarkers.pop();drawPlan(planCtx,planCanvas.width,planCanvas.height,false);tone(220,.04,'square',.01)}};
$('#markerType').onclick=()=>{markerIndex=(markerIndex+1)%markerTypes.length;markerType=markerTypes[markerIndex];$('#markerType').textContent=markerType;uiBeep()};

function feed(txt){
  const d=document.createElement('div');d.className='feed';d.textContent=txt;$('#intelFeed').prepend(d);
  while($('#intelFeed').children.length>4)$('#intelFeed').lastElementChild.remove();
  setTimeout(()=>d.remove(),5200);
}
function feedOnce(k,t){if(!once[k]){once[k]=1;feed(t)}}
function showAlert(){const a=$('#alertBanner');a.classList.remove('show');void a.offsetWidth;a.classList.add('show');tone(170,.12,'sawtooth',.02)}
function showHit(){const h=$('#hitMarker');h.classList.remove('show');void h.offsetWidth;h.classList.add('show');tone(900,.035,'square',.01)}
function showMuzzle(){const m=$('#muzzleFlash');m.classList.remove('fire');void m.offsetWidth;m.classList.add('fire')}
function showDamage(){const d=$('#damageVignette');d.classList.add('hit');setTimeout(()=>d.classList.remove('hit'),130)}
function stamp(event){const elapsed=Math.max(0,900-game.time),m=Math.floor(elapsed/60),sec=Math.floor(elapsed%60);game.timeline.push({t:`${String(m).padStart(2,'0')}:${String(sec).padStart(2,'0')}`,e:event});if(game.timeline.length>12)game.timeline.shift()}
function spawnReinforcement(){if(game.reinforcements>=2)return;const spawns=[{x:17.2,y:17.2},{x:17.2,y:2.4},{x:2.5,y:16.8}],q=spawns[game.reinforcements%spawns.length];game.reinforcements++;game.enemies.push({x:q.x,y:q.y,hp:100,state:'search',a:Math.PI,tx:game.player.x,ty:game.player.y,cd:.8,id:`H-R${game.reinforcements}`});feed(`HELIX QRF DEPLOYED // REINFORCEMENT ${game.reinforcements}`);stamp('HELIX QRF ENTERED AO');showAlert()}
function setAlarm(level,reason=''){level=clamp(level,0,3);if(level===game.alarmLevel)return;game.alarmLevel=level;if(level>0){feed(`HELIX ALERT LEVEL ${level} // ${reason||'SECURITY RESPONSE'}`);stamp(`ALERT LEVEL ${level}`)}if(level>=2&&game.reinforcements<1)spawnReinforcement();if(level>=3&&game.reinforcements<2)setTimeout(()=>{if(game&&!game.win&&game.alarmLevel>=3)spawnReinforcement()},3500)}

function showPhaseBanner(main,sub='LOCATE // SECURE // EXTRACT'){
  const el=$('#phaseBanner');$('#phaseBannerTop').textContent=`ROUND ${String(game.match.round).padStart(2,'0')} // ${game.match.side}`;$('#phaseBannerMain').textContent=main;$('#phaseBannerSub').textContent=sub;
  el.classList.remove('hidden','show');void el.offsetWidth;el.classList.add('show');setTimeout(()=>el.classList.add('hidden'),2550);
}
function cycleThrowable(){
  const p=game.player;p.throwable=p.throwable==='FLASH'?'SMOKE':'FLASH';uiBeep(true);feed(`${p.throwable} GRENADE SELECTED`);updateHud();
}
function smokeObscures(a,b){
  for(const s of game.smokes){
    const vx=b.x-a.x,vy=b.y-a.y,len2=vx*vx+vy*vy||1,t=clamp(((s.x-a.x)*vx+(s.y-a.y)*vy)/len2,0,1),px=a.x+vx*t,py=a.y+vy*t;
    if(Math.hypot(px-s.x,py-s.y)<s.r*.82)return true;
  }return false;
}
function throwThrowable(){
  if(game.spectating)return;const p=game.player;if(!game.actionStarted){feed('TACTICAL EQUIPMENT LOCKED // WAIT FOR ACTION PHASE');uiBeep(false);return}if(p.throwCd>0)return;
  const stock=p.throwable==='FLASH'?p.flash:p.smoke;if(stock<=0){feed(`${p.throwable} INVENTORY EMPTY`);uiBeep(false);return}
  if(p.throwable==='FLASH')p.flash--;else p.smoke--;p.throwCd=.8;
  const speed=6.6, lift=4.4;
  game.grenades.push({x:p.x+Math.cos(p.a)*.28,y:p.y+Math.sin(p.a)*.28,z:.7,vx:Math.cos(p.a)*speed,vy:Math.sin(p.a)*speed,vz:lift,type:p.throwable,t:1.35,bounces:0});
  noise(p.x,p.y,2.2,'throw');game.stats[p.throwable==='FLASH'?'flashes':'smokes']++;feed(`${p.throwable} OUT // ARC LIVE`);tone(310,.04,'square',.012);updateHud();
}
function detonateGrenade(g){
  if(g.type==='FLASH'){
    let affected=0;for(const e of game.enemies){if(e.hp<=0)continue;const d=dist(g,e);if(d<6.2&&los(g,e)){e.stunned=Math.max(e.stunned||0,3.2*(1-d/8));e.state='search';e.searchTimer=3.5;affected++}}
    game.squad.forEach(m=>{if(m.hp>0&&dist(g,m)<4)m.stunned=1.2});noise(g.x,g.y,7,'flash');noiseBurst(.12,.07);tone(1350,.13,'square',.04);if(dist(game.player,g)<5.2){const fx=$('#flashFX');fx.classList.remove('active');void fx.offsetWidth;fx.classList.add('active')}feed(`FLASH EFFECT // ${affected} ${game.match.side==='DEFENSE'?'DIRECTORATE':'HELIX'} SIGNATURE${affected===1?'':'S'} DISRUPTED`);if(affected){game.score+=affected*25;stamp('FLASH GRENADE EFFECTIVE')}
  }else{
    game.smokes.push({x:g.x,y:g.y,t:10,r:2.8});noise(g.x,g.y,2.5,'smoke');noiseBurst(.18,.025);feed('SMOKE SCREEN DEPLOYED // VISUAL CHANNEL DEGRADED');stamp('SMOKE DEPLOYED')
  }
}

function updateGrenades(dt){
  const done=[];
  for(const g of game.grenades){
    g.t-=dt;g.vz-=9.4*dt;let nx=g.x+g.vx*dt,ny=g.y+g.vy*dt,nz=g.z+g.vz*dt;
    if(solid(nx,ny)){g.vx*=-.34;g.vy*=-.34;g.vz=Math.max(1.1,g.vz*.4);g.bounces++;nx=g.x;ny=g.y;noise(g.x,g.y,1.6,'grenade bounce')}
    g.x=nx;g.y=ny;g.z=nz;
    if(g.z<=0){g.z=0;if(Math.abs(g.vz)>.8){g.vz=Math.abs(g.vz)*.34;g.vx*=.68;g.vy*=.68;g.bounces++}else{g.vz=0;g.vx*=.72;g.vy*=.72}}
    if(g.t<=0)done.push(g);
  }
  if(done.length){game.grenades=game.grenades.filter(g=>!done.includes(g));done.forEach(detonateGrenade)}
}
function fortify(){
  if(game.match.side!=='DEFENSE'){feed('FORTIFICATION AVAILABLE ON DEFENSE ROUNDS');uiBeep(false);return}
  if(game.actionStarted){feed('FORTIFICATION WINDOW CLOSED');uiBeep(false);return}
  if(game.barricadeStock<=0){feed('BARRICADE INVENTORY EMPTY');uiBeep(false);return}
  const t=nearTile(['2'],1.45);if(!t){feed('MOVE TO A REINFORCEABLE ENTRY');uiBeep(false);return}
  map[t.y][t.x]='6';game.barricades[`${t.x},${t.y}`]={hp:100};game.barricadeStock--;game.stats.barricades++;feed(`ENTRY FORTIFIED // ${game.barricadeStock} BARRICADE${game.barricadeStock===1?'':'S'} REMAINING`);stamp('DEFENSIVE BARRICADE PLACED');tone(185,.09,'square',.018);updateHud();
}
function navigationStep(from,target){
  const sx=Math.floor(from.x),sy=Math.floor(from.y),tx=Math.floor(target.x),ty=Math.floor(target.y);if(sx===tx&&sy===ty)return {x:target.x,y:target.y};
  const q=[[sx,sy]],prev=new Map([[`${sx},${sy}`,null]]);let head=0,found=false;
  const dirs=[[1,0],[-1,0],[0,1],[0,-1]];
  while(head<q.length&&!found){const [x,y]=q[head++];for(const [dx,dy] of dirs){const nx=x+dx,ny=y+dy,k=`${nx},${ny}`,v=map[ny]?.[nx];if(v==null||v==='1'||prev.has(k))continue;prev.set(k,[x,y]);q.push([nx,ny]);if(nx===tx&&ny===ty){found=true;break}}}
  const end=`${tx},${ty}`;if(!prev.has(end))return {x:target.x,y:target.y};let cur=[tx,ty],parent=prev.get(end),guard=0;while(parent&&!(parent[0]===sx&&parent[1]===sy)&&guard++<450){cur=parent;parent=prev.get(`${cur[0]},${cur[1]}`)}return {x:cur[0]+.5,y:cur[1]+.5,cellX:cur[0],cellY:cur[1]};
}
function damageBarricade(x,y,amount){const k=`${x},${y}`,b=game.barricades[k];if(!b)return false;b.hp-=amount;if(b.hp<=0){delete game.barricades[k];map[y][x]='0';noise(x+.5,y+.5,10,'barricade breach');spawnWorldBurst(x+.5,y+.5,'dust',18);game.ambience.breachShock=.7;feed('FORTIFICATION BREACHED // ENTRY COMPROMISED');stamp('BARRICADE DESTROYED');noiseBurst(.12,.05);return true}return false}
function shotPath(a,b){
  const dx=b.x-a.x,dy=b.y-a.y,d=Math.hypot(dx,dy),n=Math.ceil(d*18);let penetrations=0;
  for(let i=1;i<n;i++){const x=a.x+dx*i/n,y=a.y+dy*i/n,v=map[Math.floor(y)]?.[Math.floor(x)]??'1';if(v==='1')return {clear:false,penetrations};if(v==='2'||v==='6'||(v==='7'&&partialDoorBlocks(x,y))){penetrations++;if(penetrations>1)return {clear:false,penetrations}}}
  return {clear:true,penetrations};
}
function suppressNearMiss(a,shotAngle,hitTarget){
  for(const e of game.enemies){if(e.hp<=0||e===hitTarget)continue;const d=dist(a,e);if(d>13)continue;const da=Math.abs(angleDelta(Math.atan2(e.y-a.y,e.x-a.x),shotAngle));if(da<.035&&shotPath(a,e).clear){e.suppressed=Math.max(e.suppressed||0,1.4);e.state='retreat';game.stats.suppressionEvents++;}}
}
function updateDefenseAI(dt){
  const p=game.player;
  let active=0;
  for(const e of game.enemies){
    if(e.hp<=0)continue;active++;e.cd=Math.max(0,e.cd-dt);e.stunned=Math.max(0,(e.stunned||0)-dt);e.suppressed=Math.max(0,(e.suppressed||0)-dt);if(e.stunned>0)continue;
    const d=dist(e,p),sees=d<10.2&&los(e,p)&&!smokeObscures(e,p);
    let squadTarget=null,sd=99;for(const m of game.squad){if(m.hp<=0||!los(e,m)||smokeObscures(e,m))continue;const md=dist(e,m);if(md<sd){sd=md;squadTarget=m}}
    const combatTarget=sees?p:(sd<7?squadTarget:null);
    if(combatTarget){e.state='engage';e.a=Math.atan2(combatTarget.y-e.y,combatTarget.x-e.x);if(dist(e,combatTarget)>4)moveEnemy(e,Math.cos(e.a)*dt*.72,Math.sin(e.a)*dt*.72);if(e.cd<=0){e.cd=.72+Math.random()*.5;if(combatTarget===p)enemyShoot(e,d);else if(Math.random()<.62){combatTarget.hp-=rand(8,17);noise(e.x,e.y,8,'enemy gunfire');if(combatTarget.hp<=0){combatTarget.hp=0;addKillFeed(e.id,combatTarget.id,'AR-7C',false)}}}continue}
    const target=!game.defense.enemyIntelRevealed?game.terminals[0]:!game.defense.enemySecured?game.objective:game.extraction;
    e.pathCd=(e.pathCd||0)-dt;if(e.pathCd<=0){e.pathCd=.32+Math.random()*.12;e.nav=navigationStep(e,target)}const nav=e.nav||target;
    if(nav.cellX!=null&&map[nav.cellY]?.[nav.cellX]==='6'){
      const bd=Math.hypot(nav.x-e.x,nav.y-e.y);if(bd<.95){e.state='breach';damageBarricade(nav.cellX,nav.cellY,dt*36);continue}
    }
    const nd=Math.hypot(nav.x-e.x,nav.y-e.y);if(nd>.18){const a=Math.atan2(nav.y-e.y,nav.x-e.x);e.a=a;moveEnemy(e,Math.cos(a)*dt*(e.suppressed>.01?.42:.74),Math.sin(a)*dt*(e.suppressed>.01?.42:.74))}
    const td=dist(e,target);if(td<.82)e.capture=(e.capture||0)+dt;else e.capture=0;
    if(!game.defense.enemyIntelRevealed&&td<.82&&e.capture>=2){game.defense.enemyIntelRevealed=true;e.capture=0;feed('DIRECTORATE NINE BREACHED COMMS // OBJECTIVE EXPOSED');stamp('ATTACKERS ACQUIRED INTELLIGENCE');showAlert()}
    else if(game.defense.enemyIntelRevealed&&!game.defense.enemySecured&&td<.82&&e.capture>=3){game.defense.enemySecured=true;game.defense.carrierId=e.id;e.carrier=true;e.capture=0;feed(`DATA PACKAGE COMPROMISED // ${e.id} IS CARRYING INTELLIGENCE`);stamp('ATTACKERS SECURED SP DATA');showPhaseBanner('DATA COMPROMISED','INTERCEPT THE CARRIER BEFORE EXTRACTION')}
    else if(game.defense.enemySecured&&e.carrier&&td<.9){finish(false,'DIRECTORATE NINE EXTRACTED THE ENCRYPTED PACKAGE');return}
  }
  if(active===0)finish(true,'ATTACKING ELEMENT ELIMINATED — INTELLIGENCE SECURE');
}
function cycleSquadCommand(){
  const order=['FOLLOW','HOLD','ASSAULT'],cur=game.squadCommand||'FOLLOW';game.squadCommand=order[(order.indexOf(cur)+1)%order.length];game.stats.squadCommands++;uiBeep(true);feed(`SQUAD LEADER // ${game.squadCommand} ORDER ISSUED`);stamp(`SQUAD ${game.squadCommand}`);
}
function updateSquad(dt){
  const p=game.player,cmd=game.squadCommand||'FOLLOW';let idx=0;
  for(const m of game.squad){if(m.hp<=0)continue;m.cd=Math.max(0,(m.cd||0)-dt);m.stunned=Math.max(0,(m.stunned||0)-dt);if(m.stunned>0)continue;
    let target=null,bd=11;for(const e of game.enemies){if(e.hp<=0||smokeObscures(m,e)||!los(m,e))continue;const d=dist(m,e);if(d<bd){bd=d;target=e}}
    if(target){m.state='engaged';const a=Math.atan2(target.y-m.y,target.x-m.x);m.a=a;if(m.cd<=0){m.cd=.75+Math.random()*.45;target.hp-=16+Math.random()*10;noise(m.x,m.y,8,'squad gunfire');if(target.hp<=0){target.hp=0;game.stats.neutralized++;game.score+=70;const assisted=target.playerTaggedAt&&performance.now()-target.playerTaggedAt<8000;if(assisted){game.stats.assists++;game.score+=60;feed(`ASSIST // ${target.id} // +60`);addKillFeed(`YOU + ${m.id}`,target.id,'AR-7C',true,{assist:true})}else addKillFeed(m.id,target.id,'AR-7C',true);feed(`${m.id} // ${game.match.side==='DEFENSE'?'DIRECTORATE':'HELIX'} CONTACT NEUTRALIZED // +70`)}}}
    else{m.state=cmd.toLowerCase();let tx=m.x,ty=m.y;if(cmd==='FOLLOW'){const row=Math.floor(idx/2),back=1.05+row*.75,side=(idx%2?1:-1)*(.58+row*.12);tx=p.x-Math.cos(p.a)*back+Math.cos(p.a+Math.PI/2)*side;ty=p.y-Math.sin(p.a)*back+Math.sin(p.a+Math.PI/2)*side}else if(cmd==='ASSAULT'){const o=game.objectiveRevealed?game.objective:{x:11.5,y:3.5};tx=o.x;ty=o.y}
      if(cmd!=='HOLD'){const d=Math.hypot(tx-m.x,ty-m.y);if(d>.55){const a=Math.atan2(ty-m.y,tx-m.x);moveEnemy(m,Math.cos(a)*dt*1.45,Math.sin(a)*dt*1.45);m.a=a}}}
    idx++;
  }
}

function noise(x,y,r,type){game.noises.push({x,y,r,t:1.8,type})}
function solid(x,y){const v=map[Math.floor(y)]?.[Math.floor(x)]??'1';return v==='1'||v==='2'||v==='6'}
function tryMove(nx,ny){const p=game.player,r=.18;if(!solid(nx+r,p.y)&&!solid(nx-r,p.y))p.x=nx;if(!solid(p.x,ny+r)&&!solid(p.x,ny-r))p.y=ny}
function los(a,b){const dx=b.x-a.x,dy=b.y-a.y,d=Math.hypot(dx,dy),n=Math.ceil(d*14);for(let i=1;i<n;i++){const x=a.x+dx*i/n,y=a.y+dy*i/n;if(sightBlocked(x,y))return false}return true}
function nearTile(chars,range=1.1){const p=game.player;let best=null,bd=range;for(let y=0;y<map.length;y++)for(let x=0;x<map[y].length;x++)if(chars.includes(map[y][x])){const d=Math.hypot(x+.5-p.x,y+.5-p.y);if(d<bd){bd=d;best={x,y,v:map[y][x]}}}return best}

function nearestVerticalLink(){
  const p=game.player;let best=null,bd=1.05;for(const link of game.verticalLinks||[]){for(const side of ['a','b']){const q=link[side],d=Math.hypot(q.x-p.x,q.y-p.y);if(d<bd){bd=d;best={link,side,q}}}}return best;
}
function useVerticalRoute(){
  const n=nearestVerticalLink();if(!n)return false;const dest=n.side==='a'?n.link.b:n.link.a;game.player.x=dest.x;game.player.y=dest.y;game.player.floor=dest.floor;game.stats.verticalRoutes++;game.lightingPulse=.28;spawnWorldBurst(dest.x,dest.y,'dust',8);feed(`VERTICAL ROUTE // ${n.q.label} → ${dest.label} // ${dest.floor}`);stamp(`VERTICAL ROUTE ${dest.floor}`);tone(260,.06,'square',.012);return true;
}
function tryVault(){
  const p=game.player;if(!game.actionStarted||p.vault>0||p.reloading>0||p.sprinting)return false;let candidate=null,bd=1.05;for(const o of game.props){if(!['cover','bollard','planter','sofa'].includes(o.kind))continue;const d=dist(p,o);const da=Math.abs(angleDelta(Math.atan2(o.y-p.y,o.x-p.x),p.a));if(d<bd&&da<.72){bd=d;candidate=o}}if(!candidate)return false;const tx=p.x+Math.cos(p.a)*1.18,ty=p.y+Math.sin(p.a)*1.18;if(solid(tx,ty))return false;p.x=tx;p.y=ty;p.vault=.46;p.ads=false;p.sprinting=false;game.stats.vaults++;noise(p.x,p.y,2.2,'vault');feed('MOBILITY // LOW OBSTACLE VAULT');tone(155,.05,'square',.01);return true;
}
function inspectWeapon(){const p=game.player;if(p.reloading>0||p.sprinting||game.spectating)return;p.inspect=2.25;p.ads=false;game.stats.inspections++;feed(`WEAPON CHECK // AR-7C // ${p.optic} // MAG ${p.ammo}`);tone(330,.04,'square',.008)}
function toggleOptic(){const p=game.player;if(p.reloading>0||game.spectating)return;p.optic=p.optic==='REFLEX'?'MAGNIFIER':'REFLEX';feed(`OPTIC CONFIGURATION // ${p.optic}${p.optic==='MAGNIFIER'?' 2X':' 1X'}`);uiBeep(true)}
function acquireDeviceTarget(a){
  const p=game.player,cands=[];for(const c of game.cameras){if(c.destroyed)continue;const d=dist(p,c),da=Math.abs(angleDelta(Math.atan2(c.y-p.y,c.x-p.x),a));if(d<15&&da<Math.atan(.16/d)&&los(p,c))cands.push({kind:'camera',obj:c,d})}
  for(const o of game.props){if(o.kind!=='lamp'||o.destroyed)continue;const d=dist(p,o),da=Math.abs(angleDelta(Math.atan2(o.y-p.y,o.x-p.x),a));if(d<14&&da<Math.atan(.18/d)&&los(p,o))cands.push({kind:'light',obj:o,d})}
  cands.sort((x,y)=>x.d-y.d);return cands[0]||null;
}
function destroyDevice(t){
  if(!t)return false;t.obj.destroyed=true;if(t.kind==='camera'){game.stats.camerasDestroyed++;game.score+=60;spawnWorldBurst(t.obj.x,t.obj.y,'spark',14);feed(`SURVEILLANCE CAMERA DESTROYED // ${t.obj.id} // +60`);stamp(`CAMERA ${t.obj.id} DESTROYED`);if(game.cameras.every(c=>c.destroyed)){game.camerasDisabled=true;feed('CAMERA NETWORK BLIND // ALL LOCAL NODES DESTROYED')}}else{game.stats.lightsDestroyed++;game.score+=20;spawnWorldBurst(t.obj.x,t.obj.y,'spark',12);feed(`LIGHTING NODE DISABLED // ${t.obj.id} // SHADOW CORRIDOR CREATED`);stamp('LOCAL LIGHT DESTROYED')}game.lightingPulse=.22;tone(190,.05,'square',.012);return true;
}
function chooseCover(e,threat){
  const options=game.props.filter(o=>['cover','column','planter','sofa'].includes(o.kind));let best=null,score=1e9;for(const o of options){const d=Math.hypot(o.x-e.x,o.y-e.y);if(d>5.5)continue;const away=Math.hypot(o.x-threat.x,o.y-threat.y);const s=d-away*.22;if(s<score&&!solid(o.x,o.y)){score=s;best=o}}return best;
}
function reload(){
  const p=game.player;if(p.reloading>0||p.ammo>=30||p.reserve<=0)return;
  p.reloading=p.reloadTotal=1.55;feed('MAGAZINE CHANGE // MAINTAIN SECTOR');tone(240,.05,'square',.014);setTimeout(()=>tone(420,.04,'square',.01),520);setTimeout(()=>tone(610,.035,'square',.008),1120)
}
function shoot(){
  if(!deployed||paused||game.win||game.spectating)return;
  if(!game.actionStarted){feedOnce('prep-fire','WEAPONS SAFE // PREPARATION PHASE');return}
  const p=game.player;if(p.shotCd>0||p.reloading>0)return;
  if(p.ammo<=0){feedOnce('empty','MAGAZINE EMPTY // R TO RELOAD');tone(120,.035,'square',.018);return}
  const validation=serverValidateShot(performance.now());
  if(!validation.accepted){feedOnce('shot-reject',`SERVER REJECTED FIRE // ${validation.reason}`);uiBeep(false);return}
  p.ammo--;p.shotCd=.115;p.recoil=1;game.stats.shots++;game.stats.validatedShots++;showMuzzle();noiseBurst(.045,.055);tone(95,.045,'square',.025);noise(p.x,p.y,p.crouch?4.5:10.5,'gunshot');spawnCasing();game.lightingPulse=.16;if(Math.random()<.7)spawnWorldBurst(p.x+Math.cos(p.a)*.7,p.y+Math.sin(p.a)*.7,'spark',3);
  const accuracy=roles[selectedRole].accuracy*(p.armInjury?1.75:1)*(p.ads?.42:1)*(p.sprinting?1.8:1), a=p.a+(Math.random()-.5)*.026*accuracy;
  const device=acquireDeviceTarget(a),deviceD=device?device.d:999;
  let best=null,bestD=999,bestPath=null,bestDa=0;
  for(const e of game.enemies){
    if(e.hp<=0)continue;const path=shotPath(p,e);if(!path.clear)continue;
    const ang=Math.atan2(e.y-p.y,e.x-p.x),da=angleDelta(ang,a),d=dist(p,e),window=Math.atan(.28/d);
    if(Math.abs(da)<window&&d<bestD){best=e;bestD=d;bestPath=path;bestDa=da}
  }
  // Friendly obstruction is authoritative too: firing through a teammate is never ignored.
  let friendly=null,friendlyD=999;
  for(const m of game.squad){
    if(m.hp<=0)continue;const path=shotPath(p,m);if(!path.clear)continue;const d=dist(p,m),da=angleDelta(Math.atan2(m.y-p.y,m.x-p.x),a);
    if(Math.abs(da)<Math.atan(.25/d)&&d<friendlyD){friendly=m;friendlyD=d}
  }
  if(friendly&&friendlyD<Math.min(bestD,deviceD)){
    const amount=24;
    game.stats.friendlyIncidents++;game.stats.teamDamage+=amount;game.score=Math.max(0,game.score-75);
    if(game.reverseFriendlyFire){p.hp=Math.max(0,p.hp-amount);showDamage();feed('REVERSE FRIENDLY FIRE // DAMAGE REFLECTED // -75');addKillFeed('YOU','YOU','AR-7C',false,{friendlyFire:true});}
    else{friendly.hp=Math.max(0,friendly.hp-amount);feed(`TEAM DAMAGE // ${friendly.id} // -75`);addKillFeed('YOU',friendly.id,'AR-7C',true,{friendlyFire:true});}
    if(game.stats.friendlyIncidents>=2&&!game.reverseFriendlyFire){game.reverseFriendlyFire=true;feed('DISCIPLINE CONTROL // REVERSE FRIENDLY FIRE ENABLED');stamp('REVERSE FRIENDLY FIRE ENABLED')}
    suppressNearMiss(p,a,null);return;
  }
  if(device&&deviceD<bestD){destroyDevice(device);suppressNearMiss(p,a,null);return;}
  if(best){
    game.stats.hits++;showHit();const penetrated=bestPath&&bestPath.penetrations>0;
    const headWindow=Math.atan(.085/bestD),headshot=Math.abs(bestDa)<headWindow;
    const damage=penetrated?(headshot?54:18):(headshot?110:34);
    best.hp-=damage;best.playerTaggedAt=performance.now();best.playerDamage=(best.playerDamage||0)+damage;
    if(headshot){game.stats.headshots++;feed(`PRECISION HIT // ${best.id} // HEAD`)}
    if(penetrated){game.stats.penetrations++;feed('MATERIAL PENETRATION // REDUCED TERMINAL EFFECT')}
    if(best.hp<=0){best.hp=0;game.score+=headshot?125:100;game.stats.neutralized++;feed(`${best.id} NEUTRALIZED // +${headshot?125:100}`);addKillFeed('YOU',best.id,'AR-7C',true,{headshot});stamp(`${best.id} NEUTRALIZED${headshot?' // HEADSHOT':''}`)}
    else{best.state='engage';best.tx=p.x;best.ty=p.y;feedOnce(`hit-${best.id}`,`CONTACT WOUNDED // ${best.id}`)}
  }else{const impact=castRay(p.x,p.y,a);if(impact.d<MAX_DEPTH-.2){spawnWorldBurst(impact.x,impact.y,(impact.v==='2'||impact.v==='6'||impact.v==='7')?'spark':'dust',6);}}
  suppressNearMiss(p,a,best);
}
function interact(breach){
  if(game.spectating)return;
  if(!game.actionStarted){feedOnce('prep-entry','ENTRY ACTIONS LOCKED // PREPARATION PHASE');uiBeep(false);return}
  if(!breach&&useVerticalRoute())return;
  const t=nearTile(['2','7'],1.32);
  if(!t){feed('NO VALID ENTRY IN RANGE');uiBeep(false);return}
  if(breach){
    if(selectedRole!=='Breacher'&&Math.random()<.38){feed('BREACH FAILED // SPECIALIST TOOL RECOMMENDED');uiBeep(false);return}
    map[t.y][t.x]='0';noise(t.x+.5,t.y+.5,14,'breach');game.score+=75;game.stats.breaches++;game.alert=1;game.exposure=Math.max(game.exposure,.78);setAlarm(Math.max(1,game.alarmLevel),'DYNAMIC BREACH');showAlert();noiseBurst(.18,.08);tone(72,.2,'sawtooth',.04);spawnWorldBurst(t.x+.5,t.y+.5,'dust',26);spawnWorldBurst(t.x+.5,t.y+.5,'spark',8);game.ambience.breachShock=1;game.lightingPulse=.55;feed('DYNAMIC BREACH COMPLETE // +75 // SIGNATURE HIGH');stamp('DYNAMIC BREACH');
  } else {
    if(t.v==='2'){map[t.y][t.x]='7';noise(t.x+.5,t.y+.5,2.4,'door');game.stats.partialDoors++;tone(142,.05,'square',.012);feed('ENTRY PARTIALLY OPEN // PEEK ANGLE AVAILABLE');}
    else{map[t.y][t.x]='0';noise(t.x+.5,t.y+.5,3,'door');game.stats.doors++;tone(160,.05,'square',.015);feed('ENTRY FULLY OPEN // LOW-SIGNATURE ACCESS');}
  }
}

function hack(){
  if(game.spectating)return;
  if(game.match.side==='DEFENSE'){feed('DEFENDER NETWORK // INTELLIGENCE INTERFACE LOCKED TO ATTACKERS');uiBeep(false);return}
  if(!game.actionStarted){feedOnce('prep-hack','INTELLIGENCE ACTIONS LOCKED // PREPARATION PHASE');uiBeep(false);return}
  const p=game.player,term=game.terminals.find(t=>!t.used&&dist(p,t)<1.18);
  if(term){
    term.used=true;game.score+=200;game.stats.hacks++;uiBeep(true);
    if(term.type==='COMMS TERMINAL'){
      game.objectiveRevealed=true;game.phase='OBJECTIVE IDENTIFIED';feed(`D9 SIGINT // TRUE OBJECTIVE ${game.objective.id} // ${game.objective.name} // +200`);stamp(`TRUE OBJECTIVE IDENTIFIED // ${game.objective.id}`);
    }else{
      game.camerasDisabled=true;feed('SECURITY NETWORK OVERRIDDEN // CAMERAS OFFLINE // +200');stamp('SURVEILLANCE DISABLED');
    }
    return;
  }
  if(game.objectiveRevealed&&!game.secured&&dist(p,game.objective)<1.22){
    game.secured=true;game.phase='EXTRACTION ACTIVE';game.score+=250;game.intel++;uiBeep(true);feed('SP DATA PACKAGE SECURED // +250 // EXFIL AUTHORIZED');stamp('SP DATA SECURED');setAlarm(Math.max(1,game.alarmLevel),'DATA PACKAGE REMOVED');return;
  }
  feed('NO INTELLIGENCE INTERFACE IN RANGE');uiBeep(false);
}

function gadget(){
  if(game.spectating)return;
  if(!game.actionStarted){feedOnce('prep-gadget','FIELD GADGETS LOCKED // PREPARATION PHASE');uiBeep(false);return}
  const p=game.player;
  if(selectedRole==='Recon'){
    p.drone=12;game.score+=25;uiBeep();feed('RECON DRONE UPLINK // HOSTILE POSITIONS AVAILABLE ON MAP');
  }else if(selectedRole==='Tech'){
    const term=game.terminals.find(t=>!t.used&&dist(p,t)<2.65);
    if(term){term.used=true;game.score+=150;game.stats.hacks++;if(term.type==='COMMS TERMINAL'){game.objectiveRevealed=true;game.phase='OBJECTIVE IDENTIFIED'}else game.camerasDisabled=true;uiBeep();feed(`REMOTE SIGNAL OVERRIDE // ${term.type} // +150`)}else{feed('NO ELECTRONIC TARGET IN SIGNAL RANGE');uiBeep(false)}
  }else if(selectedRole==='Support'){
    if(p.hp<100||p.armInjury||p.legInjury){p.hp=Math.min(100,p.hp+38);p.armInjury=false;p.legInjury=false;uiBeep();feed('FIELD STABILIZATION COMPLETE // INJURIES TREATED')}else{feed('TRAUMA KIT NOT REQUIRED');uiBeep(false)}
  }else if(selectedRole==='Marksman'){
    p.drone=8;uiBeep();feed('RECON OPTICS ACTIVE // MOVEMENT ZONES MARKED');
  }else{
    const t=nearTile(['2','7'],1.65);if(t)interact(true);else{feed('BREACH KIT READY // MOVE TO REINFORCED ENTRY');uiBeep(false)}
  }
}

function cameraThreat(){
  if(game.match.side==='DEFENSE'||game.camerasDisabled)return 0;
  const p=game.player;let threat=0;
  for(const c of game.cameras){
    if(c.destroyed)continue;const d=dist(c,p);if(d>7.2||!los(c,p))continue;
    const to=Math.atan2(p.y-c.y,p.x-c.x);
    if(Math.abs(angleDelta(to,c.a))<.62)threat=Math.max(threat,1-d/8.5);
  }
  return threat;
}

function moveEnemy(e,dx,dy){const nx=e.x+dx,ny=e.y+dy;if(!solid(nx,e.y))e.x=nx;if(!solid(e.x,ny))e.y=ny}
function enemyShoot(e,d){
  const p=game.player;if(!los(e,p))return;
  const hit=Math.max(.2,.84-d*.06)*(p.crouch?.8:1);
  noise(e.x,e.y,9,'enemy gunfire');noiseBurst(.04,.025);
  if(Math.random()<hit){
    const dmg=rand(9,22);p.hp-=dmg;p.suppressed=.55;game.stats.damageTaken+=dmg;showDamage();tone(70,.09,'sawtooth',.02);
    if(p.hp<=0){p.hp=0;addKillFeed(e.id,'YOU','AR-7C',false)}
    if(Math.random()<.24){
      if(Math.random()<.5){p.armInjury=true;feed('TRAUMA // ARM INJURY // STABILITY REDUCED')}
      else{p.legInjury=true;feed('TRAUMA // LEG INJURY // MOBILITY REDUCED')}
    }
  }
}


function updateNetwork(dt){
  if(!game?.network)return;
  const n=game.network;
  // Small deterministic-looking fluctuation keeps telemetry alive without pretending to be real WAN measurement.
  n.ping=clamp(n.ping+(Math.random()-.5)*dt*9,28,58);n.jitter=clamp(n.jitter+(Math.random()-.5)*dt*2,1,8);n.loss=clamp(n.loss+(Math.random()-.5)*dt*.08,0,.9);
  matchSession.ping=n.ping;matchSession.loss=n.loss;
  if(matchSession.connectionState==='RECONNECTING'||matchSession.connectionState==='DISCONNECTED'){
    n.reconnectRemaining=Math.max(0,n.reconnectRemaining-dt);const timer=$('#reconnectTimer');if(timer)timer.textContent=Math.ceil(n.reconnectRemaining);
    if(n.reconnectRemaining<=0&&matchSession.connectionState!=='DISCONNECTED'){matchSession.connectionState='DISCONNECTED';n.authenticated=false;feed('RECONNECT GRACE EXPIRED // COMPETITIVE SLOT RELEASED');}
  }
  const ping=$('#netPing'),loss=$('#netLoss'),sync=$('#netSync');
  if(ping){ping.textContent=`${Math.round(n.ping)} MS`;ping.className=n.ping>90?'bad':n.ping>60?'warn':''}
  if(loss){loss.textContent=`${n.loss.toFixed(1)}%`;loss.className=n.loss>2?'bad':n.loss>1?'warn':''}
  if(sync){sync.textContent=matchSession.connectionState==='CONNECTED'?'LOCKED':'HOLD';sync.className=matchSession.connectionState==='CONNECTED'?'':'warn'}
}

function update(dt){
  if(!deployed||paused||game.win)return;
  const p=game.player;
  updateNetwork(dt);
  if(matchSession.connectionState==='DISCONNECTED'&&game.network.reconnectRemaining<=0)return finish(false,'SECURE SESSION LOST — RECONNECT GRACE EXPIRED');
  p.throwCd=Math.max(0,p.throwCd-dt);
  if(!game.actionStarted){
    game.prepTime=Math.max(0,game.prepTime-dt);game.phase=`PREPARATION // ${Math.ceil(game.prepTime)}`;game.match.state='PREPARATION';
    if(game.prepTime<=0){game.actionStarted=true;game.phase='INFILTRATION';game.match.state='ACTION';game.missionStarted=performance.now();stamp('ACTION PHASE');showPhaseBanner('ACTION PHASE','LOCATE // SECURE // EXTRACT');feed(`ROUND LIVE // PROTOCOL ${game.match.side} PHASE`);tone(820,.08,'square',.018)}
    updateFeedback(dt);hudAccumulator+=dt;if(hudAccumulator>=.08){updateHud();hudAccumulator=0}return;
  }
  if(game.spectating){
    game.spectatorTime=Math.max(0,game.spectatorTime-dt);game.time=Math.max(0,game.time-dt);updateSquad(dt);updateGrenades(dt);game.smokes.forEach(s=>s.t-=dt);game.smokes=game.smokes.filter(s=>s.t>0);game.phase=`SPECTATOR // ${game.spectatorMode}`;
    if(game.spectatorMode==='FREE'){const c=game.freeCam,sp=3.3;if(keys.KeyW){c.x+=Math.cos(c.a)*sp*dt;c.y+=Math.sin(c.a)*sp*dt}if(keys.KeyS){c.x-=Math.cos(c.a)*sp*dt;c.y-=Math.sin(c.a)*sp*dt}if(keys.KeyA){c.x+=Math.cos(c.a-Math.PI/2)*sp*dt;c.y+=Math.sin(c.a-Math.PI/2)*sp*dt}if(keys.KeyD){c.x+=Math.cos(c.a+Math.PI/2)*sp*dt;c.y+=Math.sin(c.a+Math.PI/2)*sp*dt}}
    if(game.spectatorTime<=0||!game.squad.some(m=>m.hp>0))return finish(false,'OPERATOR ELEMENT LOST — ROUND CONCEDED');
    updateFeedback(dt);hudAccumulator+=dt;if(hudAccumulator>=.08){updateHud();hudAccumulator=0}return;
  }
  game.time-=dt;if(game.time<=0){const objectiveLive=(game.match.side==='ATTACK'&&game.secured)||(game.match.side==='DEFENSE'&&game.defense.enemySecured);if(objectiveLive&&!game.overtimeUsed){game.overtimeUsed=true;game.time=30;game.match.state='OVERTIME';game.phase='OVERTIME';$('#overtimeBadge').classList.remove('hidden');showPhaseBanner('OVERTIME','OBJECTIVE ACTIVE // RESOLVE THE EXTRACTION');feed('OVERTIME AUTHORIZED // ACTIVE INTELLIGENCE CARRIER');stamp('OVERTIME')}else return finish(game.match.side==='DEFENSE','MISSION TIMER EXPIRED — DEFENSE HOLDS')}
  p.shotCd=Math.max(0,p.shotCd-dt);p.recoil=Math.max(0,p.recoil-dt*(p.ads?9:7));p.suppressed=Math.max(0,p.suppressed-dt);p.vault=Math.max(0,p.vault-dt);p.inspect=Math.max(0,p.inspect-dt);p.leanTarget=(keys.KeyZ?-1:keys.KeyC?1:0);p.lean+=(p.leanTarget-p.lean)*Math.min(1,dt*10);
  if(p.reloading>0){p.reloading-=dt;if(p.reloading<=0){const n=Math.min(30-p.ammo,p.reserve);p.ammo+=n;p.reserve-=n;tone(520,.04,'square',.012)}}
  if(p.drone>0)p.drone-=dt;
  updateGrenades(dt);
  game.smokes.forEach(s=>s.t-=dt);game.smokes=game.smokes.filter(s=>s.t>0);$('#smokeFX').classList.toggle('active',game.smokes.some(s=>dist(p,s)<s.r*1.15));
  updateSquad(dt);

  const wantsSprint=(keys.ShiftLeft||keys.ShiftRight)&&!p.crouch&&!p.ads&&p.stamina>2&&p.vault<=0&&p.inspect<=0;
  p.sprinting=!!wantsSprint;
  const speed=(p.crouch?1.28:p.sprinting?3.75:2.45)*(p.legInjury?.68:1), move={x:0,y:0};
  if(p.vault<=0&&p.inspect<=0){if(keys.KeyW){move.x+=Math.cos(p.a);move.y+=Math.sin(p.a)}
  if(keys.KeyS){move.x-=Math.cos(p.a);move.y-=Math.sin(p.a)}
  if(keys.KeyA){move.x+=Math.cos(p.a-Math.PI/2);move.y+=Math.sin(p.a-Math.PI/2)}
  if(keys.KeyD){move.x+=Math.cos(p.a+Math.PI/2);move.y+=Math.sin(p.a+Math.PI/2)}}
  const moving=Math.hypot(move.x,move.y)>.1,l=Math.hypot(move.x,move.y)||1;
  if(p.sprinting&&moving)p.stamina=Math.max(0,p.stamina-dt*22);else p.stamina=Math.min(100,p.stamina+dt*(p.crouch?14:10));
  if(p.stamina<=0)p.sprinting=false;
  tryMove(p.x+move.x/l*speed*dt,p.y+move.y/l*speed*dt);
  if(keys.ArrowLeft)p.a-=1.8*dt;if(keys.ArrowRight)p.a+=1.8*dt;
  if(mouseDown)shoot();
  p.moveBlend+=(Number(moving)-p.moveBlend)*Math.min(1,dt*7);if(moving)p.bob+=dt*(p.crouch?5:p.sprinting?11:8);
  p.footstepCd=Math.max(0,(p.footstepCd||0)-dt);const env=environmentAt(p.x,p.y);
  if(moving&&p.footstepCd<=0){p.footstepCd=p.crouch?.62:p.sprinting?.27:.42;footstep(env.surface,p.crouch?.45:p.sprinting?1.35:.82);if(p.sprinting)noise(p.x,p.y,7.5,'sprint');else if(!p.crouch)noise(p.x,p.y,3.4,'footstep');}
  updateFeedback(dt);

  game.noises.forEach(n=>n.t-=dt);game.noises=game.noises.filter(n=>n.t>0);
  if(!game.camerasDisabled)game.cameras.forEach(c=>{if(!c.destroyed)c.a=c.base+Math.sin(performance.now()/1700+c.phase)*.72});

  if(game.match.side==='DEFENSE'){
    updateDefenseAI(dt);
    if(game.win)return;
    if(game.time<=0)return finish(true,'MISSION TIMER EXPIRED — HELIX DEFENSE HOLDS');
    document.body.classList.toggle('ads',p.ads);hudAccumulator+=dt;if(hudAccumulator>=.08){updateHud();hudAccumulator=0}return;
  }

  let anyContact=false, activeSearch=0;
  for(const e of game.enemies){
    if(e.hp<=0)continue;e.cd=Math.max(0,e.cd-dt);e.stunned=Math.max(0,(e.stunned||0)-dt);if(e.stunned>0){e.a+=dt*.9;continue}
    const d=dist(e,p), viewRange=p.crouch?7.1:p.sprinting?11.4:9.5;
    const facing=Math.abs(angleDelta(Math.atan2(p.y-e.y,p.x-e.x),e.a));
    const sees=d<viewRange&&los(e,p)&&!smokeObscures(e,p)&&(facing<1.28||d<3.0||e.state==='engage');
    if(sees){e.state='engage';e.tx=p.x;e.ty=p.y;e.lastSeen=2.6;anyContact=true;game.exposure=Math.min(1,game.exposure+dt*.8)}
    else{
      e.lastSeen=Math.max(0,(e.lastSeen||0)-dt);
      const n=game.noises.find(n=>Math.hypot(e.x-n.x,e.y-n.y)<n.r);
      if(n&&e.state!=='engage'){e.state='investigate';e.tx=n.x;e.ty=n.y;e.searchTimer=4.5}
      if(e.state==='engage'&&e.lastSeen<=0){e.state='search';e.tx=p.x;e.ty=p.y;e.searchTimer=6}
    }
    if((e.hp<45||e.suppressed>.2)&&e.state==='engage'){const cv=chooseCover(e,p);e.state='retreat';if(cv){e.tx=cv.x;e.ty=cv.y;e.cover=cv}else{e.tx=clamp(e.x+(e.x-p.x)*2,1.5,18.5);e.ty=clamp(e.y+(e.y-p.y)*2,1.5,18.5)}}
    if(e.state==='engage'&&sees){
      e.a=Math.atan2(p.y-e.y,p.x-e.x);
      // alternate between pressure and a simple lateral flank.
      if(d>4.6){const flank=(Number(e.id.charCodeAt(e.id.length-1)%2)*2-1)*.5;moveEnemy(e,(Math.cos(e.a)+Math.cos(e.a+Math.PI/2)*flank)*dt*.72,(Math.sin(e.a)+Math.sin(e.a+Math.PI/2)*flank)*dt*.72)}
      else if(d<2.4)moveEnemy(e,-Math.cos(e.a)*dt*.5,-Math.sin(e.a)*dt*.5);
      if(e.cd<=0){e.cd=.66+Math.random()*.48;enemyShoot(e,d)}
    }else{
      if(e.state==='search')activeSearch++;
      const td=Math.hypot(e.tx-e.x,e.ty-e.y);
      if(td>.35){const a=Math.atan2(e.ty-e.y,e.tx-e.x);e.a=a;moveEnemy(e,Math.cos(a)*dt*(e.state==='retreat'?.82:e.state==='search'?.64:.52),Math.sin(a)*dt*(e.state==='retreat'?.82:e.state==='search'?.64:.52))}
      else if(e.state==='investigate'){e.state='search';e.searchTimer=4;e.a+=dt*.5}
      else if(e.state==='search'){
        e.searchTimer=(e.searchTimer||4)-dt;e.a+=dt*.6;
        if(e.searchTimer<=0){e.state='guard';e.tx=e.x;e.ty=e.y}
      } else if(e.state==='retreat'){e.state='guard'}
      else if(e.state==='patrol'&&Math.random()<dt*.35){e.tx=2+Math.random()*16;e.ty=2+Math.random()*16}
    }
  }
  if(anyContact&&!game.contact){game.contact=true;showAlert();feed('CONTACT // HELIX ELEMENT HAS VISUAL');stamp('FIRST CONTACT');setAlarm(Math.max(1,game.alarmLevel),'VISUAL CONTACT')}
  if(!anyContact)game.contact=false;

  const recentNoise=game.noises.reduce((m,n)=>Math.max(m,n.r/14*n.t/1.8),0),cam=cameraThreat();
  const targetAlert=clamp(Math.max(recentNoise,cam,anyContact?1:0,activeSearch?0.38:0),0,1);game.alert+=(targetAlert-game.alert)*Math.min(1,dt*5);
  if(cam>.18){game.exposure=Math.min(1,game.exposure+dt*(.2+cam*.9));feedOnce('camera','SURVEILLANCE WARNING // CAMERA NETWORK HAS LINE OF SIGHT')}
  else if(!anyContact)game.exposure=Math.max(0,game.exposure-dt*.16);
  if(game.exposure>.3&&game.alarmLevel<1)setAlarm(1,'SUSPICIOUS ACTIVITY');
  if(game.exposure>.62&&game.alarmLevel<2)setAlarm(2,'OPERATOR IDENTIFIED');
  if(game.exposure>.9&&game.alarmLevel<3)setAlarm(3,'FULL SECURITY RESPONSE');

  if(game.secured&&dist(p,game.extraction)<.82)finish(true,'ENCRYPTED INTELLIGENCE SECURED AND EXTRACTED');
  if(game.enemies.filter(e=>e.hp>0).length===0&&!game.secured)feedOnce('allDown','HELIX ELEMENT NEUTRALIZED // OBJECTIVE STILL REQUIRED');
  if(p.hp<=0&&!game.spectating){game.spectating=true;game.spectatorTime=Math.min(18,game.time);game.phase='SPECTATOR';p.ads=false;p.sprinting=false;mouseDown=false;feed('OPERATOR DOWN // OBSERVER FEED AVAILABLE // N CYCLE // M FREE CAM');stamp('OPERATOR DOWN');showPhaseBanner('SPECTATOR FEED','N CYCLE TARGET // M FREE CAMERA')}
  document.body.classList.toggle('ads',p.ads);
  hudAccumulator+=dt;if(hudAccumulator>=.08){updateHud();hudAccumulator=0}
}

function objectiveCopy(){
  if(game.spectating)return [`SPECTATOR // ${game.spectatorMode}`,'N cycles surviving teammates. M toggles the free tactical observer camera.'];
  if(!game.actionStarted)return game.match.side==='DEFENSE' ? ['FORTIFY THE EMBASSY','Reinforce breachable entries with X, position the squad, and deny Directorate Nine access to the intelligence.'] : ['PREPARE FOR CONTACT','Check role, equipment, squad order, and the tactical route before the round goes live.'];
  if(game.match.side==='DEFENSE'){
    if(!game.defense.enemyIntelRevealed)return ['DENY COMMUNICATIONS ACCESS','Protect the communications terminal. Attackers must discover the true data location before they can steal it.'];
    if(!game.defense.enemySecured)return ['DEFEND THE SP DATA PACKAGE','Attackers know the true objective. Hold the archive and intercept the assault element.'];
    return ['INTERCEPT THE INTELLIGENCE CARRIER','The encrypted package is compromised. Stop the carrier before extraction.'];
  }
  if(!game.objectiveRevealed)return ['LOCATE AND HACK A COMMUNICATIONS TERMINAL','Search the embassy for an active Directorate-compatible communications node.'];
  if(!game.secured)return [`SECURE SP DATA // ${game.objective.id}`,`Confirmed site: ${game.objective.name}. Recover the encrypted package and prepare for extraction.`];
  return ['REACH EXTRACTION // NORTH-EAST PERIMETER','Carry the encrypted package to the green extraction zone.'];
}
function updateCompass(){
  const dirs=['N','030','060','E','120','150','S','210','240','W','300','330','N'];
  const deg=((game.player.a*180/Math.PI)%360+360)%360;
  const bin=Math.round(deg/30);if(bin===lastCompassBin)return;lastCompassBin=bin;
  let html='';for(let i=-6;i<=6;i++){const d=((Math.round(deg/30)+i)%12+12)%12;const label=dirs[d];html+=`<span class="${['N','E','S','W'].includes(label)?'major':''}">${label}</span>`}
  $('#compass').innerHTML=html;
}
function updateHud(){
  if(!game)return;const p=game.player,[obj,hint]=objectiveCopy();
  $('#phase').textContent=game.phase;$('#objective').textContent=obj;$('#objectiveHint').textContent=hint;
  const clock=game.actionStarted?game.time:game.prepTime;$('#timer').textContent=`${String(Math.floor(Math.max(0,clock)/60)).padStart(2,'0')}:${String(Math.floor(Math.max(0,clock)%60)).padStart(2,'0')}`;
  $('#roundLabel').textContent=`ROUND ${String(game.match.round).padStart(2,'0')}`;$('#roundState').textContent=game.match.state;$('#roundMode').textContent=`PROTOCOL // ${game.match.side} // ${game.match.spawnGroup}`;$('#attackScore').textContent=game.match.attackScore;$('#defendScore').textContent=game.match.defendScore;$('#squadFaction').textContent=game.match.side==='DEFENSE'?'HELIX CELL // ALPHA':'SPECTRE // ALPHA';const rh=$('.roundHeader'),mp=matchPointState();rh.classList.toggle('action',game.match.state==='ACTION');rh.classList.toggle('overtime',game.match.state==='OVERTIME');rh.classList.toggle('matchpoint',!!mp);$('#matchPoint').classList.toggle('hidden',!mp);$('#matchPoint').textContent=mp;$('#overtimeBadge').classList.toggle('hidden',game.match.state!=='OVERTIME');
  $('#hp').textContent=Math.max(0,Math.ceil(p.hp));$('#ammo').textContent=p.reloading>0?'--':p.ammo;$('#reserve').textContent=p.reserve;
  $('#roleHud').textContent=selectedRole.toUpperCase();$('#gadgetHud').textContent=roles[selectedRole].gadget;$('#throwableHud').textContent=`${p.throwable} ×${p.throwable==='FLASH'?p.flash:p.smoke}`;
  $('#injury').textContent=p.armInjury&&p.legInjury?'MULTIPLE INJURIES':p.armInjury?'ARM INJURY':p.legInjury?'LEG INJURY':'STABLE';
  $('#stance').textContent=p.crouch?'CROUCHED // LOW SIGNATURE':p.sprinting?'SPRINTING // HIGH SIGNATURE':p.ads?'AIMING // CONTROLLED':'STANDING // NORMAL SIGNATURE';
  const sig=clamp(game.alert,0,1);$('#signatureFill').style.width=`${Math.max(7,sig*100)}%`;$('#signatureFill').style.background=sig>.7?'#d33d49':sig>.35?'#d2a650':'#669f7c';
  $('#signatureText').textContent=sig>.72?'COMPROMISED':sig>.35?'EXPOSED':'COVERT';
  $('#staminaFill').style.width=`${p.stamina}%`;$('#staminaText').textContent=`${Math.round(p.stamina)}%`;
  const alarm=$('.alarmBlock');alarm.classList.toggle('warning',game.alarmLevel===1||game.alarmLevel===2);alarm.classList.toggle('critical',game.alarmLevel>=3);$('#alarmLevel').textContent=game.alarmLevel;$('#alarmText').textContent=['UNAWARE','SUSPICIOUS','SEARCHING','LOCKDOWN'][game.alarmLevel];
  const rb=$('#reloadBar');rb.classList.toggle('active',p.reloading>0);rb.querySelector('i').style.width=p.reloading>0?`${(1-p.reloading/p.reloadTotal)*100}%`:'0%';
  let prompt='';
  if(!game.actionStarted&&game.match.side==='DEFENSE'&&nearTile(['2'],1.45))prompt=`<kbd>X</kbd> FORTIFY ENTRY <span>//</span> ${game.barricadeStock} REMAINING`;
  else if(nearestVerticalLink())prompt='<kbd>E</kbd> USE VERTICAL ROUTE';
  else if(game.actionStarted&&game.match.side==='ATTACK'&&nearTile(['2','7','6'],1.15)){const d=nearTile(['2','7'],1.15);prompt=d&&d.v==='7'?'<kbd>E</kbd> OPEN FULLY <span>//</span> <kbd>B</kbd> BREACH':'<kbd>E</kbd> CRACK ENTRY <span>//</span> <kbd>B</kbd> DYNAMIC BREACH';}
  else if(game.actionStarted){const f=game.props.find(o=>['cover','bollard','planter','sofa'].includes(o.kind)&&dist(p,o)<1.0&&Math.abs(angleDelta(Math.atan2(o.y-p.y,o.x-p.x),p.a))<.75);if(f)prompt='<kbd>SPACE</kbd> VAULT LOW COVER';}
  if(game.match.side==='ATTACK'){const t=game.terminals.find(t=>!t.used&&dist(p,t)<1.15);if(t)prompt=`<kbd>H</kbd> HACK // ${t.type}`;if(game.objectiveRevealed&&!game.secured&&dist(p,game.objective)<1.15)prompt='<kbd>H</kbd> SECURE ENCRYPTED INTELLIGENCE';if(game.secured&&dist(p,game.extraction)<1.5)prompt='<kbd>→</kbd> ENTER EXTRACTION ZONE';}
  $('#prompt').innerHTML=prompt;$('#prompt').style.display=prompt?'flex':'none';
  $('#mapPhase').textContent=game.phase;$('#mapIntel').textContent=`${game.intel} / 1`;$('#mapCameras').textContent=game.camerasDisabled?'OFFLINE':'ONLINE';$('#mapRole').textContent=selectedRole.toUpperCase();
  const env=game.sector||environmentAt(p.x,p.y);$('#locationName').textContent=env.name;$('#surfaceName').textContent=`${env.surface} // ${p.floor}`;$('#mapSector').textContent=`${env.name} // ${p.floor}`;$('#mapSurface').textContent=env.surface;$('#surfaceName').textContent=`${env.surface} // ${p.floor}`;
  const ws=$('#weaponState');if(ws){ws.className='';ws.textContent=p.reloading>0?'RELOADING':p.inspect>0?'INSPECT':p.vault>0?'VAULT':p.sprinting?'LOW READY':p.suppressed>.1?'SUPPRESSED':p.lean!==0?`LEAN ${p.lean<0?'L':'R'}`:p.ads?`${p.optic==='MAGNIFIER'?'2X ':''}ADS`:'READY';if(p.reloading>0)ws.classList.add('reload');else if(p.inspect>0)ws.classList.add('weaponStateInspect');else if(p.sprinting)ws.classList.add('sprint');else if(p.suppressed>.1)ws.classList.add('suppressed');}const os=$('#opticState');if(os)os.textContent=p.optic==='MAGNIFIER'?'MAG 2X':'REFLEX 1X';document.body.classList.toggle('lean-left',p.lean<-.18);document.body.classList.toggle('lean-right',p.lean>.18);document.body.classList.toggle('magnified',p.ads&&p.optic==='MAGNIFIER');document.body.classList.toggle('shadow-corridor',game.props.some(o=>o.kind==='lamp'&&o.destroyed&&dist(p,o)<5));
  const squadHud=[['bravoName','bravoState','bravoLife'],['charlieName','charlieState','charlieLife'],['deltaName','deltaState','deltaLife'],['echoName','echoState','echoLife']];
  game.squad.forEach((m,i)=>{const ids=squadHud[i];if(!ids)return;const name=$(`#${ids[0]}`),state=$(`#${ids[1]}`),life=$(`#${ids[2]}`);if(name)name.textContent=m.id;if(state){state.textContent=m.hp>0?m.state.toUpperCase():'DOWN';state.classList.toggle('engaged',m.state==='engaged');state.classList.toggle('command',m.state!=='follow'&&m.state!=='engaged')}if(life)life.classList.toggle('alive',m.hp>0)});
  const members=[{id:'YOU',hp:p.hp,state:p.hp>0?'active':'down'},...game.squad];$('#teamAliveStrip').innerHTML=members.map((m,i)=>`<span class="member ${i===0?'you ':''}${m.hp<=0?'down ':''}${m.state==='engaged'?'engaged':''}">${m.id}</span>`).join('');
  updateCompass();
}

function finish(win,reason){
  if(game.win)return;game.win=true;game.match.state='ROUND COMPLETE';
  const winner = win ? (game.match.side==='ATTACK'?'D9':'HELIX') : (game.match.side==='ATTACK'?'HELIX':'D9');
  if(winner==='D9')matchSession.d9++;else matchSession.helix++;
  game.match.attackScore=matchSession.d9;game.match.defendScore=matchSession.helix;
  const matchComplete=matchSession.d9>=matchSession.roundsToWin||matchSession.helix>=matchSession.roundsToWin||matchSession.round>=matchSession.maxRounds;game.matchComplete=matchComplete;
  deployed=false;paused=true;document.exitPointerLock?.();hud.classList.add('hidden');resumeOverlay.classList.add('hidden');$('#mapOverlay').classList.add('hidden');
  const elapsed=Math.max(1,(performance.now()-game.missionStarted)/1000),accuracy=game.stats.shots?Math.round(game.stats.hits/game.stats.shots*100):0;
  let grade='D';if(win){const quality=game.score+(accuracy*4)+(game.camerasDisabled?150:0)+game.stats.barricades*55+game.stats.penetrations*25-game.stats.damageTaken*1.5;if(quality>1050)grade='S';else if(quality>820)grade='A';else if(quality>620)grade='B';else grade='C'}
  $('#endTitle').textContent=matchComplete?'MATCH COMPLETE':win?'ROUND WON':'ROUND LOST';$('#endReason').textContent=reason;$('#grade').textContent=grade;
  $('#scoreboard').innerHTML=`
    <div class="scoreCell"><small>TACTICAL SCORE</small><b>${game.score}</b></div>
    <div class="scoreCell"><small>ACCURACY</small><b>${accuracy}%</b></div>
    <div class="scoreCell"><small>HEADSHOTS / ASSISTS</small><b>${game.stats.headshots} / ${game.stats.assists}</b></div>
    <div class="scoreCell"><small>NEUTRALIZED</small><b>${game.stats.neutralized}/${5+game.reinforcements}</b></div>`;
  $('#reviewStats').textContent=`ROUND ${game.match.round}/9 // SIDE ${game.match.side} // TIME ${Math.floor(elapsed/60)}:${String(Math.floor(elapsed%60)).padStart(2,'0')} // BARRICADES ${game.stats.barricades} // BREACHES ${game.stats.breaches} // HACKS ${game.stats.hacks} // FLASH ${game.stats.flashes} // SMOKE ${game.stats.smokes} // PENETRATION ${game.stats.penetrations} // SUPPRESSION ${game.stats.suppressionEvents} // CAMERAS ${game.stats.camerasDestroyed} // LIGHTS ${game.stats.lightsDestroyed} // VAULTS ${game.stats.vaults} // VERTICAL ${game.stats.verticalRoutes} // HEADSHOTS ${game.stats.headshots} // ASSISTS ${game.stats.assists} // TEAM DMG ${Math.round(game.stats.teamDamage)} // DAMAGE ${Math.round(game.stats.damageTaken)} // ROLE ${selectedRole.toUpperCase()} // SPAWN ${game.match.spawnGroup} // OVERTIME ${game.overtimeUsed?'YES':'NO'}`;
  const stealth=game.alarmLevel===0?'UNDETECTED':game.alarmLevel<3?'CONTESTED':'COMPROMISED';const discipline=game.stats.teamDamage===0?'CLEAN':game.stats.teamDamage<30?'WARNING':'POOR';const intel=game.stats.hacks>0||game.secured?'ACQUIRED':'LIMITED';
  $('#reviewHighlights').innerHTML=`<div class="${stealth==='UNDETECTED'?'good':stealth==='COMPROMISED'?'hot':'warn'}"><small>STEALTH PROFILE</small><b>${stealth}</b></div><div class="${discipline==='CLEAN'?'good':'warn'}"><small>FIRE DISCIPLINE</small><b>${discipline}</b></div><div class="${intel==='ACQUIRED'?'good':'warn'}"><small>INTELLIGENCE</small><b>${intel}</b></div><div><small>FINAL SECTOR</small><b>${(game.sector||environmentAt(game.player.x,game.player.y)).name}</b></div>`;
  $('#roundResult').innerHTML=`<div class="${winner==='D9'?'winner':''}"><small>DIRECTORATE NINE</small><b>${matchSession.d9} ROUND${matchSession.d9===1?'':'S'}</b></div><div class="${winner==='HELIX'?'winner':''}"><small>HELIX NETWORK</small><b>${matchSession.helix} ROUND${matchSession.helix===1?'':'S'}</b></div>`;
  $('#reviewTimeline').innerHTML=game.timeline.slice(-4).map(x=>`<div><small>${x.t}</small><b>${x.e}</b></div>`).join('');
  const next=$('#nextRoundBtn');next.classList.toggle('hidden',matchComplete);next.innerHTML=matchComplete?'MATCH COMPLETE':`CONTINUE TO ROUND ${String(matchSession.round+1).padStart(2,'0')} // ${matchPointState()||'SIDE SWITCH'} <span>→</span>`;
  $('#restartBtn').textContent=matchComplete?'RETURN TO OPERATIONS':'ABANDON MATCH // OPERATIONS';endScreen.classList.add('show');uiBeep(win);
}

function castRay(px,py,a){
  const sin=Math.sin(a),cos=Math.cos(a);
  for(let d=.035;d<MAX_DEPTH;d+=.035){
    const x=px+cos*d,y=py+sin*d,v=map[Math.floor(y)]?.[Math.floor(x)]??'1';
    if(v==='1'||v==='2'||v==='6'||(v==='7'&&partialDoorBlocks(x,y)))return {d,v,x,y,frac:((x+y)*4)%1};
  }
  return {d:MAX_DEPTH,v:'1',x:px+cos*MAX_DEPTH,y:py+sin*MAX_DEPTH,frac:0};
}

function drawBackground(){
  const sky=ctx.createLinearGradient(0,0,0,H*.52);sky.addColorStop(0,'#070d12');sky.addColorStop(.65,'#1b2328');sky.addColorStop(1,'#283036');ctx.fillStyle=sky;ctx.fillRect(0,0,W,H*.53);
  const floor=ctx.createLinearGradient(0,H*.48,0,H);floor.addColorStop(0,'#252d31');floor.addColorStop(.35,'#171d20');floor.addColorStop(1,'#060809');ctx.fillStyle=floor;ctx.fillRect(0,H*.48,W,H*.52);
  // Perspective floor bands and ceiling seams.
  ctx.strokeStyle='rgba(190,205,212,.035)';ctx.lineWidth=1;
  for(let y=H*.54;y<H;y+=Math.max(9,(y-H*.5)*.13)){ctx.beginPath();ctx.moveTo(0,y);ctx.lineTo(W,y);ctx.stroke()}
  ctx.strokeStyle='rgba(190,205,212,.025)';for(let i=-8;i<=8;i++){ctx.beginPath();ctx.moveTo(W/2,H*.5);ctx.lineTo(W/2+i*170,H);ctx.stroke()}
  ctx.fillStyle='rgba(180,210,220,.035)';for(let x=80;x<W;x+=310)ctx.fillRect(x,64,120,3);
}

function renderWorld(){
  drawBackground();if(!game)return;
  const spectator=game.spectating&&game.spectatorMode!=='FREE'?game.squad.filter(m=>m.hp>0)[game.spectatorIndex%Math.max(1,game.squad.filter(m=>m.hp>0).length)]:null;const p=game.spectating&&game.spectatorMode==='FREE'?game.freeCam:(spectator||game.player),depth=[];if(spectator&&p.a==null)p.a=game.player.a;
  const vf=currentFov(p),lean=!game.spectating?(p.lean||0):0,view={...p,x:p.x+Math.cos(p.a+Math.PI/2)*lean*.14,y:p.y+Math.sin(p.a+Math.PI/2)*lean*.14};
  const pitch=game.spectating?0:(p.recoil*5 + Math.sin(p.bob)*p.moveBlend*1.8 + (p.vault>0?Math.sin((.46-p.vault)/.46*Math.PI)*18:0));
  for(let i=0;i<RAYS;i++){
    const ra=view.a-vf/2+vf*i/RAYS,r=castRay(view.x,view.y,ra),cd=Math.max(.02,r.d*Math.cos(ra-view.a)),hh=Math.min(H*1.35,H/(cd*.73)),base=clamp(171-cd*8,22,160);
    const stripe=(Math.floor((r.x+r.y)*3)%2)*6, door=r.v==='2'||r.v==='7',partial=r.v==='7',barricade=r.v==='6';
    let rr=base+stripe,gg=base+stripe+3,bb=base+stripe+6;
    if(door){rr=Math.min(135,base+(partial?20:42));gg=Math.max(22,base*(partial?.48:.34));bb=Math.max(27,base*(partial?.5:.38))}if(barricade){rr=Math.min(125,base+24);gg=Math.min(112,base+8);bb=Math.max(22,base*.3)}
    const x=i*W/RAYS,y=H/2-hh/2+pitch,w=W/RAYS+1;
    ctx.fillStyle=`rgb(${Math.floor(rr)},${Math.floor(gg)},${Math.floor(bb)})`;ctx.fillRect(x,y,w,hh);
    if(i%7===0){ctx.fillStyle=`rgba(0,0,0,${clamp(cd/38,.02,.22)})`;ctx.fillRect(x,y,w,hh)}
    if(door&&i%5===0){ctx.fillStyle=partial?'rgba(110,150,164,.13)':'rgba(231,72,82,.18)';ctx.fillRect(x,y+hh*.16,w,hh*.06)}if(barricade&&i%4===0){ctx.fillStyle='rgba(218,170,76,.2)';ctx.fillRect(x,y+hh*.18,w,hh*.035);ctx.fillRect(x,y+hh*.48,w,hh*.035);ctx.fillRect(x,y+hh*.78,w,hh*.035)}
    depth[i]=cd;
  }
  // distance haze
  const haze=ctx.createLinearGradient(0,H*.1,0,H*.75);haze.addColorStop(0,'rgba(17,29,36,.06)');haze.addColorStop(1,'rgba(0,0,0,.02)');ctx.fillStyle=haze;ctx.fillRect(0,0,W,H);

  const sprites=[];
  game.terminals.filter(t=>!t.used).forEach(t=>sprites.push({...t,kind:'terminal'}));
  if(game.objectiveRevealed&&!game.secured)sprites.push({...game.objective,kind:'objective'});
  if(game.secured)sprites.push({...game.extraction,kind:'extract'});
  if(!game.camerasDisabled)game.cameras.filter(c=>!c.destroyed).forEach(c=>sprites.push({...c,kind:'camera'}));
  game.props.forEach(o=>sprites.push(o));
  game.smokes.forEach(o=>sprites.push({...o,kind:'smoke'}));game.grenades.forEach(o=>sprites.push({...o,kind:'grenade'}));
  game.squad.filter(m=>m.hp>0&&m!==spectator).forEach(m=>sprites.push({...m,kind:'squad'}));
  game.enemies.filter(e=>e.hp>0).forEach(e=>sprites.push({...e,kind:'enemy'}));
  sprites.sort((a,b)=>dist(p,b)-dist(p,a));
  for(const s of sprites)drawSprite(s,view,depth,pitch);
  drawWorldParticles(p,depth,pitch);
  const env=game.sector||environmentAt(p.x,p.y);const deadLights=game.props.filter(o=>o.kind==='lamp'&&o.destroyed&&dist(view,o)<5).length;if(deadLights){ctx.fillStyle=`rgba(0,0,0,${Math.min(.26,.09*deadLights)})`;ctx.fillRect(0,0,W,H)}if(env.light==='LOW'){ctx.fillStyle='rgba(0,0,0,.10)';ctx.fillRect(0,0,W,H)}else if(env.light==='WARM'){ctx.fillStyle='rgba(113,73,45,.025)';ctx.fillRect(0,0,W,H)}else{ctx.fillStyle='rgba(64,101,118,.018)';ctx.fillRect(0,0,W,H)}
  if(game.alarmLevel>=3){ctx.fillStyle=`rgba(117,10,18,${.012+.012*Math.sin(performance.now()/280)})`;ctx.fillRect(0,0,W,H)}
  if(!game.spectating)drawWeapon(game.player);else{ctx.fillStyle='rgba(4,7,9,.58)';ctx.fillRect(18,H-86,245,54);ctx.strokeStyle='#4d5b63';ctx.strokeRect(18,H-86,245,54);ctx.fillStyle='#c8d0d4';ctx.font='11px Consolas';ctx.fillText(`SPECTATOR // ${game.spectatorMode==='FREE'?'FREE CAM':spectator?spectator.id:'TEAM FEED'}`,32,H-57);ctx.fillStyle='#79858c';ctx.font='8px Consolas';ctx.fillText('OBSERVATION ONLY // ROUND IN PROGRESS',32,H-40)}
  drawScreenParticles();
}

function spriteProjection(s,p,depth,pitch){
  const dx=s.x-p.x,dy=s.y-p.y,d=Math.hypot(dx,dy);let a=angleDelta(Math.atan2(dy,dx),p.a);
  const vf=currentFov(p);if(Math.abs(a)>vf*.66||!los(p,s))return null;
  const sx=W/2+(a/(vf/2))*W/2,size=Math.min(H*.82,H/(d*.78)),ray=Math.floor((sx/W)*RAYS);
  if(ray<0||ray>=RAYS||d>depth[ray]+.4)return null;
  return {d,sx,size,x:sx-size/2,y:H/2-size/2+pitch};
}
function drawSprite(s,p,depth,pitch){
  const q=spriteProjection(s,p,depth,pitch);if(!q)return;const {d,size,x,y}=q;
  ctx.save();ctx.globalAlpha=clamp(1-d/24,.35,1);
  if(s.kind==='enemy'){
    const engaged=s.state==='engage',directorate=String(s.id||'').startsWith('D9');
    // legs
    ctx.fillStyle='#111619';ctx.fillRect(x+size*.38,y+size*.58,size*.1,size*.3);ctx.fillRect(x+size*.53,y+size*.58,size*.1,size*.3);
    // body armor
    ctx.fillStyle=directorate?(engaged?'#244a59':'#26343a'):(engaged?'#4b171d':'#242b30');ctx.beginPath();ctx.moveTo(x+size*.32,y+size*.28);ctx.lineTo(x+size*.68,y+size*.28);ctx.lineTo(x+size*.64,y+size*.65);ctx.lineTo(x+size*.36,y+size*.65);ctx.closePath();ctx.fill();
    ctx.fillStyle='#0d1012';ctx.fillRect(x+size*.37,y+size*.34,size*.26,size*.18);
    // head / helmet
    ctx.fillStyle='#171c20';ctx.beginPath();ctx.arc(x+size*.5,y+size*.2,size*.115,0,TAU);ctx.fill();ctx.fillStyle='#050708';ctx.fillRect(x+size*.39,y+size*.13,size*.22,size*.07);
    // rifle
    ctx.strokeStyle='#080a0b';ctx.lineWidth=Math.max(2,size*.035);ctx.beginPath();ctx.moveTo(x+size*.28,y+size*.43);ctx.lineTo(x+size*.74,y+size*.47);ctx.stroke();
    if(engaged){ctx.fillStyle='#e54b57';ctx.beginPath();ctx.arc(x+size*.5,y+size*.04,Math.max(2,size*.024),0,TAU);ctx.fill()}
    ctx.fillStyle='rgba(228,233,235,.78)';ctx.font=`${Math.max(8,size*.044)}px Consolas`;ctx.textAlign='center';ctx.fillText(s.id,x+size*.5,y+size*.97);
  }else if(s.kind==='squad'){
    ctx.fillStyle='#151c20';ctx.fillRect(x+size*.39,y+size*.58,size*.09,size*.28);ctx.fillRect(x+size*.53,y+size*.58,size*.09,size*.28);
    ctx.fillStyle=s.state==='engaged'?'#264a55':'#273238';ctx.beginPath();ctx.moveTo(x+size*.33,y+size*.29);ctx.lineTo(x+size*.67,y+size*.29);ctx.lineTo(x+size*.63,y+size*.64);ctx.lineTo(x+size*.37,y+size*.64);ctx.closePath();ctx.fill();
    ctx.fillStyle='#1c2428';ctx.beginPath();ctx.arc(x+size*.5,y+size*.2,size*.11,0,TAU);ctx.fill();ctx.strokeStyle='#7aa8b5';ctx.lineWidth=Math.max(1,size*.012);ctx.stroke();ctx.strokeStyle='#111719';ctx.lineWidth=Math.max(2,size*.03);ctx.beginPath();ctx.moveTo(x+size*.29,y+size*.43);ctx.lineTo(x+size*.72,y+size*.47);ctx.stroke();drawWorldLabel(`${game.match.side==='DEFENSE'?'HELIX':'SPECTRE'} // ${s.id}`,x+size*.5,y+size*.96,game.match.side==='DEFENSE'?'#d49aa0':'#8fc1cf');
  }else if(s.kind==='terminal'){
    ctx.fillStyle='#182027';ctx.fillRect(x+size*.31,y+size*.3,size*.38,size*.48);ctx.strokeStyle='#b18b3a';ctx.lineWidth=Math.max(1,size*.012);ctx.strokeRect(x+size*.31,y+size*.3,size*.38,size*.48);
    ctx.fillStyle='#b99542';ctx.fillRect(x+size*.36,y+size*.36,size*.28,size*.16);ctx.fillStyle='rgba(255,224,140,.28)';ctx.fillRect(x+size*.38,y+size*.38,size*.24,size*.03);
    drawWorldLabel(s.type,x+size*.5,y+size*.86,'#d9b45a');
  }else if(s.kind==='objective'){
    ctx.fillStyle='#14222a';ctx.fillRect(x+size*.34,y+size*.38,size*.32,size*.28);ctx.strokeStyle='#6ea9c1';ctx.strokeRect(x+size*.34,y+size*.38,size*.32,size*.28);ctx.fillStyle='#75bad5';ctx.fillRect(x+size*.39,y+size*.43,size*.22,size*.04);drawWorldLabel('SP DATA',x+size*.5,y+size*.77,'#83c7df');
  }else if(s.kind==='extract'){
    ctx.strokeStyle='#65ba88';ctx.lineWidth=Math.max(2,size*.018);ctx.beginPath();ctx.arc(x+size*.5,y+size*.54,size*.2,0,TAU);ctx.stroke();ctx.beginPath();ctx.arc(x+size*.5,y+size*.54,size*.28,0,TAU);ctx.stroke();drawWorldLabel('EXTRACT',x+size*.5,y+size*.9,'#7fe0a6');
  }else if(s.kind==='camera'){
    ctx.fillStyle='#151a1d';ctx.fillRect(x+size*.43,y+size*.35,size*.2,size*.1);ctx.fillStyle='#a43a42';ctx.beginPath();ctx.arc(x+size*.6,y+size*.4,Math.max(2,size*.022),0,TAU);ctx.fill();ctx.strokeStyle='#2d3439';ctx.beginPath();ctx.moveTo(x+size*.47,y+size*.45);ctx.lineTo(x+size*.44,y+size*.6);ctx.stroke();
  }else if(s.kind==='crate'){
    ctx.fillStyle='#242b2f';ctx.fillRect(x+size*.31,y+size*.54,size*.38,size*.28);ctx.strokeStyle='#505a60';ctx.lineWidth=Math.max(1,size*.01);ctx.strokeRect(x+size*.31,y+size*.54,size*.38,size*.28);ctx.strokeStyle='rgba(180,190,195,.18)';ctx.beginPath();ctx.moveTo(x+size*.33,y+size*.57);ctx.lineTo(x+size*.67,y+size*.79);ctx.moveTo(x+size*.67,y+size*.57);ctx.lineTo(x+size*.33,y+size*.79);ctx.stroke();
  }else if(s.kind==='server'){
    ctx.fillStyle='#12181c';ctx.fillRect(x+size*.37,y+size*.2,size*.26,size*.64);ctx.strokeStyle='#3e4a51';ctx.strokeRect(x+size*.37,y+size*.2,size*.26,size*.64);for(let i=0;i<7;i++){ctx.fillStyle=i%2?'#315443':'#24343d';ctx.fillRect(x+size*.41,y+size*(.27+i*.07),size*.18,size*.018)}
  }else if(s.kind==='desk'){
    ctx.fillStyle='#20272b';ctx.fillRect(x+size*.25,y+size*.57,size*.5,size*.09);ctx.fillRect(x+size*.29,y+size*.65,size*.055,size*.22);ctx.fillRect(x+size*.66,y+size*.65,size*.055,size*.22);ctx.fillStyle='#0f1417';ctx.fillRect(x+size*.43,y+size*.39,size*.2,size*.16);
  }else if(s.kind==='cover'){
    ctx.fillStyle='#30383d';ctx.fillRect(x+size*.22,y+size*.49,size*.56,size*.34);ctx.strokeStyle='#616b70';ctx.lineWidth=Math.max(1,size*.012);ctx.strokeRect(x+size*.22,y+size*.49,size*.56,size*.34);ctx.fillStyle='#171c1f';ctx.fillRect(x+size*.29,y+size*.55,size*.42,size*.07);ctx.fillStyle='#8b3038';ctx.fillRect(x+size*.25,y+size*.75,size*.5,size*.025);drawWorldLabel('HELIX COVER',x+size*.5,y+size*.9,'#a95a61');
  }else if(s.kind==='smoke'){
    const g=ctx.createRadialGradient(x+size*.5,y+size*.52,size*.03,x+size*.5,y+size*.52,size*.42);g.addColorStop(0,'rgba(190,198,200,.52)');g.addColorStop(.5,'rgba(118,126,130,.28)');g.addColorStop(1,'rgba(82,89,92,0)');ctx.fillStyle=g;ctx.fillRect(x+size*.05,y+size*.08,size*.9,size*.9);
  }else if(s.kind==='grenade'){const gy=y+size*.58-(s.z||0)*size*.18;ctx.fillStyle=s.type==='FLASH'?'#b8c0c3':'#596267';ctx.beginPath();ctx.arc(x+size*.5,gy,Math.max(2,size*.035),0,TAU);ctx.fill();ctx.strokeStyle='#20272b';ctx.stroke();
  }else if(s.kind==='lamp'){
    if(s.destroyed){ctx.fillStyle='#202529';ctx.fillRect(x+size*.49,y+size*.36,size*.02,size*.38);ctx.fillStyle='#111416';ctx.beginPath();ctx.arc(x+size*.5,y+size*.36,size*.07,0,TAU);ctx.fill();ctx.strokeStyle='#7c373d';ctx.beginPath();ctx.moveTo(x+size*.45,y+size*.32);ctx.lineTo(x+size*.55,y+size*.4);ctx.moveTo(x+size*.55,y+size*.32);ctx.lineTo(x+size*.45,y+size*.4);ctx.stroke();ctx.restore();return;}
    const g=ctx.createRadialGradient(x+size*.5,y+size*.38,1,x+size*.5,y+size*.38,size*.28);g.addColorStop(0,'rgba(196,214,218,.18)');g.addColorStop(1,'rgba(196,214,218,0)');ctx.fillStyle=g;ctx.fillRect(x+size*.2,y+size*.1,size*.6,size*.6);ctx.fillStyle='#6f7b80';ctx.fillRect(x+size*.49,y+size*.36,size*.02,size*.38);ctx.fillRect(x+size*.43,y+size*.72,size*.14,size*.025);
  }else if(s.kind==='column'){
    const g=ctx.createLinearGradient(x+size*.32,y,x+size*.68,y);g.addColorStop(0,'#1a2024');g.addColorStop(.5,'#4a5459');g.addColorStop(1,'#151a1d');ctx.fillStyle=g;ctx.fillRect(x+size*.36,y+size*.12,size*.28,size*.76);ctx.fillStyle='rgba(201,214,219,.16)';ctx.fillRect(x+size*.4,y+size*.18,size*.035,size*.62);
  }else if(s.kind==='planter'){
    ctx.fillStyle='#242b2f';ctx.fillRect(x+size*.31,y+size*.61,size*.38,size*.22);ctx.fillStyle='#202d27';for(let i=0;i<6;i++){ctx.beginPath();ctx.ellipse(x+size*(.38+i*.05),y+size*(.48-(i%2)*.05),size*.055,size*.16,(i-2)*.18,0,TAU);ctx.fill()}
  }else if(s.kind==='locker'){
    ctx.fillStyle='#232b30';ctx.fillRect(x+size*.34,y+size*.22,size*.32,size*.65);ctx.strokeStyle='#515d64';ctx.strokeRect(x+size*.34,y+size*.22,size*.32,size*.65);for(let i=1;i<3;i++){ctx.beginPath();ctx.moveTo(x+size*.34,y+size*(.22+i*.215));ctx.lineTo(x+size*.66,y+size*(.22+i*.215));ctx.stroke()}ctx.fillStyle='#9a353e';ctx.fillRect(x+size*.58,y+size*.31,size*.025,size*.04);
  }else if(s.kind==='archive'){
    ctx.fillStyle='#161d21';ctx.fillRect(x+size*.3,y+size*.25,size*.4,size*.58);ctx.strokeStyle='#455159';ctx.strokeRect(x+size*.3,y+size*.25,size*.4,size*.58);for(let i=0;i<5;i++){ctx.fillStyle=i%2?'#39434a':'#252e33';ctx.fillRect(x+size*.34,y+size*(.3+i*.095),size*.32,size*.055)}
  }else if(s.kind==='bollard'){
    ctx.fillStyle='#20272b';ctx.fillRect(x+size*.44,y+size*.48,size*.12,size*.38);ctx.fillStyle='#8f333a';ctx.fillRect(x+size*.44,y+size*.55,size*.12,size*.035);ctx.fillStyle='#4a555b';ctx.beginPath();ctx.arc(x+size*.5,y+size*.48,size*.06,Math.PI,TAU);ctx.fill();
  }else if(s.kind==='stairs'){
    ctx.fillStyle='#1c2327';for(let i=0;i<6;i++){ctx.fillRect(x+size*(.25+i*.045),y+size*(.68-i*.065),size*.46,size*.055)}ctx.strokeStyle='#77848b';ctx.lineWidth=Math.max(1,size*.01);ctx.beginPath();ctx.moveTo(x+size*.28,y+size*.7);ctx.lineTo(x+size*.67,y+size*.3);ctx.stroke();drawWorldLabel('VERTICAL ROUTE',x+size*.5,y+size*.91,'#7fb0c0');
  }else if(s.kind==='sofa'){
    ctx.fillStyle='#252d31';ctx.fillRect(x+size*.25,y+size*.55,size*.5,size*.23);ctx.fillStyle='#323b40';ctx.fillRect(x+size*.28,y+size*.44,size*.44,size*.17);ctx.fillStyle='#151a1d';ctx.fillRect(x+size*.22,y+size*.51,size*.07,size*.24);ctx.fillRect(x+size*.71,y+size*.51,size*.07,size*.24);
  }else if(s.kind==='sculpture'){
    ctx.fillStyle='#21292d';ctx.fillRect(x+size*.42,y+size*.65,size*.16,size*.22);ctx.fillStyle='#59656b';ctx.beginPath();ctx.moveTo(x+size*.5,y+size*.18);ctx.lineTo(x+size*.65,y+size*.58);ctx.lineTo(x+size*.38,y+size*.58);ctx.closePath();ctx.fill();ctx.strokeStyle='rgba(230,235,238,.2)';ctx.stroke();
  }else if(s.kind==='display'){
    ctx.fillStyle='#12181c';ctx.fillRect(x+size*.27,y+size*.3,size*.46,size*.38);ctx.strokeStyle='#45545c';ctx.strokeRect(x+size*.27,y+size*.3,size*.46,size*.38);const dg=ctx.createLinearGradient(x+size*.3,y+size*.34,x+size*.7,y+size*.62);dg.addColorStop(0,'rgba(53,101,116,.45)');dg.addColorStop(1,'rgba(75,24,31,.25)');ctx.fillStyle=dg;ctx.fillRect(x+size*.31,y+size*.34,size*.38,size*.26);ctx.fillStyle='#5f6b72';ctx.fillRect(x+size*.48,y+size*.68,size*.04,size*.16);
  }
  ctx.restore();
}
function drawWorldLabel(text,x,y,color){
  ctx.textAlign='center';ctx.font='10px Consolas';ctx.fillStyle='rgba(5,8,10,.75)';const m=ctx.measureText(text).width;ctx.fillRect(x-m/2-6,y-10,m+12,15);ctx.fillStyle=color;ctx.fillText(text,x,y+1);ctx.textAlign='left';
}
function drawWeapon(p){
  const bobX=Math.sin(p.bob*.5)*7*p.moveBlend,bobY=Math.abs(Math.cos(p.bob))*6*p.moveBlend,recoil=p.recoil*18;
  const ads=p.ads?1:0,reload=p.reloading>0?clamp(p.reloading/p.reloadTotal,0,1):0,sprint=p.sprinting?1:0,inspect=p.inspect>0?clamp(p.inspect/2.25,0,1):0;const inspectWave=inspect>0?Math.sin((1-inspect)*Math.PI):0;const ox=W*(.58-ads*.075)+bobX*(1-ads*.7)+p.lean*16,oy=H*(.73-ads*.18)+bobY*(1-ads*.7)+recoil+sprint*72+Math.sin((1-reload)*Math.PI)*reload*48-p.vault*35;
  ctx.save();ctx.translate(ox,oy);ctx.rotate(sprint*.16 + p.lean*.045 + inspectWave*.42 + (p.reloading>0?-.22*Math.sin((1-reload)*Math.PI):0));ctx.translate(-ox,-oy);
  // hands
  ctx.fillStyle='#292f32';ctx.beginPath();ctx.ellipse(ox+58,oy+102,38,28,-.4,0,TAU);ctx.fill();ctx.beginPath();ctx.ellipse(ox-58,oy+56,31,23,.15,0,TAU);ctx.fill();
  // rifle body
  const metal=ctx.createLinearGradient(ox-150,oy,ox+220,oy+130);metal.addColorStop(0,'#151a1d');metal.addColorStop(.45,'#333b40');metal.addColorStop(1,'#101416');ctx.fillStyle=metal;
  ctx.beginPath();ctx.moveTo(ox-180,oy+35);ctx.lineTo(ox+145,oy+24);ctx.lineTo(ox+212,oy+54);ctx.lineTo(ox+90,oy+78);ctx.lineTo(ox-92,oy+85);ctx.lineTo(ox-190,oy+68);ctx.closePath();ctx.fill();
  // upper rail / optic
  ctx.fillStyle='#0a0d0f';ctx.fillRect(ox-74,oy+12,155,13);ctx.fillRect(ox-12,oy-18,74,30);ctx.strokeStyle='#485158';ctx.strokeRect(ox-12,oy-18,74,30);ctx.fillStyle='#11181c';ctx.beginPath();ctx.arc(ox+25,oy-3,16,0,TAU);ctx.fill();ctx.fillStyle='rgba(192,52,64,.26)';ctx.beginPath();ctx.arc(ox+25,oy-3,7,0,TAU);ctx.fill();
  // mag and grip
  ctx.fillStyle='#1a2024';ctx.beginPath();ctx.moveTo(ox+27,oy+74);ctx.lineTo(ox+75,oy+74);ctx.lineTo(ox+65,oy+155);ctx.lineTo(ox+25,oy+145);ctx.closePath();ctx.fill();ctx.fillStyle='#12171a';ctx.beginPath();ctx.moveTo(ox-45,oy+76);ctx.lineTo(ox-10,oy+76);ctx.lineTo(ox-22,oy+126);ctx.lineTo(ox-51,oy+117);ctx.closePath();ctx.fill();
  // muzzle / laser-ish barrel detail
  ctx.fillStyle='#090b0c';ctx.fillRect(ox+145,oy+35,165,17);ctx.fillStyle='#363e43';ctx.fillRect(ox+240,oy+31,78,25);ctx.fillStyle='#090b0c';ctx.fillRect(ox+307,oy+28,28,31);
  ctx.fillStyle='#a12b35';ctx.fillRect(ox-120,oy+42,34,3);
  if(p.optic==='MAGNIFIER'){ctx.fillStyle='#171c20';ctx.fillRect(ox+54,oy-15,42,25);ctx.strokeStyle='#65727a';ctx.strokeRect(ox+54,oy-15,42,25)}
  if(p.ads){ctx.strokeStyle='rgba(197,207,212,.18)';ctx.lineWidth=2;ctx.beginPath();ctx.arc(W/2,H/2,52,0,TAU);ctx.stroke();ctx.fillStyle='rgba(173,40,52,.55)';ctx.beginPath();ctx.arc(W/2,H/2,2.3,0,TAU);ctx.fill()}
  ctx.restore();
}

function render(){renderWorld()}
function loop(t){const dt=Math.min(.05,(t-last)/1000);last=t;update(dt);render();requestAnimationFrame(loop)}
requestAnimationFrame(loop);

addEventListener('keydown',e=>{
  keys[e.code]=true;
  if(!deployed)return;
  if(e.code==='Space'){e.preventDefault();if(!tryVault())shoot()}
  if(e.code==='KeyR')reload();
  if(e.code==='KeyE')interact(false);
  if(e.code==='KeyH')hack();
  if(e.code==='KeyB')interact(true);
  if(e.code==='KeyQ')gadget();
  if(e.code==='KeyG')cycleThrowable();
  if(e.code==='KeyF')throwThrowable();
  if(e.code==='KeyV')cycleSquadCommand();
  if(e.code==='KeyX')fortify();
  if(e.code==='KeyN'&&game.spectating){const alive=game.squad.filter(m=>m.hp>0);if(alive.length){game.spectatorIndex=(game.spectatorIndex+1)%alive.length;game.spectatorMode='FOLLOW';feed(`OBSERVER TARGET // ${alive[game.spectatorIndex].id}`)}}
  if(e.code==='KeyM'&&game.spectating){feedOnce('observer-lock','RANKED OBSERVER LOCK // TEAM-FOLLOW ONLY DURING LIVE ROUND');uiBeep(false)}
  if(e.code==='KeyI')inspectWeapon();
  if(e.code==='KeyY')toggleOptic();
  if((e.code==='ControlLeft'||e.code==='ControlRight'))game.player.crouch=true;
  if(e.code==='Tab'){
    e.preventDefault();showMap=!showMap;$('#mapOverlay').classList.toggle('hidden',!showMap);paused=showMap;
    if(showMap){document.exitPointerLock?.();drawPlan(miniCtx,miniMap.width,miniMap.height,true)}
    else{canvas.requestPointerLock?.()}
  }
});
addEventListener('keyup',e=>{keys[e.code]=false;if((e.code==='ControlLeft'||e.code==='ControlRight')&&game)game.player.crouch=false});
canvas.addEventListener('mousedown',e=>{initAudio();if(e.button===0){mouseDown=true;shoot()}if(e.button===2&&deployed&&game){game.player.ads=true;game.player.sprinting=false}});
canvas.addEventListener('contextmenu',e=>e.preventDefault());
addEventListener('mouseup',e=>{if(e.button===0)mouseDown=false;if(e.button===2&&game)game.player.ads=false});
document.addEventListener('mousemove',e=>{if(deployed&&!paused&&document.pointerLockElement===canvas){if(game.spectating&&game.spectatorMode==='FREE')game.freeCam.a+=e.movementX*.00215;else game.player.a+=e.movementX*.00215*(game.player.ads&&game.player.optic==='MAGNIFIER'?.62:1)}});
document.addEventListener('pointerlockchange',()=>{
  if(!deployed||showMap||game.win)return;
  const locked=document.pointerLockElement===canvas;paused=!locked;resumeOverlay.classList.toggle('hidden',locked);
});
resumeOverlay.onclick=()=>{if(deployed&&!showMap){resumeOverlay.classList.add('hidden');canvas.requestPointerLock?.()}};

// Competitive connection-state hooks. A production client would use authenticated reconnect tickets.
addEventListener('offline',()=>setConnectionState('RECONNECTING'));
addEventListener('online',()=>setConnectionState('CONNECTED'));

// Keyboard shortcut from opening menu.
addEventListener('keydown',e=>{if(e.code==='Enter'&&menu.classList.contains('show'))$('#playBtn').click()});

})();
