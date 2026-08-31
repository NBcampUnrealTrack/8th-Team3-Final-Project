"""
카드 프레임 텍스처를 재서 M_BGCardFrame 을 그 그림에 맞춘다.

에디터에서 실행:
    Tools ▸ Execute Python Script ▸ 이 파일

아트가 바뀔 때마다 이것만 돌리면 된다. build_frame_material.py 는 그래프를 만드는
스크립트라 아트가 바뀌어도 다시 돌릴 필요가 없다.

하는 일
  1. T_Card_* 를 읽어 글로우가 번질 **투명 여백**이 있는지 본다.
     모자라면 그림 픽셀은 그대로 두고 캔버스만 키워 다시 임포트한다(종횡비 유지).
  2. 플러드 필로 **아트 창**(바깥과 안 이어진 투명 영역)을 찾아 상자를 잰다.
  3. 그 값을 MI_BGCardFrame_Default 에 써 넣는다.

왜 코드가 재야 하는가
  글로우 모양은 픽셀 하나의 이웃만 보면 되므로 셰이더가 스스로 알아낸다. 그런데
  "이 투명한 자리가 카드 바깥인가 창 안인가" 는 **그림 전체의 연결 관계**라
  셰이더가 알 수 없다. 그래서 여기서 한 번 재어 파라미터로 넘긴다.

  ⚠️ 셰이더 안에서 풀려다 두 번 실패했다. 카드 중심에서 광선을 쏘는 방법은 프레임
     띠(0.054)보다 첫 걸음(0.094)이 커서 띠를 뛰어넘었고, "경계에 가까우면 바깥"
     으로 가르는 방법은 상단 장식 사이 오목한 곳이 창보다 깊이 파고들어 순서가
     뒤집혔다. 위상은 재서 넘기는 것이 맞다.
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

#	잰 상자를 이만큼 넓힌다. 딱 맞추면 HoleBoxSoft 전환 구간이 창 가장자리에 걸려
#	글로우가 절반만 죽고 창 둘레에 얇은 선이 남는다.
BOX_EXPAND_MAX = 0.030

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


def hole_box(gw, gh, clear, outer):
    """
    구멍 전체를 감싸는 상자(UV)와, 바깥을 건드리지 않고 넓힐 수 있는 최대 폭.
    구멍이 없으면 None.
    """
    holes = [(x, y) for y in range(gh) for x in range(gw)
             if clear[y * gw + x] and not outer[y * gw + x]]
    if not holes:
        return None

    u0 = min(x for x, _ in holes) / gw
    u1 = (max(x for x, _ in holes) + 1) / gw
    v0 = min(y for _, y in holes) / gh
    v1 = (max(y for _, y in holes) + 1) / gh

    #	상자를 넓히다가 바깥 픽셀이 들어오는 순간 멈춘다.
    room = 0.0
    while room < 0.20:
        e = room + 0.005
        hit = any(outer[y * gw + x]
                  and (u0 - e) <= (x + 0.5) / gw <= (u1 + e)
                  and (v0 - e) <= (y + 0.5) / gh <= (v1 + e)
                  for y in range(gh) for x in range(gw))
        if hit:
            break
        room = e
    return u0, u1, v0, v1, room


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

        # ── 2. 아트 창 ───────────────────────────────────────────────
        gw, gh, clear, outer = classify(w, h, px)
        box = hole_box(gw, gh, clear, outer)
        if box is None:
            unreal.log("[Fit]   구멍 없음 — 이 그림은 상자가 필요 없다")
            continue
        u0, u1, v0, v1, room = box
        unreal.log("[Fit]   창 u[%.3f~%.3f] v[%.3f~%.3f]  넓힐 여지 %.3f"
                   % (u0, u1, v0, v1, room))
        boxes[name] = (u0, u1, v0, v1, room)

    # ── 3. 세 장을 하나의 상자로 ─────────────────────────────────────
    mi = unreal.load_asset(INSTANCE_PATH)
    if not mi:
        unreal.log_error("[Fit] %s 가 없다 — build_frame_material.py 를 먼저 돌릴 것"
                         % INSTANCE_PATH)
        return

    if not boxes:
        #	구멍이 하나도 없다. 상자를 눌러 두면 window() 가 0 이 되어 아무것도 안 죽인다.
        mel.set_material_instance_vector_parameter_value(
            mi, "HoleBoxMin", unreal.LinearColor(0.5, 0.5, 0.0, 0.0))
        mel.set_material_instance_vector_parameter_value(
            mi, "HoleBoxMax", unreal.LinearColor(0.5, 0.5, 0.0, 0.0))
        eal.save_asset(INSTANCE_PATH)
        unreal.log("[Fit] 구멍이 없어 상자를 껐다")
        return

    #	모든 창을 덮어야 하므로 합집합. 넓힐 여지는 가장 빡빡한 것을 따른다.
    u0 = min(b[0] for b in boxes.values()); u1 = max(b[1] for b in boxes.values())
    v0 = min(b[2] for b in boxes.values()); v1 = max(b[3] for b in boxes.values())
    room = min(b[4] for b in boxes.values())

    spread = max(max(abs(b[0] - u0), abs(b[1] - u1),
                     abs(b[2] - v0), abs(b[3] - v1)) for b in boxes.values())
    if spread > 0.01:
        unreal.log_warning(
            "[Fit] ⚠️ 카드 타입마다 창 위치가 %.3f 만큼 다르다. 상자 하나로 덮으면 "
            "가장 큰 창에 맞춰지므로 작은 창 쪽 프레임 글로우가 깎인다 — 타입별 MI 를 "
            "따로 두고 RefreshFrame 이 머테리얼을 갈아 끼우는 편이 낫다." % spread)

    expand = min(room * 0.6, BOX_EXPAND_MAX)
    if expand < 0.005:
        unreal.log_warning(
            "[Fit] ⚠️ 상자를 넓힐 여지가 %.3f 뿐이다 — 창 둘레에 얇은 선이 남을 수 있다. "
            "HoleBoxSoft 를 줄여 볼 것." % room)

    lo = unreal.LinearColor(u0 - expand, v0 - expand, 0.0, 0.0)
    hi = unreal.LinearColor(u1 + expand, v1 + expand, 0.0, 0.0)
    mel.set_material_instance_vector_parameter_value(mi, "HoleBoxMin", lo)
    mel.set_material_instance_vector_parameter_value(mi, "HoleBoxMax", hi)
    eal.save_asset(INSTANCE_PATH)

    unreal.log("[Fit] MI 에 기록 — HoleBoxMin (%.3f, %.3f)  HoleBoxMax (%.3f, %.3f)  "
               "(실측을 %.3f 넓힌 값)" % (lo.r, lo.g, hi.r, hi.g, expand))

    if padded_any:
        unreal.log_warning("[Fit] ⚠️ 여백을 넣느라 캔버스를 키웠다. 프레임이 텍스처에서 "
                           "차지하는 비율이 줄었으므로 **WBP 의 Border_Root 를 키워야** "
                           "화면에서 예전 크기가 된다:")
        for name, ow, oh, nw, nh, ratio in padded_any:
            unreal.log_warning("[Fit]     %s  %dx%d → %dx%d   Border_Root ×%.3f"
                               % (name, ow, oh, nw, nh, 1.0 / ratio))

fit()
