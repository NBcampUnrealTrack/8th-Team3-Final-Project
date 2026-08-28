/**
 * 칼자국 번쩍임(Slash Flash) 텍스처를 굽는다.
 *
 *   node gen_slash_flash.mjs
 *
 * ⚠️ 디졸브 마스크가 아니다. 마스크는 "언제 사라질지"를 정하는 그라디언트라
 *    얇고 밝은 선이 될 수 없다. 이것은 카드 **위에 얹어 발광으로 그리는 그림**이고,
 *    UMG 애니메이션으로 번쩍였다 사라지게 쓴다.
 *
 * 검정 바탕에 흰 궤적이라 머테리얼에서 그대로 Emissive 로 쓰면 되고,
 * 알파가 필요하면 같은 채널을 Opacity 에 물리면 된다(Additive 면 알파도 필요 없다).
 */
import { deflateSync } from "node:zlib";
import { mkdirSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const SIZE = 1024;
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
  ihdr[8] = 8;
  ihdr[9] = 0;

  const raw = Buffer.alloc((size + 1) * size);
  for (let y = 0; y < size; y++) {
    raw[y * (size + 1)] = 0;
    raw.set(pixels.subarray(y * size, (y + 1) * size), y * (size + 1) + 1);
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

// ── 곡선 획 ───────────────────────────────────────────────────────────

let seed = 0x1f2e3d4c;
function rand() {
  seed ^= seed << 13;
  seed ^= seed >>> 17;
  seed ^= seed << 5;
  return ((seed >>> 0) % 100000) / 100000;
}

/**
 * 2차 베지에 위의 점. 직선이 아니라 호를 그려야 휘두른 궤적으로 읽힌다.
 * 제어점을 두 끝점 사이에서 옆으로 밀어 곡률을 만든다.
 */
function bezier(p0, p1, p2, t) {
  const s = 1 - t;
  return [
    s * s * p0[0] + 2 * s * t * p1[0] + t * t * p2[0],
    s * s * p0[1] + 2 * s * t * p1[1] + t * t * p2[1],
  ];
}

/**
 * 획 하나를 그린다. 점을 촘촘히 찍고 각 점에서 반경 안쪽을 밝힌다.
 *
 * ⚠️ 굵기를 일정하게 두면 칼이 아니라 붓자국이 된다. 가운데가 굵고 양끝이
 *    바늘처럼 가늘어야 "스치고 지나갔다" 로 읽힌다.
 */
function drawStroke(buf, size, stroke) {
  const { p0, p1, p2, width, intensity, taperPow } = stroke;
  const STEPS = 2400;

  for (let i = 0; i <= STEPS; i++) {
    const t = i / STEPS;
    const [cx, cy] = bezier(p0, p1, p2, t);

    //	양끝으로 갈수록 얇아진다. 지수가 클수록 끝이 더 뾰족하다.
    const taper = Math.pow(Math.sin(Math.PI * t), taperPow);
    const w = width * taper;
    if (w <= 0.0004) continue;

    const px = cx * size;
    const py = cy * size;
    const rad = w * size;
    const lo = Math.max(0, Math.floor(px - rad));
    const hi = Math.min(size - 1, Math.ceil(px + rad));
    const loY = Math.max(0, Math.floor(py - rad));
    const hiY = Math.min(size - 1, Math.ceil(py + rad));

    for (let y = loY; y <= hiY; y++) {
      for (let x = lo; x <= hi; x++) {
        const d = Math.hypot(x - px, y - py) / rad;
        if (d >= 1) continue;
        //	⚠️ 지수를 크게 주면 중심만 겨우 밝고 나머지가 회색으로 깔린다 —
        //	   발광이 아니라 연필로 그은 것처럼 보인다. 코어를 넓게 태우고
        //	   가장자리만 급히 떨구는 편이 칼빛으로 읽힌다.
        const core = Math.pow(1 - d, 0.9);
        const v = core * intensity * Math.pow(taper, 0.5);
        const idx = y * size + x;
        if (v > buf[idx]) buf[idx] = v;
      }
    }
  }
}

/** 교차점에서 튀는 불꽃. 가는 직선을 방사형으로 흩뿌린다. */
function drawSparks(buf, size, origin, count) {
  for (let i = 0; i < count; i++) {
    const angle = rand() * Math.PI * 2;
    const len = 0.05 + rand() * 0.22;
    const bright = 0.5 + rand() * 0.6;
    const STEPS = Math.ceil(len * size);

    for (let s = 0; s < STEPS; s++) {
      const t = s / STEPS;
      const x = Math.round((origin[0] + Math.cos(angle) * len * t) * size);
      const y = Math.round((origin[1] + Math.sin(angle) * len * t) * size);
      if (x < 0 || y < 0 || x >= size || y >= size) break;

      //	뿌리가 밝고 끝으로 갈수록 사라진다.
      const v = bright * Math.pow(1 - t, 1.4);
      const idx = y * size + x;
      if (v > buf[idx]) buf[idx] = v;
    }
  }
}

// ── 구성 ──────────────────────────────────────────────────────────────
//
//	좌하단 → 우상단으로 휘두른 궤적. 굵은 본선 하나에 가는 곁줄기를 붙이면
//	한 번 그은 것이 아니라 날이 지나가며 갈라진 것처럼 보인다.

function makeFlash(size) {
  const buf = new Float32Array(size * size);

  const strokes = [
    //	본선 — 가장 굵고 밝다
    { p0: [0.14, 0.90], p1: [0.42, 0.44], p2: [0.90, 0.14],
      width: 0.020, intensity: 1.9, taperPow: 0.55 },

    //	위쪽 곁줄기 — 본선보다 크게 휜다
    { p0: [0.30, 0.82], p1: [0.44, 0.36], p2: [0.86, 0.20],
      width: 0.007, intensity: 1.7, taperPow: 0.8 },

    //	아래쪽 곁줄기 — 짧고 가늘게
    { p0: [0.36, 0.72], p1: [0.60, 0.58], p2: [0.82, 0.46],
      width: 0.005, intensity: 1.5, taperPow: 0.9 },

    //	머리카락 같은 잔선. 있으면 날이 여러 겹으로 스친 느낌이 난다
    { p0: [0.22, 0.86], p1: [0.46, 0.50], p2: [0.88, 0.18],
      width: 0.0018, intensity: 1.8, taperPow: 1.1 },
  ];

  for (const stroke of strokes) drawStroke(buf, size, stroke);

  //	획들이 가장 가까워지는 지점에서 불꽃이 튄다.
  drawSparks(buf, size, [0.44, 0.55], 90);

  //	교차점 자체를 한 번 더 태워 흰 코어를 만든다.
  drawStroke(buf, size, {
    p0: [0.38, 0.62], p1: [0.44, 0.55], p2: [0.52, 0.48],
    width: 0.012, intensity: 2.6, taperPow: 0.4,
  });

  //	1 을 넘긴 값은 잘리므로 코어가 자연히 새하얗게 뭉친다.
  //	감마를 살짝 낮춰 중간 밝기를 끌어올리면 궤적 전체가 빛나 보인다.
  const out = Buffer.alloc(buf.length);
  for (let i = 0; i < buf.length; i++) {
    const v = Math.pow(Math.min(buf[i], 1), 0.7);
    out[i] = Math.round(v * 255);
  }
  return out;
}

// ── 실행 ──────────────────────────────────────────────────────────────

mkdirSync(OUT_DIR, { recursive: true });
writeGrayPng(join(OUT_DIR, "T_BGSlashFlash.png"), makeFlash(SIZE), SIZE);
console.log(`T_BGSlashFlash.png — 칼자국 번쩍임 ${SIZE}x${SIZE} → ${OUT_DIR}`);
