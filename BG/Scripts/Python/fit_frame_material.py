"""
카드 프레임 텍스처를 재서 M_BGCardFrame 을 그 그림에 맞춘다.

에디터에서 실행:
    Tools ▸ Execute Python Script ▸ 이 파일

아트가 바뀔 때마다 이것만 돌리면 된다. build_frame_material.py 는 그래프를 만드는
스크립트라 아트가 바뀌어도 다시 돌릴 필요가 없다.

하는 일
  1. T_Card_* 를 읽어 글로우가 번질 **투명 여백**이 있는지 본다.
     모자라면 그림 픽셀은 그대로 두고 캔버스만 키워 다시 임포트한다(종횡비 유지).
  2. 플러드 필로 **아트 창**을 찾아 그 중심을 GlowCenter 에 써 넣는다.

GlowCenter 가 뭔가
  머테리얼은 바깥과 창을 **런타임에** 가른다 — 불투명한 덩어리가 기준점 쪽에 쏠려
  있으면 바깥, 반대쪽에 쏠려 있으면 창이다. 그 기준점이 GlowCenter 이고,
  **창 안에 있기만 하면** 된다.

  카드 그림은 창이 가운데 있어 기본값 (0.5, 0.5) 로 이미 맞는다(실측 여유 0.119).
  창이 한쪽으로 치우친 그림에서만 이 스크립트가 재 준 값이 필요하다 — 기본값으로
  두면 그런 그림에서는 창 판정이 70% 언저리로 떨어진다.

  ⚠️ 창의 **모양**은 이제 안 잰다. 예전에는 사각 상자(HoleBoxMin/Max)로 잘랐는데,
     그러면 텍스처를 바꿀 때마다 이 스크립트를 돌려야 글로우가 맞았다. 지금은
     머테리얼이 알파만 보고 판정하므로 그림이 바뀌어도 바로 따라간다.
"""

import os
import struct
import zlib

import unreal

TEX_DIR = "/Game/UI/Texture/Card"
TEX_NAMES = ("T_Card_Attack", "T_Card_Equipment", "T_Card_Magic")
MATERIAL_PATH = "/Game/UI/Material/M_BGCardFrame"
INSTANCE_PATH = "/Game/UI/Material/MI_BGCardFrame_Default"

#	위상 판정용 다운샘플. 1230×1640 을 그대로 플러드 필하면 파이썬에서 너무 느리고,
#	4 배로 줄여도 창과 바깥을 가르는 데는 넉넉하다.
CLASSIFY_STEP = 4

#	알파가 이 값 이하면 '비었다'고 본다. 0 으로 두면 경계의 앤티에일리어싱 찌꺼기가
#	바깥과 창을 이어 버려 플러드 필이 창까지 새어 들어간다.
CLEAR_ALPHA = 8

#	잰 창 중심이 기본값에서 이보다 적게 떨어져 있으면 굳이 오버라이드하지 않는다.
#	기본값으로 두는 편이 애셋 차이가 적고, 이 정도 어긋남은 판정 여유 안에 든다.
CENTER_EPS = 0.02

mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary


# ══════════════════════════════════════════════════════════════════════
#  PNG — 언리얼 Python 에는 이미지 라이브러리가 없어서 직접 다룬다
# ══════════════════════════════════════════════════════════════════════

def png_read(path):
    """8bit RGBA PNG 를 (w, h, bytes) 로 읽는다."""
    raw = open(path, "rb").read()
    pos, idat = 8, b""
    w = h = depth = ctype = None
    while pos < len(raw):
        length, typ = struct.unpack(">I4s", raw[pos:pos + 8])
        if typ == b"IHDR":
            w, h, depth, ctype = struct.unpack(">IIBB", raw[pos + 8:pos + 18])
        elif typ == b"IDAT":
            idat += raw[pos + 8:pos + 8 + length]
        pos += 12 + length
    if depth != 8 or ctype != 6:
        raise RuntimeError("8bit RGBA PNG 가 아니다 (depth=%s type=%s)" % (depth, ctype))

    data = zlib.decompress(idat)
    bpp, stride = 4, w * 4
    out, prev, p = bytearray(), bytearray(stride), 0
    for _ in range(h):
        f = data[p]; p += 1
        line = bytearray(data[p:p + stride]); p += stride
        if f:
            #	PNG 필터 되돌리기. f == 0 이면 손댈 것이 없어 건너뛴다.
            for i in range(stride):
                a = line[i - bpp] if i >= bpp else 0
                b = prev[i]
                c = prev[i - bpp] if i >= bpp else 0
                if f == 1:   line[i] = (line[i] + a) & 255
                elif f == 2: line[i] = (line[i] + b) & 255
                elif f == 3: line[i] = (line[i] + (a + b) // 2) & 255
                else:
                    pp = a + b - c
                    pa, pb, pc = abs(pp - a), abs(pp - b), abs(pp - c)
                    line[i] = (line[i] + (a if (pa <= pb and pa <= pc)
                                          else (b if pb <= pc else c))) & 255
        out += line
        prev = line
    return w, h, bytes(out)


def png_write(path, w, h, px):
    def chunk(tag, data):
        body = tag + data
        return (struct.pack(">I", len(data)) + body
                + struct.pack(">I", zlib.crc32(body) & 0xffffffff))
    raw = b"".join(b"\x00" + px[y * w * 4:(y + 1) * w * 4] for y in range(h))
    open(path, "wb").write(
        b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0))
        + chunk(b"IDAT", zlib.compress(raw, 9))
        + chunk(b"IEND", b""))


# ══════════════════════════════════════════════════════════════════════
#  텍스처 입출력
# ══════════════════════════════════════════════════════════════════════

def export_png(tex, path):
    task = unreal.AssetExportTask()
    task.object = tex
    task.filename = path
    task.automated = True
    task.prompt = False
    task.exporter = unreal.TextureExporterPNG()
    return unreal.Exporter.run_asset_export_task(task)


def reimport_png(name, path):
    """
    PNG 를 같은 애셋 위에 다시 임포트한다.

    ⚠️ 임포트는 압축 · sRGB · 밉 설정을 기본값으로 되돌려 놓는다. 원래 값을 붙잡아
       두었다가 되돌린다 — 카드 텍스처는 TC_EDITOR_ICON(무압축)이라 기본값인 DXT 로
       바뀌면 금색 장식에 블록 깨짐이 생긴다.
    """
    asset_path = TEX_DIR + "/" + name
    old = unreal.load_asset(asset_path)
    keep = {}
    if old:
        for key in ("compression_settings", "srgb", "mip_gen_settings",
                    "compression_no_alpha", "lod_group", "filter"):
            try:
                keep[key] = old.get_editor_property(key)
            except Exception:
                pass

    task = unreal.AssetImportTask()
    task.filename = path
    task.destination_path = TEX_DIR
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = True
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    tex = unreal.load_asset(asset_path)
    if not tex:
        unreal.log_error("[Fit] 재임포트 실패: %s" % asset_path)
        return None
    for key, value in keep.items():
        try:
            tex.set_editor_property(key, value)
        except Exception as err:
            unreal.log_warning("[Fit] %s.%s 복원 실패: %s" % (name, key, err))
    eal.save_asset(asset_path)
    return tex


# ══════════════════════════════════════════════════════════════════════
#  측정
# ══════════════════════════════════════════════════════════════════════

def edge_margins(w, h, px):
    """각 변에서 '알파가 있는 첫 픽셀' 까지의 거리(px). 변마다 최솟값을 본다."""
    def first(seq):
        for i, (x, y) in enumerate(seq):
            if px[(y * w + x) * 4 + 3] > CLEAR_ALPHA:
                return i
        return max(w, h)
    top    = min(first([(x, y) for y in range(h)]) for x in range(w))
    bottom = min(first([(x, h - 1 - y) for y in range(h)]) for x in range(w))
    left   = min(first([(x, y) for x in range(w)]) for y in range(h))
    right  = min(first([(w - 1 - x, y) for x in range(w)]) for y in range(h))
    return top, bottom, left, right


def classify(w, h, px):
    """
    다운샘플 격자에서 '비었고 테두리와 이어진' 칸을 바깥으로 표시한다.
    나머지 빈 칸이 구멍(아트 창)이다.
    """
    from collections import deque
    s = CLASSIFY_STEP
    gw, gh = w // s, h // s
    clear = bytearray(gw * gh)
    for y in range(gh):
        row = (y * s) * w
        for x in range(gw):
            if px[(row + x * s) * 4 + 3] <= CLEAR_ALPHA:
                clear[y * gw + x] = 1

    outer = bytearray(gw * gh)
    q = deque()
    def seed(x, y):
        if clear[y * gw + x] and not outer[y * gw + x]:
            outer[y * gw + x] = 1
            q.append((x, y))
    for x in range(gw):
        seed(x, 0); seed(x, gh - 1)
    for y in range(gh):
        seed(0, y); seed(gw - 1, y)
    while q:
        x, y = q.popleft()
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            nx, ny = x + dx, y + dy
            if 0 <= nx < gw and 0 <= ny < gh:
                if clear[ny * gw + nx] and not outer[ny * gw + nx]:
                    outer[ny * gw + nx] = 1
                    q.append((nx, ny))
    return gw, gh, clear, outer


def hole_center(gw, gh, clear, outer):
    """구멍(아트 창) 전체의 중심(UV). 구멍이 없으면 None."""
    holes = [(x, y) for y in range(gh) for x in range(gw)
             if clear[y * gw + x] and not outer[y * gw + x]]
    if not holes:
        return None
    u0 = min(x for x, _ in holes) / gw
    u1 = (max(x for x, _ in holes) + 1) / gw
    v0 = min(y for _, y in holes) / gh
    v1 = (max(y for _, y in holes) + 1) / gh
    return (u0 + u1) * 0.5, (v0 + v1) * 0.5, (u0, u1, v0, v1)


# ══════════════════════════════════════════════════════════════════════
#  실행
# ══════════════════════════════════════════════════════════════════════

def glow_width():
    """머테리얼에 잡혀 있는 GlowWidth(가로폭 비율). 여백이 얼마나 필요한지의 기준."""
    mat = unreal.load_asset(MATERIAL_PATH)
    if not mat:
        unreal.log_warning("[Fit] %s 가 없다 — GlowWidth 0.05 로 가정한다." % MATERIAL_PATH)
        return 0.05
    mi = unreal.load_asset(INSTANCE_PATH)
    if mi:
        try:
            return mel.get_material_instance_scalar_parameter_value(mi, "GlowWidth")
        except Exception:
            pass
    return mel.get_material_default_scalar_parameter_value(mat, "GlowWidth")


def fit():
    work = unreal.Paths.convert_relative_path_to_full(
        unreal.Paths.project_saved_dir()) + "FitFrame/"
    os.makedirs(work, exist_ok=True)

    need_ratio = glow_width()
    unreal.log("[Fit] GlowWidth %.3f — 여백이 가로폭의 그만큼 필요하다" % need_ratio)

    boxes, padded_any = {}, []
    for name in TEX_NAMES:
        tex = unreal.load_asset(TEX_DIR + "/" + name)
        if not tex:
            unreal.log_warning("[Fit] 없음: %s/%s — 건너뛴다" % (TEX_DIR, name))
            continue

        png = work + name + ".png"
        if not export_png(tex, png):
            unreal.log_error("[Fit] 내보내기 실패: %s" % name)
            continue
        w, h, px = png_read(png)

        # ── 1. 여백 ──────────────────────────────────────────────────
        top, bottom, left, right = edge_margins(w, h, px)
        need = need_ratio * w
        short = max(0.0, need - min(top, bottom, left, right))
        unreal.log("[Fit] %s  %dx%d  여백 상%d 하%d 좌%d 우%d  (필요 %.0f)"
                   % (name, w, h, top, bottom, left, right, need))

        if short > 0.5:
            #	가로세로에 **비례해서** 넣는다. 같은 픽셀 수를 넣으면 종횡비가 바뀌어
            #	프레임이 늘어난다.
            pad_x = int(short) + 1
            pad_y = int(round(pad_x * h / float(w)))
            nw, nh = w + 2 * pad_x, h + 2 * pad_y
            buf = bytearray(nw * nh * 4)          # 전부 투명
            for y in range(h):
                src = (y * w) * 4
                dst = ((y + pad_y) * nw + pad_x) * 4
                buf[dst:dst + w * 4] = px[src:src + w * 4]
            png_write(png, nw, nh, bytes(buf))
            if not reimport_png(name, png):
                continue
            padded_any.append((name, w, h, nw, nh, w / float(nw)))
            unreal.log("[Fit]   여백 부족 %.0fpx — %dx%d 로 키웠다(가로 %d 세로 %d)"
                       % (short, nw, nh, pad_x, pad_y))
            w, h, px = nw, nh, bytes(buf)

        # ── 2. 아트 창 중심 ─────────────────────────────────────────
        gw, gh, clear, outer = classify(w, h, px)
        found = hole_center(gw, gh, clear, outer)
        if found is None:
            unreal.log("[Fit]   구멍 없음 — 통짜 카드다. 기준점이 필요 없다")
            continue
        cu, cv, box = found
        unreal.log("[Fit]   창 u[%.3f~%.3f] v[%.3f~%.3f]  중심 (%.3f, %.3f)"
                   % (box[0], box[1], box[2], box[3], cu, cv))
        boxes[name] = (cu, cv)

    # ── 3. 기준점 기록 ──────────────────────────────────────────────
    mi = unreal.load_asset(INSTANCE_PATH)
    if not mi:
        unreal.log_error("[Fit] %s 가 없다 — build_frame_material.py 를 먼저 돌릴 것"
                         % INSTANCE_PATH)
        return

    if not boxes:
        unreal.log("[Fit] 구멍이 있는 그림이 없어 기준점은 그대로 둔다")
    else:
        cu = sum(c[0] for c in boxes.values()) / len(boxes)
        cv = sum(c[1] for c in boxes.values()) / len(boxes)
        spread = max(max(abs(c[0] - cu), abs(c[1] - cv)) for c in boxes.values())
        if spread > 0.05:
            unreal.log_warning(
                "[Fit] ⚠️ 카드 타입마다 창 중심이 %.3f 만큼 다르다. 기준점은 하나뿐이라 "
                "평균을 쓴다 — 각 창 안에만 들어가면 판정은 맞지만, 여유가 줄어든다. "
                "많이 어긋나면 타입별 MI 를 따로 둘 것." % spread)

        if abs(cu - 0.5) < CENTER_EPS and abs(cv - 0.5) < CENTER_EPS:
            unreal.log("[Fit] 창 중심 (%.3f, %.3f) 이 기본값과 가깝다 — 오버라이드하지 않는다"
                       % (cu, cv))
        else:
            mel.set_material_instance_vector_parameter_value(
                mi, "GlowCenter", unreal.LinearColor(cu, cv, 0.0, 0.0))
            eal.save_asset(INSTANCE_PATH)
            unreal.log("[Fit] MI 에 기록 — GlowCenter (%.3f, %.3f)" % (cu, cv))

    if padded_any:
        unreal.log_warning("[Fit] ⚠️ 여백을 넣느라 캔버스를 키웠다. 프레임이 텍스처에서 "
                           "차지하는 비율이 줄었으므로 **WBP 의 Border_Root 를 키워야** "
                           "화면에서 예전 크기가 된다:")
        for name, ow, oh, nw, nh, ratio in padded_any:
            unreal.log_warning("[Fit]     %s  %dx%d → %dx%d   Border_Root ×%.3f"
                               % (name, ow, oh, nw, nh, 1.0 / ratio))


fit()
