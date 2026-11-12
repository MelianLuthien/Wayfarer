"""Live, playable mockup of the menu in a browser: the real menu code as WebAssembly (tools/mockbuild.py),
driven by the keyboard like the controller. Reloads itself, keeping your place, when the build changes.

usage: python tools/mockserver.py        (prints the address; picks a free port)
"""
import http.server
import json
import os
import socketserver

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MOCK = os.path.join(ROOT, "build", "mock")
ART = os.path.join(ROOT, "build", "art")
FILES = {"/menu.wasm": os.path.join(MOCK, "menu.wasm")}
for n in ("fonts.bin", "panel.bin", "icons64.bin", "logo.bin"):
    FILES["/" + n] = os.path.join(ART, n)

PAGE = r"""<!doctype html><meta charset=utf-8><title>Wayfarer menu, playable</title>
<style>
body{margin:0;background:#1b140e;color:#e9d9b5;font:14px system-ui,sans-serif}
#wrap{padding:14px;display:flex;flex-direction:column;align-items:center;gap:10px}
canvas{max-width:100%;height:auto;box-shadow:0 6px 30px #000a;cursor:crosshair;outline:none}
#pad{display:flex;gap:6px;flex-wrap:wrap;justify-content:center}
button{background:#4a3320;color:#f3ead2;border:1px solid #a8893f;border-radius:14px;padding:6px 14px;cursor:pointer;font:inherit}
button:active{background:#f0a23a;color:#3a2412}
#info{color:#c9b27a;font-variant-numeric:tabular-nums;text-align:center;line-height:1.5}
</style>
<div id=wrap>
<canvas id=c width=1080 height=680 tabindex=0></canvas>
<div id=pad></div>
<div id=info></div>
</div>
<script>
const KEYS={ArrowUp:13,ArrowDown:15,ArrowLeft:12,ArrowRight:14,Enter:0," ":0,z:0,Escape:1,Backspace:1,b:1,x:2,y:3,q:6,PageUp:6,e:7,PageDown:7};
const BTN=[["Left",12],["Up",13],["Down",15],["Right",14],["A",0],["B",1],["X",2],["Y",3],["L",6],["R",7]];
const c=document.getElementById("c"),ctx=c.getContext("2d"),info=document.getElementById("info"),pad=document.getElementById("pad");
let ex=null,log=[],stamp=0;
try{log=JSON.parse(sessionStorage.wflog||"[]")}catch(e){}
function draw(){const p=ex.wf_paint();ctx.putImageData(new ImageData(new Uint8ClampedArray(ex.memory.buffer,p,1080*680*4).slice(),1080,680),0,0)}
function press(b,rec=true){ex.wf_key(b);if(rec){log.push(b);try{sessionStorage.wflog=JSON.stringify(log)}catch(e){}}draw()}
async function load(){
 const get=async u=>new Uint8Array(await (await fetch(u+"?t="+stamp,{cache:"no-store"})).arrayBuffer());
 const [w,f,p,i,l]=await Promise.all(["/menu.wasm","/fonts.bin","/panel.bin","/icons64.bin","/logo.bin"].map(get));
 const inst=await WebAssembly.instantiate(w,{});ex=inst.instance.exports;
 const put=d=>{const q=ex.wf_alloc(d.length);new Uint8Array(ex.memory.buffer,q,d.length).set(d);return q};
 ex.wf_setup(put(f),put(p),put(i),put(l));
 for(const b of log)ex.wf_key(b);
 draw();
}
BTN.forEach(([n,b])=>{const e=document.createElement("button");e.textContent=n;e.onclick=()=>{press(b);c.focus()};pad.appendChild(e)});
const rs=document.createElement("button");rs.textContent="Restart";rs.onclick=()=>{log=[];sessionStorage.wflog="[]";load()};pad.appendChild(rs);
addEventListener("keydown",e=>{const k=e.key.length==1?e.key.toLowerCase():e.key;if(k in KEYS&&ex){e.preventDefault();press(KEYS[k])}});
c.onclick=e=>{const r=c.getBoundingClientRect();info.dataset.pos="x "+Math.round((e.clientX-r.left)/r.width*540)+", y "+Math.round((e.clientY-r.top)/r.height*340);fill()};
function fill(){info.innerHTML="Keys: arrows move, Enter = A, Esc = B, X, Y, Q / E = L / R (or use the buttons).<br>"+(info.dataset.pos?"Clicked: "+info.dataset.pos+" (menu units, 540 x 340)":"Click the menu to read a position.")}
fill();
setInterval(async()=>{try{const s=(await (await fetch("/stamp.json",{cache:"no-store"})).json()).t;if(s!=stamp){stamp=s;await load()}}catch(e){}},700);
</script>"""


class H(http.server.BaseHTTPRequestHandler):
    def send(self, code, ctype, body):
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        p = self.path.split("?")[0]
        if p in ("/", "/index.html"):
            self.send(200, "text/html; charset=utf-8", PAGE.encode())
        elif p == "/stamp.json":
            t = max((os.path.getmtime(f) for f in FILES.values() if os.path.exists(f)), default=0)
            self.send(200, "application/json", json.dumps({"t": t}).encode())
        elif p in FILES and os.path.exists(FILES[p]):
            self.send(200, "application/wasm" if p.endswith(".wasm") else "application/octet-stream", open(FILES[p], "rb").read())
        else:
            self.send(404, "text/plain", b"not found")

    def log_message(self, *a):
        pass


socketserver.TCPServer.allow_reuse_address = True
with socketserver.ThreadingTCPServer(("127.0.0.1", 0), H) as srv:
    print("http://127.0.0.1:%d/" % srv.server_address[1], flush=True)
    srv.serve_forever()
