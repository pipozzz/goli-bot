#pragma once

// The setup page served from Goli's hotspot. {{NAME}} placeholders are filled in by setup.cpp.
static const char kSetupPage[] = R"HTML(<!doctype html>
<html lang="en"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Goli setup</title>
<style>
:root{--bg:#f4f5fb;--card:#fff;--ink:#2b2d42;--muted:#6b6f86;--line:#d9dcec;--accent:#6c7ae0}
@media (prefers-color-scheme:dark){:root{--bg:#14151f;--card:#1e2030;--ink:#eceefa;--muted:#9a9fb8;--line:#34374d;--accent:#8e99f0}}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--ink);font:16px/1.45 system-ui,-apple-system,sans-serif}
main{max-width:440px;margin:0 auto;padding:20px 16px 40px}
h1{font-size:24px;margin:8px 0 2px}p.sub{color:var(--muted);margin:0 0 18px}
fieldset{border:1px solid var(--line);background:var(--card);border-radius:14px;padding:14px 14px 6px;margin:0 0 14px}
legend{font-weight:700;padding:0 6px}
label{display:block;font-size:14px;color:var(--muted);margin:0 0 10px}
input,textarea,select{width:100%;margin-top:4px;padding:10px 12px;border:1px solid var(--line);border-radius:10px;background:var(--bg);color:var(--ink);font:inherit}
textarea{min-height:90px;font:12px ui-monospace,monospace}
.row{display:flex;gap:10px}.row label{flex:1}
input[type=range]{padding:0;accent-color:var(--accent)}
small{display:block;color:var(--muted);font-size:12.5px;margin-top:4px}
code{background:var(--bg);padding:1px 5px;border-radius:5px}
button{width:100%;padding:13px;border:0;border-radius:12px;background:var(--accent);color:#fff;font:600 17px system-ui;cursor:pointer}
details{margin:0 0 10px}summary{cursor:pointer;color:var(--muted);font-size:14px}
</style></head><body><main>
<h1>Hi, I'm Goli 👋</h1>
<p class="sub">Tell me your Wi-Fi and your Goliash, and I'll ring when something needs you.</p>
<form method="post" action="/save">
<fieldset><legend>Wi-Fi</legend>
<label>Network<input name="ssid" list="nets" value="{{SSID}}" required autocomplete="off" autocapitalize="none">
<datalist id="nets"></datalist><small id="scan">Looking for networks…</small></label>
<label>Password<input name="pass" type="password" placeholder="{{PASS_HINT}}" autocomplete="off"></label>
</fieldset>
<fieldset><legend>Goliash</legend>
<label>Address<input name="url" type="url" value="{{URL}}" placeholder="https://goliash.example.com" autocapitalize="none"></label>
<label>API token<input name="token" type="password" placeholder="{{TOKEN_HINT}}" autocomplete="off">
<small>Read-only is enough: <code>goliash token create -name goli-bot -role viewer</code>, or Settings → Users → API tokens.</small></label>
<details><summary>Certificate (optional)</summary>
<label>CA certificate, PEM<textarea name="ca" placeholder="-----BEGIN CERTIFICATE-----">{{CA}}</textarea>
<small>Leave empty to skip the certificate check.</small></label></details>
</fieldset>
<fieldset><legend>Goli</legend>
<label>Volume <output id="v">{{VOL}}</output>%<input name="vol" type="range" min="0" max="100" step="5" value="{{VOL}}" oninput="v.value=this.value"></label>
<div class="row"><label>Quiet from<input name="qfrom" type="number" min="0" max="23" value="{{QFROM}}"></label>
<label>until<input name="qto" type="number" min="0" max="23" value="{{QTO}}"></label></div>
<small style="margin:-4px 0 10px">No sounds during these hours; alerts still show on my face.</small>
</fieldset>
<button>Save</button>
</form></main>
<script>
fetch('/scan').then(r=>r.json()).then(n=>{const d=document.getElementById('nets');
n.forEach(s=>{const o=document.createElement('option');o.value=s;d.appendChild(o)});
document.getElementById('scan').textContent=n.length?n.length+' networks nearby: tap the field to pick one':'No networks found, type the name'})
.catch(()=>document.getElementById('scan').textContent='');
</script></body></html>
)HTML";

static const char kSavedPage[] = R"HTML(<!doctype html>
<html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Goli</title>
<style>body{font:18px system-ui;text-align:center;padding:60px 20px;background:#14151f;color:#eceefa}</style></head>
<body><h1>Saved 🔔</h1><p>Goli is restarting and joining your Wi-Fi.<br>You can close this page.</p></body></html>
)HTML";
