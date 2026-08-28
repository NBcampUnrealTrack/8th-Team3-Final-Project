/**
 * 디졸브 마스크 텍스처 2장을 PNG 로 굽는다.
 *
 *   node gen_dissolve_textures.mjs
 *
 * 결과는 같은 폴더의 _gen/ 에 떨어지고, build_dissolve_material.py 가 임포트한다.
 * 외부 의존성 없이 zlib 만 쓴다 — 팀원이 npm install 없이 그대로 돌릴 수 있어야 한다.
 */
import { deflateSync } from "node:zlib";
import { mkdirSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const SIZE = 512;
const OUT_DIR = join(dirname(fileURLToPath(import.meta.url)), "_gen");

// ── PNG (8bit 그레이스케일) ────────────────────────────────────────────

const CRC_TABLE = (() => {
  const t = new Int32Array(256);
  for (let n = 0; n < 256; n++) {
    let c = n;
    for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1;
    t[n] = c;
  }
  return t;
})();

function crc32(buf) {
  let c = -1;
  for (let i = 0; i < buf.length; i++) c = CRC_TABLE[(c ^ buf[i]) & 0xff] ^ (c >>> 8);
  return (c ^ -1) >>> 0;
}

function chunk(type, data) {
  const len = Buffer.alloc(4);
  len.writeUInt32BE(data.length);
  const body = Buffer.concat([Buffer.from(type, "ascii"), data]);
  const crc = Buffer.alloc(4);
  crc.writeUInt32BE(crc32(body));
  return Buffer.concat([len, body, crc]);
}

function writeGrayPng(path, pixels, size) {
  const ihdr = Buffer.alloc(13);
  ihdr.writeUInt32BE(size, 0);
  ihdr.writeUInt32BE(size, 4);
  ihdr[8] = 8;    // bit depth
  ihdr[9] = 0;    // color type: grayscale
  ihdr[10] = 0;   // compression
  ihdr[11] = 0;   // filter
  ihdr[12] = 0;   // interlace

  //	스캔라인마다 필터 바이트(0 = None)가 앞에 붙는다.
  const raw = Buffer.alloc((size + 1) * size);
  for (let y = 0; y < size; y++) {
    raw[y * (size + 1)] = 0;
    pixels.copy
      ? pixels.copy(raw, y * (size + 1) + 1, y * size, (y + 1) * size)
      : raw.set(pixels.subarray(y * size, (y + 1) * size), y * (size + 1) + 1);
  }

  writeFileSync(
    path,
    Buffer.concat([
      Buffer.from([0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a]),
      chunk("IHDR", ihdr),
      chunk("IDAT", deflateSync(raw, { level: 9 })),
      chunk("IEND", Buffer.alloc(0)),
    ]),
  );
}

// ── 노이즈 ────────────────────────────────────────────────────────────

//	고정 시드. 같은 스크립트가 항상 같은 그림을 내야 텍스처를 다시 구울 때
//	연출 튜닝이 흔들리지 않는다.
let seed = 0x9e3779b9;
function rand() {
  seed ^= seed << 13;
  seed ^= seed >>> 17;
  seed ^= seed << 5;
  return ((seed >>> 0) % 100000) / 100000;
}

const smooth = (t) => t * t * (3 - 2 * t);
const lerp = (a, b, t) => a + (b - a) * t;

/** 격자에 난수를 깔고 보간한다. 격자가 이어지도록 wrap 해서 타일링이 된다. */
function valueNoise(cells) {
  const g = new Float32Array(cells * cells);
  for (let i = 0; i < g.length; i++) g[i] = rand();

  return (x, y) => {
    const fx = x * cells;
    const fy = y * cells;
    const x0 = Math.floor(fx) % cells;
    const y0 = Math.floor(fy) % cells;
    const x1 = (x0 + 1) % cells;
    const y1 = (y0 + 1) % cells;
    const tx = smooth(fx - Math.floor(fx));
    const ty = smooth(fy - Math.floor(fy));

    return lerp(
      lerp(g[y0 * cells + x0], g[y0 * cells + x1], tx),
      lerp(g[y1 * cells + x0], g[y1 * cells + x1], tx),
      ty,
    );
  };
}

/** 옥타브를 겹쳐 잔불처럼 얼룩덜룩하게. */
function fbm(x, y, layers) {
  let sum = 0;
  let amp = 1;
  let total = 0;
  for (const n of layers) {
    sum += n(x, y) * amp;
    total += amp;
    amp *= 0.5;
  }
  return sum / total;
}

function normalize(values) {
  let lo = Infinity;
  let hi = -Infinity;
  for (const v of values) {
    if (v < lo) lo = v;
    if (v > hi) hi = v;
  }
  const span = hi - lo || 1;
  const out = Buffer.alloc(values.length);
  for (let i = 0; i < values.length; i++) {
    out[i] = Math.round(((values[i] - lo) / span) * 255);
  }
  return out;
}

// ── 1. 재 · 잔불 — 불규칙하고 대비가 세다 ─────────────────────────────

function makeEmber(size) {
  const layers = [valueNoise(6), valueNoise(13), valueNoise(29), valueNoise(61)];
  const values = new Float32Array(size * size);

  for (let y = 0; y < size; y++) {
    for (let x = 0; x < size; x++) {
      const v = fbm(x / size, y / size, layers);
      //	대비를 세게. 타는 자국은 얼룩이 또렷해야 재처럼 보인다.
      values[y * size + x] = Math.pow(v, 1.8);
    }
  }
  return normalize(values);
}

// ── 2. 부드러운 Voronoi — 고르게 스러진다 ─────────────────────────────

function makeSoft(size) {
  const COUNT = 28;
  const pts = [];
  for (let i = 0; i < COUNT; i++) pts.push([rand(), rand()]);

  const values = new Float32Array(size * size);
  for (let y = 0; y < size; y++) {
    for (let x = 0; x < size; x++) {
      const u = x / size;
      const v = y / size;

      let best = Infinity;
      for (const [px, py] of pts) {
        //	좌우 · 상하로 감싸서 이음매가 안 보이게 한다.
        for (let ox = -1; ox <= 1; ox++) {
          for (let oy = -1; oy <= 1; oy++) {
            const dx = u - (px + ox);
            const dy = v - (py + oy);
            const d = dx * dx + dy * dy;
            if (d < best) best = d;
          }
        }
      }
      //	거리의 제곱근을 완만하게 눌러 셀 경계가 날카롭지 않게.
      values[y * size + x] = Math.pow(Math.sqrt(best), 0.7);
    }
  }
  return normalize(values);
}

// ── 3. 칼자국 — 휘두른 궤적 ───────────────────────────────────────────
//
//	⚠️ 밝은 선을 그리는 것이 아니다. **칼자국까지의 거리**를 굽는다 —
//	   벤 자리가 0, 멀어질수록 1. DissolveAmount 가 커지면 그 선에서부터
//	   바깥으로 벌어지며 사라진다. 그것이 "베였다" 로 읽히는 이유다.
//
//	직선으로 그으면 종이를 자른 것처럼 보인다. 칼은 호를 그리므로 베지에를 쓴다.

/** 2차 베지에 위의 점. */
function bezier(p0, p1, p2, t) {
  const u = 1 - t;
  return [
    u * u * p0[0] + 2 * u * t * p1[0] + t * t * p2[0],
    u * u * p0[1] + 2 * u * t * p1[1] + t * t * p2[1],
  ];
}

//	벤 선에서 이만큼 떨어지면 완전히 흰색. 작을수록 칼자국이 가늘고 또렷하다.
const SLASH_BAND = 0.30;

function makeSlash(size) {
  //	본선 하나에 곁줄기를 붙이면 한 번 그은 것이 아니라 날이 스치며 갈라진 것으로 보인다.
  const strokes = [
    { p0: [0.10, 0.92], p1: [0.40, 0.42], p2: [0.92, 0.10], weight: 1.00 },
    { p0: [0.28, 0.84], p1: [0.44, 0.34], p2: [0.88, 0.18], weight: 0.55 },
    { p0: [0.34, 0.74], p1: [0.60, 0.58], p2: [0.84, 0.44], weight: 0.40 },
  ];

  //	곡선을 점으로 펴 둔다. 픽셀마다 다시 계산하면 512² × 획수만큼 베지에를 풀어야 한다.
  const SAMPLES = 260;
  const polylines = strokes.map(({ p0, p1, p2, weight }) => {
    const pts = [];
    for (let i = 0; i <= SAMPLES; i++) {
      const t = i / SAMPLES;
      const [x, y] = bezier(p0, p1, p2, t);
      //	양끝이 얇아야 휘두른 자국처럼 보인다. 얇을수록 거리를 크게 쳐서 빨리 희어진다.
      const taper = Math.pow(Math.sin(Math.PI * t), 0.45) * weight;
      pts.push([x, y, Math.max(taper, 0.08)]);
    }
    return pts;
  });

  const layers = [valueNoise(11), valueNoise(27), valueNoise(53)];
  const values = new Float32Array(size * size);

  for (let y = 0; y < size; y++) {
    for (let x = 0; x < size; x++) {
      const u = x / size;
      const v = y / size;

      let best = Infinity;
      for (const pts of polylines) {
        for (let i = 0; i < pts.length; i++) {
          const dx = u - pts[i][0];
          const dy = v - pts[i][1];
          const d = Math.sqrt(dx * dx + dy * dy) / pts[i][2];
          if (d < best) best = d;
        }
      }

      //	띠 폭으로 잘라야 벤 선 둘레만 어둡고 나머지는 흰색으로 남는다.
      const banded = Math.min(best / SLASH_BAND, 1);

      //	노이즈로 거리를 밀고 당겨 가장자리를 뜯는다.
      //	곱셈이라 획에 가까울수록 덜 흔들린다 — 벤 선 자체는 살아 있어야 한다.
      const n = fbm(u, v, layers);
      values[y * size + x] = Math.pow(banded, 0.9) * (0.8 + n * 0.4);
    }
  }
  return normalize(values);
}

// ── 실행 ──────────────────────────────────────────────────────────────

mkdirSync(OUT_DIR, { recursive: true });

writeGrayPng(join(OUT_DIR, "T_BGDissolve_Ember.png"), makeEmber(SIZE), SIZE);
console.log("T_BGDissolve_Ember.png  — 재 · 잔불 (공격)");

writeGrayPng(join(OUT_DIR, "T_BGDissolve_Soft.png"), makeSoft(SIZE), SIZE);
console.log("T_BGDissolve_Soft.png   — 부드러운 셀 (회복)");

writeGrayPng(join(OUT_DIR, "T_BGDissolve_Slash.png"), makeSlash(SIZE), SIZE);
console.log("T_BGDissolve_Slash.png  — 휘두른 궤적 (공격 · 베인 자국)");

console.log(`\n${SIZE}x${SIZE} 8bit 그레이스케일 → ${OUT_DIR}`);
