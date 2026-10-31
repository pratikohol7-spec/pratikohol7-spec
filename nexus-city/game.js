// Nexus City: Anomaly Hunters — an original urban-supernatural open-world game for mobile & desktop.
import * as THREE from './lib/three.module.min.js';

// =====================================================================
// Utilities
// =====================================================================
const $ = (id) => document.getElementById(id);
const clamp = (v, a, b) => (v < a ? a : v > b ? b : v);
const lerp = (a, b, t) => a + (b - a) * t;
const damp = (a, b, k, dt) => lerp(a, b, 1 - Math.exp(-k * dt));
const smooth = (a, b, x) => { const t = clamp((x - a) / (b - a), 0, 1); return t * t * (3 - 2 * t); };
function mulberry32(a) { return function () { a |= 0; a = (a + 0x6d2b79f5) | 0; let t = Math.imul(a ^ (a >>> 15), 1 | a); t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t; return ((t ^ (t >>> 14)) >>> 0) / 4294967296; }; }
const rng = mulberry32(20261031);
const rr = (a, b) => a + rng() * (b - a);
const ri = (a, b) => Math.floor(rr(a, b + 1));
const pick = (arr) => arr[Math.floor(rng() * arr.length)];
const angDiff = (a, b) => { let d = b - a; while (d > Math.PI) d -= Math.PI * 2; while (d < -Math.PI) d += Math.PI * 2; return d; };
const V3 = THREE.Vector3;
const tmpV = new V3(), tmpV2 = new V3();
const isTouch = matchMedia('(pointer: coarse)').matches || 'ontouchstart' in window;

const SAVE_KEY = 'nexus-city-save-v1';
const SET_KEY = 'nexus-city-settings-v1';
function loadJSON(k) { try { const s = localStorage.getItem(k); return s ? JSON.parse(s) : null; } catch (e) { return null; } }
function saveJSON(k, v) { try { localStorage.setItem(k, JSON.stringify(v)); } catch (e) { /* storage unavailable */ } }
function removeKey(k) { try { localStorage.removeItem(k); } catch (e) { /* ignore */ } }
const settings = Object.assign({ quality: isTouch ? 'medium' : 'high', sens: 1, sound: true }, loadJSON(SET_KEY) || {});

// =====================================================================
// Renderer / scene
// =====================================================================
const renderer = new THREE.WebGLRenderer({ antialias: !isTouch, powerPreference: 'high-performance' });
$('game').appendChild(renderer.domElement);
const scene = new THREE.Scene();
const camera = new THREE.PerspectiveCamera(62, innerWidth / innerHeight, 0.2, 1200);
scene.fog = new THREE.Fog(0x2a1640, 80, 420);

function applyQuality() {
  const q = settings.quality;
  const pr = q === 'low' ? 0.7 : q === 'medium' ? 1 : Math.min(devicePixelRatio, 2);
  renderer.setPixelRatio(Math.min(pr, devicePixelRatio || 1));
  renderer.setSize(innerWidth, innerHeight);
  const far = q === 'low' ? 240 : q === 'medium' ? 360 : 500;
  scene.fog.near = far * 0.2; scene.fog.far = far;
  camera.far = Math.max(far + 50, 950); camera.updateProjectionMatrix();
}
function onResize() {
  camera.aspect = innerWidth / innerHeight; camera.updateProjectionMatrix();
  renderer.setSize(innerWidth, innerHeight);
  $('rotateHint').classList.toggle('hidden', !(isTouch && innerHeight > innerWidth));
}
addEventListener('resize', onResize);
applyQuality();

const hemi = new THREE.HemisphereLight(0xbfd8ff, 0x302838, 1.0); scene.add(hemi);
const sun = new THREE.DirectionalLight(0xffffff, 1.8); scene.add(sun); scene.add(sun.target);
const heroLight = new THREE.PointLight(0xffe6d0, 30, 22, 1.4); scene.add(heroLight);

// toon gradient
const gradTex = new THREE.DataTexture(new Uint8Array([95, 95, 95, 255, 175, 175, 175, 255, 255, 255, 255, 255]), 3, 1, THREE.RGBAFormat);
gradTex.minFilter = gradTex.magFilter = THREE.NearestFilter; gradTex.needsUpdate = true;
const toon = (c, e) => new THREE.MeshToonMaterial({ color: c, gradientMap: gradTex, emissive: e || 0x000000 });
const basic = (c, extra) => new THREE.MeshBasicMaterial(Object.assign({ color: c }, extra || {}));
const outlineMat = new THREE.MeshBasicMaterial({ color: 0x0a0612, side: THREE.BackSide });

// geometry helpers ---------------------------------------------------
function prep(geo, matrix, color) {
  const g = geo.index ? geo.toNonIndexed() : geo.clone();
  if (matrix) g.applyMatrix4(matrix);
  if (color !== undefined) {
    const c = new THREE.Color(color); const n = g.attributes.position.count; const a = new Float32Array(n * 3);
    for (let i = 0; i < n; i++) { a[i * 3] = c.r; a[i * 3 + 1] = c.g; a[i * 3 + 2] = c.b; }
    g.setAttribute('color', new THREE.BufferAttribute(a, 3));
  }
  return g;
}
function mergeGeos(geos) {
  const out = new THREE.BufferGeometry();
  for (const n of Object.keys(geos[0].attributes)) {
    let len = 0; const item = geos[0].attributes[n].itemSize;
    for (const g of geos) len += g.attributes[n].array.length;
    const arr = new Float32Array(len); let o = 0;
    for (const g of geos) { arr.set(g.attributes[n].array, o); o += g.attributes[n].array.length; }
    out.setAttribute(n, new THREE.BufferAttribute(arr, item));
  }
  out.computeBoundingSphere();
  return out;
}
const _q = new THREE.Quaternion(), _e = new THREE.Euler(), _s = new V3(), _p = new V3();
function M(x, y, z, ry = 0, sx = 1, sy = 1, sz = 1, rx = 0, rz = 0) {
  return new THREE.Matrix4().compose(_p.set(x, y, z), _q.setFromEuler(_e.set(rx, ry, rz)), _s.set(sx, sy, sz));
}

// =====================================================================
// Audio (synthesised, no assets)
// =====================================================================
let actx = null, noiseBuf = null;
function initAudio() {
  if (!actx) {
    try {
      actx = new (window.AudioContext || window.webkitAudioContext)();
      noiseBuf = actx.createBuffer(1, actx.sampleRate * 0.5, actx.sampleRate);
      const d = noiseBuf.getChannelData(0); for (let i = 0; i < d.length; i++) d[i] = Math.random() * 2 - 1;
    } catch (e) { actx = null; }
  }
  if (actx && actx.state === 'suspended') actx.resume();
}
const SFX = {
  hit: ['square', 260, 90, 0.08, 0.06], slash: ['sawtooth', 600, 180, 0.09, 0.04], jump: ['sine', 300, 620, 0.12, 0.06],
  pickup: ['sine', 700, 1400, 0.18, 0.08], skill: ['sawtooth', 160, 700, 0.3, 0.07], ult: ['sawtooth', 90, 900, 0.7, 0.09],
  hurt: ['square', 180, 60, 0.18, 0.08], bolt: ['triangle', 1200, 300, 0.1, 0.05], swap: ['sine', 500, 900, 0.15, 0.06],
  ui: ['sine', 660, 660, 0.06, 0.05], dash: ['triangle', 400, 120, 0.15, 0.05], horn: ['square', 330, 330, 0.25, 0.05],
  win: ['sine', 523, 1046, 0.6, 0.08], boom: ['noise', 0, 0, 0.35, 0.12], crash: ['noise', 0, 0, 0.25, 0.1],
};
function sfx(name) {
  if (!settings.sound || !actx) return;
  const p = SFX[name]; if (!p) return;
  const t = actx.currentTime; const g = actx.createGain();
  g.gain.setValueAtTime(p[4], t); g.gain.exponentialRampToValueAtTime(0.0001, t + p[3]); g.connect(actx.destination);
  if (p[0] === 'noise') {
    const s = actx.createBufferSource(); s.buffer = noiseBuf; const f = actx.createBiquadFilter(); f.type = 'lowpass'; f.frequency.value = 900;
    s.connect(f); f.connect(g); s.start(t); s.stop(t + p[3]);
  } else {
    const o = actx.createOscillator(); o.type = p[0]; o.frequency.setValueAtTime(p[1], t);
    o.frequency.exponentialRampToValueAtTime(Math.max(20, p[2]), t + p[3]); o.connect(g); o.start(t); o.stop(t + p[3]);
  }
}

// =====================================================================
// World layout constants
// =====================================================================
const GRID = 9, ROAD = 16, CELL = 72, BLOCK = CELL - ROAD, HALF = (GRID * CELL) / 2, SW = 4, SWH = 0.15;
const WORLD_LIM = HALF + 6;
const roadLine = (k) => -HALF + k * CELL;
const blockMin = (i) => roadLine(i) + ROAD / 2;
const blockMax = (i) => roadLine(i + 1) - ROAD / 2;
const blockCenter = (i) => roadLine(i) + CELL / 2;
const SPIRE_POS = new V3(0, 0, 0);
const IRIS_POS = new V3(3, SWH, 97.6);
const SPAWN_POS = new V3(-8, SWH, 98);
const RIFT_BLOCKS = [[3, 6], [1, 1], [7, 1], [7, 6], [4, 8]];
const SHOP_BLOCK = [4, 5];
const SPIRE_BLOCK = [4, 4];

// collision grid --------------------------------------------------------
const colliders = []; const colGrid = new Map(); const EMPTY = [];
const cellOf = (v) => Math.floor((v + HALF) / CELL);
function addCollider(minX, maxX, minZ, maxZ, top, kind) {
  const c = { minX, maxX, minZ, maxZ, top, kind };
  colliders.push(c);
  for (let i = cellOf(minX - 3); i <= cellOf(maxX + 3); i++)
    for (let j = cellOf(minZ - 3); j <= cellOf(maxZ + 3); j++) {
      const k = i * 1000 + j; if (!colGrid.has(k)) colGrid.set(k, []); colGrid.get(k).push(c);
    }
  return c;
}
const nearby = (x, z) => colGrid.get(cellOf(x) * 1000 + cellOf(z)) || EMPTY;
function sidewalkAt(x, z) {
  const lx = (((x + HALF) % CELL) + CELL) % CELL, lz = (((z + HALF) % CELL) + CELL) % CELL;
  return Math.abs(x) < HALF && Math.abs(z) < HALF && lx > ROAD / 2 && lx < CELL - ROAD / 2 && lz > ROAD / 2 && lz < CELL - ROAD / 2;
}
function groundAt(x, z, y) {
  let g = sidewalkAt(x, z) ? SWH : 0;
  for (const c of nearby(x, z)) if (x > c.minX && x < c.maxX && z > c.minZ && z < c.maxZ && c.top <= y + 0.6 && c.top > g) g = c.top;
  return g;
}
function insideBuilding(x, y, z) {
  for (const c of nearby(x, z)) if (x > c.minX && x < c.maxX && z > c.minZ && z < c.maxZ && y < c.top) return true;
  return false;
}
function collide(p, r, footY) {
  let hit = null;
  for (const c of nearby(p.x, p.z)) {
    if (footY >= c.top - 0.5) continue;
    const qx = clamp(p.x, c.minX, c.maxX), qz = clamp(p.z, c.minZ, c.maxZ);
    let dx = p.x - qx, dz = p.z - qz; const d2 = dx * dx + dz * dz;
    if (d2 < r * r) {
      if (d2 > 1e-6) { const d = Math.sqrt(d2); dx /= d; dz /= d; p.x = qx + dx * r; p.z = qz + dz * r; hit = { x: dx, z: dz, c }; }
      else {
        const l = p.x - c.minX, rt = c.maxX - p.x, b = p.z - c.minZ, t = c.maxZ - p.z, m = Math.min(l, rt, b, t);
        if (m === l) { p.x = c.minX - r; hit = { x: -1, z: 0, c }; } else if (m === rt) { p.x = c.maxX + r; hit = { x: 1, z: 0, c }; }
        else if (m === b) { p.z = c.minZ - r; hit = { x: 0, z: -1, c }; } else { p.z = c.maxZ + r; hit = { x: 0, z: 1, c }; }
      }
    }
  }
  p.x = clamp(p.x, -WORLD_LIM, WORLD_LIM); p.z = clamp(p.z, -WORLD_LIM, WORLD_LIM);
  return hit;
}

// =====================================================================
// Textures
// =====================================================================
function canvasTex(w, h, draw, opts = {}) {
  const c = document.createElement('canvas'); c.width = w; c.height = h; draw(c.getContext('2d'), w, h);
  const t = new THREE.CanvasTexture(c); t.colorSpace = THREE.SRGBColorSpace;
  if (opts.repeat) { t.wrapS = t.wrapT = THREE.RepeatWrapping; }
  t.anisotropy = 4; return t;
}
const FACADES = [
  { wall: '#3b3f58', frame: '#262839', glass: '#1d2a44', lit: ['#ffd98a', '#ffe9b8', '#9fe8ff'] },
  { wall: '#5a4a5e', frame: '#3a2d40', glass: '#24243a', lit: ['#ffb36b', '#ffd98a', '#ff8fd0'] },
  { wall: '#2b3346', frame: '#1b2030', glass: '#2a4a6a', lit: ['#8fe0ff', '#c8f3ff', '#ffffff'] },
  { wall: '#6a6266', frame: '#4a4448', glass: '#2a2c3c', lit: ['#ffe2a0', '#fff1cc', '#ffc070'] },
];
function makeFacade(f, seed) {
  const r = mulberry32(seed);
  const COLS = 8, ROWS = 8, S = 256, cw = S / COLS, ch = S / ROWS;
  const lit = [];
  const map = canvasTex(S, S, (g) => {
    g.fillStyle = f.wall; g.fillRect(0, 0, S, S);
    for (let y = 0; y < ROWS; y++) {
      g.fillStyle = f.frame; g.fillRect(0, y * ch + ch - 5, S, 3);
      for (let x = 0; x < COLS; x++) {
        const on = r() < 0.42; lit.push(on ? f.lit[Math.floor(r() * f.lit.length)] : null);
        g.fillStyle = f.frame; g.fillRect(x * cw + 4, y * ch + 5, cw - 8, ch - 12);
        g.fillStyle = on ? lit[lit.length - 1] : f.glass; g.globalAlpha = on ? 0.55 : 1;
        g.fillRect(x * cw + 6, y * ch + 7, cw - 12, ch - 16); g.globalAlpha = 1;
      }
    }
    g.fillStyle = f.wall; g.fillRect(0, S - 3, 3, 3);
  }, { repeat: true });
  const emis = canvasTex(S, S, (g) => {
    g.fillStyle = '#000'; g.fillRect(0, 0, S, S); let i = 0;
    for (let y = 0; y < ROWS; y++) for (let x = 0; x < COLS; x++) { const l = lit[i++]; if (l) { g.fillStyle = l; g.fillRect(x * cw + 6, y * ch + 7, cw - 12, ch - 16); } }
  }, { repeat: true });
  return new THREE.MeshLambertMaterial({ map, emissiveMap: emis, emissive: 0xffffff, emissiveIntensity: 1 });
}
const facadeMats = FACADES.map((f, i) => makeFacade(f, 99 + i * 31));

const SIGNS = [
  ['NEXUS', '#ff4fd8'], ['ECHO MART', '#45e6ff'], ['NIGHT OWL', '#ffd36b'], ['VOLT CAFÉ', '#7dff9e'],
  ['ARCADE ∞', '#b47bff'], ['RAMEN 24H', '#ff6a4a'], ['KITSUNE', '#ff4f7a'], ['HOTEL LUMEN', '#9fe8ff'],
  ['NOCTURNE CURIOS', '#ffd36b'], ['KARAOKE', '#ff8fd0'], ['PHASE GYM', '#45e6ff'], ['MOCHI BAR', '#ffb3e6'],
];
const SIGN_ROWS = SIGNS.length;
const signTex = canvasTex(512, 64 * SIGN_ROWS, (g) => {
  g.fillStyle = '#05030a'; g.fillRect(0, 0, 512, 64 * SIGN_ROWS);
  g.textAlign = 'center'; g.textBaseline = 'middle';
  SIGNS.forEach(([txt, col], i) => {
    const y = i * 64 + 32; g.strokeStyle = col; g.lineWidth = 3; g.strokeRect(6, i * 64 + 6, 500, 52);
    g.font = 'bold 40px system-ui, sans-serif'; g.shadowColor = col; g.shadowBlur = 14; g.fillStyle = col; g.fillText(txt, 256, y + 2);
    g.shadowBlur = 0; g.fillStyle = '#fff'; g.globalAlpha = 0.55; g.fillText(txt, 256, y + 2); g.globalAlpha = 1;
  });
});
const glowTex = canvasTex(64, 64, (g) => {
  const gr = g.createRadialGradient(32, 32, 0, 32, 32, 32); gr.addColorStop(0, 'rgba(255,255,255,1)'); gr.addColorStop(0.3, 'rgba(255,255,255,.6)'); gr.addColorStop(1, 'rgba(255,255,255,0)');
  g.fillStyle = gr; g.fillRect(0, 0, 64, 64);
});

// =====================================================================
// Sky
// =====================================================================
const skyUniforms = { top: { value: new THREE.Color(0x0a0820) }, bottom: { value: new THREE.Color(0x3a1d55) } };
const sky = new THREE.Mesh(new THREE.SphereGeometry(800, 24, 16), new THREE.ShaderMaterial({
  uniforms: skyUniforms, side: THREE.BackSide, depthWrite: false, fog: false,
  vertexShader: 'varying vec3 vP; void main(){ vP = position; gl_Position = projectionMatrix * modelViewMatrix * vec4(position,1.0); }',
  fragmentShader: 'uniform vec3 top; uniform vec3 bottom; varying vec3 vP; void main(){ float h = normalize(vP).y; float t = smoothstep(-0.05, 0.55, h); gl_FragColor = vec4(mix(bottom, top, t), 1.0); }',
}));
sky.renderOrder = -10; scene.add(sky);
const starGeo = new THREE.BufferGeometry(); { const a = []; for (let i = 0; i < 700; i++) { const th = rr(0, Math.PI * 2), ph = rr(0.08, 1.4); a.push(Math.cos(th) * Math.cos(ph) * 700, Math.sin(ph) * 700, Math.sin(th) * Math.cos(ph) * 700); } starGeo.setAttribute('position', new THREE.Float32BufferAttribute(a, 3)); }
const starMat = new THREE.PointsMaterial({ color: 0xffffff, size: 2.2, sizeAttenuation: false, transparent: true, fog: false, depthWrite: false });
const stars = new THREE.Points(starGeo, starMat); scene.add(stars);
const moon = new THREE.Sprite(new THREE.SpriteMaterial({ map: glowTex, color: 0xdfe6ff, fog: false, depthWrite: false, transparent: true }));
moon.scale.set(90, 90, 1); scene.add(moon);

// =====================================================================
// City generation
// =====================================================================
const buildings = []; // {x,z,w,d,h}
const lampPositions = []; const treeData = []; const parkBlocks = new Set();
const blockType = (i, j) => {
  if (i === SPIRE_BLOCK[0] && j === SPIRE_BLOCK[1]) return 'spire';
  if (i === SHOP_BLOCK[0] && j === SHOP_BLOCK[1]) return 'shop';
  if (RIFT_BLOCKS.some((b) => b[0] === i && b[1] === j)) return 'plaza';
  return parkBlocks.has(i * 100 + j) ? 'park' : 'city';
};
function buildCity() {
  for (let i = 0; i < GRID; i++) for (let j = 0; j < GRID; j++) if (rng() < 0.11) parkBlocks.add(i * 100 + j);
  const facadeGeos = FACADES.map(() => []);
  const ground = [], marks = [], roofs = [], signGeos = [];
  const addBuilding = (x0, x1, z0, z1, h, style) => {
    const w = x1 - x0, d = z1 - z0, x = (x0 + x1) / 2, z = (z0 + z1) / 2;
    const g = new THREE.BoxGeometry(w, h, d); const uv = g.attributes.uv;
    for (let f = 0; f < 6; f++) for (let v = 0; v < 4; v++) {
      const idx = f * 4 + v; let u = uv.getX(idx), vv = uv.getY(idx);
      if (f === 0 || f === 1) { u *= d / 16; vv *= h / 28; } else if (f === 2 || f === 3) { u = 0.002; vv = 0.002; } else { u *= w / 16; vv *= h / 28; }
      uv.setXY(idx, u, vv);
    }
    facadeGeos[style].push(prep(g, M(x, h / 2, z)));
    addCollider(x0, x1, z0, z1, h, 'building');
    buildings.push({ x, z, w, d, h, x0, x1, z0, z1 });
    // rooftop details
    if (rng() < 0.7) roofs.push(prep(new THREE.BoxGeometry(rr(1.5, 3), rr(0.8, 1.6), rr(1.5, 3)), M(x + rr(-w / 4, w / 4), h + 0.5, z + rr(-d / 4, d / 4)), 0x555a66));
    roofs.push(prep(new THREE.BoxGeometry(w + 0.4, 0.5, 0.4), M(x, h + 0.25, z0), 0x2a2d3a));
    roofs.push(prep(new THREE.BoxGeometry(w + 0.4, 0.5, 0.4), M(x, h + 0.25, z1), 0x2a2d3a));
    roofs.push(prep(new THREE.BoxGeometry(0.4, 0.5, d), M(x0, h + 0.25, z), 0x2a2d3a));
    roofs.push(prep(new THREE.BoxGeometry(0.4, 0.5, d), M(x1, h + 0.25, z), 0x2a2d3a));
    if (h > 60 && rng() < 0.6) roofs.push(prep(new THREE.CylinderGeometry(0.15, 0.25, 12, 6), M(x, h + 6, z), 0x8a8f9c));
    // neon sign
    if (rng() < 0.45 && h > 10) {
      const row = Math.floor(rng() * SIGN_ROWS); if (SIGNS[row][0] === 'NOCTURNE CURIOS') return;
      const sw = Math.min(10, Math.max(w, d) * 0.8), sh = sw / 8, side = Math.floor(rng() * 4), y = rr(6, Math.min(h - 2, 16));
      const pg = new THREE.PlaneGeometry(sw, sh); const puv = pg.attributes.uv;
      for (let k = 0; k < 4; k++) puv.setY(k, 1 - (row + 1 - puv.getY(k)) / SIGN_ROWS);
      const pos = [[x, z1 + 0.12, 0], [x, z0 - 0.12, Math.PI], [x1 + 0.12, z, Math.PI / 2], [x0 - 0.12, z, -Math.PI / 2]][side];
      signGeos.push(prep(pg, M(pos[0], y, pos[1], pos[2])));
    }
  };

  // asphalt
  const asphalt = new THREE.Mesh(new THREE.PlaneGeometry(GRID * CELL + ROAD + 40, GRID * CELL + ROAD + 40), new THREE.MeshLambertMaterial({ color: 0x23232b }));
  asphalt.rotation.x = -Math.PI / 2; scene.add(asphalt);
  const water = new THREE.Mesh(new THREE.PlaneGeometry(4000, 4000), new THREE.MeshLambertMaterial({ color: 0x0d2340 }));
  water.rotation.x = -Math.PI / 2; water.position.y = -1.2; scene.add(water);
  // seawall
  const L = GRID * CELL + ROAD + 40;
  for (const s of [-1, 1]) {
    ground.push(prep(new THREE.BoxGeometry(L, 1.6, 1), M(0, -0.4, s * L / 2), 0x3b3b46));
    ground.push(prep(new THREE.BoxGeometry(1, 1.6, L), M(s * L / 2, -0.4, 0), 0x3b3b46));
  }
  // road markings
  for (let k = 0; k <= GRID; k++) {
    const c = roadLine(k);
    for (let t = -HALF; t < HALF; t += 6) {
      const lt = (((t + HALF) % CELL) + CELL) % CELL; if (lt < ROAD / 2 + 1 || lt > CELL - ROAD / 2 - 1) continue;
      marks.push(prep(new THREE.PlaneGeometry(0.25, 2.6), M(c, 0.02, t + 1.5, 0, 1, 1, 1, -Math.PI / 2), 0xd8c98a));
      marks.push(prep(new THREE.PlaneGeometry(2.6, 0.25), M(t + 1.5, 0.02, c, 0, 1, 1, 1, -Math.PI / 2), 0xd8c98a));
    }
  }
  // crosswalk stripes near intersections
  for (let k = 0; k <= GRID; k++) for (let m = 0; m <= GRID; m++) {
    const cx = roadLine(k), cz = roadLine(m);
    for (let s = -3; s <= 3; s++) {
      if (m < GRID) marks.push(prep(new THREE.PlaneGeometry(0.7, 3), M(cx + s * 1.6, 0.021, cz + ROAD / 2 + 2, 0, 1, 1, 1, -Math.PI / 2), 0xcfcfd6));
      if (k < GRID) marks.push(prep(new THREE.PlaneGeometry(3, 0.7), M(cx + ROAD / 2 + 2, 0.021, cz + s * 1.6, 0, 1, 1, 1, -Math.PI / 2), 0xcfcfd6));
    }
  }

  for (let i = 0; i < GRID; i++) for (let j = 0; j < GRID; j++) {
    const x0 = blockMin(i), x1 = blockMax(i), z0 = blockMin(j), z1 = blockMax(j), cx = (x0 + x1) / 2, cz = (z0 + z1) / 2;
    const type = blockType(i, j);
    const green = type === 'park' || type === 'plaza';
    ground.push(prep(new THREE.BoxGeometry(BLOCK, SWH, BLOCK), M(cx, SWH / 2, cz), type === 'spire' ? 0x4a4458 : 0x5b5a63));
    if (green) ground.push(prep(new THREE.BoxGeometry(BLOCK - SW * 2, 0.05, BLOCK - SW * 2), M(cx, SWH + 0.025, cz), type === 'plaza' ? 0x3a5a46 : 0x2f6b3c));
    // lamps
    for (const t of [0.25, 0.75]) {
      lampPositions.push([lerp(x0, x1, t), z0 + 0.8], [lerp(x0, x1, t), z1 - 0.8], [x0 + 0.8, lerp(z0, z1, t)], [x1 - 0.8, lerp(z0, z1, t)]);
    }
    if (type === 'city') {
      const nx = ri(1, 2), nz = ri(1, 2); const ix0 = x0 + SW, iz0 = z0 + SW, lot = (BLOCK - SW * 2);
      const dist = Math.hypot(cx, cz) / HALF;
      for (let a = 0; a < nx; a++) for (let b = 0; b < nz; b++) {
        const lx0 = ix0 + (lot / nx) * a + 1, lx1 = ix0 + (lot / nx) * (a + 1) - 1, lz0 = iz0 + (lot / nz) * b + 1, lz1 = iz0 + (lot / nz) * (b + 1) - 1;
        const sh = rr(0, 2.5);
        let h = rr(10, 26) + Math.pow(Math.max(0, 1 - dist), 1.5) * rr(10, 90);
        if (rng() < 0.08) h += 40;
        addBuilding(lx0 + rr(0, sh), lx1 - rr(0, sh), lz0 + rr(0, sh), lz1 - rr(0, sh), Math.round(h), Math.floor(rng() * FACADES.length));
      }
      if (rng() < 0.4) for (let t = 0; t < 4; t++) treeData.push([lerp(x0 + 2, x1 - 2, rng()), pick([z0 + 2, z1 - 2]), rr(0.8, 1.1)]);
    } else if (type === 'shop') {
      addBuilding(x0 + SW + 1, cx - 3, z0 + SW + 1, z0 + 30, 34, 0);
      addBuilding(cx + 3, x1 - SW - 1, z0 + SW + 1, z0 + 30, 26, 2);
      addBuilding(x0 + SW + 1, cx - 11, z1 - 18, z1 - SW - 0.5, 14, 3);
      addBuilding(cx + 11, x1 - SW - 1, z1 - 18, z1 - SW - 0.5, 18, 1);
      // Nocturne Curios shop
      const sx0 = cx - 9, sx1 = cx + 9, sz0 = z1 - 16, sz1 = z1 - SW - 0.5;
      addBuilding(sx0, sx1, sz0, sz1, 7, 1);
      const row = 8; const pg = new THREE.PlaneGeometry(12, 1.5); const puv = pg.attributes.uv;
      for (let k = 0; k < 4; k++) puv.setY(k, 1 - (row + 1 - puv.getY(k)) / SIGN_ROWS);
      signGeos.push(prep(pg, M(cx, 5.3, sz1 + 0.12)));
      ground.push(prep(new THREE.BoxGeometry(18.6, 0.3, 2.5), M(cx, 4.2, sz1 + 1.2), 0x6b2c4a)); // awning
      ground.push(prep(new THREE.BoxGeometry(6, 3.2, 0.1), M(cx - 4, 1.8, sz1 + 0.06), 0xffc98a)); // window glow
    } else if (type === 'park' || type === 'plaza') {
      const n = type === 'park' ? 22 : 10;
      for (let t = 0; t < n; t++) {
        let tx = rr(x0 + 6, x1 - 6), tz = rr(z0 + 6, z1 - 6);
        if (type === 'plaza' && Math.hypot(tx - cx, tz - cz) < 14) continue;
        treeData.push([tx, tz, rr(0.9, 1.5)]);
      }
      if (type === 'park') ground.push(prep(new THREE.CylinderGeometry(5, 5.5, 0.6, 16), M(cx, SWH + 0.3, cz), 0x456c8a));
    }
  }

  // Spire (central landmark)
  const spire = new THREE.Group();
  const spireMat = new THREE.MeshLambertMaterial({ color: 0x2b2840, emissive: 0x150c2a });
  const sp = new THREE.Mesh(new THREE.CylinderGeometry(6, 10, 160, 8), spireMat); sp.position.y = 80; spire.add(sp);
  const ringMat = basic(0xb47bff, { transparent: true, opacity: 0.9 });
  const spireRings = [];
  for (let k = 0; k < 6; k++) {
    const r = new THREE.Mesh(new THREE.TorusGeometry(lerp(11, 7, k / 6), 0.35, 6, 32), ringMat);
    r.rotation.x = Math.PI / 2; r.position.y = 20 + k * 24; spire.add(r); spireRings.push(r);
  }
  const beacon = new THREE.Sprite(new THREE.SpriteMaterial({ map: glowTex, color: 0xc28bff, transparent: true, depthWrite: false, blending: THREE.AdditiveBlending }));
  beacon.scale.set(40, 40, 1); beacon.position.y = 166; spire.add(beacon);
  const tip = new THREE.Mesh(new THREE.ConeGeometry(3, 14, 8), spireMat); tip.position.y = 167; spire.add(tip);
  ground.push(prep(new THREE.CylinderGeometry(20, 22, 0.5, 32), M(0, SWH + 0.25, 0), 0x3a3550));
  scene.add(spire);
  addCollider(-8.5, 8.5, -8.5, 8.5, 160, 'spire');
  world.spireRings = spireRings; world.beacon = beacon;

  // meshes
  facadeGeos.forEach((list, k) => { if (list.length) scene.add(new THREE.Mesh(mergeGeos(list), facadeMats[k])); });
  const vc = new THREE.MeshLambertMaterial({ vertexColors: true });
  scene.add(new THREE.Mesh(mergeGeos(ground), vc));
  scene.add(new THREE.Mesh(mergeGeos(roofs), vc));
  const markMesh = new THREE.Mesh(mergeGeos(marks), new THREE.MeshBasicMaterial({ vertexColors: true })); scene.add(markMesh);
  world.signMesh = new THREE.Mesh(mergeGeos(signGeos), new THREE.MeshBasicMaterial({ map: signTex })); scene.add(world.signMesh);

  // lamps (instanced)
  const poleGeo = new THREE.CylinderGeometry(0.08, 0.12, 5, 6); poleGeo.translate(0, 2.5, 0);
  const poles = new THREE.InstancedMesh(poleGeo, new THREE.MeshLambertMaterial({ color: 0x3a3a46 }), lampPositions.length);
  world.bulbMat = basic(0xffd59a);
  const bulbs = new THREE.InstancedMesh(new THREE.SphereGeometry(0.32, 8, 6), world.bulbMat, lampPositions.length);
  lampPositions.forEach(([x, z], i) => { poles.setMatrixAt(i, M(x, SWH, z)); bulbs.setMatrixAt(i, M(x, SWH + 5.1, z)); });
  scene.add(poles, bulbs);

  // trees (instanced)
  const trunk = new THREE.CylinderGeometry(0.2, 0.3, 2.4, 6); trunk.translate(0, 1.2, 0);
  const crown = new THREE.IcosahedronGeometry(1.8, 0); crown.translate(0, 3.6, 0);
  const tI = new THREE.InstancedMesh(trunk, new THREE.MeshLambertMaterial({ color: 0x4a3326 }), treeData.length);
  const cI = new THREE.InstancedMesh(crown, new THREE.MeshLambertMaterial({ color: 0xffffff }), treeData.length);
  const leafCols = [0x2f7a4a, 0x3d8f55, 0xc85a8a, 0x2a6a5a];
  treeData.forEach(([x, z, s], i) => { tI.setMatrixAt(i, M(x, SWH, z, rng() * 6, s, s, s)); cI.setMatrixAt(i, M(x, SWH, z, rng() * 6, s, s, s)); cI.setColorAt(i, new THREE.Color(pick(leafCols))); });
  scene.add(tI, cI);
}
const world = {};
buildCity();

// =====================================================================
// FX: particles, rings, beams, generic effects
// =====================================================================
const PMAX = 700;
const pGeo = new THREE.BufferGeometry();
const pPos = new Float32Array(PMAX * 3).fill(-9999), pCol = new Float32Array(PMAX * 3);
pGeo.setAttribute('position', new THREE.BufferAttribute(pPos, 3)); pGeo.setAttribute('color', new THREE.BufferAttribute(pCol, 3));
const pts = new THREE.Points(pGeo, new THREE.PointsMaterial({ size: 0.7, map: glowTex, vertexColors: true, transparent: true, depthWrite: false, blending: THREE.AdditiveBlending }));
pts.frustumCulled = false; scene.add(pts);
const pData = Array.from({ length: PMAX }, () => ({ life: 0 })); let pHead = 0;
function burst(pos, color, n = 16, speed = 6, life = 0.6, up = 2, grav = 6) {
  const c = new THREE.Color(color);
  for (let i = 0; i < n; i++) {
    const p = pData[pHead]; pHead = (pHead + 1) % PMAX;
    const th = Math.random() * Math.PI * 2, ph = Math.random() * Math.PI - Math.PI / 2, s = speed * (0.4 + Math.random() * 0.6);
    p.x = pos.x; p.y = pos.y; p.z = pos.z; p.vx = Math.cos(th) * Math.cos(ph) * s; p.vy = Math.sin(ph) * s + up; p.vz = Math.sin(th) * Math.cos(ph) * s;
    p.life = p.max = life * (0.6 + Math.random() * 0.6); p.r = c.r; p.g = c.g; p.b = c.b; p.grav = grav;
  }
}
function updateParticles(dt) {
  for (let i = 0; i < PMAX; i++) {
    const p = pData[i];
    if (p.life <= 0) { if (pPos[i * 3 + 1] !== -9999) pPos[i * 3 + 1] = -9999; continue; }
    p.life -= dt; p.vy -= p.grav * dt; p.x += p.vx * dt; p.y += p.vy * dt; p.z += p.vz * dt;
    const k = Math.max(0, p.life / p.max);
    pPos[i * 3] = p.x; pPos[i * 3 + 1] = p.life > 0 ? p.y : -9999; pPos[i * 3 + 2] = p.z;
    pCol[i * 3] = p.r * k; pCol[i * 3 + 1] = p.g * k; pCol[i * 3 + 2] = p.b * k;
  }
  pGeo.attributes.position.needsUpdate = true; pGeo.attributes.color.needsUpdate = true;
}

const effects = [];
const ringGeo = new THREE.RingGeometry(0.85, 1, 40); ringGeo.rotateX(-Math.PI / 2);
function ring(pos, color, radius, dur = 0.45, opts = {}) {
  const m = new THREE.Mesh(ringGeo, basic(color, { transparent: true, opacity: 0.9, blending: THREE.AdditiveBlending, depthWrite: false, side: THREE.DoubleSide }));
  m.position.copy(pos); m.position.y += 0.15; scene.add(m);
  let t = 0;
  effects.push((dt) => {
    t += dt; const k = t / dur;
    const s = opts.static ? radius : lerp(radius * 0.2, radius, 1 - Math.pow(1 - k, 3));
    m.scale.set(s, 1, s); m.material.opacity = opts.static ? 0.35 + 0.4 * Math.abs(Math.sin(t * 12)) : 0.9 * (1 - k);
    if (opts.follow) { m.position.x = opts.follow.x; m.position.z = opts.follow.z; }
    if (k >= 1) { scene.remove(m); m.material.dispose(); return false; }
    return true;
  });
  return m;
}
const beamGeo = new THREE.BoxGeometry(0.18, 0.18, 1); beamGeo.translate(0, 0, 0.5);
function beam(a, b, color, dur = 0.25, width = 1) {
  const m = new THREE.Mesh(beamGeo, basic(color, { transparent: true, blending: THREE.AdditiveBlending, depthWrite: false }));
  m.position.copy(a); m.lookAt(b); m.scale.set(width, width, a.distanceTo(b)); scene.add(m);
  let t = 0;
  effects.push((dt) => { t += dt; m.material.opacity = 1 - t / dur; m.scale.x = m.scale.y = width * (1 + Math.random()); if (t >= dur) { scene.remove(m); m.material.dispose(); return false; } return true; });
}
function zigzag(a, b, color) {
  let prev = a.clone(); const n = 5;
  for (let i = 1; i <= n; i++) {
    const p = a.clone().lerp(b, i / n); if (i < n) p.add(tmpV.set(rr(-0.8, 0.8), rr(-0.8, 0.8), rr(-0.8, 0.8)));
    beam(prev, p, color, 0.22, 1.3); prev = p;
  }
}
let timers = [];
function later(t, fn) { timers.push({ t, fn }); }
function updateEffects(dt) {
  for (let i = effects.length - 1; i >= 0; i--) if (!effects[i](dt)) effects.splice(i, 1);
  for (let i = timers.length - 1; i >= 0; i--) { const tm = timers[i]; tm.t -= dt; if (tm.t <= 0) { timers.splice(i, 1); tm.fn(); } }
}

// =====================================================================
// Characters (the esper team)
// =====================================================================
const CHARS = [
  { key: 'rei', name: 'Rei', element: 'Pyro', color: 0xff5a3c, css: '#ff6a4a', hair: 0xd8352a, coat: 0x1d1b28, trim: 0xff5a3c, skin: 0xffe1cc, ranged: false, atkRate: 0.3, mult: 1.0, hairStyle: 'pony', weapon: 'blade' },
  { key: 'mika', name: 'Mika', element: 'Volt', color: 0x45e6ff, css: '#45e6ff', hair: 0xf7d66d, coat: 0xe9ecf6, trim: 0x2fb6e8, skin: 0xffe6d6, ranged: true, atkRate: 0.22, mult: 0.62, hairStyle: 'twin', weapon: 'gun' },
  { key: 'sora', name: 'Sora', element: 'Void', color: 0xa66bff, css: '#b47bff', hair: 0xcfc1ff, coat: 0x2f2448, trim: 0xa66bff, skin: 0xf6dccb, ranged: false, atkRate: 0.42, mult: 1.45, hairStyle: 'long', weapon: 'gauntlet' },
];
function addOutline(mesh, s = 1.07) {
  const o = new THREE.Mesh(mesh.geometry, outlineMat); o.scale.setScalar(s); mesh.add(o); return o;
}
function buildEsper(c) {
  const g = new THREE.Group(); const P = {};
  const skin = toon(c.skin), coat = toon(c.coat), hair = toon(c.hair), dark = toon(0x17151f), trim = toon(c.trim, c.trim & 0x333333);
  const glow = basic(c.color);
  const legGeo = new THREE.CylinderGeometry(0.12, 0.1, 0.86, 8); legGeo.translate(0, -0.43, 0);
  const shoeGeo = new THREE.BoxGeometry(0.2, 0.12, 0.32);
  for (const s of [-1, 1]) {
    const leg = new THREE.Mesh(legGeo, dark); leg.position.set(0.14 * s, 0.9, 0); g.add(leg); addOutline(leg);
    const shoe = new THREE.Mesh(shoeGeo, trim); shoe.position.set(0, -0.86, 0.05); leg.add(shoe);
    P[s < 0 ? 'legL' : 'legR'] = leg;
  }
  const torso = new THREE.Mesh(new THREE.CylinderGeometry(0.25, 0.21, 0.66, 10), coat); torso.position.y = 1.22; g.add(torso); addOutline(torso);
  const tail = new THREE.Mesh(new THREE.CylinderGeometry(0.23, 0.38, 0.55, 10, 1, true), coat); tail.material.side = THREE.DoubleSide; tail.position.y = 0.88; g.add(tail);
  const belt = new THREE.Mesh(new THREE.CylinderGeometry(0.235, 0.235, 0.07, 10), trim); belt.position.y = 0.98; g.add(belt);
  const collar = new THREE.Mesh(new THREE.TorusGeometry(0.17, 0.05, 6, 12), trim); collar.rotation.x = Math.PI / 2; collar.position.y = 1.55; g.add(collar);
  const armGeo = new THREE.CylinderGeometry(0.075, 0.065, 0.62, 8); armGeo.translate(0, -0.31, 0);
  const handGeo = c.weapon === 'gauntlet' ? new THREE.BoxGeometry(0.2, 0.22, 0.2) : new THREE.SphereGeometry(0.08, 8, 6);
  for (const s of [-1, 1]) {
    const arm = new THREE.Mesh(armGeo, coat); arm.position.set(0.33 * s, 1.5, 0); g.add(arm); addOutline(arm);
    const hand = new THREE.Mesh(handGeo, c.weapon === 'gauntlet' ? trim : skin); hand.position.y = -0.64; arm.add(hand);
    P[s < 0 ? 'armL' : 'armR'] = arm; P[s < 0 ? 'handL' : 'handR'] = hand;
  }
  const head = new THREE.Mesh(new THREE.SphereGeometry(0.24, 14, 12), skin); head.position.y = 1.8; g.add(head); addOutline(head, 1.05);
  const cap = new THREE.Mesh(new THREE.SphereGeometry(0.27, 14, 10, 0, Math.PI * 2, 0, Math.PI * 0.55), hair); cap.position.set(0, 0.04, -0.02); head.add(cap); addOutline(cap, 1.04);
  const back = new THREE.Mesh(new THREE.SphereGeometry(0.26, 12, 10), hair); back.scale.set(1, 1, 0.7); back.position.set(0, 0.0, -0.1); head.add(back);
  for (let k = -2; k <= 2; k++) { const b = new THREE.Mesh(new THREE.ConeGeometry(0.06, 0.2, 5), hair); b.position.set(k * 0.075, 0.08, 0.21); b.rotation.x = Math.PI + 0.35; b.rotation.z = k * 0.12; head.add(b); }
  if (c.hairStyle === 'pony') { const p = new THREE.Mesh(new THREE.ConeGeometry(0.11, 0.7, 7), hair); p.position.set(0, 0.0, -0.32); p.rotation.x = -2.5; head.add(p); P.pony = p; }
  if (c.hairStyle === 'twin') for (const s of [-1, 1]) { const p = new THREE.Mesh(new THREE.ConeGeometry(0.1, 0.62, 7), hair); p.position.set(0.28 * s, -0.12, -0.08); p.rotation.set(Math.PI, 0, -0.35 * s); head.add(p); }
  if (c.hairStyle === 'long') { const p = new THREE.Mesh(new THREE.BoxGeometry(0.46, 0.7, 0.12), hair); p.position.set(0, -0.3, -0.18); head.add(p); }
  for (const s of [-1, 1]) { const e = new THREE.Mesh(new THREE.BoxGeometry(0.055, 0.08, 0.02), glow); e.position.set(0.085 * s, -0.01, 0.225); head.add(e); }
  P.head = head;
  if (c.weapon === 'blade') {
    const blade = new THREE.Mesh(new THREE.BoxGeometry(0.05, 0.05, 1.15), basic(0xffb08a)); blade.position.set(0, -0.08, 0.55); P.handR.add(blade); P.weapon = blade;
  } else if (c.weapon === 'gun') {
    const gun = new THREE.Mesh(new THREE.BoxGeometry(0.1, 0.14, 0.34), toon(0x2a2f3a)); gun.position.set(0, -0.02, 0.12); P.handR.add(gun);
    const tipm = new THREE.Mesh(new THREE.BoxGeometry(0.06, 0.06, 0.06), glow); tipm.position.z = 0.19; gun.add(tipm);
  }
  const orb = new THREE.Mesh(new THREE.OctahedronGeometry(0.11, 0), glow); orb.position.set(-0.45, 1.95, -0.3); g.add(orb); P.orb = orb;
  const shadow = new THREE.Mesh(new THREE.CircleGeometry(0.45, 16), basic(0x000000, { transparent: true, opacity: 0.35, depthWrite: false })); shadow.rotation.x = -Math.PI / 2; shadow.position.y = 0.03;
  g.userData.P = P; g.userData.shadow = shadow;
  return g;
}

// =====================================================================
// Player state
// =====================================================================
const player = {
  pos: SPAWN_POS.clone(), vel: new V3(), yaw: Math.PI, grounded: true, jumps: 0, gliding: false, climbing: false,
  dashT: 0, dashCd: 0, invuln: 0, attackT: 0, combo: 0, comboT: 0, swingT: 0, lunge: new V3(), lungeT: 0, pendingHit: null,
  skillCd: [0, 0, 0], energy: [0, 0, 0], hp: [100, 100, 100], maxHp: 100, active: 0, swapCd: 0, mode: 'foot', car: null,
  lastHurt: -99, animT: 0, walkPhase: 0, ultCast: 0,
};
const models = CHARS.map((c) => { const m = buildEsper(c); m.visible = false; scene.add(m); return m; });
const playerShadow = models[0].userData.shadow; scene.add(playerShadow);
models[0].visible = true;

const state = {
  mode: 'title', paused: false, time: 0, timeScale: 1, slowT: 0, dayTime: 0.833, credits: 0, level: 1,
  shardsGot: new Set(), riftsClosed: new Set(), questIdx: 0, metIris: false, bossDefeated: false, bossActive: false,
  inCombatT: 0, saveT: 0, lastCamInput: -99, questTimer: 0,
};
const atkPower = () => 10 * (1 + 0.18 * (state.level - 1));

// =====================================================================
// Vehicles
// =====================================================================
const carGeoCache = new Map();
function carGeo(color) {
  if (carGeoCache.has(color)) return carGeoCache.get(color);
  const parts = [
    prep(new THREE.BoxGeometry(2, 0.62, 4.3), M(0, 0.68, 0), color),
    prep(new THREE.BoxGeometry(1.72, 0.58, 2.2), M(0, 1.26, -0.25), 0x151b28),
    prep(new THREE.BoxGeometry(1.76, 0.08, 2.0), M(0, 1.58, -0.25), color),
    prep(new THREE.BoxGeometry(0.5, 0.18, 0.06), M(-0.62, 0.78, 2.16), 0xfff1c4),
    prep(new THREE.BoxGeometry(0.5, 0.18, 0.06), M(0.62, 0.78, 2.16), 0xfff1c4),
    prep(new THREE.BoxGeometry(0.5, 0.15, 0.06), M(-0.62, 0.8, -2.16), 0xff2a40),
    prep(new THREE.BoxGeometry(0.5, 0.15, 0.06), M(0.62, 0.8, -2.16), 0xff2a40),
    prep(new THREE.BoxGeometry(2.02, 0.08, 4.32), M(0, 0.5, 0), 0x111118),
  ];
  for (const sx of [-1, 1]) for (const sz of [-1, 1]) parts.push(prep(new THREE.CylinderGeometry(0.38, 0.38, 0.3, 10), M(0.92 * sx, 0.38, 1.38 * sz, 0, 1, 1, 1, 0, Math.PI / 2), 0x111116));
  const g = mergeGeos(parts); carGeoCache.set(color, g); return g;
}
const carMat = new THREE.MeshLambertMaterial({ vertexColors: true });
const CAR_COLORS = [0xd8d8e0, 0x2a2a33, 0xc0283a, 0x2b6fd6, 0xf0b429, 0x1fa37a, 0x8a4fd8, 0xff6aa8];
const cars = [];
function makeCar(color, x, z, yaw, traffic) {
  const mesh = new THREE.Mesh(carGeo(color), carMat); mesh.position.set(x, 0, z); mesh.rotation.y = yaw; scene.add(mesh);
  const car = { mesh, pos: new V3(x, 0, z), yaw, speed: 0, traffic, color, stopT: 0 };
  cars.push(car); return car;
}
// traffic: drive on the right lane along the road grid
const DIRS = [[1, 0], [-1, 0], [0, 1], [0, -1]];
function initTraffic(n) {
  for (let k = 0; k < n; k++) {
    const ni = ri(0, GRID), nj = ri(0, GRID);
    const opts = DIRS.filter(([dx, dz]) => ni + dx >= 0 && ni + dx <= GRID && nj + dz >= 0 && nj + dz <= GRID);
    const d = pick(opts);
    const car = makeCar(pick(CAR_COLORS), 0, 0, 0, true);
    Object.assign(car, { ni, nj, dx: d[0], dz: d[1], t: rr(0, CELL), cruise: rr(10, 15) });
    placeTraffic(car, true);
  }
}
function placeTraffic(car, snap) {
  const bx = roadLine(car.ni), bz = roadLine(car.nj);
  const x = bx + car.dx * car.t + -car.dz * 3.6, z = bz + car.dz * car.t + car.dx * 3.6;
  car.pos.set(x, 0, z);
  const yaw = Math.atan2(car.dx, car.dz);
  if (snap) { car.mesh.position.copy(car.pos); car.mesh.rotation.y = yaw; car.yaw = yaw; }
  return yaw;
}
function updateTraffic(dt) {
  for (const car of cars) {
    if (!car.traffic) continue;
    if (car.mesh.position.distanceToSquared(player.pos) > 300 * 300) { // far: cheap update
      car.t += car.cruise * dt;
    } else {
      // brake for obstacles ahead (player, player car, other cars)
      const fx = car.dx, fz = car.dz; let block = false;
      const check = (p) => { const rx = p.x - car.pos.x, rz = p.z - car.pos.z; const ahead = rx * fx + rz * fz, side = Math.abs(rx * -fz + rz * fx); return ahead > 0 && ahead < 8 && side < 2.6; };
      if (check(player.pos)) block = true;
      for (const o of cars) if (o !== car && check(o.pos)) { block = true; break; }
      car.speed = damp(car.speed, block ? 0 : car.cruise, block ? 8 : 1.5, dt);
      car.t += car.speed * dt;
    }
    if (car.t >= CELL) {
      car.t -= CELL; car.ni += car.dx; car.nj += car.dz;
      let opts = DIRS.filter(([dx, dz]) => !(dx === -car.dx && dz === -car.dz) && car.ni + dx >= 0 && car.ni + dx <= GRID && car.nj + dz >= 0 && car.nj + dz <= GRID);
      if (!opts.length) opts = [[-car.dx, -car.dz]];
      const straight = opts.find(([dx, dz]) => dx === car.dx && dz === car.dz);
      const d = straight && rng() < 0.55 ? straight : pick(opts);
      car.dx = d[0]; car.dz = d[1];
    }
    const yaw = placeTraffic(car, false);
    car.mesh.position.x = damp(car.mesh.position.x, car.pos.x, 6, dt);
    car.mesh.position.z = damp(car.mesh.position.z, car.pos.z, 6, dt);
    car.yaw = car.mesh.rotation.y = car.mesh.rotation.y + angDiff(car.mesh.rotation.y, yaw) * Math.min(1, dt * 6);
  }
}
function initParked() {
  const spots = [[-14, 101.4, Math.PI / 2], [16, 114.6, -Math.PI / 2]];
  for (let k = 0; k < 12; k++) {
    const vertical = rng() < 0.5, line = roadLine(ri(0, GRID)), along = roadLine(ri(0, GRID - 1)) + rr(14, CELL - 14), s = rng() < 0.5 ? -1 : 1;
    spots.push(vertical ? [line + 6.6 * s, along, s > 0 ? Math.PI : 0] : [along, line + 6.6 * s, s > 0 ? -Math.PI / 2 : Math.PI / 2]);
  }
  for (const [x, z, yaw] of spots) makeCar(pick(CAR_COLORS), x, z, yaw, false);
}
initTraffic(isTouch ? 22 : 30);
initParked();

// =====================================================================
// Pedestrians (instanced)
// =====================================================================
const PED_N = isTouch ? 36 : 56;
const pedBodyGeo = new THREE.CylinderGeometry(0.22, 0.26, 1.2, 8); pedBodyGeo.translate(0, 0.75, 0);
const pedHeadGeo = new THREE.SphereGeometry(0.2, 8, 6); pedHeadGeo.translate(0, 1.55, 0);
const pedBody = new THREE.InstancedMesh(pedBodyGeo, new THREE.MeshLambertMaterial(), PED_N);
const pedHead = new THREE.InstancedMesh(pedHeadGeo, new THREE.MeshLambertMaterial({ color: 0xffe0c8 }), PED_N);
pedBody.frustumCulled = pedHead.frustumCulled = false;
scene.add(pedBody, pedHead);
const peds = [];
for (let k = 0; k < PED_N; k++) {
  let bi, bj; do { bi = ri(0, GRID - 1); bj = ri(0, GRID - 1); } while (bi === 4 && bj === 4);
  peds.push({ bi, bj, s: rr(0, 4 * (BLOCK - 3)), speed: rr(1.1, 1.9) * (rng() < 0.5 ? -1 : 1), hop: 0, x: 0, z: 0, flee: 0 });
  pedBody.setColorAt(k, new THREE.Color(pick([0x3b4a6b, 0x6b3b4a, 0x2f2f3a, 0xd9d2c0, 0x4a6b3b, 0x7a5aa8, 0xc87a3a])));
}
const pedM = new THREE.Matrix4();
function updatePeds(dt) {
  const side = BLOCK - 3;
  for (let k = 0; k < PED_N; k++) {
    const p = peds[k];
    let sp = p.speed * (p.flee > 0 ? 3 : 1); p.flee -= dt;
    p.s = (((p.s + sp * dt) % (side * 4)) + side * 4) % (side * 4);
    const x0 = blockMin(p.bi) + 1.5, z0 = blockMin(p.bj) + 1.5; let x, z, yaw; const s = p.s;
    if (s < side) { x = x0 + s; z = z0; yaw = Math.PI / 2; } else if (s < side * 2) { x = x0 + side; z = z0 + s - side; yaw = 0; }
    else if (s < side * 3) { x = x0 + side - (s - side * 2); z = z0 + side; yaw = -Math.PI / 2; } else { x = x0; z = z0 + side - (s - side * 3); yaw = Math.PI; }
    if (p.speed < 0) yaw += Math.PI;
    p.x = x; p.z = z;
    const dx = x - player.pos.x, dz = z - player.pos.z;
    if (player.mode === 'drive' && dx * dx + dz * dz < 16 && Math.abs(player.car.speed) > 4 && p.hop <= 0) { p.hop = 0.6; p.flee = 3; }
    if (p.hop > 0) p.hop -= dt;
    const hy = p.hop > 0 ? Math.sin((p.hop / 0.6) * Math.PI) * 1.2 : Math.abs(Math.sin(state.time * 8 + k)) * 0.06;
    pedM.compose(_p.set(x, SWH + hy, z), _q.setFromEuler(_e.set(0, yaw, 0)), _s.set(1, 1, 1));
    pedBody.setMatrixAt(k, pedM); pedHead.setMatrixAt(k, pedM);
  }
  pedBody.instanceMatrix.needsUpdate = pedHead.instanceMatrix.needsUpdate = true;
}

// =====================================================================
// NPC: Iris (Nocturne Curios)
// =====================================================================
const iris = buildEsper({ color: 0x5dff9a, hair: 0x2b2b3a, coat: 0x5a2a48, trim: 0xffd36b, skin: 0xffe3cf, hairStyle: 'long', weapon: 'none' });
iris.position.copy(IRIS_POS); iris.rotation.y = 0; scene.add(iris); iris.add(iris.userData.shadow); iris.userData.shadow.position.y = 0.02;
const irisMarker = new THREE.Sprite(new THREE.SpriteMaterial({ map: canvasTex(64, 64, (g) => { g.fillStyle = '#ffd36b'; g.font = 'bold 54px sans-serif'; g.textAlign = 'center'; g.textBaseline = 'middle'; g.fillText('!', 32, 34); }), transparent: true, depthWrite: false }));
irisMarker.scale.set(0.9, 0.9, 1); irisMarker.position.y = 2.6; iris.add(irisMarker);

// =====================================================================
// Echo Shards (collectibles)
// =====================================================================
const shardPositions = [[-36, 1.3, 78], [36, 1.3, 128], [-14, 1.3, 108]];
{
  const roofCands = buildings.filter((b) => b.h < 70).sort(() => rng() - 0.5).slice(0, 18);
  for (const b of roofCands) shardPositions.push([b.x, b.h + 1.3, b.z]);
  for (let k = 0; k < 15; k++) {
    const vertical = rng() < 0.5, line = roadLine(ri(0, GRID)), along = roadLine(ri(0, GRID - 1)) + rr(12, CELL - 12);
    shardPositions.push(vertical ? [line + rr(-5, 5), 1.3, along] : [along, 1.3, line + rr(-5, 5)]);
  }
  shardPositions.push([0, 162, 0]); // top of the spire!
}
const shardMesh = new THREE.InstancedMesh(new THREE.OctahedronGeometry(0.45, 0), basic(0x6ff0ff), shardPositions.length);
shardMesh.frustumCulled = false; scene.add(shardMesh);
const shardGlow = new THREE.InstancedMesh(new THREE.PlaneGeometry(2.2, 2.2), new THREE.MeshBasicMaterial({ map: glowTex, color: 0x45e6ff, transparent: true, depthWrite: false, blending: THREE.AdditiveBlending }), shardPositions.length);
shardGlow.frustumCulled = false; scene.add(shardGlow);
function updateShards(dt) {
  const t = state.time;
  for (let i = 0; i < shardPositions.length; i++) {
    const [x, y, z] = shardPositions[i]; const got = state.shardsGot.has(i);
    const s = got ? 0 : 1;
    shardMesh.setMatrixAt(i, M(x, y + Math.sin(t * 2 + i) * 0.2, z, t * 1.5 + i, s, s * 1.4, s));
    _q.copy(camera.quaternion); pedM.compose(_p.set(x, y + Math.sin(t * 2 + i) * 0.2, z), _q, _s.set(s, s, s)); shardGlow.setMatrixAt(i, pedM);
    if (!got && player.pos.distanceToSquared(tmpV.set(x, y - 1, z)) < 3.2) collectShard(i);
  }
  shardMesh.instanceMatrix.needsUpdate = shardGlow.instanceMatrix.needsUpdate = true;
}
function collectShard(i) {
  state.shardsGot.add(i); state.credits += 40; addEnergy(10);
  burst(new V3(...shardPositions[i]), 0x6ff0ff, 24, 5, 0.7); sfx('pickup');
  toast(`Echo Shard ${state.shardsGot.size}/${shardPositions.length}  +40◆`);
  if (state.shardsGot.size === shardPositions.length) toast('All Echo Shards found! +2000◆', true), (state.credits += 2000);
  save();
}

// =====================================================================
// Enemies (Anomalies)
// =====================================================================
const enemies = [];
const ENEMY_TYPES = {
  glitch: { hp: 45, speed: 5.2, dmg: 7, range: 1.8, cd: 1.2, hover: 1.1, radius: 0.7, credits: 15, color: 0xff2bd6 },
  wisp: { hp: 32, speed: 3.8, dmg: 8, range: 13, cd: 2.2, hover: 3.2, radius: 0.7, credits: 18, color: 0x45e6ff },
  brute: { hp: 170, speed: 2.7, dmg: 16, range: 3.2, cd: 2.6, hover: 0, radius: 1.3, credits: 45, color: 0xff4b3a },
  boss: { hp: 3200, speed: 3.0, dmg: 14, range: 40, cd: 1, hover: 6, radius: 4, credits: 1500, color: 0xb47bff },
};
function buildEnemyMesh(type) {
  const g = new THREE.Group(); const mats = []; const T = ENEMY_TYPES[type];
  const mk = (c, e) => { const m = toon(c, e); mats.push(m); return m; };
  const parts = {};
  if (type === 'glitch') {
    const core = new THREE.Mesh(new THREE.IcosahedronGeometry(0.55, 0), mk(0x1d0f2e, 0x3a0a40)); g.add(core); addOutline(core, 1.1);
    const eye = new THREE.Mesh(new THREE.SphereGeometry(0.16, 8, 6), basic(T.color)); eye.position.z = 0.48; g.add(eye);
    parts.orbit = new THREE.Group(); g.add(parts.orbit);
    for (let k = 0; k < 4; k++) { const c = new THREE.Mesh(new THREE.BoxGeometry(0.22, 0.22, 0.22), basic(k % 2 ? T.color : 0x7a2bff)); c.position.set(Math.cos(k * 1.57) * 0.9, (k % 2) * 0.3 - 0.15, Math.sin(k * 1.57) * 0.9); parts.orbit.add(c); }
  } else if (type === 'wisp') {
    const core = new THREE.Mesh(new THREE.OctahedronGeometry(0.6, 0), mk(0x10203a, 0x0a3550)); g.add(core); addOutline(core, 1.1);
    const eye = new THREE.Mesh(new THREE.SphereGeometry(0.18, 8, 6), basic(T.color)); eye.position.z = 0.45; g.add(eye);
    parts.orbit = new THREE.Mesh(new THREE.TorusGeometry(0.95, 0.05, 6, 24), basic(T.color)); g.add(parts.orbit);
    const tailM = new THREE.Mesh(new THREE.ConeGeometry(0.35, 1.3, 6), mk(0x10203a, 0x0a3550)); tailM.position.y = -0.9; tailM.rotation.x = Math.PI; g.add(tailM);
  } else if (type === 'brute') {
    const body = new THREE.Mesh(new THREE.BoxGeometry(1.9, 1.7, 1.4), mk(0x241820, 0x2a0808)); body.position.y = 1.55; g.add(body); addOutline(body, 1.05);
    const head = new THREE.Mesh(new THREE.BoxGeometry(0.9, 0.7, 0.8), mk(0x1a1218, 0x200606)); head.position.set(0, 2.6, 0.25); g.add(head);
    const visor = new THREE.Mesh(new THREE.BoxGeometry(0.7, 0.12, 0.05), basic(T.color)); visor.position.set(0, 2.62, 0.66); g.add(visor);
    for (const s of [-1, 1]) {
      const arm = new THREE.Mesh(new THREE.BoxGeometry(0.55, 1.6, 0.55), mk(0x241820, 0x2a0808)); arm.geometry.translate(0, -0.7, 0); arm.position.set(1.25 * s, 2.2, 0); g.add(arm); parts[s < 0 ? 'armL' : 'armR'] = arm;
      const leg = new THREE.Mesh(new THREE.BoxGeometry(0.6, 0.8, 0.6), mk(0x1a1218, 0x200606)); leg.position.set(0.55 * s, 0.4, 0); g.add(leg);
      const crack = new THREE.Mesh(new THREE.BoxGeometry(0.08, 1.2, 0.05), basic(T.color)); crack.position.set(0.5 * s, 1.6, 0.72); crack.rotation.z = 0.3 * s; g.add(crack);
    }
  } else if (type === 'boss') {
    const face = new THREE.Mesh(new THREE.CylinderGeometry(3.2, 3.2, 0.6, 32), mk(0x150c26, 0x1a0a33)); face.rotation.x = Math.PI / 2; g.add(face); addOutline(face, 1.04);
    const dial = new THREE.Mesh(new THREE.RingGeometry(2.4, 2.8, 12, 1), basic(T.color, { side: THREE.DoubleSide })); dial.position.z = 0.32; g.add(dial);
    parts.hour = new THREE.Mesh(new THREE.BoxGeometry(0.25, 1.6, 0.1), basic(0xff4fd8)); parts.hour.geometry.translate(0, 0.8, 0); parts.hour.position.z = 0.38; g.add(parts.hour);
    parts.min = new THREE.Mesh(new THREE.BoxGeometry(0.15, 2.4, 0.1), basic(0xffffff)); parts.min.geometry.translate(0, 1.2, 0); parts.min.position.z = 0.4; g.add(parts.min);
    parts.r1 = new THREE.Mesh(new THREE.TorusGeometry(4.6, 0.15, 6, 48), basic(T.color)); g.add(parts.r1);
    parts.r2 = new THREE.Mesh(new THREE.TorusGeometry(5.4, 0.1, 6, 48), basic(0xff4fd8)); g.add(parts.r2);
    const eye = new THREE.Mesh(new THREE.SphereGeometry(0.5, 12, 10), basic(0xff4fd8)); eye.position.z = 0.5; g.add(eye);
  }
  return { g, mats, parts };
}
const barGeo = new THREE.PlaneGeometry(1, 1); barGeo.translate(0.5, 0, 0);
function spawnEnemy(type, x, z, opts = {}) {
  const T = ENEMY_TYPES[type]; const { g, mats, parts } = buildEnemyMesh(type);
  const gy = groundAt(x, z, 50);
  g.position.set(x, gy + T.hover, z); scene.add(g);
  const bar = new THREE.Group();
  const bg = new THREE.Mesh(barGeo, basic(0x000000, { transparent: true, opacity: 0.6, depthTest: false })); bg.scale.set(1.3, 0.14, 1); bg.position.x = -0.65;
  const fg = new THREE.Mesh(barGeo, basic(type === 'brute' ? 0xffa040 : 0xff4d6d, { depthTest: false })); fg.scale.set(1.26, 0.1, 1); fg.position.set(-0.63, 0, 0.01);
  bg.renderOrder = 10; fg.renderOrder = 11; bar.add(bg, fg); bar.visible = type !== 'boss'; scene.add(bar);
  const hpMul = 1 + (state.riftsClosed.size * 0.12);
  const e = {
    type, T, mesh: g, mats, parts, bar, barFg: fg, pos: g.position, vel: new V3(), hp: T.hp * hpMul, maxHp: T.hp * hpMul, atkCd: rr(0.5, 1.5),
    alive: true, aggro: !!opts.aggro, flash: 0, t: rr(0, 10), zone: opts.zone ?? null, roaming: !!opts.roaming, home: new V3(x, 0, z),
    windup: 0, lift: 0, phase: 0, phaseT: 0, stun: 0,
  };
  enemies.push(e);
  burst(tmpV.set(x, gy + 1, z), T.color, 20, 5, 0.6);
  return e;
}
function removeEnemy(e) {
  e.alive = false; scene.remove(e.mesh); scene.remove(e.bar);
  e.mats.forEach((m) => m.dispose());
  const i = enemies.indexOf(e); if (i >= 0) enemies.splice(i, 1);
}
function hurtEnemy(e, dmg, knock, color = 0xffffff) {
  if (!e.alive) return;
  const crit = Math.random() < 0.15; const d = Math.round(dmg * (crit ? 1.8 : 1) * rr(0.9, 1.1));
  e.hp -= d; e.flash = 0.12; e.aggro = true; state.inCombatT = 6;
  if (knock && e.type !== 'boss') e.vel.add(knock.multiplyScalar(e.type === 'brute' ? 0.3 : 1));
  showDamage(e.pos, d, crit, color);
  addEnergy(crit ? 4 : 2.5);
  if (e.hp <= 0) killEnemy(e);
}
function killEnemy(e) {
  burst(e.pos, e.T.color, 40, 9, 0.9, 3); burst(e.pos, 0xffffff, 12, 5, 0.5);
  ring(e.pos.clone().setY(groundAt(e.pos.x, e.pos.z, e.pos.y)), e.T.color, e.type === 'boss' ? 30 : 4, 0.5);
  const c = Math.round(e.T.credits * rr(0.8, 1.2)); state.credits += c;
  sfx(e.type === 'boss' ? 'win' : 'boom');
  if (e.type === 'boss') onBossDefeated();
  removeEnemy(e);
}
function nearestEnemy(p, maxD, filter) {
  let best = null, bd = maxD * maxD;
  for (const e of enemies) { if (filter && !filter(e)) continue; const d = e.pos.distanceToSquared(p); if (d < bd) { bd = d; best = e; } }
  return best;
}
function enemiesInRadius(p, r) { const r2 = r * r; return enemies.filter((e) => { const dx = e.pos.x - p.x, dz = e.pos.z - p.z, dy = (e.pos.y - p.y) * 0.5; return dx * dx + dz * dz + dy * dy < r2 + e.T.radius * e.T.radius; }); }

// projectiles ----------------------------------------------------------
const projectiles = [];
const projGeo = new THREE.SphereGeometry(0.28, 8, 6);
function shoot(from, dir, speed, dmg, color, enemy, opts = {}) {
  const m = new THREE.Mesh(projGeo, basic(color)); m.position.copy(from); m.scale.setScalar(opts.size || 1); scene.add(m);
  const glow = new THREE.Sprite(new THREE.SpriteMaterial({ map: glowTex, color, transparent: true, depthWrite: false, blending: THREE.AdditiveBlending })); glow.scale.setScalar(1.8 * (opts.size || 1)); m.add(glow);
  projectiles.push({ m, vel: dir.clone().normalize().multiplyScalar(speed), dmg, color, enemy, life: opts.life || 3, target: opts.target || null, onHit: opts.onHit });
}
function updateProjectiles(dt) {
  for (let i = projectiles.length - 1; i >= 0; i--) {
    const p = projectiles[i]; p.life -= dt;
    if (p.target && p.target.alive) { tmpV.copy(p.target.pos).sub(p.m.position).normalize().multiplyScalar(p.vel.length()); p.vel.lerp(tmpV, Math.min(1, dt * 6)); }
    p.m.position.addScaledVector(p.vel, dt);
    let dead = p.life <= 0 || insideBuilding(p.m.position.x, p.m.position.y, p.m.position.z) || p.m.position.y < 0;
    if (!dead && p.enemy) {
      tmpV.copy(player.pos); tmpV.y += 1;
      if (p.m.position.distanceToSquared(tmpV) < (player.mode === 'drive' ? 4 : 1.1)) { damagePlayer(p.dmg, p.m.position); dead = true; }
    } else if (!dead) {
      for (const e of enemies) if (e.pos.distanceToSquared(p.m.position) < (e.T.radius + 0.6) ** 2) { hurtEnemy(e, p.dmg, p.vel.clone().setY(0).normalize().multiplyScalar(2), p.color); if (p.onHit) p.onHit(e); dead = true; break; }
    }
    if (dead) { burst(p.m.position, p.color, 8, 4, 0.35); scene.remove(p.m); p.m.children[0].material.dispose(); p.m.material.dispose(); projectiles.splice(i, 1); }
  }
}

// enemy AI ----------------------------------------------------------------
function updateEnemies(dt) {
  const ppos = player.pos;
  for (let ei = enemies.length - 1; ei >= 0; ei--) {
    const e = enemies[ei]; if (!e || !e.alive) continue; const T = e.T; e.t += dt;
    const dx = ppos.x - e.pos.x, dz = ppos.z - e.pos.z; const dist = Math.hypot(dx, dz);
    if (e.roaming && dist > 140) { removeEnemy(e); continue; }
    if (!e.aggro && dist < 26) e.aggro = true;
    const gy = groundAt(e.pos.x, e.pos.z, e.pos.y);
    e.stun -= dt;
    let mvx = 0, mvz = 0;
    if (e.aggro && e.stun <= 0 && e.lift <= 0) {
      const nx = dx / (dist || 1), nz = dz / (dist || 1);
      if (e.type === 'glitch') { if (dist > T.range * 0.8) { mvx = nx; mvz = nz; } }
      else if (e.type === 'wisp') { if (dist > 11) { mvx = nx; mvz = nz; } else if (dist < 7) { mvx = -nx; mvz = -nz; } else { mvx = -nz * 0.6; mvz = nx * 0.6; } }
      else if (e.type === 'brute') { if (e.windup <= 0 && dist > 2.4) { mvx = nx; mvz = nz; } }
      else if (e.type === 'boss') updateBoss(e, dt, dist, nx, nz);
      if (e.type !== 'boss') e.mesh.rotation.y = e.mesh.rotation.y + angDiff(e.mesh.rotation.y, Math.atan2(dx, dz)) * Math.min(1, dt * 8);
      // attacks
      e.atkCd -= dt;
      if (e.type === 'glitch' && dist < T.range + 0.6 && e.atkCd <= 0) {
        e.atkCd = T.cd; e.vel.x += nx * 9; e.vel.z += nz * 9; later(0.12, () => { if (e.alive && e.pos.distanceTo(player.pos) < T.range + 1.4) damagePlayer(T.dmg, e.pos); });
      }
      if (e.type === 'wisp' && dist < T.range + 4 && e.atkCd <= 0) {
        e.atkCd = T.cd; tmpV2.copy(ppos); tmpV2.y += 1; shoot(e.pos, tmpV2.sub(e.pos), 15, T.dmg, T.color, true);
      }
      if (e.type === 'brute' && dist < T.range + 1 && e.atkCd <= 0 && e.windup <= 0) {
        e.windup = 0.9; e.atkCd = T.cd; const at = e.pos.clone().add(tmpV.set(nx * 1.8, 0, nz * 1.8)); at.y = gy;
        ring(at, 0xff3a3a, 4.2, 0.9, { static: true });
        later(0.9, () => {
          if (!e.alive) return; ring(at, 0xff6a3a, 5, 0.4); burst(at, 0xff6a3a, 30, 8, 0.6); sfx('boom'); shake(0.4);
          if (Math.hypot(player.pos.x - at.x, player.pos.z - at.z) < 4.4 && player.pos.y - at.y < 2.5) damagePlayer(T.dmg, at);
        });
      }
    }
    if (e.windup > 0) { e.windup -= dt; if (e.parts.armL) { e.parts.armL.rotation.x = e.parts.armR.rotation.x = -2.6 * (e.windup / 0.9); } }
    else if (e.parts.armL) { const sw = Math.sin(e.t * 6) * 0.4 * Math.min(1, Math.hypot(mvx, mvz)); e.parts.armL.rotation.x = sw; e.parts.armR.rotation.x = -sw; }
    // motion
    const sp = T.speed * (e.type === 'boss' ? 1 : 1);
    e.pos.x += (mvx * sp + e.vel.x) * dt; e.pos.z += (mvz * sp + e.vel.z) * dt;
    e.vel.multiplyScalar(Math.exp(-dt * 6));
    if (e.lift > 0) { e.lift -= dt; e.pos.y = damp(e.pos.y, gy + 6, 5, dt); }
    else if (e.type === 'boss') { /* boss sets its own height */ }
    else e.pos.y = damp(e.pos.y, gy + T.hover + (T.hover ? Math.sin(e.t * 2.5) * 0.25 : 0), 8, dt);
    if (e.type !== 'boss') collide(e.pos, T.radius, e.pos.y - T.hover);
    // separation
    for (let k = 0; k < enemies.length; k++) {
      const o = enemies[k]; if (o === e) continue; const sx = e.pos.x - o.pos.x, sz = e.pos.z - o.pos.z; const d2 = sx * sx + sz * sz; const rr2 = (T.radius + o.T.radius);
      if (d2 < rr2 * rr2 && d2 > 1e-4) { const d = Math.sqrt(d2), push = (rr2 - d) * 0.5; e.pos.x += (sx / d) * push; e.pos.z += (sz / d) * push; }
    }
    // driving into enemies
    if (player.mode === 'drive' && Math.abs(player.car.speed) > 8 && dist < T.radius + 2 && e.type !== 'boss') {
      hurtEnemy(e, Math.abs(player.car.speed) * 2.2, new V3(dx, 0, dz).normalize().multiplyScalar(-14), 0xffffff); player.car.speed *= 0.75; sfx('crash'); shake(0.25);
      if (!e.alive) continue;
    }
    // visuals
    if (e.parts.orbit) { e.parts.orbit.rotation.y += dt * 3; e.parts.orbit.rotation.x = Math.sin(e.t) * 0.4; }
    if (e.flash > 0) e.flash -= dt;
    const fl = e.flash > 0;
    for (const m of e.mats) { if (!m.userData.base) m.userData.base = m.emissive.getHex(); m.emissive.setHex(fl ? 0xffffff : m.userData.base); }
    if (e.type === 'glitch' && Math.random() < dt * 3) e.mesh.position.x += rr(-0.15, 0.15);
    // hp bar
    if (e.bar.visible) {
      e.bar.position.set(e.pos.x, e.pos.y + (e.type === 'brute' ? 3.4 : 1.5), e.pos.z); e.bar.quaternion.copy(camera.quaternion);
      e.barFg.scale.x = 1.26 * Math.max(0, e.hp / e.maxHp);
      e.bar.visible = dist < 60;
    } else if (e.type !== 'boss') e.bar.visible = dist < 60;
  }
}

// =====================================================================
// Rifts (anomaly zones)
// =====================================================================
const RIFT_WAVES = [
  [['glitch', 'glitch', 'glitch'], ['glitch', 'wisp', 'glitch']],
  [['glitch', 'glitch', 'wisp'], ['wisp', 'wisp', 'glitch', 'glitch'], ['brute']],
  [['wisp', 'wisp', 'glitch', 'glitch'], ['brute', 'glitch', 'glitch']],
  [['glitch', 'glitch', 'glitch', 'wisp'], ['brute', 'wisp', 'wisp'], ['brute', 'brute']],
  [['brute', 'glitch', 'glitch'], ['wisp', 'wisp', 'wisp', 'glitch'], ['brute', 'brute', 'wisp']],
];
const riftMat = basic(0xc04bff, { transparent: true, opacity: 0.55, side: THREE.DoubleSide, blending: THREE.AdditiveBlending, depthWrite: false });
const rifts = RIFT_BLOCKS.map(([i, j], idx) => {
  const pos = new V3(blockCenter(i), SWH, blockCenter(j));
  const g = new THREE.Group(); g.position.copy(pos); scene.add(g);
  const ringM = new THREE.Mesh(new THREE.TorusGeometry(3.4, 0.25, 8, 40), basic(0xe08bff)); ringM.position.y = 4.5; g.add(ringM);
  const disc = new THREE.Mesh(new THREE.CircleGeometry(3.2, 32), riftMat); disc.position.y = 4.5; g.add(disc);
  const disc2 = disc.clone(); disc2.scale.setScalar(0.6); g.add(disc2);
  const pillar = new THREE.Mesh(new THREE.CylinderGeometry(0.4, 2.5, 60, 12, 1, true), basic(0xa040ff, { transparent: true, opacity: 0.18, blending: THREE.AdditiveBlending, depthWrite: false, side: THREE.DoubleSide }));
  pillar.position.y = 30; g.add(pillar);
  const calm = new THREE.Mesh(new THREE.OctahedronGeometry(1.1, 0), basic(0x5dff9a)); calm.position.y = 2.2; calm.visible = false; g.add(calm);
  return { idx, pos, g, ringM, disc, disc2, pillar, calm, waves: RIFT_WAVES[idx], wave: 0, active: false, enemies: [], waveDelay: 0 };
});
function updateRifts(dt) {
  for (const r of rifts) {
    const closed = state.riftsClosed.has(r.idx);
    r.ringM.visible = r.disc.visible = r.disc2.visible = r.pillar.visible = !closed; r.calm.visible = closed;
    if (closed) { r.calm.rotation.y += dt; continue; }
    r.g.rotation.y += dt * 0.6; r.disc.rotation.z += dt; r.disc2.rotation.z -= dt * 2; r.ringM.lookAt(camera.position); r.disc.lookAt(camera.position); r.disc2.lookAt(camera.position);
    r.disc.scale.setScalar(1 + Math.sin(state.time * 3) * 0.06);
    const d = Math.hypot(player.pos.x - r.pos.x, player.pos.z - r.pos.z);
    if (!r.active && d < 34) { r.active = true; r.wave = 0; r.waveDelay = 0.5; toast('⚠ Anomaly Rift — clear all waves!'); sfx('skill'); }
    if (r.active) {
      if (d > 90) { r.active = false; r.enemies.forEach((e) => e.alive && removeEnemy(e)); r.enemies = []; toast('Rift signal lost'); continue; }
      r.enemies = r.enemies.filter((e) => e.alive);
      if (r.enemies.length === 0) {
        if (r.waveDelay > 0) { r.waveDelay -= dt; if (r.waveDelay <= 0) spawnWave(r); }
        else if (r.wave >= r.waves.length) closeRift(r);
        else r.waveDelay = 1.6;
      }
    }
  }
}
function spawnWave(r) {
  const list = r.waves[r.wave]; r.wave++;
  toast(`Wave ${r.wave}/${r.waves.length}`);
  list.forEach((t, k) => { const a = (k / list.length) * Math.PI * 2 + rr(0, 1); r.enemies.push(spawnEnemy(t, r.pos.x + Math.cos(a) * 8, r.pos.z + Math.sin(a) * 8, { aggro: true, zone: r.idx })); });
}
function closeRift(r) {
  r.active = false; state.riftsClosed.add(r.idx); state.credits += 300;
  burst(r.pos.clone().setY(4), 0xe08bff, 80, 14, 1.2, 4); ring(r.pos, 0x5dff9a, 22, 0.9);
  toast(`Rift sealed! (${state.riftsClosed.size}/${rifts.length})  +300◆`, true); sfx('win'); save();
}

// roaming anomalies
let roamT = 6;
function updateRoaming(dt) {
  roamT -= dt;
  if (roamT > 0) return;
  const night = 1 - dayFactor();
  roamT = lerp(16, 8, night);
  const roamers = enemies.filter((e) => e.roaming).length;
  if (roamers >= (night > 0.5 ? 7 : 4) || state.bossActive) return;
  for (let tries = 0; tries < 8; tries++) {
    const a = rr(0, Math.PI * 2), d = rr(32, 55); const x = player.pos.x + Math.cos(a) * d, z = player.pos.z + Math.sin(a) * d;
    if (Math.abs(x) > HALF - 4 || Math.abs(z) > HALF - 4 || insideBuilding(x, 1, z) || Math.hypot(x, z) < 30) continue;
    if (Math.hypot(x - IRIS_POS.x, z - IRIS_POS.z) < 30) continue;
    const n = ri(1, 3);
    for (let k = 0; k < n; k++) spawnEnemy(rng() < 0.65 ? 'glitch' : 'wisp', x + rr(-3, 3), z + rr(-3, 3), { roaming: true });
    if (night > 0.6 && rng() < 0.25) spawnEnemy('brute', x, z + 3, { roaming: true });
    break;
  }
}

// =====================================================================
// Boss: The Hollow Clock
// =====================================================================
let boss = null;
function startBoss() {
  if (boss || state.bossDefeated) return;
  state.bossActive = true;
  enemies.filter((e) => e.roaming).forEach(removeEnemy);
  boss = spawnEnemy('boss', 0, 24, { aggro: true });
  boss.pos.y = 8; boss.phase = 0; boss.phaseT = 3;
  $('bossBar').classList.remove('hidden');
  toast('THE HOLLOW CLOCK awakens', true); sfx('ult'); shake(0.6);
}
function updateBoss(e, dt, dist, nx, nz) {
  const p = e.parts; p.hour.rotation.z -= dt * 0.6; p.min.rotation.z -= dt * 3; p.r1.rotation.x += dt * 0.8; p.r2.rotation.y += dt * 1.1;
  e.mesh.lookAt(player.pos.x, e.pos.y, player.pos.z);
  const rage = e.hp < e.maxHp * 0.5;
  // drift around arena near the spire
  const ang = state.time * 0.3; const tx = Math.cos(ang) * 20, tz = 22 + Math.sin(ang) * 6;
  e.pos.x = damp(e.pos.x, tx, 0.6, dt); e.pos.z = damp(e.pos.z, tz, 0.6, dt); e.pos.y = damp(e.pos.y, 7 + Math.sin(state.time) * 1.5, 2, dt);
  e.phaseT -= dt;
  if (e.phaseT <= 0) {
    e.phase = (e.phase + 1) % 4; const ph = e.phase;
    if (ph === 0) { // spiral volley
      e.phaseT = 4.5; let k = 0; const n = rage ? 36 : 24;
      const fire = () => { if (!e.alive || k >= n) return; const a = k * 0.55; for (const off of [0, Math.PI]) shoot(e.pos, tmpV.set(Math.cos(a + off), -0.12, Math.sin(a + off)), 12, e.T.dmg, 0xff4fd8, true, { life: 4 }); k++; later(0.11, fire); };
      fire();
    } else if (ph === 1) { // summon
      e.phaseT = 3; toast('The Clock tears open small rifts…');
      const n = rage ? 4 : 3; for (let k = 0; k < n; k++) spawnEnemy(k === 0 && rage ? 'brute' : 'glitch', e.pos.x + rr(-6, 6), e.pos.z + rr(-6, 6), { aggro: true });
    } else if (ph === 2) { // time slams
      e.phaseT = 4; const n = rage ? 5 : 3;
      for (let k = 0; k < n; k++) later(k * 0.7, () => {
        if (!e.alive) return; const at = player.pos.clone(); at.y = groundAt(at.x, at.z, at.y + 0.5);
        ring(at, 0xff3a6a, 5, 1.0, { static: true });
        later(1.0, () => { if (!e.alive) return; beam(e.pos, at, 0xff4fd8, 0.3, 4); ring(at, 0xff4fd8, 6, 0.4); burst(at, 0xff4fd8, 30, 9, 0.6); sfx('boom'); shake(0.4); if (Math.hypot(player.pos.x - at.x, player.pos.z - at.z) < 5 && player.pos.y - at.y < 3) damagePlayer(20, at); });
      });
    } else { // aimed bursts
      e.phaseT = 3;
      for (let k = 0; k < (rage ? 5 : 3); k++) later(k * 0.4, () => { if (!e.alive) return; tmpV2.copy(player.pos).setY(player.pos.y + 1); const dir = tmpV2.sub(e.pos).normalize(); for (let s = -2; s <= 2; s++) shoot(e.pos, dir.clone().applyAxisAngle(new V3(0, 1, 0), s * 0.18), 18, e.T.dmg, 0xb47bff, true); sfx('bolt'); });
    }
  }
  $('bossFill').style.width = `${Math.max(0, (e.hp / e.maxHp) * 100)}%`;
}
function onBossDefeated() {
  state.bossDefeated = true; state.bossActive = false; boss = null;
  $('bossBar').classList.add('hidden');
  enemies.slice().forEach(removeEnemy);
  toast('THE HOLLOW CLOCK has been silenced', true); toast('+1500◆  Nexus City is safe… for now.');
  later(1.5, () => openDialog([
    { name: 'Iris', text: 'The Spire\'s hum just… stopped. You did it.' },
    { name: 'Iris', text: 'Anomalies will still drift in at night — the city never fully sleeps. Come by the shop anytime to grow your resonance.' },
    { name: 'System', text: 'Main story complete! Keep exploring: find every Echo Shard (one is on top of the Spire) and hunt roaming anomalies.' },
  ]));
  save();
}

// =====================================================================
// Player combat
// =====================================================================
function addEnergy(v) { player.energy[player.active] = Math.min(100, player.energy[player.active] + v); }
function activeModel() { return models[player.active]; }
function faceTarget(t) { if (t) player.yaw = Math.atan2(t.pos.x - player.pos.x, t.pos.z - player.pos.z); }
function forwardVec(out = new V3()) { return out.set(Math.sin(player.yaw), 0, Math.cos(player.yaw)); }
function chestPos(out = new V3()) { return out.copy(player.pos).setY(player.pos.y + 1.2); }

function doAttack() {
  if (state.mode !== 'play' || state.paused || dialogOpen()) return;
  if (player.mode === 'drive') { sfx('horn'); peds.forEach((p) => { if (Math.hypot(p.x - player.pos.x, p.z - player.pos.z) < 14) p.flee = 2; }); return; }
  if (player.attackT > 0 || player.climbing) return;
  const ch = CHARS[player.active];
  const target = nearestEnemy(player.pos, ch.ranged ? 26 : 12);
  faceTarget(target);
  player.combo = player.comboT > 0 ? (player.combo + 1) % 3 : 0; player.comboT = 0.9;
  player.attackT = ch.atkRate * (player.combo === 2 ? 1.4 : 1); player.swingT = 0.25;
  const dmg = atkPower() * ch.mult * (player.combo === 2 ? 1.6 : 1);
  if (ch.ranged) {
    const from = chestPos(); const f = forwardVec();
    const dir = target ? tmpV.copy(target.pos).sub(from).normalize() : f;
    shoot(from.add(f.clone().multiplyScalar(0.6)), dir, 34, dmg, ch.color, false, { target, size: player.combo === 2 ? 1.6 : 1, onHit: player.combo === 2 ? (e) => { const n = nearestEnemy(e.pos, 9, (o) => o !== e); if (n) { zigzag(e.pos, n.pos, ch.color); hurtEnemy(n, dmg * 0.6, null, ch.color); } } : null });
    sfx('bolt');
  } else {
    if (target) {
      const d = Math.hypot(target.pos.x - player.pos.x, target.pos.z - player.pos.z);
      if (d > 2.4 && d < 10) { player.lunge.set(Math.sin(player.yaw), 0, Math.cos(player.yaw)).multiplyScalar((d - 2) / 0.14); player.lungeT = 0.14; }
    }
    player.pendingHit = { t: 0.13, dmg, combo: player.combo };
    sfx('slash');
  }
}
function meleeHit(h) {
  const ch = CHARS[player.active]; const f = forwardVec(); const c = chestPos();
  const center = c.clone().addScaledVector(f, 1.3);
  const range = ch.key === 'sora' ? 3.6 : 3.2;
  let any = false;
  for (const e of enemiesInRadius(center, range)) {
    any = true; hurtEnemy(e, h.dmg, f.clone().multiplyScalar(h.combo === 2 ? 14 : 5), ch.color);
    if (ch.key === 'sora' && h.combo === 2) e.stun = 0.8;
  }
  burst(center, ch.color, any ? 18 : 8, 6, 0.35, 1, 0);
  if (h.combo === 2) ring(player.pos, ch.color, 3.5, 0.3);
  if (any) { sfx('hit'); hitStop(0.05); }
}
function doSkill() {
  if (state.mode !== 'play' || state.paused || dialogOpen()) return;
  if (player.mode === 'drive') return;
  const a = player.active; if (player.skillCd[a] > 0) return;
  const ch = CHARS[a]; const P = player.pos; const pw = atkPower();
  player.skillCd[a] = [6, 7, 9][a]; sfx('skill'); player.swingT = 0.35;
  if (ch.key === 'rei') { // Blaze Whirl
    ring(P, ch.color, 7, 0.45); ring(P, 0xffd36b, 5, 0.35); burst(chestPos(), ch.color, 60, 12, 0.6, 2, 2);
    for (const e of enemiesInRadius(P, 7.5)) hurtEnemy(e, pw * 3.2, tmpV.copy(e.pos).sub(P).setY(0).normalize().multiplyScalar(16), ch.color);
    player.vel.y = Math.max(player.vel.y, 4);
  } else if (ch.key === 'mika') { // Chain Bolt
    let from = chestPos(); const hitSet = new Set(); let src = { pos: P };
    for (let k = 0; k < 5; k++) {
      const n = nearestEnemy(src.pos, k === 0 ? 22 : 12, (e) => !hitSet.has(e)); if (!n) break;
      hitSet.add(n); zigzag(from, n.pos, ch.color); burst(n.pos, ch.color, 14, 6, 0.4);
      const dmg = pw * 2.4; later(k * 0.06, () => hurtEnemy(n, dmg, null, ch.color)); if (n.type !== 'boss') n.stun = 1.2;
      from = n.pos.clone(); src = n;
    }
    if (!hitSet.size) { const f = forwardVec(); zigzag(chestPos(), chestPos().addScaledVector(f, 10), ch.color); }
  } else { // Singularity
    const f = forwardVec(); const t = nearestEnemy(P, 16); const at = t ? t.pos.clone() : P.clone().addScaledVector(f, 7); at.y = groundAt(at.x, at.z, at.y + 1) + 1.2;
    const core = new THREE.Mesh(new THREE.SphereGeometry(0.9, 16, 12), basic(0x000000)); core.position.copy(at); scene.add(core);
    const halo = new THREE.Sprite(new THREE.SpriteMaterial({ map: glowTex, color: ch.color, transparent: true, depthWrite: false, blending: THREE.AdditiveBlending })); halo.scale.setScalar(6); core.add(halo);
    let tt = 0;
    effects.push((dt) => {
      tt += dt; halo.material.rotation += dt * 4; core.scale.setScalar(1 + Math.sin(tt * 20) * 0.1);
      for (const e of enemiesInRadius(at, 10)) { if (e.type === 'boss') continue; tmpV.copy(at).sub(e.pos).setY(0); const d = tmpV.length(); if (d > 0.8) e.pos.addScaledVector(tmpV.normalize(), Math.min(d, 9 * dt)); }
      if (Math.random() < 0.5) burst(at.clone().add(tmpV.set(rr(-5, 5), rr(-1, 2), rr(-5, 5))), ch.color, 1, 1, 0.4, 0, 0);
      if (tt > 1.5) {
        scene.remove(core); halo.material.dispose(); ring(at, ch.color, 8, 0.5); burst(at, ch.color, 60, 12, 0.7); sfx('boom'); shake(0.3);
        for (const e of enemiesInRadius(at, 8)) hurtEnemy(e, pw * 4, tmpV.copy(e.pos).sub(at).setY(0).normalize().multiplyScalar(10), ch.color);
        return false;
      }
      return true;
    });
  }
}
function doUlt() {
  if (state.mode !== 'play' || state.paused || dialogOpen() || player.mode === 'drive') return;
  const a = player.active; if (player.energy[a] < 100) return;
  player.energy[a] = 0; const ch = CHARS[a]; const P = player.pos.clone(); const pw = atkPower();
  sfx('ult'); shake(0.5); player.invuln = 1.5; slowMo(0.5, 0.4);
  toast(`${ch.name}: ${['Inferno Requiem', 'Thunderdome', 'Event Horizon'][a]}!`, true);
  if (ch.key === 'rei') {
    for (let k = 0; k < 14; k++) later(0.15 + k * 0.12, () => {
      const t = nearestEnemy(P, 22); const at = (t && rng() < 0.6 ? t.pos.clone() : P.clone().add(tmpV.set(rr(-12, 12), 0, rr(-12, 12))));
      at.y = groundAt(at.x, at.z, at.y + 1);
      const m = new THREE.Mesh(projGeo, basic(0xffb03a)); m.scale.setScalar(3); m.position.copy(at).add(tmpV.set(-6, 26, -6)); scene.add(m);
      const start = m.position.clone(); let tt = 0;
      effects.push((dt) => { tt += dt / 0.35; m.position.lerpVectors(start, at, Math.min(1, tt)); burst(m.position, 0xff6a2a, 2, 1, 0.3, 0, 0);
        if (tt >= 1) { scene.remove(m); m.material.dispose(); ring(at, ch.color, 5, 0.4); burst(at, 0xff6a2a, 30, 10, 0.6); sfx('boom'); for (const e of enemiesInRadius(at, 4.8)) hurtEnemy(e, pw * 3, tmpV.copy(e.pos).sub(at).setY(0).normalize().multiplyScalar(8), ch.color); return false; } return true; });
    });
  } else if (ch.key === 'mika') {
    ring(P, ch.color, 20, 0.8);
    for (let k = 0; k < 22; k++) later(k * 0.15, () => {
      const t = nearestEnemy(player.pos, 22, (e) => Math.random() < 0.7) || nearestEnemy(player.pos, 22); if (!t) return;
      const top = t.pos.clone().setY(t.pos.y + 30); zigzag(top, t.pos, 0xbff6ff); burst(t.pos, ch.color, 16, 8, 0.4); hurtEnemy(t, pw * 1.9, null, ch.color); sfx('bolt'); if (t.type !== 'boss') t.stun = 0.5;
    });
  } else {
    const list = enemiesInRadius(P, 18);
    ring(P, ch.color, 18, 1.0); burst(chestPos(), ch.color, 80, 14, 1.0, 4);
    list.forEach((e) => { if (e.type !== 'boss') e.lift = 1.4; });
    later(1.4, () => {
      ring(P, ch.color, 18, 0.5); sfx('boom'); shake(0.8);
      for (const e of list) if (e.alive) { burst(e.pos, ch.color, 30, 10, 0.6); hurtEnemy(e, pw * 8, null, ch.color); if (e.alive) e.vel.y = -20; }
    });
  }
}
function swapTo(i) {
  if (state.mode !== 'play' || state.paused) return;
  if (i === player.active || player.swapCd > 0 || player.hp[i] <= 0 || player.mode === 'drive') return;
  models[player.active].visible = false; player.active = i; models[i].visible = true;
  player.swapCd = 1.2; sfx('swap');
  const ch = CHARS[i]; ring(player.pos, ch.color, 4.5, 0.35); burst(chestPos(), ch.color, 30, 8, 0.5);
  for (const e of enemiesInRadius(player.pos, 4.5)) hurtEnemy(e, atkPower() * 1.5, tmpV.copy(e.pos).sub(player.pos).setY(0).normalize().multiplyScalar(8), ch.color);
  buildParty(); updateUltColor();
}
function damagePlayer(dmg, from) {
  if (state.mode !== 'play') return;
  if (player.dashT > 0 && player.mode === 'foot') { // perfect dodge
    if (state.slowT <= 0) { toast('Perfect Dodge!'); slowMo(0.8, 0.3); addEnergy(15); burst(chestPos(), 0xffffff, 20, 6, 0.4); }
    return;
  }
  if (player.invuln > 0) return;
  const a = player.active; const d = Math.round(dmg * (player.mode === 'drive' ? 0.5 : 1) * (1 - Math.min(0.4, (state.level - 1) * 0.04)));
  player.hp[a] = Math.max(0, player.hp[a] - d); player.invuln = 0.45; state.lastHurt = state.time; state.inCombatT = 6;
  showDamage(chestPos(), d, false, 0xff4d6d, true); sfx('hurt'); shake(0.2);
  if (from && player.mode === 'foot') { tmpV.copy(player.pos).sub(from).setY(0).normalize(); player.vel.x += tmpV.x * 6; player.vel.z += tmpV.z * 6; }
  if (player.hp[a] <= 0) {
    const next = player.hp.findIndex((h) => h > 0);
    if (next >= 0) { toast(`${CHARS[a].name} is down!`); forceActive(next); player.swapCd = 1.2; }
    else teamWipe();
  }
}
function teamWipe() {
  toast('Your team was overwhelmed…', true);
  state.credits = Math.floor(state.credits * 0.9);
  if (player.mode === 'drive') exitCar();
  player.pos.copy(SPAWN_POS); player.vel.set(0, 0, 0); player.hp = [100, 100, 100]; player.invuln = 2;
  enemies.filter((e) => e.roaming || e.type === 'boss' || e.zone !== null).forEach((e) => removeEnemy(e));
  rifts.forEach((r) => { r.active = false; r.enemies = []; });
  if (boss) { boss = null; state.bossActive = false; $('bossBar').classList.add('hidden'); }
  forceActive(0);
}
let hitStopT = 0;
function hitStop(t) { hitStopT = Math.max(hitStopT, t); }
function slowMo(dur, scale) { state.slowT = dur; state.timeScale = scale; }
let shakeT = 0;
function shake(a) { shakeT = Math.max(shakeT, a); }

// =====================================================================
// Movement
// =====================================================================
const GRAV = 26, RUN = 8.5, JUMP = 10.5;
function tickPlayerTimers(dt) {
  const P = player;
  P.attackT -= dt; P.comboT -= dt; P.swingT -= dt; P.dashT -= dt; P.dashCd -= dt; P.invuln -= dt; P.swapCd -= dt;
  for (let i = 0; i < 3; i++) P.skillCd[i] = Math.max(0, P.skillCd[i] - dt);
  state.inCombatT -= dt;
  if (state.time - state.lastHurt > 6) for (let i = 0; i < 3; i++) P.hp[i] = Math.min(P.maxHp, P.hp[i] + dt * (P.hp[i] <= 0 ? 2 : 5));
}
function forceActive(i) {
  models.forEach((m) => (m.visible = false)); player.active = i;
  models[i].visible = player.mode === 'foot'; buildParty(); updateUltColor();
}
function jumpPress() {
  if (state.mode !== 'play' || state.paused || dialogOpen()) return;
  if (player.mode === 'drive') return;
  if (player.climbing) { player.climbing = false; player.vel.y = JUMP * 0.9; tmpV.set(Math.sin(player.yaw), 0, Math.cos(player.yaw)); player.vel.x -= tmpV.x * 6; player.vel.z -= tmpV.z * 6; player.yaw += Math.PI; sfx('jump'); return; }
  if (player.grounded) { player.vel.y = JUMP; player.grounded = false; player.jumps = 1; sfx('jump'); }
  else if (player.jumps < 2) { player.vel.y = JUMP * 0.9; player.jumps = 2; burst(player.pos, CHARS[player.active].color, 12, 3, 0.4, 0, 0); sfx('jump'); }
  else player.gliding = !player.gliding;
}
function dashPress() {
  if (state.mode !== 'play' || state.paused || dialogOpen()) return;
  if (player.mode === 'drive') { player.car.boost = 1.2; return; }
  if (player.dashCd > 0 || player.climbing) return;
  player.dashT = 0.28; player.dashCd = 0.7; player.gliding = false; sfx('dash');
  const mv = moveVector(); if (mv.lengthSq() > 0.01) player.yaw = Math.atan2(mv.x, mv.z);
  burst(chestPos(), CHARS[player.active].color, 16, 4, 0.35, 0, 0);
}
const _mv = new V3();
function moveVector() {
  let ix = input.jx, iy = input.jy;
  const k = input.keys;
  const kx = (k.KeyD || k.ArrowRight ? 1 : 0) - (k.KeyA || k.ArrowLeft ? 1 : 0), ky = (k.KeyW || k.ArrowUp ? 1 : 0) - (k.KeyS || k.ArrowDown ? 1 : 0);
  if (kx || ky) { ix = kx; iy = ky; const l = Math.hypot(ix, iy); ix /= l; iy /= l; }
  const a = cam.yaw; const fx = -Math.sin(a), fz = -Math.cos(a), rx = Math.cos(a), rz = -Math.sin(a);
  return _mv.set(fx * iy + rx * ix, 0, fz * iy + rz * ix);
}
function updatePlayer(dt) {
  const P = player; const pos = P.pos; const m = activeModel(); const ch = CHARS[P.active];
  if (P.pendingHit) { P.pendingHit.t -= dt; if (P.pendingHit.t <= 0) { meleeHit(P.pendingHit); P.pendingHit = null; } }

  const mv = moveVector(); const mag = Math.min(1, mv.length());
  const attacking = P.attackT > 0 && P.grounded;
  let speed = RUN * (mag > 0.85 || input.keys.ShiftLeft ? 1.15 : 0.85) * (attacking ? 0.25 : 1);
  if (P.dashT > 0) { const f = forwardVec(tmpV2); pos.x += f.x * 24 * dt; pos.z += f.z * 24 * dt; }
  else if (P.lungeT > 0) { P.lungeT -= dt; pos.addScaledVector(P.lunge, dt); }
  else if (!P.climbing && mag > 0.05) {
    const targetYaw = Math.atan2(mv.x, mv.z);
    P.yaw += angDiff(P.yaw, targetYaw) * Math.min(1, dt * 14);
    pos.x += mv.x * speed * dt; pos.z += mv.z * speed * dt;
  }
  // knockback velocity (horizontal)
  pos.x += P.vel.x * dt; pos.z += P.vel.z * dt; P.vel.x *= Math.exp(-dt * 8); P.vel.z *= Math.exp(-dt * 8);

  // vertical
  if (P.climbing) {
    P.vel.y = mag > 0.1 ? 7.5 : 0; P.gliding = false;
    pos.y += P.vel.y * dt;
    // stay against wall
    pos.x -= P.climbNormal.x * 2 * dt; pos.z -= P.climbNormal.z * 2 * dt;
  } else {
    const g = P.gliding && P.vel.y < 0 ? GRAV * 0.12 : GRAV;
    P.vel.y -= g * dt; if (P.gliding) P.vel.y = Math.max(P.vel.y, -2.6);
    pos.y += P.vel.y * dt;
    if (P.gliding) { const f = forwardVec(tmpV2); pos.x += f.x * 4 * dt * (mag > 0.1 ? 1 : 0.3); pos.z += f.z * 4 * dt * (mag > 0.1 ? 1 : 0.3); }
  }
  // collisions
  const hit = collide(pos, 0.45, pos.y);
  if (P.climbing) {
    if (!hit) { // reached the top or slipped off
      P.climbing = false; P.vel.y = 5.5; const f = forwardVec(tmpV2); pos.x += f.x * 0.6; pos.z += f.z * 0.6;
    }
  } else if (hit && mag > 0.3 && P.dashT <= 0 && (mv.x * -hit.x + mv.z * -hit.z) / (mag || 1) > 0.55 && hit.c.top - pos.y > 1.0) {
    P.climbing = true; P.climbNormal = { x: hit.x, z: hit.z }; P.yaw = Math.atan2(-hit.x, -hit.z); P.jumps = 0;
  }
  const gy = groundAt(pos.x, pos.z, pos.y);
  if (pos.y <= gy && P.vel.y <= 0) {
    if (!P.grounded && P.vel.y < -18) { burst(pos, 0xffffff, 10, 3, 0.3, 0, 0); }
    pos.y = gy; P.vel.y = 0; P.grounded = true; P.jumps = 0; P.gliding = false; P.climbing = false;
  } else if (pos.y > gy + 0.05) P.grounded = false;
  if (!input.jumpHeld && P.gliding && P.vel.y < 0 && !input.keys.Space) { /* glide toggled by press */ }

  // animation
  m.position.copy(pos); m.rotation.y = P.yaw;
  playerShadow.position.set(pos.x, gy + 0.03, pos.z); playerShadow.scale.setScalar(clamp(1 - (pos.y - gy) * 0.08, 0.3, 1));
  animateEsper(m, dt, { moving: mag > 0.05 && !attacking, speed: mag, grounded: P.grounded, climbing: P.climbing, gliding: P.gliding, swing: P.swingT, combo: P.combo, dash: P.dashT > 0, ranged: ch.ranged });
  if (P.gliding && Math.random() < 0.5) burst(tmpV.copy(pos).setY(pos.y + 1.6), ch.color, 1, 0.5, 0.5, -1, 0);
  m.visible = !(P.invuln > 0 && P.invuln < 0.45 && Math.floor(state.time * 30) % 2 === 0);
}
function animateEsper(m, dt, s) {
  const P = m.userData.P; m.userData.ph = (m.userData.ph || 0) + dt * (s.moving ? 9 + s.speed * 4 : 2);
  const ph = m.userData.ph; const t = state.time;
  let legL = 0, legR = 0, armL = 0.1, armR = 0.1, armLz = 0.12, armRz = -0.12, lean = 0, bob = 0;
  if (s.climbing) { armL = -2.6 + Math.sin(t * 10) * 0.5; armR = -2.6 - Math.sin(t * 10) * 0.5; legL = Math.sin(t * 10) * 0.6; legR = -legL; }
  else if (s.gliding) { armLz = 1.5; armRz = -1.5; legL = 0.3; legR = 0.1; lean = 0.5; }
  else if (!s.grounded) { legL = -0.9; legR = 0.3; armL = -0.6; armR = -0.4; }
  else if (s.moving) { const a = Math.sin(ph) * 0.9 * (0.5 + s.speed * 0.5); legL = a; legR = -a; armL = -a * 0.8; armR = a * 0.8; lean = 0.15 * s.speed; bob = Math.abs(Math.cos(ph)) * 0.06; }
  else { armL = 0.05 + Math.sin(t * 2) * 0.04; armR = 0.05 - Math.sin(t * 2) * 0.04; bob = Math.sin(t * 2) * 0.015; }
  if (s.dash) { lean = 0.6; legL = -0.6; legR = 0.6; armL = 0.8; armR = 0.8; }
  if (s.swing > 0) {
    const k = s.swing / 0.25;
    if (s.ranged) { armR = -1.5; armRz = -0.1; }
    else if (s.combo === 1) { armR = -0.3; armRz = -1.8 + k * 2.6; }
    else { armR = -2.6 + (1 - k) * 3.2; armRz = -0.3; }
    lean = 0.25;
  }
  const L = (v, cur) => damp(cur, v, 18, dt);
  P.legL.rotation.x = L(legL, P.legL.rotation.x); P.legR.rotation.x = L(legR, P.legR.rotation.x);
  P.armL.rotation.x = L(armL, P.armL.rotation.x); P.armR.rotation.x = L(armR, P.armR.rotation.x);
  P.armL.rotation.z = L(armLz, P.armL.rotation.z); P.armR.rotation.z = L(armRz, P.armR.rotation.z);
  m.rotation.x = L(lean, m.rotation.x); m.position.y += bob;
  P.orb.position.y = 1.95 + Math.sin(t * 3) * 0.08; P.orb.rotation.y += dt * 3;
  if (P.pony) P.pony.rotation.x = -2.5 + Math.sin(ph) * 0.15 * (s.moving ? 1 : 0.2);
}

// driving ---------------------------------------------------------------
function enterCar(car) {
  player.mode = 'drive'; player.car = car; car.traffic = false; car.speed = 0; car.boost = 0;
  car.yaw = car.mesh.rotation.y; car.pos.copy(car.mesh.position);
  activeModel().visible = false; playerShadow.visible = false; player.climbing = player.gliding = false;
  sfx('horn'); state.lastCamInput = -99;
  if (car._wasTraffic === undefined) car._wasTraffic = true;
}
function exitCar() {
  const car = player.car; player.mode = 'foot'; player.car = null;
  const side = new V3(Math.cos(car.yaw), 0, -Math.sin(car.yaw));
  player.pos.copy(car.pos).addScaledVector(side, -2.2); player.pos.y = 0;
  if (insideBuilding(player.pos.x, 0.5, player.pos.z)) player.pos.copy(car.pos).addScaledVector(side, 2.2);
  player.yaw = car.yaw; activeModel().visible = true; playerShadow.visible = true; car.speed = 0;
}
function updateDriving(dt) {
  const car = player.car; const k = input.keys;
  let ix = input.jx, iy = input.jy;
  const kx = (k.KeyD || k.ArrowRight ? 1 : 0) - (k.KeyA || k.ArrowLeft ? 1 : 0), ky = (k.KeyW || k.ArrowUp ? 1 : 0) - (k.KeyS || k.ArrowDown ? 1 : 0);
  if (kx || ky) { ix = kx; iy = ky; }
  car.boost = Math.max(0, (car.boost || 0) - dt);
  if (k.ShiftLeft) car.boost = Math.max(car.boost, 0.1);
  const maxV = car.boost > 0 ? 50 : 34;
  if (iy > 0.1) car.speed += iy * (car.boost > 0 ? 40 : 20) * dt;
  else if (iy < -0.1) car.speed += iy * (car.speed > 0 ? 40 : 14) * dt;
  else car.speed *= Math.exp(-dt * 0.8);
  car.speed = clamp(car.speed, -12, maxV);
  if (car.speed > maxV) car.speed = damp(car.speed, maxV, 3, dt);
  const steer = ix * 1.9 * clamp(car.speed / 8, -1, 1) / (1 + Math.abs(car.speed) / 45);
  car.yaw -= steer * dt;
  car.pos.x += Math.sin(car.yaw) * car.speed * dt; car.pos.z += Math.cos(car.yaw) * car.speed * dt;
  const hit = collide(car.pos, 1.7, 0);
  if (hit) { if (Math.abs(car.speed) > 10) { sfx('crash'); shake(Math.min(0.6, Math.abs(car.speed) / 60)); burst(car.pos.clone().setY(1), 0xffc070, 12, 5, 0.4); } car.speed *= -0.25; }
  for (const o of cars) {
    if (o === car) continue; const dx = o.pos.x - car.pos.x, dz = o.pos.z - car.pos.z; const d2 = dx * dx + dz * dz;
    if (d2 < 9) { const d = Math.sqrt(d2) || 1; car.pos.x -= (dx / d) * (3 - d); car.pos.z -= (dz / d) * (3 - d); if (Math.abs(car.speed) > 8) { sfx('crash'); shake(0.3); } car.speed *= 0.4; }
  }
  car.mesh.position.copy(car.pos); car.mesh.rotation.y = car.yaw;
  car.mesh.rotation.z = damp(car.mesh.rotation.z, steer * 0.04 * Math.sign(car.speed), 6, dt);
  player.pos.copy(car.pos); player.yaw = car.yaw;
  if (car.boost > 0 && Math.random() < 0.8) burst(car.pos.clone().add(tmpV.set(-Math.sin(car.yaw) * 2.3, 0.6, -Math.cos(car.yaw) * 2.3)), 0x45e6ff, 2, 2, 0.4, 0, 0);
}

// =====================================================================
// Camera
// =====================================================================
const cam = { yaw: 0, pitch: 0.32, dist: 7.5, target: new V3(), curDist: 7.5 };
function updateCamera(dt, rawDt) {
  cam.yaw -= input.camDX * 0.0055 * settings.sens; cam.pitch = clamp(cam.pitch + input.camDY * 0.0045 * settings.sens, -0.35, 1.25);
  if (input.camDX || input.camDY) state.lastCamInput = state.time;
  input.camDX = input.camDY = 0;
  let dist = 7.5, h = 1.6;
  if (player.mode === 'drive') {
    dist = 11 + Math.abs(player.car.speed) * 0.08; h = 2.2;
    if (state.time - state.lastCamInput > 1.2) { cam.yaw += angDiff(cam.yaw, player.car.yaw + Math.PI) * Math.min(1, rawDt * 2.5); cam.pitch = damp(cam.pitch, 0.22, 2, rawDt); }
  } else if (player.climbing) dist = 9;
  else if (boss) dist = 11;
  cam.target.x = damp(cam.target.x, player.pos.x, 18, rawDt); cam.target.z = damp(cam.target.z, player.pos.z, 18, rawDt);
  cam.target.y = damp(cam.target.y, player.pos.y + h, 10, rawDt);
  const cp = Math.cos(cam.pitch), sp = Math.sin(cam.pitch);
  const dir = tmpV.set(Math.sin(cam.yaw) * cp, sp, Math.cos(cam.yaw) * cp);
  // camera collision (march)
  let allowed = dist;
  for (let s = 1; s <= 12; s++) {
    const d = (dist * s) / 12; const x = cam.target.x + dir.x * d, y = cam.target.y + dir.y * d, z = cam.target.z + dir.z * d;
    if (insideBuilding(x, y, z) || y < 0.3) { allowed = Math.max(1.2, d - 0.8); break; }
  }
  cam.curDist = allowed < cam.curDist ? allowed : damp(cam.curDist, allowed, 4, rawDt);
  camera.position.copy(cam.target).addScaledVector(dir, cam.curDist);
  if (shakeT > 0) { shakeT -= rawDt; const a = shakeT * 0.6; camera.position.add(tmpV2.set(rr(-a, a), rr(-a, a), rr(-a, a))); }
  camera.lookAt(cam.target);
  const fov = player.mode === 'drive' ? 62 + Math.abs(player.car.speed) * 0.25 : player.dashT > 0 ? 68 : 62;
  camera.fov = damp(camera.fov, fov, 6, rawDt); camera.updateProjectionMatrix();
  sky.position.copy(camera.position); stars.position.copy(camera.position);
}

// =====================================================================
// Day / night
// =====================================================================
const DAY_LEN = 600; // seconds per in-game day
const cDayTop = new THREE.Color(0x3f86e0), cDayBot = new THREE.Color(0xbcd8f2), cNightTop = new THREE.Color(0x05061a), cNightBot = new THREE.Color(0x3a1650), cDusk = new THREE.Color(0xff7a4a);
const tmpC = new THREE.Color(), tmpC2 = new THREE.Color();
function sunElev() { return -Math.cos(state.dayTime * Math.PI * 2); }
function dayFactor() { return smooth(-0.12, 0.3, sunElev()); }
function updateDayNight(dt) {
  state.dayTime = (state.dayTime + dt / DAY_LEN) % 1;
  const el = sunElev(); const day = dayFactor(); const dusk = Math.max(0, 1 - Math.abs(el) / 0.28);
  skyUniforms.top.value.copy(cNightTop).lerp(cDayTop, day);
  tmpC.copy(cNightBot).lerp(cDayBot, day).lerp(cDusk, dusk * 0.6); skyUniforms.bottom.value.copy(tmpC);
  scene.fog.color.copy(tmpC).lerp(cNightTop, 0.25 * (1 - day));
  hemi.intensity = lerp(0.55, 1.25, day); hemi.color.setHex(0xbfd8ff).lerp(tmpC2.setHex(0x8070c0), 1 - day);
  hemi.groundColor.setHex(0x302838);
  const ang = state.dayTime * Math.PI * 2;
  const sx = Math.sin(ang) * 0.6, sy = Math.max(0.15, Math.abs(el)), sz = 0.5;
  sun.position.set(player.pos.x + sx * 100, sy * 100, player.pos.z + sz * 100); sun.target.position.copy(player.pos);
  sun.intensity = lerp(0.35, 2.0, day); sun.color.setHex(0xffffff).lerp(tmpC2.setHex(0x8fa0ff), 1 - day).lerp(cDusk, dusk * 0.5 * day);
  const night = 1 - day;
  for (const m of facadeMats) m.emissiveIntensity = lerp(0.08, 1.05, night);
  world.bulbMat.color.setHex(0x3a3a40).lerp(tmpC2.setHex(0xffd59a), smooth(0.2, 0.6, night));
  starMat.opacity = smooth(0.4, 0.9, night);
  moon.position.set(camera.position.x - 300, camera.position.y + 320, camera.position.z - 600); moon.material.opacity = smooth(0.3, 0.8, night);
  world.signMesh.material.color.setScalar(lerp(0.75, 1.0, night));
  // spire rings pulse
  world.spireRings.forEach((r, i) => { r.rotation.z += dt * (i % 2 ? 0.4 : -0.4); r.scale.setScalar(1 + Math.sin(state.time * 2 + i) * 0.04); });
  world.beacon.material.opacity = 0.6 + Math.sin(state.time * 3) * 0.3;
}

// =====================================================================
// Quests
// =====================================================================
const nearestOpenRift = () => { let b = null, bd = 1e9; for (const r of rifts) if (!state.riftsClosed.has(r.idx)) { const d = r.pos.distanceToSquared(player.pos); if (d < bd) { bd = d; b = r; } } return b; };
const nearestShard = () => { let b = null, bd = 1e9; shardPositions.forEach((p, i) => { if (state.shardsGot.has(i)) return; const d = (p[0] - player.pos.x) ** 2 + (p[2] - player.pos.z) ** 2; if (d < bd) { bd = d; b = new V3(p[0], p[1], p[2]); } }); return b; };
const QUESTS = [
  { title: 'A Strange Signal', desc: () => 'Meet Iris at Nocturne Curios', target: () => IRIS_POS, done: () => state.metIris },
  { title: 'Echo Hunt', desc: () => `Collect Echo Shards (${Math.min(3, state.shardsGot.size)}/3)`, target: nearestShard, done: () => state.shardsGot.size >= 3, reward: 150 },
  { title: 'First Contact', desc: () => 'Seal an Anomaly Rift', target: () => nearestOpenRift()?.pos, done: () => state.riftsClosed.size >= 1, reward: 200 },
  { title: 'Night Ride', desc: () => 'Get in a car and drive to the Nexus Spire', target: () => SPIRE_POS, done: () => player.mode === 'drive' && Math.hypot(player.pos.x, player.pos.z) < 45, reward: 200 },
  { title: 'Seal the City', desc: () => `Seal every Anomaly Rift (${state.riftsClosed.size}/${rifts.length})`, target: () => nearestOpenRift()?.pos, done: () => state.riftsClosed.size >= rifts.length, reward: 500 },
  { title: 'The Hollow Clock', desc: () => (state.bossActive ? 'Defeat the Hollow Clock' : 'Approach the base of the Nexus Spire'), target: () => SPIRE_POS, done: () => state.bossDefeated },
  { title: 'Free Roam', desc: () => `Find all Echo Shards (${state.shardsGot.size}/${shardPositions.length})`, target: nearestShard, done: () => false },
];
function updateQuests() {
  const q = QUESTS[state.questIdx];
  if (q.done()) {
    if (q.reward) { state.credits += q.reward; }
    toast(`Quest complete: ${q.title}${q.reward ? `  +${q.reward}◆` : ''}`, true); sfx('win');
    state.questIdx = Math.min(QUESTS.length - 1, state.questIdx + 1);
    const nq = QUESTS[state.questIdx]; later(1.2, () => toast(`New quest: ${nq.title}`));
    if (state.questIdx === 3) later(2, () => toast('Tip: walk up to any car and tap DRIVE'));
    save();
  }
  if (state.questIdx === 5 && !state.bossActive && !state.bossDefeated && Math.hypot(player.pos.x, player.pos.z) < 48) startBoss();
}

// =====================================================================
// Dialog
// =====================================================================
let dlgQueue = [], dlgChoiceActive = false;
const dialogOpen = () => !$('dialog').classList.contains('hidden');
function openDialog(lines) { dlgQueue = lines.slice(); $('dialog').classList.remove('hidden'); nextLine(); }
function nextLine() {
  if (dlgChoiceActive) return;
  const l = dlgQueue.shift();
  if (!l) { $('dialog').classList.add('hidden'); return; }
  $('dlgName').textContent = l.name; $('dlgText').textContent = l.text;
  const box = $('dlgChoices'); box.innerHTML = '';
  if (l.choices) {
    dlgChoiceActive = true; $('dlgHint').style.visibility = 'hidden';
    l.choices.forEach(([label, fn]) => {
      const b = document.createElement('button'); b.className = 'ui-btn'; b.textContent = label;
      b.addEventListener('pointerdown', (ev) => { ev.stopPropagation(); dlgChoiceActive = false; sfx('ui'); fn(); });
      box.appendChild(b);
    });
  } else { dlgChoiceActive = false; $('dlgHint').style.visibility = 'visible'; }
  if (l.onShow) l.onShow();
}
$('dialog').addEventListener('pointerdown', (e) => { e.stopPropagation(); if (!dlgChoiceActive) { sfx('ui'); nextLine(); } });
const upgradeCost = () => 200 + (state.level - 1) * 250;
function talkIris() {
  if (!state.metIris) {
    openDialog([
      { name: 'Iris', text: 'Ah — so you\'re the ones the Spire keeps whispering about. Welcome to Nocturne Curios.' },
      { name: 'Iris', text: 'Ever since the Nexus Spire lit up, the city\'s been fraying at the seams. Things slip through — Anomalies. Glitches in reality with teeth.' },
      { name: 'Iris', text: 'Your team resonates with them. Rei burns them, Mika shocks them, Sora crushes them. Swap between them mid-fight — it keeps the Anomalies off-balance.' },
      { name: 'Iris', text: 'Start small. Echo Shards are scattered across the streets and rooftops — you can climb any building, by the way. Bring me three and we\'ll talk rifts.' },
      { name: 'System', text: 'Tip: Push into a wall to climb. Jump twice to double-jump, press jump again in mid-air to glide.', onShow: () => { state.metIris = true; save(); } },
    ]);
    return;
  }
  const cost = upgradeCost();
  const lines = [{ name: 'Iris', text: pick(['The rifts hum louder at night. Be careful out there.', 'Resonance grows with Echoes. I can tune your team… for a price.', 'Fancy a tea? No? Business, then.', 'I heard someone saw an Echo Shard glinting on top of the Spire. Madness.']) }];
  lines.push({
    name: 'Iris', text: `Team Resonance Lv.${state.level}. Raising it to Lv.${state.level + 1} costs ${cost}◆ (you have ${state.credits}◆). Higher resonance = more damage and toughness.`,
    choices: [
      [`Upgrade (${cost}◆)`, () => { if (state.credits >= cost) { state.credits -= cost; state.level++; sfx('win'); burst(chestPos(), 0xffd36b, 40, 6, 0.8); toast(`Resonance Lv.${state.level}!`, true); save(); } else toast('Not enough credits'); nextLine(); }],
      ['Heal team', () => { player.hp = [100, 100, 100]; toast('Team fully restored'); nextLine(); }],
      ['Leave', () => nextLine()],
    ],
  });
  openDialog(lines);
}

// =====================================================================
// Interaction
// =====================================================================
let interactTarget = null;
function findInteract() {
  if (player.mode === 'drive') return { label: 'Exit', fn: exitCar };
  if (player.pos.distanceTo(IRIS_POS) < 4.5) return { label: 'Talk', fn: talkIris };
  let best = null, bd = 4.2 * 4.2;
  for (const c of cars) { const d = c.mesh.position.distanceToSquared(player.pos); if (d < bd) { bd = d; best = c; } }
  if (best && player.grounded) return { label: 'Drive', fn: () => enterCar(best) };
  return null;
}
function interactPress() { if (state.mode !== 'play' || state.paused || dialogOpen()) return; if (interactTarget) { sfx('ui'); interactTarget.fn(); } }

// =====================================================================
// HUD
// =====================================================================
function toast(msg, big) {
  const el = document.createElement('div'); el.className = 'toast-item' + (big ? ' big' : ''); el.textContent = msg;
  const box = $('toast'); box.appendChild(el); while (box.children.length > 4) box.firstChild.remove();
  setTimeout(() => el.remove(), big ? 3200 : 2400);
}
const dmgPool = []; for (let i = 0; i < 28; i++) { const el = document.createElement('div'); el.className = 'dmg'; el.style.display = 'none'; $('dmgLayer').appendChild(el); dmgPool.push({ el, pos: new V3(), life: 0 }); }
let dmgHead = 0;
function showDamage(pos, n, crit, color, self) {
  const d = dmgPool[dmgHead]; dmgHead = (dmgHead + 1) % dmgPool.length;
  d.pos.copy(pos).add(tmpV2.set(rr(-0.5, 0.5), 1 + rr(0, 0.5), rr(-0.5, 0.5))); d.life = 0.9;
  d.el.textContent = self ? `-${n}` : n; d.el.className = 'dmg' + (crit ? ' crit' : ''); d.el.style.color = '#' + new THREE.Color(color).getHexString(); d.el.style.display = 'block';
}
function updateDamageNumbers(dt) {
  for (const d of dmgPool) {
    if (d.life <= 0) continue; d.life -= dt; d.pos.y += dt * 1.6;
    if (d.life <= 0) { d.el.style.display = 'none'; continue; }
    tmpV.copy(d.pos).project(camera);
    if (tmpV.z > 1) { d.el.style.display = 'none'; continue; }
    d.el.style.display = 'block';
    const x = (tmpV.x * 0.5 + 0.5) * innerWidth, y = (-tmpV.y * 0.5 + 0.5) * innerHeight;
    d.el.style.transform = `translate(${x}px, ${y}px) translate(-50%, -50%) scale(${0.8 + Math.min(1, d.life * 3) * 0.3})`; d.el.style.opacity = Math.min(1, d.life * 2.5);
  }
}
function buildParty() {
  const box = $('party'); box.innerHTML = '';
  CHARS.forEach((c, i) => {
    const b = document.createElement('button'); b.className = 'member' + (i === player.active ? ' active' : ''); b.style.setProperty('--c', c.css);
    b.innerHTML = `<div class="dot"></div><span class="lbl">${c.name[0]}</span><div class="mhp"><i></i></div><div class="swapcd"></div>`;
    b.addEventListener('pointerdown', (e) => { e.stopPropagation(); initAudio(); swapTo(i); });
    box.appendChild(b);
  });
}
function updateUltColor() { $('btnUlt').style.setProperty('--ultc', CHARS[player.active].css); }
let hudT = 0;
function updateHUD(dt) {
  hudT -= dt; const a = player.active; const ch = CHARS[a];
  // fast-changing bits
  $('btnSkill').querySelector('.cd').style.setProperty('--p', `${(player.skillCd[a] / [6, 7, 9][a]) * 100}%`);
  $('btnUlt').querySelector('.fill').style.setProperty('--p', `${player.energy[a]}%`);
  $('btnUlt').classList.toggle('ready', player.energy[a] >= 100);
  updateWaypoint();
  if (hudT > 0) return; hudT = 0.12;
  $('charName').textContent = `${ch.name} · ${ch.element}  ·  Resonance Lv.${state.level}`;
  $('hpFill').style.width = `${(player.hp[a] / player.maxHp) * 100}%`; $('hpText').textContent = `${Math.ceil(player.hp[a])}/${player.maxHp}`;
  const ms = $('party').children;
  for (let i = 0; i < ms.length; i++) {
    ms[i].classList.toggle('active', i === a); ms[i].classList.toggle('dead', player.hp[i] <= 0);
    ms[i].querySelector('.mhp i').style.width = `${(player.hp[i] / player.maxHp) * 100}%`;
    ms[i].querySelector('.swapcd').style.setProperty('--p', `${Math.max(0, player.swapCd / 1.2) * 100}%`);
  }
  $('credits').textContent = state.credits; $('shards').textContent = state.shardsGot.size; $('shardTotal').textContent = shardPositions.length;
  const mins = Math.floor(state.dayTime * 24 * 60); $('clock').textContent = `${String(Math.floor(mins / 60)).padStart(2, '0')}:${String(mins % 60).padStart(2, '0')} ${dayFactor() > 0.5 ? '☀' : '☾'}`;
  const q = QUESTS[state.questIdx]; $('questTitle').textContent = q.title; $('questDesc').textContent = q.desc();
  const tgt = q.target(); $('questDist').textContent = tgt ? `${Math.round(Math.hypot(tgt.x - player.pos.x, tgt.z - player.pos.z))} m` : '';
  interactTarget = findInteract();
  const ib = $('btnInteract'); ib.classList.toggle('hidden', !interactTarget); if (interactTarget) ib.textContent = interactTarget.label;
  $('btnAttack').textContent = player.mode === 'drive' ? '📯' : '⚔';
  $('btnDash').textContent = player.mode === 'drive' ? '⚡' : '»';
  $('btnSkill').style.opacity = $('btnUlt').style.opacity = player.mode === 'drive' ? 0.35 : 1;
}
function updateWaypoint() {
  const q = QUESTS[state.questIdx]; const tgt = q.target(); const el = $('waypoint');
  if (!tgt || state.mode !== 'play') { el.classList.add('hidden'); return; }
  const d = Math.hypot(tgt.x - player.pos.x, tgt.z - player.pos.z);
  if (d < 6) { el.classList.add('hidden'); return; }
  el.classList.remove('hidden');
  tmpV.set(tgt.x, (tgt.y || 0) + 3, tgt.z).project(camera);
  let x = tmpV.x, y = tmpV.y; const behind = tmpV.z > 1;
  if (behind) { x = -x; y = -y; }
  const margin = 0.88; let onScreen = !behind && Math.abs(x) < margin && Math.abs(y) < margin;
  if (!onScreen) { const m = Math.max(Math.abs(x), Math.abs(y)) || 1; x = (x / m) * margin; y = (y / m) * margin; }
  const sx = (x * 0.5 + 0.5) * innerWidth, sy = (-y * 0.5 + 0.5) * innerHeight;
  el.style.left = sx + 'px'; el.style.top = sy + 'px';
  el.querySelector('.arrow').style.transform = onScreen ? 'rotate(180deg)' : `rotate(${Math.atan2(x, y)}rad)`;
  $('wpDist').textContent = `${Math.round(d)}m`;
}

// minimap -------------------------------------------------------------------
const MAPS = GRID * CELL + ROAD; // 664
const mapCanvas = document.createElement('canvas'); mapCanvas.width = mapCanvas.height = MAPS;
{
  const g = mapCanvas.getContext('2d');
  g.fillStyle = '#0d1a2e'; g.fillRect(0, 0, MAPS, MAPS);
  g.fillStyle = '#2a2833'; g.fillRect(0, 0, MAPS, MAPS);
  const o = MAPS / 2;
  for (let i = 0; i < GRID; i++) for (let j = 0; j < GRID; j++) {
    const t = blockType(i, j); g.fillStyle = t === 'park' ? '#25462f' : t === 'plaza' ? '#2c3f35' : '#4a4856';
    g.fillRect(blockMin(i) + o, blockMin(j) + o, BLOCK, BLOCK);
  }
  g.fillStyle = '#6f6a85'; for (const b of buildings) g.fillRect(b.x0 + o, b.z0 + o, b.x1 - b.x0, b.z1 - b.z0);
  g.fillStyle = '#b47bff'; g.beginPath(); g.arc(o, o, 9, 0, Math.PI * 2); g.fill();
}
const mm = $('minimap').getContext('2d'); const bm = $('bigmap').getContext('2d');
function drawMarkers(g, toX, toY, scale, big) {
  const dot = (x, z, r, c) => { g.fillStyle = c; g.beginPath(); g.arc(toX(x), toY(z), r, 0, Math.PI * 2); g.fill(); };
  if (big) shardPositions.forEach((p, i) => { if (!state.shardsGot.has(i)) dot(p[0], p[2], 2.5, '#45e6ff'); });
  for (const r of rifts) dot(r.pos.x, r.pos.z, big ? 7 : 5, state.riftsClosed.has(r.idx) ? '#5dff9a55' : '#c04bff');
  dot(IRIS_POS.x, IRIS_POS.z, big ? 6 : 4, '#5dff9a');
  for (const e of enemies) dot(e.pos.x, e.pos.z, e.type === 'boss' ? 6 : 2.5, '#ff4d6d');
  const t = QUESTS[state.questIdx].target(); if (t) { g.strokeStyle = '#ffd36b'; g.lineWidth = 2; g.beginPath(); g.arc(toX(t.x), toY(t.z), big ? 9 : 6, 0, Math.PI * 2); g.stroke(); }
  // player arrow
  g.save(); g.translate(toX(player.pos.x), toY(player.pos.z)); g.rotate(-player.yaw + Math.PI);
  g.fillStyle = '#fff'; g.beginPath(); g.moveTo(0, -7); g.lineTo(5, 5); g.lineTo(0, 2); g.lineTo(-5, 5); g.closePath(); g.fill(); g.restore();
}
function drawMinimap() {
  const S = 140, R = 90; // world units visible radius
  const sc = S / (R * 2);
  mm.save(); mm.clearRect(0, 0, S, S); mm.beginPath(); mm.arc(S / 2, S / 2, S / 2, 0, Math.PI * 2); mm.clip();
  mm.fillStyle = '#0d1a2e'; mm.fillRect(0, 0, S, S);
  mm.drawImage(mapCanvas, player.pos.x + MAPS / 2 - R, player.pos.z + MAPS / 2 - R, R * 2, R * 2, 0, 0, S, S);
  const toX = (x) => (x - player.pos.x) * sc + S / 2, toY = (z) => (z - player.pos.z) * sc + S / 2;
  drawMarkers(mm, toX, toY, sc, false);
  mm.restore();
  // edge indicator for quest target
  const t = QUESTS[state.questIdx].target();
  if (t) { const dx = t.x - player.pos.x, dz = t.z - player.pos.z; const d = Math.hypot(dx, dz); if (d * sc > S / 2 - 6) { mm.fillStyle = '#ffd36b'; mm.beginPath(); mm.arc(S / 2 + (dx / d) * (S / 2 - 7), S / 2 + (dz / d) * (S / 2 - 7), 4, 0, Math.PI * 2); mm.fill(); } }
}
function drawBigMap() {
  bm.clearRect(0, 0, MAPS, MAPS); bm.drawImage(mapCanvas, 0, 0);
  drawMarkers(bm, (x) => x + MAPS / 2, (z) => z + MAPS / 2, 1, true);
}

// =====================================================================
// Input
// =====================================================================
const input = { jx: 0, jy: 0, keys: {}, camDX: 0, camDY: 0, jumpHeld: false };
const pointers = new Map(); let joyId = null;
const tl = $('touchLayer'); const joyBase = $('joyBase'), joyKnob = $('joyKnob');
tl.addEventListener('pointerdown', (e) => {
  initAudio();
  if (state.mode !== 'play' || state.paused) return;
  if (dialogOpen()) { if (!dlgChoiceActive) nextLine(); return; }
  tl.setPointerCapture(e.pointerId);
  if (e.pointerType !== 'mouse' && e.clientX < innerWidth * 0.42 && joyId === null) {
    joyId = e.pointerId; pointers.set(e.pointerId, { type: 'joy', sx: e.clientX, sy: e.clientY });
    joyBase.style.display = 'block'; joyBase.style.left = e.clientX + 'px'; joyBase.style.top = e.clientY + 'px'; joyKnob.style.transform = '';
  } else pointers.set(e.pointerId, { type: 'cam', x: e.clientX, y: e.clientY, sx: e.clientX, sy: e.clientY, t: performance.now(), mouse: e.pointerType === 'mouse' });
});
tl.addEventListener('pointermove', (e) => {
  const p = pointers.get(e.pointerId); if (!p) return;
  if (p.type === 'joy') {
    let dx = e.clientX - p.sx, dy = e.clientY - p.sy; const R = 52; const l = Math.hypot(dx, dy);
    if (l > R) { // drag base along (floating joystick)
      p.sx += (dx / l) * (l - R); p.sy += (dy / l) * (l - R); dx = (dx / l) * R; dy = (dy / l) * R;
      joyBase.style.left = p.sx + 'px'; joyBase.style.top = p.sy + 'px';
    }
    input.jx = dx / R; input.jy = -dy / R; joyKnob.style.transform = `translate(${dx}px, ${dy}px)`;
  } else { input.camDX += e.clientX - p.x; input.camDY += e.clientY - p.y; p.x = e.clientX; p.y = e.clientY; }
});
const endPointer = (e) => {
  const p = pointers.get(e.pointerId); if (!p) return; pointers.delete(e.pointerId);
  if (p.type === 'joy') { joyId = null; input.jx = input.jy = 0; joyBase.style.display = 'none'; }
  else if (p.mouse && e.button === 0 && Math.hypot(e.clientX - p.sx, e.clientY - p.sy) < 6 && performance.now() - p.t < 350) doAttack();
};
tl.addEventListener('pointerup', endPointer); tl.addEventListener('pointercancel', endPointer);
tl.addEventListener('contextmenu', (e) => e.preventDefault());
document.addEventListener('gesturestart', (e) => e.preventDefault());

function bindBtn(id, down, up) {
  const el = $(id);
  el.addEventListener('pointerdown', (e) => { e.preventDefault(); e.stopPropagation(); initAudio(); el.classList.add('pressed'); down(); });
  const rel = (e) => { el.classList.remove('pressed'); if (up) up(); };
  el.addEventListener('pointerup', rel); el.addEventListener('pointercancel', rel); el.addEventListener('pointerleave', rel);
}
let attackHeld = false;
bindBtn('btnAttack', () => { attackHeld = true; doAttack(); }, () => (attackHeld = false));
bindBtn('btnSkill', doSkill); bindBtn('btnUlt', doUlt);
bindBtn('btnJump', () => { input.jumpHeld = true; jumpPress(); }, () => (input.jumpHeld = false));
bindBtn('btnDash', dashPress); bindBtn('btnInteract', interactPress);
$('menuBtn').addEventListener('pointerdown', (e) => { e.stopPropagation(); setPaused(true); });
$('minimap').addEventListener('pointerdown', (e) => { e.stopPropagation(); openMap(); });

addEventListener('keydown', (e) => {
  if (e.repeat) { input.keys[e.code] = true; return; }
  input.keys[e.code] = true; initAudio();
  if (state.mode !== 'play') return;
  if (e.code === 'Escape') { if (!$('mapOverlay').classList.contains('hidden')) closeMap(); else setPaused(!state.paused); return; }
  if (state.paused) return;
  if (dialogOpen()) { if (['Space', 'Enter', 'KeyF'].includes(e.code) && !dlgChoiceActive) nextLine(); return; }
  switch (e.code) {
    case 'Space': input.jumpHeld = true; jumpPress(); e.preventDefault(); break;
    case 'KeyJ': attackHeld = true; doAttack(); break;
    case 'KeyE': doSkill(); break;
    case 'KeyQ': doUlt(); break;
    case 'KeyF': interactPress(); break;
    case 'ShiftLeft': case 'ShiftRight': dashPress(); break;
    case 'Digit1': swapTo(0); break; case 'Digit2': swapTo(1); break; case 'Digit3': swapTo(2); break;
    case 'KeyM': openMap(); break;
  }
});
addEventListener('keyup', (e) => { input.keys[e.code] = false; if (e.code === 'Space') input.jumpHeld = false; if (e.code === 'KeyJ') attackHeld = false; });
addEventListener('blur', () => { input.keys = {}; input.jx = input.jy = 0; attackHeld = false; joyId = null; pointers.clear(); joyBase.style.display = 'none'; });

// =====================================================================
// Menus
// =====================================================================
function setPaused(p) {
  state.paused = p; $('pause').classList.toggle('hidden', !p);
  if (p) { $('optQuality').value = settings.quality; $('optSens').value = settings.sens; $('optSound').checked = settings.sound; save(); }
}
$('btnResume').addEventListener('click', () => setPaused(false));
$('btnMap').addEventListener('click', () => { setPaused(false); openMap(); });
$('btnReset').addEventListener('click', () => { if (confirm('Erase all progress and restart?')) { removeKey(SAVE_KEY); location.reload(); } });
$('optQuality').addEventListener('change', (e) => { settings.quality = e.target.value; saveJSON(SET_KEY, settings); applyQuality(); });
$('optSens').addEventListener('input', (e) => { settings.sens = parseFloat(e.target.value); saveJSON(SET_KEY, settings); });
$('optSound').addEventListener('change', (e) => { settings.sound = e.target.checked; saveJSON(SET_KEY, settings); });
function openMap() { if (state.mode !== 'play') return; state.paused = true; drawBigMap(); $('mapOverlay').classList.remove('hidden'); }
function closeMap() { state.paused = false; $('mapOverlay').classList.add('hidden'); }
$('mapClose').addEventListener('click', closeMap);
$('pause').addEventListener('pointerdown', (e) => e.stopPropagation());

// =====================================================================
// Save / load
// =====================================================================
function save() {
  if (state.mode !== 'play') return;
  saveJSON(SAVE_KEY, {
    v: 1, credits: state.credits, level: state.level, shards: [...state.shardsGot], rifts: [...state.riftsClosed], questIdx: state.questIdx,
    metIris: state.metIris, bossDefeated: state.bossDefeated, dayTime: state.dayTime,
    pos: player.mode === 'foot' && player.grounded ? [player.pos.x, player.pos.y, player.pos.z] : null, energy: player.energy, active: player.active,
  });
}
function load(s) {
  state.credits = s.credits || 0; state.level = s.level || 1; state.shardsGot = new Set(s.shards || []); state.riftsClosed = new Set(s.rifts || []);
  state.questIdx = clamp(s.questIdx || 0, 0, QUESTS.length - 1); state.metIris = !!s.metIris; state.bossDefeated = !!s.bossDefeated;
  if (typeof s.dayTime === 'number') state.dayTime = s.dayTime;
  if (s.pos) player.pos.set(s.pos[0], s.pos[1], s.pos[2]);
  if (s.energy) player.energy = s.energy.slice(0, 3);
  if (typeof s.active === 'number') { models[player.active].visible = false; player.active = s.active; models[s.active].visible = true; }
}
const existing = loadJSON(SAVE_KEY);
if (existing) $('btnContinue').classList.remove('hidden');

function startGame(cont) {
  initAudio();
  if (cont && existing) load(existing); else removeKey(SAVE_KEY);
  state.mode = 'play'; $('title').classList.add('hidden'); $('hud').classList.remove('hidden');
  cam.yaw = Math.PI / 2; cam.pitch = 0.3; cam.target.copy(player.pos);
  buildParty(); updateUltColor();
  try { if (isTouch && document.documentElement.requestFullscreen) document.documentElement.requestFullscreen().then(() => screen.orientation?.lock?.('landscape').catch(() => {})).catch(() => {}); } catch (e) { /* not supported */ }
  if (!cont || !existing) later(0.8, () => toast('Nexus City · 20:00 — something is wrong with the Spire…', true));
  else toast('Welcome back, hunter');
  onResize();
}
$('btnNew').addEventListener('click', () => startGame(false));
$('btnContinue').addEventListener('click', () => startGame(true));
$('loadMsg').textContent = isTouch ? 'Left side: move · Right side: look · Buttons: fight' : 'WASD move · drag to look · click to attack';

// =====================================================================
// Main loop
// =====================================================================
const clock = new THREE.Clock();
function update(dt) {
  state.time += dt;
  updateDayNight(dt);
  tickPlayerTimers(dt);
  if (player.mode === 'drive') updateDriving(dt); else updatePlayer(dt);
  if (attackHeld && player.mode === 'foot') doAttack();
  updateTraffic(dt); updatePeds(dt);
  updateEnemies(dt); updateRifts(dt); updateRoaming(dt);
  updateProjectiles(dt); updateEffects(dt); updateShards(dt);
  iris.rotation.y = Math.atan2(player.pos.x - IRIS_POS.x, player.pos.z - IRIS_POS.z) * (player.pos.distanceTo(IRIS_POS) < 12 ? 1 : 0);
  animateEsper(iris, dt, { moving: false, grounded: true, swing: 0 }); iris.position.y = IRIS_POS.y;
  irisMarker.visible = state.questIdx === 0 || player.pos.distanceTo(IRIS_POS) < 25; irisMarker.position.y = 2.6 + Math.sin(state.time * 3) * 0.1;
  updateQuests();
  state.saveT += dt; if (state.saveT > 15) { state.saveT = 0; save(); }
}
let frameCount = 0;
function loop() {
  requestAnimationFrame(loop);
  const raw = Math.min(0.05, clock.getDelta());
  if (state.mode === 'play' && !state.paused && !dialogOpen()) {
    if (state.slowT > 0) { state.slowT -= raw; if (state.slowT <= 0) state.timeScale = 1; }
    let dt = raw * state.timeScale;
    if (hitStopT > 0) { hitStopT -= raw; dt *= 0.1; }
    update(dt);
    updateParticles(dt);
    updateDamageNumbers(raw);
  } else if (state.mode === 'title') {
    state.time += raw; state.dayTime = 0.833;
    updateDayNight(0); updateTraffic(raw); updatePeds(raw); updateParticles(raw);
    cam.yaw += raw * 0.05; cam.pitch = 0.18;
  }
  if (state.mode === 'title') {
    const a = state.time * 0.06; camera.position.set(Math.sin(a) * 150, 55, Math.cos(a) * 150); camera.lookAt(0, 45, 0);
    sky.position.copy(camera.position); stars.position.copy(camera.position);
  } else updateCamera(raw, raw);
  heroLight.position.set(player.pos.x, player.pos.y + 4, player.pos.z + 1);
  heroLight.intensity = lerp(45, 6, dayFactor());
  if (state.mode === 'play') { updateHUD(raw); if (++frameCount % 3 === 0) drawMinimap(); }
  renderer.render(scene, camera);
}
// title-screen camera: orbit the spire
player.pos.copy(SPAWN_POS); cam.target.copy(player.pos);
onResize();
loop();
window.__nexus = { state, player, enemies, cars, startGame, rifts, spawnEnemy };
