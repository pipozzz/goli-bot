#pragma once

// Goli's management page, served at http://goli-xxxx.local/ on your network (api.cpp).
// A browser pairs once with a code Goli shows on his face; after that everything is set here.
static const char kWebPage[] PROGMEM = R"HTML(<!doctype html>
<html lang="en"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Goli</title>
<style>
:root{--bg:#f4f5fb;--card:#fff;--ink:#2b2d42;--muted:#6b6f86;--line:#d9dcec;--accent:#6c7ae0;--on:#fff;--red:#e5484d;--green:#3fb27f}
@media (prefers-color-scheme:dark){:root{--bg:#14151f;--card:#1e2030;--ink:#eceefa;--muted:#9a9fb8;--line:#34374d;--accent:#8e99f0;--on:#14151f}}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--ink);font:15px/1.45 system-ui,-apple-system,sans-serif}
main{max-width:560px;margin:0 auto;padding:18px 16px 60px}
h1{font-size:24px;margin:6px 0 0}.sub{color:var(--muted);margin:0 0 14px}
nav{display:flex;gap:6px;overflow-x:auto;margin:0 0 14px;padding-bottom:4px}
nav button{flex:none;border:1px solid var(--line);background:var(--card);color:var(--ink);border-radius:999px;padding:7px 13px;font:inherit;cursor:pointer}
nav button.on{background:var(--accent);color:var(--on);border-color:var(--accent)}
section{display:none}section.on{display:block}
.card{background:var(--card);border:1px solid var(--line);border-radius:14px;padding:14px;margin:0 0 12px}
.card h2{font-size:15px;margin:0 0 10px}
label{display:block;font-size:13.5px;color:var(--muted);margin:0 0 10px}
input,select,textarea{width:100%;margin-top:4px;padding:9px 11px;border:1px solid var(--line);border-radius:10px;background:var(--bg);color:var(--ink);font:inherit}
textarea{min-height:76px;font:13px ui-monospace,monospace}
input[type=checkbox]{width:auto;margin:0}input[type=range]{padding:0;accent-color:var(--accent)}
label.check{display:flex;gap:9px;align-items:center;color:var(--ink)}
.row{display:flex;gap:8px;align-items:end}.row>*{flex:1}
button.b{border:0;border-radius:10px;padding:10px 14px;background:var(--accent);color:var(--on);font:600 15px system-ui;cursor:pointer}
button.ghost{background:var(--bg);color:var(--ink);border:1px solid var(--line);font-weight:500}
.bar{display:flex;flex-wrap:wrap;gap:8px}
small{display:block;color:var(--muted);font-size:12.5px;margin-top:4px}
code{background:var(--bg);padding:1px 5px;border-radius:5px;word-break:break-all}
.log div{display:flex;gap:8px;padding:5px 0;border-top:1px solid var(--line);font-size:14px}.log div:first-child{border:0}
.dot{width:9px;height:9px;border-radius:50%;margin-top:6px;flex:none}
.kv{display:grid;grid-template-columns:auto 1fr;gap:4px 12px;font-size:14px}.kv b{font-weight:500;color:var(--muted)}
.toast{position:fixed;left:50%;bottom:20px;transform:translateX(-50%);background:var(--ink);color:var(--bg);padding:9px 15px;border-radius:10px;opacity:0;transition:.3s;pointer-events:none}
.toast.show{opacity:1}
#pair{text-align:center}#pair input{font-size:28px;text-align:center;letter-spacing:.3em;max-width:220px}
</style></head><body><main>
<h1>Goli</h1><p class="sub" id="who">on your network</p>

<div id="pair" class="card" hidden>
<h2>Pair this browser</h2>
<p class="sub">Goli shows a 6-digit code on his face. Type it here.</p>
<button class="b" onclick="pairStart()">Show me a code</button>
<p><input id="code" inputmode="numeric" maxlength="6" placeholder="000000"></p>
<button class="b" onclick="pair()">Pair</button>
</div>

<div id="app" hidden>
<nav id="tabs"></nav>

<section data-tab="Home">
<div class="card"><h2>Right now</h2><div class="kv" id="state"></div></div>
<div class="card"><h2>Play with him</h2><div class="bar">
<button class="b ghost" onclick="act('poke')">Boop</button><button class="b ghost" id="focusBtn" onclick="act('focus')">Focus</button>
<button class="b ghost" onclick="act('demo')">Demo loop</button></div></div>
<div class="card"><h2>Test an alert</h2><label>Title<input id="t_title" value="Hello from the web page"></label>
<div class="bar"><button class="b ghost" onclick="testAlert('info')">Something new</button>
<button class="b ghost" onclick="testAlert('critical')">Critical</button><button class="b ghost" onclick="testAlert('good')">Good news</button></div></div>
<div class="card"><h2>Today's meetings</h2><div class="log" id="meetings"></div></div>
<div class="card"><h2>Latest alerts</h2><div class="log" id="log"></div></div>
</section>

<section data-tab="Sounds">
<div class="card"><h2>What he plays</h2>
<label>Something new<div class="row"><select data-k="snd_info"></select><button class="b ghost" style="flex:none" onclick="play('snd_info')">&#9654;</button></div></label>
<label>Something critical<div class="row"><select data-k="snd_crit"></select><button class="b ghost" style="flex:none" onclick="play('snd_crit')">&#9654;</button></div></label>
<label>Good news<div class="row"><select data-k="snd_good"></select><button class="b ghost" style="flex:none" onclick="play('snd_good')">&#9654;</button></div></label>
<label class="check"><input type="checkbox" data-k="talk">Goli talks: "hm?" when booped, "ya-ay!" when happy</label>
<label>Volume <output id="volv"></output>%<input type="range" min="0" max="100" step="5" data-k="vol" oninput="volv.value=this.value"></label>
<div class="row"><label>Quiet from<input type="number" min="0" max="23" data-k="qfrom"></label><label>until<input type="number" min="0" max="23" data-k="qto"></label></div>
<small>No sounds at night; alerts still show on his face.</small></div>
<div class="card"><h2>Your own sound</h2>
<p class="sub" id="customState"></p>
<input type="file" id="file" accept="audio/*"><small>Any audio file; the first 3 seconds are kept, converted here in the browser.</small>
<p><button class="b ghost" onclick="uploadSound()">Upload</button></p></div>
</section>

<section data-tab="Alerts">
<div class="card"><h2>Goliash (optional)</h2>
<label>Address<input data-k="url" placeholder="https://goliash.example.com" autocapitalize="none"></label>
<label>API token<input data-k="token" type="password" placeholder="unchanged" autocomplete="off"><small>Read-only is enough: <code>goliash token create -name goli-bot -role viewer</code></small></label>
<label class="check"><input type="checkbox" data-k="push">Goliash sends alerts to me (webhook, instant)</label>
<small id="hookHelp"></small></div>
<div class="card"><h2>Which alerts ring</h2>
<label>Ring for<select data-k="minlvl"><option value="0">everything</option><option value="2">only critical</option></select></label>
<label class="check"><input type="checkbox" data-k="prod">Only production</label>
<label>Only these services<input data-k="svcs" placeholder="all services"></label></div>
<div class="card"><h2>GitHub</h2>
<label>Token<input data-k="gh" type="password" placeholder="classic token with the notifications scope" autocomplete="off">
<small>Review requests, failed CI on your PRs, mentions. Leave empty to keep; type <code>-</code> to remove.</small></label></div>
<div class="card"><h2>Keep an eye on</h2>
<label>URLs, one per line<textarea data-k="watch" placeholder="https://shop.example.com/health"></textarea>
<small>Checked every minute; he is alarmed when one stops answering and happy when it's back.</small></label></div>
<div class="card"><h2>Ring me from anything</h2><p class="sub">Scripts, CI, Home Assistant:</p><code id="curl"></code>
<label class="check" style="margin-top:10px"><input type="checkbox" id="newkey">Make a new key (the old one stops working)</label></div>
</section>

<section data-tab="Day">
<div class="card"><h2>Calendar</h2>
<label>Secret iCal address<input data-k="ics" type="password" placeholder="unchanged" autocomplete="off">
<small>Google: Settings &rarr; your calendar &rarr; Secret address in iCal format. Type <code>-</code> to remove.</small></label>
<small>An amber ring counts down the last 10 minutes before a meeting; he rings when it starts.</small></div>
<div class="card"><h2>Weather</h2>
<div class="row"><label>Latitude<input data-k="lat"></label><label>Longitude<input data-k="lon"></label></div>
<button class="b ghost" onclick="locate()">Use my location</button>
<small>Umbrella when rain is coming, sunglasses on a sunny afternoon, a scarf when it freezes. From Open-Meteo.</small></div>
<div class="card"><h2>Desk buddy</h2>
<label class="check"><input type="checkbox" data-k="buddy">Good morning, stretch breaks and "it's Friday"</label>
<label>Stretch break after (minutes at the desk, 0 = never)<input type="number" min="0" max="240" data-k="brk"></label>
<label>Focus mode lasts (minutes; 3 taps on his face)<input type="number" min="1" max="180" data-k="focus"></label>
<label>Your birthday (MM-DD)<input data-k="bday" placeholder="05-17" maxlength="5"><small>A party hat, and a "happy birthday" in the morning.</small></label>
<label class="check"><input type="checkbox" data-k="mic">Listen for claps and noise (loudness only, nothing is recorded or sent)</label></div>
<div class="card"><h2>Team</h2><p class="sub">Golis next to this one look its way when it rings.</p>
<div class="row"><label>On my left<select data-k="team_left"></select></label><label>On my right<select data-k="team_right"></select></label></div></div>
</section>

<section data-tab="Goli">
<div class="card"><h2>Wi-Fi</h2>
<label>Network<input data-k="ssid" autocapitalize="none"></label>
<label>Password<input data-k="pass" type="password" placeholder="unchanged" autocomplete="off"></label><small>Changing Wi-Fi takes a restart.</small></div>
<div class="card"><h2>Updates</h2><label class="check"><input type="checkbox" data-k="ota">Install new versions by himself (once a day)</label><p class="sub" id="ver"></p></div>
<div class="card"><h2>More</h2><div class="bar"><button class="b ghost" onclick="act('restart')">Restart</button>
<button class="b ghost" onclick="act('setup')">Setup mode (hotspot)</button><button class="b ghost" onclick="unpair()">Forget this browser</button></div></div>
</section>

<p><button class="b" style="width:100%" onclick="save()">Save</button></p>
</div>
</main><div class="toast" id="toast"></div>
<script>
const $=s=>document.querySelector(s),$$=s=>[...document.querySelectorAll(s)];
let token=localStorage.getItem('goli')||'',cfg={};
function toast(t){const e=$('#toast');e.textContent=t;e.classList.add('show');clearTimeout(e.t);e.t=setTimeout(()=>e.classList.remove('show'),3000)}
async function api(path,body,raw){const o={headers:{Authorization:'Bearer '+token}};
 if(body!==undefined){o.method='POST';if(raw)o.body=body;else{o.body=JSON.stringify(body);o.headers['Content-Type']='application/json'}}
 const r=await fetch(path,o);let d={};try{d=await r.json()}catch(e){}
 if(r.status==401){showPair();throw 'unpaired'}if(!r.ok)throw d.error||r.status;return d}
function showPair(){$('#pair').hidden=false;$('#app').hidden=true}
async function pairStart(){try{await fetch('/api/pair/start',{method:'POST'});toast('Look at Goli')}catch(e){}}
async function pair(){const r=await fetch('/api/pair',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({code:+$('#code').value})});
 const d=await r.json().catch(()=>({}));if(!r.ok)return toast(d.error||'wrong code');token=d.token;localStorage.setItem('goli',token);start()}
function unpair(){localStorage.removeItem('goli');token='';showPair()}
const secrets=['token','gh','ics','pass'];
function fill(){for(const el of $$('[data-k]')){const k=el.dataset.k,v=cfg[k];
 if(el.tagName=='SELECT'&&k.startsWith('snd_')){el.innerHTML=cfg.sounds.map(s=>`<option value="${s.name}">${s.label}</option>`).join('')}
 if(el.tagName=='SELECT'&&k.startsWith('team_')){const peers=[...new Set([...(state.peers||[]),cfg.team_left,cfg.team_right].filter(Boolean))];
  el.innerHTML='<option value="">nobody</option>'+peers.map(p=>`<option>${esc(p)}</option>`).join('')}
 if(secrets.includes(k)){el.value='';el.placeholder=cfg['has_'+(k=='gh'?'gh':k=='ics'?'ics':k)]?'unchanged':(k=='ics'?'https://calendar.google.com/...basic.ics':'');continue}
 if(el.type=='checkbox')el.checked=!!v;else el.value=v??''}
 volv.value=cfg.vol;$('#focusBtn').textContent='Focus '+cfg.focus+' min';$('#who').textContent=cfg.host+'.local';$('#ver').textContent='Version '+cfg.version;
 $('#customState').textContent=cfg.custom?'Uploaded: pick "Your own sound" above to use it.':'None yet.';
 $('#curl').textContent=`curl -H "Authorization: Bearer ${cfg.key}" -H "Content-Type: application/json" -d '{"title":"Build failed","level":"critical"}' http://${cfg.host}.local/notify`;
 $('#hookHelp').textContent=`In Goliash add a Webhook channel: URL http://${cfg.host}.local/notify (or my IP), secret ${cfg.key}.`}
let state={};
function ago(t){if(!t)return'';const d=new Date(t*1000);return d.toLocaleTimeString([],{hour:'2-digit',minute:'2-digit'})}
async function refresh(){try{state=await api('/api/state')}catch(e){return}
 const kv={IP:state.ip,'Wi-Fi':state.rssi+' dBm',Goliash:state.goliash,Calendar:state.calendar,Weather:state.weather||'off',GitHub:state.github,'Sites down':state.down,'Up for':Math.round(state.uptime_s/60)+' min'};
 $('#state').innerHTML=Object.entries(kv).map(([k,v])=>`<b>${k}</b><span>${v}</span>`).join('');
 const col={critical:'var(--red)',good:'var(--green)',info:'var(--accent)'};
 $('#log').innerHTML=state.log.length?state.log.map(l=>`<div><span class="dot" style="background:${col[l.level]}"></span><span>${ago(l.at)} ${esc(l.title)}</span></div>`).join(''):'<div>Nothing yet.</div>';
 $('#meetings').innerHTML=state.meetings.length?state.meetings.map(m=>`<div><span>${ago(m.start)} ${esc(m.title)}</span></div>`).join(''):'<div>Nothing more today.</div>'}
function esc(s){return String(s).replace(/[&<>"]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]))}
async function save(){const b={};for(const el of $$('[data-k]')){const k=el.dataset.k;
 if(secrets.includes(k)){if(el.value)b[k]=el.value=='-'?'':el.value;continue}
 b[k]=el.type=='checkbox'?el.checked:(el.type=='number'||el.type=='range'||k=='minlvl')?+el.value:el.value}
 if($('#newkey').checked)b.newkey=true;
 try{const r=await api('/api/config',b);toast(r.restart?'Saved. Restart Goli for the new Wi-Fi or Goliash.':'Saved');$('#newkey').checked=false;cfg=await api('/api/config');fill()}catch(e){toast('Not saved: '+e)}}
async function act(what){try{await api('/api/do',{do:what});toast('OK')}catch(e){toast(''+e)}}
async function testAlert(level){try{await api('/api/test',{level,title:$('#t_title').value});setTimeout(refresh,800)}catch(e){toast(''+e)}}
async function play(k){try{await api('/api/play',{sound:$(`[data-k=${k}]`).value})}catch(e){}}
function locate(){navigator.geolocation.getCurrentPosition(p=>{$('[data-k=lat]').value=p.coords.latitude.toFixed(3);$('[data-k=lon]').value=p.coords.longitude.toFixed(3)},()=>toast('No location'))}
async function uploadSound(){const f=$('#file').files[0];if(!f)return toast('Pick a file');
 const ctx=new AudioContext(),buf=await ctx.decodeAudioData(await f.arrayBuffer());
 const len=Math.min(buf.duration,3)*16000|0,off=new OfflineAudioContext(1,len,16000),src=off.createBufferSource();
 src.buffer=buf;src.connect(off.destination);src.start();const out=(await off.startRendering()).getChannelData(0);
 let peak=0;for(const v of out)peak=Math.max(peak,Math.abs(v));const g=peak>0?0.9/peak:1,pcm=new Int16Array(out.length);
 for(let i=0;i<out.length;i++)pcm[i]=Math.max(-1,Math.min(1,out[i]*g))*32767;
 const fd=new FormData();fd.append('sound',new Blob([pcm.buffer]),'custom.pcm');
 try{await api('/api/sound',fd,true);toast('Uploaded');cfg.custom=true;fill()}catch(e){toast('Upload failed: '+e)}}
function tabs(){const names=$$('section').map(s=>s.dataset.tab);
 $('#tabs').innerHTML=names.map((n,i)=>`<button class="${i?'':'on'}">${n}</button>`).join('');
 $$('#tabs button').forEach((b,i)=>b.onclick=()=>{$$('#tabs button').forEach(x=>x.classList.remove('on'));b.classList.add('on');
  $$('section').forEach((s,j)=>s.classList.toggle('on',i==j))});$$('section')[0].classList.add('on')}
async function start(){try{cfg=await api('/api/config')}catch(e){return}
 $('#pair').hidden=true;$('#app').hidden=false;await refresh();fill();setInterval(refresh,5000)}
tabs();start();
</script></body></html>
)HTML";
