#!/usr/bin/env python3
"""Builds ONE self-contained interactive html page from the runner csv files (api_card_runner.py).

Rows = tested API calls (area / class / method / argument), columns = cards (identified by ATR), one coloured cell per card.
Inspired by https://www.fi.muni.cz/~xsvenda/jcalgtest/table.html (cards c1..cN with abbreviations, one cell per card).

    python3 api_results_html.py                                   # docs/card_results/*.csv -> docs/card_results/api_table.html
    python3 api_results_html.py --in-dir some/dir --out table.html

Input: every *.csv with the runner header (row_type;timestamp;card_name;atr;...). Other csv files are skipped.
Several files of one card (same ATR) are merged per call: a real result beats SKIPPED_BY_USER, then the newest one wins.
Card names: --names "ATR=label,..." beats <in-dir>/atr_names_local.csv (hand-added cards), which beats
<in-dir>/atr_names.csv (ATR -> name from the public table, see api_atr_names.py), which beats the card_name found in the csv.
"""
import argparse
import csv
import glob
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from api_exception_codes import parse as parse_exc  # noqa: E402
from api_atr_names import load as load_atr_names, norm_atr  # noqa: E402
from collections import Counter

HERE = os.path.dirname(os.path.abspath(__file__))
REQUIRED = {"row_type", "timestamp", "card_name", "atr", "api_version", "area", "class", "method", "argument", "status"}
WEAK = {"SKIPPED_BY_USER"}


def read_files(in_dir):
    cards, used, skipped = {}, [], []
    stages = {}   # atr -> area -> (timestamp, failure_type, detail) of the latest failed UPLOAD/TEST stage
    for path in sorted(glob.glob(os.path.join(in_dir, "*.csv"))):
        with open(path, newline="", encoding="utf-8-sig") as f:
            rd = csv.DictReader(f, delimiter=";")
            if not rd.fieldnames or not REQUIRED <= set(rd.fieldnames):
                skipped.append(os.path.basename(path))
                continue
            allrows = list(rd)
        for r in allrows:
            if r["row_type"] == "STAGE" and r["atr"] and r["stage_result"] == "FAILED" and r["stage"] in ("UPLOAD", "TEST"):
                old = stages.setdefault(r["atr"], {}).get(r["area"])
                if old is None or r["timestamp"] >= old[0]:
                    stages[r["atr"]][r["area"]] = (r["timestamp"], r["failure_type"], r["detail"])
        rows = [r for r in allrows if r["row_type"] == "CALL" and r["atr"]]
        if not rows:
            skipped.append(os.path.basename(path))
            continue
        used.append(os.path.basename(path))
        for r in rows:
            c = cards.setdefault(r["atr"], {"atr": r["atr"], "names": Counter(), "api": Counter(), "first": r["timestamp"], "calls": {}, "files": set()})
            c["names"][r["card_name"]] += 1
            c["api"][r["api_version"]] += 1
            c["first"] = min(c["first"], r["timestamp"])
            c["files"].add(os.path.basename(path))
            key = (r["area"], r["class"], r["method"], r["argument"])
            old = c["calls"].get(key)
            new = (r["status"], r["call_details"], r["timestamp"])
            if old is None or (old[0] in WEAK and new[0] not in WEAK) or ((old[0] in WEAK) == (new[0] in WEAK) and new[2] >= old[2]):
                c["calls"][key] = new
    for atr, c in cards.items():
        c["stages"] = stages.get(atr, {})
    return cards, used, skipped


def build_data(cards, names):
    order = sorted(cards.values(), key=lambda c: c["first"])
    keys, seen = [], set()
    for c in order:  # keep the order of the first card that has the call, then the others
        for k in c["calls"]:
            if k not in seen:
                seen.add(k)
                keys.append(k)
    area_rank = {}
    for k in keys:
        area_rank.setdefault(k[0], len(area_rank))
    keys.sort(key=lambda k: (area_rank[k[0]], k[1]))  # stable: method order inside a class stays as tested
    out_cards = []
    for i, c in enumerate(order, 1):
        out_cards.append({
            "id": "c%d" % i, "atr": c["atr"], "name": names.get(c["atr"]) or names.get(norm_atr(c["atr"])) or c["names"].most_common(1)[0][0],
            "api": c["api"].most_common(1)[0][0], "files": sorted(c["files"]),
            "counts": dict(Counter(v[0] for v in c["calls"].values())),
            "stages": {a: [t[1], t[2][:300]] for a, t in c["stages"].items()},
        })
    rows = []
    for k in keys:
        cells = []
        for c in order:
            v = c["calls"].get(k)
            if v is None:
                cells.append(None)
                continue
            pe = parse_exc(k[1], k[2], v[0], v[1])
            cells.append([v[0], v[1], pe["text"], pe["short"]] if pe else [v[0], v[1]])
        rows.append({"area": k[0], "cls": k[1], "method": k[2], "arg": k[3], "cells": cells})
    return {"cards": out_cards, "rows": rows}


PAGE = r"""<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>JCAlgTest API support table</title>
<style>
:root{--bg:#fff;--fg:#1c1e21;--mut:#667;--bd:#d5d8dd;--hd:#f2f4f7;--ok:#bfe8c4;--no:#f6c6c6;--warn:#fbe4a6;--na:#e3e5e8;--info:#cfe0f8}
@media(prefers-color-scheme:dark){:root{--bg:#16181c;--fg:#e6e8eb;--mut:#9aa;--bd:#383c44;--hd:#21252b;--ok:#25502d;--no:#5d2b2b;--warn:#5c4a16;--na:#2c3036;--info:#25405f}}
body{margin:0;font:13px/1.35 system-ui,sans-serif;background:var(--bg);color:var(--fg)}
header{padding:12px 16px;border-bottom:1px solid var(--bd)}h1{font-size:18px;margin:0 0 4px}.mut{color:var(--mut)}
#bar{position:sticky;top:0;z-index:5;background:var(--bg);display:flex;flex-wrap:wrap;gap:8px 14px;align-items:center;padding:8px 16px;border-bottom:1px solid var(--bd)}
#bar input[type=search],#bar select{font:inherit;padding:4px 6px;background:var(--bg);color:var(--fg);border:1px solid var(--bd);border-radius:4px}
#bar label{white-space:nowrap}
#cardlist{margin:6px 16px;padding:0 0 0 18px;font-family:ui-monospace,monospace;font-size:12px}#bar button{font:inherit;padding:3px 9px}
#legend{display:flex;flex-wrap:wrap;gap:4px 6px;padding:6px 16px}
.chip{padding:1px 7px;border-radius:9px;border:1px solid var(--bd);cursor:pointer;user-select:none}.chip.off{opacity:.35;text-decoration:line-through}
#wrap{margin:0 16px 16px;border:1px solid var(--bd)}
table{border-collapse:separate;border-spacing:0;table-layout:fixed}
th,td{border-right:1px solid var(--bd);border-bottom:1px solid var(--bd);padding:2px 6px;background:var(--bg)}
thead th{position:sticky;top:var(--barh,44px);background:var(--hd);z-index:3;text-align:center;font-weight:600}
td.m,th.m{position:sticky;left:0;z-index:2;text-align:left;overflow:hidden;text-overflow:ellipsis;white-space:nowrap;background:var(--hd)}
thead th.m{z-index:4}
td.m small{color:var(--mut)}
tr.grp td{position:sticky;left:0;background:var(--hd);font-weight:600;cursor:pointer}
td.c{text-align:center;cursor:pointer;font-size:14px;overflow:hidden}
th{position:relative}.rz{position:absolute;top:0;right:-3px;bottom:0;width:7px;cursor:col-resize;z-index:6}.rz:hover,.rz.on{background:#2a7de155}
.s-OK{background:var(--ok)!important}.s-bad{background:var(--no)!important}.s-warn{background:var(--warn)!important}.s-na{background:var(--na)!important}.s-info{background:var(--info)!important}
td.c.none{color:var(--mut)}
#help{position:fixed;top:0;right:0;bottom:0;width:380px;max-width:92vw;overflow:auto;background:var(--hd);border-left:1px solid var(--bd);padding:8px 14px;z-index:20;display:none;box-shadow:-2px 0 12px #0004}
#help h2{margin:0 0 6px;font-size:16px}#help summary{cursor:pointer;font-weight:600;margin:6px 0}#help dt{margin-top:8px}#help dd{margin:2px 0 0 0;color:var(--mut)}
#help dt span{padding:1px 7px;border-radius:9px;border:1px solid var(--bd);font-weight:600}
#detail{position:fixed;right:12px;bottom:12px;max-width:560px;background:var(--hd);border:1px solid var(--bd);padding:8px 12px;border-radius:6px;display:none;z-index:9;box-shadow:0 2px 12px #0004}
#detail pre{white-space:pre-wrap;margin:4px 0 0}
th .sub{font-weight:400;font-size:10px;color:var(--mut);display:block}
</style></head><body>
<header><h1>JCAlgTest &ndash; JavaCard API support</h1>
<div class="mut">Generated __GEN__ &middot; <span id="nc"></span> cards &middot; <span id="nr"></span> API calls. Click a cell for details, a group header to collapse it, a card header to sort by it.</div></header>
<div id="bar">
<input id="q" type="search" placeholder="filter class / method / argument" size="34">
<label>Area <select id="area"><option value="">all</option></select></label>
<label><input id="diff" type="checkbox"> only rows where cards differ</label>
<label><input id="miss" type="checkbox"> hide rows nobody supports</label>
<label><input id="showexc" type="checkbox"> show exception reason in cells</label>
<label>Card <select id="card"><option value="">all</option></select></label>
<button id="helpbtn">? Help</button><button id="colall">Collapse all</button><button id="expall">Expand all</button>
<span class="mut" id="shown"></span></div>
<ul id="cardlist"></ul>
<div id="legend"></div>
<div id="wrap"><table id="tbl"><colgroup id="cg"></colgroup><thead id="th"></thead><tbody id="tb"></tbody></table></div>
<aside id="help"><a href="#" id="hx" style="float:right">close</a><h2>Help</h2>
<details open><summary>What is shown</summary>
<p>Each <b>row</b> is one call of the JavaCard API (method, constructor or constant check) made by the on-card test applet, grouped by area and class (e.g. <i>Crypto / javacard.security.KeyAgreement</i>). The text in brackets is the argument variant that was tried (e.g. an algorithm or key-length constant).</p>
<p>Each <b>column</b> is one tested card (c1, c2, ...), listed with its ATR and the API version of the CAP that was run on it, above the table. A cell shows the result of that call on that card; the colour groups similar outcomes. An empty cell (&middot;) means the call was not part of that card's run (area not loaded, run aborted, or the card was tested with a different API version).</p>
<p><b>Symbols</b> replace the long result names in the cells (colours are kept): the key above the table and the <i>Result strings</i> list below give the meaning of each symbol; hover a cell for the full result name and the decoded exception reason. <b>&#10003;</b> yes, <b>&#10007;</b> unsupported per spec (card refuses an optional feature), <b>&ndash;</b> not in the CAP API version (not called), <b>&#8856;</b> skipped by user, <b>&ne;</b> wrong result, <b>EX</b> JavaCard exception, <b>RT</b> other runtime exception, <b>MEM</b> out of memory, <b>SW</b> status word error, <b>&#9888;</b> card error, <b>&empty;</b> applet not installed.</p>
<p>The header of a card shows the number of <i>yes</i> (OK) results; hover for all counts and the source csv files. If a card was run several times, results are merged per call: a real result wins over <i>skipped by user</i>, otherwise the newest wins.</p></details>
<details open><summary>Controls</summary><ul>
<li><b>Filter</b>: text match on class / method / argument; <b>Area</b>, <b>Card</b> selectors.</li>
<li><b>Only rows where cards differ</b>: hides rows where all shown cards have the same status.</li>
<li><b>Hide rows nobody supports</b>: hides rows without any <i>yes</i>.</li>
<li><b>Status chips</b> (legend): click to hide or show all cells of a status; rows with only hidden statuses disappear.</li>
<li><b>Collapse all / Expand all</b> and a click on a group header fold the groups.</li>
<li><b>Card header</b>: sorts rows by that card (yes first); click again to reverse. Sorting disables grouping.</li>
<li><b>Cell click</b>: details returned by the test (returned values, exception, status word).</li></ul></details>
<details open><summary>Exception reasons</summary>
<p>Cell details such as <code>EXC_CARDRUNTIME reason=0x0003</code> are decoded with the reason constants of the Java Card 3.1 API (e.g. CryptoException: 1 ILLEGAL_VALUE, 2 UNINITIALIZED_KEY, 3 NO_SUCH_ALGORITHM, 4 INVALID_INIT, 5 ILLEGAL_USE; SystemException: 1 ILLEGAL_VALUE, 2 NO_TRANSIENT_SPACE, ... 6 ILLEGAL_USE). When the csv names only CardRuntimeException, the concrete class is inferred from the tested class (javacard.security / javacardx.crypto &rarr; CryptoException, javacardx.framework.tlv &rarr; TLVException, ...) and the popup says so. Hover a cell for the decoded reason, click it for details, or tick <i>show exception reason in cells</i> to print it instead of the status. Status words (<code>SW=6985</code>) are named too.</p></details>
<details open><summary>Result strings</summary><dl id="hdefs"></dl></details>
</aside>
<script>
const D=__DATA__;
const CLS={OK:'s-OK',NOT_IN_CAP_API_VERSION:'s-na',UNSUPPORTED_PER_SPEC:'s-na',SKIPPED_BY_USER:'s-na',NO_MEMORY:'s-warn',WRONG_RESULT:'s-warn',SW_ERROR:'s-bad',CARD_ERROR:'s-bad'};
const cls=s=>CLS[s]||(s.startsWith('EXC_')?'s-bad':'s-info');
const SHORT=s=>s==='OK'?'yes':s.replace(/_/g,' ').toLowerCase();
const SYM={OK:'\u2713',UNSUPPORTED_PER_SPEC:'\u2717',NOT_IN_CAP_API_VERSION:'\u2013',SKIPPED_BY_USER:'\u2298',WRONG_RESULT:'\u2260',EXC_CARDRUNTIME:'EX',EXC_RUNTIME:'RT',NO_MEMORY:'MEM',SW_ERROR:'SW',CARD_ERROR:'\u26A0',APPLET_NOT_INSTALLED:'\u2205'};
const SYMB=s=>SYM[s]||SHORT(s);
const VER=v=>/^\d{3}$/.test(v)?v[0]+'.'+v[1]+'.'+v[2]:v;
function noteEmpty(r,c){const st=(c.stages||{})[r.area];
 const h='Not tested: this call is not part of the run of '+c.id+' (API '+VER(c.api)+').';
 if(st)return h+' Area '+r.area+' stopped with '+st[0].replace(/_/g,' ').toLowerCase()+(st[1]?' ('+st[1]+')':'')+'.';
 return h+' Reasons: the area was not run, its CAP was not loaded, or the run ended before this call (no failed stage is recorded for area '+r.area+').'}
function explain(r,c,x){const s=x[0],d=x[1]||'',p=x[2]||'';const dv=(d.match(/returned=(\S+)/)||[])[1],ef=(d.match(/effect=([^;\s]+(?: [^;\s]+)*)/)||[])[1];
 switch(s){
 case 'OK':return 'Call made, result as expected'+(dv?' (returned '+dv+')':'')+'.';
 case 'NOT_IN_CAP_API_VERSION':{const seen=r.cells.map((y,i)=>[y,i]).filter(z=>z[0]&&z[0][0]!=='NOT_IN_CAP_API_VERSION'&&z[0][0]!=='APPLET_NOT_INSTALLED'&&z[0][0]!=='SKIPPED_BY_USER').map(z=>D.cards[z[1]]).filter(k=>+k.api>+c.api);
  const sn=seen.length?' Exists in API '+VER(String(Math.min(...seen.map(k=>+k.api))))+' (called on '+seen.map(k=>k.id).join(', ')+').':'';
  return 'Call NOT made: the member is not part of the API version of the CAP tested on this card (CAP built for API '+VER(c.api)+').'+sn+' Says nothing about the card.'}
 case 'UNSUPPORTED_PER_SPEC':return 'Call made; the card refused it'+(p?' with '+p:(d?' ('+d.replace(/;$/,'')+')':''))+', which the specification allows for an optional feature. Not a defect.';
 case 'SKIPPED_BY_USER':return 'Call NOT made: excluded from the run on purpose ('+d.replace(/;$/,'')+'), usually because it hangs or resets the card. Support unknown.';
 case 'WRONG_RESULT':return 'Call made and completed, but the outcome differs from the specification: '+(dv?'returned value '+dv+(ef==='UNEXPECTED'?' and an unexpected observable effect':', effect '+ef+' (so the returned value itself differs from the expected one)'):'effect '+(ef||'unexpected'))+'. The expected value is not stored in the csv; see the test source.';
 case 'EXC_CARDRUNTIME':return 'Call made; it threw a JavaCard exception not allowed here: '+(p||d.replace(/;$/,''))+'.';
 case 'EXC_RUNTIME':return 'Call made; it threw a non-JavaCard RuntimeException (e.g. NullPointerException, ArrayIndexOutOfBounds, ClassCast). The applet does not record the exception class.';
 case 'NO_MEMORY':return 'Call made; the card ran out of memory ('+(p||d.replace(/;$/,''))+'). Not a statement about support.';
 case 'SW_ERROR':return 'Call made; the card answered with an unexpected status word ('+d.replace(/;$/,'')+') instead of 9000.';
 case 'CARD_ERROR':return 'Communication with the card failed ('+d.replace(/;$/,'')+'): card lost / reset / reader problem. This call or an earlier one probably caused it; it is not an API verdict.';
 case 'APPLET_NOT_INSTALLED':return 'Call NOT made: the CAP of this area is not installed on the card.';
 default:return SHORT(s)}}
const ci0=()=>document.getElementById('card').value;
const $=id=>document.getElementById(id);
const HELP={
OK:['yes','The call succeeded and the returned value / observable effect matched the specification (or the expected value).'],
UNSUPPORTED_PER_SPEC:['unsupported per spec','The card refused the call in a way the specification allows: an optional feature or algorithm is not implemented (e.g. CryptoException.NO_SUCH_ALGORITHM, SystemException.ILLEGAL_USE, no logical channels). Not a bug of the card.'],
NOT_IN_CAP_API_VERSION:['not in CAP API version','The member does not exist in the API version the CAP was built for (e.g. a JavaCard 3.0.5 method when the card was tested with the 3.0.4 CAP), so it was not called. Says nothing about the card.'],
SKIPPED_BY_USER:['skipped by user','Deliberately not run (the runner was started with --exclude), typically because the call is known to hang or reset the card (e.g. RSA 2048 key generation). Unknown support.'],
WRONG_RESULT:['wrong result','The call completed but the returned value or effect differs from the specification (e.g. a wrong output length or signature). A card deviation or a known reference-runtime issue; see the cell details.'],
EXC_CARDRUNTIME:['exception (CardRuntimeException)','The call threw a JavaCard runtime exception that the specification does not list as acceptable here (e.g. a CryptoException / SystemException / APDUException with another reason code). The reason is in the cell details.'],
EXC_RUNTIME:['exception (RuntimeException)','The call threw a non-JavaCard runtime exception (e.g. NullPointerException, ArrayIndexOutOfBounds, ClassCast). Usually an implementation bug of the card or a precondition the card does not honour.'],
NO_MEMORY:['no memory','The card ran out of transient or persistent memory (SystemException NO_TRANSIENT_SPACE / NO_RESOURCE) so the test could not be evaluated. Not a statement about support; may succeed on a card with more RAM or EEPROM.'],
SW_ERROR:['status word error','The card answered the test APDU with an unexpected ISO 7816 status word (not 9000), see cell details.'],
CARD_ERROR:['card error','Communication with the card failed during the call (card lost / reset / unresponsive / timeout). The call was probably the cause; later calls of the run may be missing.'],
APPLET_NOT_INSTALLED:['applet not installed','The CAP of this area was not installed on the card (or the simulator), so the call could not be made.']};
const W={m:380,c:80};const CW={};
const off=new Set(),collapsed=new Set();let sortCard=-1,sortDir=1;
const statuses=[...new Set(D.rows.flatMap(r=>r.cells.filter(Boolean).map(c=>c[0])))].sort();
$('nc').textContent=D.cards.length;$('nr').textContent=D.rows.length;
[...new Set(D.rows.map(r=>r.area))].forEach(a=>$('area').add(new Option(a,a)));
D.cards.forEach((c,i)=>$('card').add(new Option(c.id+' '+c.name+' ('+c.atr+')',i)));
statuses.forEach(s=>{const e=document.createElement('span');e.className='chip '+cls(s);e.textContent=SYMB(s)+' '+SHORT(s);e.title=s+' (click to hide/show)';
 e.onclick=()=>{off.has(s)?off.delete(s):off.add(s);e.classList.toggle('off');draw()};$('legend').append(e)});
const e0=document.createElement('span');e0.className='mut';e0.textContent=' empty cell = call not present in that card\'s run';$('legend').append(e0);
function widths(cards){const cg=$('cg');cg.innerHTML='<col style="width:'+W.m+'px">'+cards.map(c=>'<col style="width:'+(CW[c.idx]||W.c)+'px">').join('');
 $('tbl').style.width=(W.m+cards.reduce((a,c)=>a+(CW[c.idx]||W.c),0))+'px'}
function head(cards){
 let h='<tr><th class="m">API call<span class="rz" data-k="m"></span></th>';
 cards.forEach(c=>{const n=Object.entries(c.counts).map(([k,v])=>SHORT(k)+': '+v).join('\n');
  h+=`<th data-i="${c.idx}" title="${c.id} ${c.name}\nATR ${c.atr}\nAPI ${c.api}\n${n}\nfiles: ${c.files.join(', ')}" style="cursor:pointer">${c.id}<span class="sub" style="white-space:normal;font-weight:600">${c.name}</span><span class="sub">API ${c.api}</span><span class="sub">${(c.counts.OK||0)} yes</span><span class="rz" data-k="${c.idx}"></span></th>`});
 $('th').innerHTML=h+'</tr>';widths(cards);
 $('th').querySelectorAll('.rz').forEach(z=>{z.onclick=e=>e.stopPropagation();z.onmousedown=e=>{e.preventDefault();e.stopPropagation();
  const k=z.dataset.k,x0=e.clientX,w0=k==='m'?W.m:(CW[k]||W.c);z.classList.add('on');
  const mv=ev=>{const w=Math.max(40,w0+ev.clientX-x0);if(k==='m')W.m=w;else CW[k]=w;widths(D.cards.map((c,i)=>({idx:i})).filter(c=>ci0()===''||c.idx==ci0()))};
  const up=()=>{document.removeEventListener('mousemove',mv);document.removeEventListener('mouseup',up);z.classList.remove('on')};
  document.addEventListener('mousemove',mv);document.addEventListener('mouseup',up)};
 z.ondblclick=e=>{e.stopPropagation();const k=z.dataset.k;if(k==='m')W.m=380;else delete CW[k];draw()}});
 $('th').querySelectorAll('th[data-i]').forEach(t=>t.onclick=()=>{const i=+t.dataset.i;sortDir=sortCard===i?-sortDir:1;sortCard=i;draw()});}
const esc=s=>String(s).replace(/[&<>"]/g,m=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[m]));
function draw(){
 const q=$('q').value.toLowerCase(),area=$('area').value,ci=$('card').value,diff=$('diff').checked,miss=$('miss').checked;
 const cards=D.cards.map((c,i)=>({...c,idx:i})).filter(c=>ci===''||c.idx==ci);head(cards);
 let rows=D.rows.filter(r=>{
  if(area&&r.area!==area)return false;
  if(q&&!(r.cls+' '+r.method+' '+r.arg).toLowerCase().includes(q))return false;
  const v=cards.map(c=>r.cells[c.idx]);
  if(v.every(x=>x&&off.has(x[0])))return false;
  if(miss&&!v.some(x=>x&&x[0]==='OK'))return false;
  if(diff&&new Set(v.map(x=>x?x[0]:'')).size<2)return false;return true});
 if(sortCard>=0){const rk=r=>{const x=r.cells[sortCard];return x?(x[0]==='OK'?0:cls(x[0])==='s-na'?2:1):3};
  rows=rows.map((r,i)=>[r,i]).sort((a,b)=>sortDir*(rk(a[0])-rk(b[0]))||a[1]-b[1]).map(x=>x[0]);}
 let h='',last='';
 rows.forEach((r,ri)=>{const g=r.area+' / '+r.cls;
  if(sortCard<0&&g!==last){last=g;h+=`<tr class="grp" data-g="${esc(g)}"><td class="m" colspan="${cards.length+1}">${collapsed.has(g)?'&#9654;':'&#9660;'} ${esc(g)}</td></tr>`}
  if(sortCard<0&&collapsed.has(g))return;
  h+=`<tr><td class="m" title="${esc(r.cls+'\n'+r.method+'\n'+r.arg)}">${esc(r.method)} <small>${r.arg&&r.arg!=='-'?'('+esc(r.arg)+')':''}</small></td>`;
  cards.forEach(c=>{const x=r.cells[c.idx];
   h+=x?`<td class="c ${cls(x[0])}" data-r="${D.rows.indexOf(r)}" data-c="${c.idx}" title="${esc(SHORT(x[0])+'\n'+explain(r,c,x))}">${$('showexc').checked&&x[3]?esc(x[3]):esc(SYMB(x[0]))}</td>`:'<td class="c none" title="${esc(noteEmpty(r,c))}">&middot;</td>'});
  h+='</tr>'});
 $('tb').innerHTML=h;$('shown').textContent=rows.length+' rows shown';
 $('tb').querySelectorAll('td.c[data-r]').forEach(t=>t.onclick=()=>{const r=D.rows[+t.dataset.r],c=D.cards[+t.dataset.c],x=r.cells[+t.dataset.c];
  const d=$('detail');d.style.display='block';d.innerHTML=`<b>${c.id}</b> <a href="#" id="x" style="float:right">close</a><div class="mut">${esc(r.cls)}</div><div>${esc(r.method)} &mdash; ${esc(r.arg)}</div><div><b>${esc(x[0])}</b></div><div>${esc(explain(r,c,x))}</div>${x[2]?'<div>Parsed: <b>'+esc(x[2])+'</b></div>':''}<pre>${esc(x[1])}</pre>`;
  $('x').onclick=e=>{e.preventDefault();d.style.display='none'}});
 $('tb').querySelectorAll('tr.grp').forEach(t=>t.onclick=()=>{const g=t.dataset.g;collapsed.has(g)?collapsed.delete(g):collapsed.add(g);draw()});
}
D.cards.forEach(c=>{const li=document.createElement('li');li.textContent=c.id+'  '+c.name+'  ATR '+c.atr+'  API '+c.api;$('cardlist').append(li)});
$('colall').onclick=()=>{sortCard=-1;D.rows.forEach(r=>collapsed.add(r.area+' / '+r.cls));draw()};
$('expall').onclick=()=>{collapsed.clear();draw()};
Object.keys(HELP).concat(statuses.filter(s=>!(s in HELP))).forEach(s=>{const h=HELP[s]||[s.replace(/_/g,' ').toLowerCase(),'Status reported by the test client; see the cell details.'];
 const n=D.cards.reduce((a,c)=>a+(c.counts[s]||0),0);if(!n&&!(s in HELP))return;
 $('hdefs').insertAdjacentHTML('beforeend',`<dt><span class="${cls(s)}" style="font-size:15px;min-width:2.2em;display:inline-block;text-align:center">${esc(SYMB(s))}</span> <span class="${cls(s)}">${esc(h[0])}</span> <code>${s}</code> <small class="mut">&times;${n}</small></dt><dd>${esc(h[1])}</dd>`)});
$('helpbtn').onclick=()=>{const h=$('help');h.style.display=h.style.display==='block'?'none':'block'};
$('hx').onclick=e=>{e.preventDefault();$('help').style.display='none'};
['q','area','diff','miss','card','showexc'].forEach(i=>$(i).addEventListener('input',draw));
const setBarH=()=>document.documentElement.style.setProperty('--barh',$('bar').offsetHeight+'px');setBarH();addEventListener('resize',setBarH);
draw();
</script></body></html>
"""


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--in-dir", default=os.path.join(HERE, "..", "docs", "card_results"))
    ap.add_argument("--out", default=None, help="default: <in-dir>/api_table.html")
    ap.add_argument("--names", default="", help='"ATR=label,ATR=label" display names')
    a = ap.parse_args()
    out = a.out or os.path.join(a.in_dir, "api_table.html")
    names = load_atr_names(os.path.join(a.in_dir, "atr_names.csv"))
    names.update(load_atr_names(os.path.join(a.in_dir, "atr_names_local.csv")))
    names.update((norm_atr(k), v) for k, v in (p.split("=", 1) for p in a.names.split(",") if "=" in p))
    cards, used, skipped = read_files(a.in_dir)
    if not cards:
        sys.exit("no runner csv with CALL rows in " + a.in_dir)
    data = build_data(cards, names)
    from datetime import datetime
    html = PAGE.replace("__GEN__", datetime.now().strftime("%Y-%m-%d %H:%M")).replace("__DATA__", json.dumps(data, separators=(",", ":")).replace("</", "<\\/"))
    with open(out, "w", encoding="utf-8") as f:
        f.write(html)
    print("cards=%d rows=%d -> %s" % (len(data["cards"]), len(data["rows"]), out))
    print("used:", ", ".join(used))
    if skipped:
        print("skipped (no CALL rows / other format):", ", ".join(skipped))
    for c in data["cards"]:
        print(" ", c["id"], c["atr"], "API", c["api"], c["name"], "OK=%d/%d" % (c["counts"].get("OK", 0), sum(c["counts"].values())))


if __name__ == "__main__":
    main()
