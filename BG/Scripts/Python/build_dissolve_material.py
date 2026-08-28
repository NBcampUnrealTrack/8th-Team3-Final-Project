"""
M_BGCardDissolve 의 그래프를 통째로 다시 만든다.

에디터에서 실행:
    Tools ▸ Execute Python Script ▸ 이 파일
  또는 출력 로그 하단 명령창을 Python 으로 바꾸고
    exec(open(r"...\\BG\\Scripts\\Python\\build_dissolve_material.py").read())

⚠️ 기존 노드를 전부 지우고 새로 만든다. 손으로 만든 것이 있으면 먼저 백업할 것.

만드는 것
  1. _gen/ 의 PNG 2장을 /Game/UI/Material 로 임포트
  2. 파라미터 14개 + 노드 40여 개로 그래프 구성
  3. 도메인 · 블렌드 모드 설정 후 컴파일

파라미터 이름은 BGCardWidget.h 의 Dissolve|Param 카테고리와 **한 글자도 달라선 안 된다** —
틀리면 DMI 가 조용히 무시해서 값을 바꿔도 아무 반응이 없다.
"""

import os
import unreal

MATERIAL_PATH = "/Game/UI/Material/M_BGCardDissolve"
ASSET_DIR = "/Game/UI/Material"

_HERE = os.path.dirname(os.path.abspath(__file__))
PNG_DIR = os.path.join(_HERE, "_gen")

mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary


# ══════════════════════════════════════════════════════════════════════
#  텍스처 임포트
# ══════════════════════════════════════════════════════════════════════

def import_mask(name):
    """마스크 PNG 한 장을 임포트한다. 이미 있으면 덮어쓴다."""
    png = os.path.join(PNG_DIR, name + ".png")
    if not os.path.exists(png):
        unreal.log_error("[Dissolve] PNG 가 없다: %s — gen_dissolve_textures.mjs 를 먼저 돌릴 것" % png)
        return None

    task = unreal.AssetImportTask()
    task.filename = png
    task.destination_path = ASSET_DIR
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = True
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    tex = unreal.load_asset("%s/%s" % (ASSET_DIR, name))
    if not tex:
        unreal.log_error("[Dissolve] 임포트 실패: %s" % name)
        return None

    #	마스크는 색이 아니라 숫자다. sRGB 를 켜 두면 감마가 먹어 경계가 밀린다.
    tex.set_editor_property("srgb", False)
    tex.set_editor_property("compression_settings",
                            unreal.TextureCompressionSettings.TC_GRAYSCALE)
    eal.save_asset("%s/%s" % (ASSET_DIR, name))
    return tex


def editor_still_open(subsystem, asset):
    """
    에디터가 아직 이 애셋을 들고 있는가.

    ⚠️ find_editor_for_asset 은 5.8 Python 에 노출돼 있지 않다. 편집 중인 애셋
       목록을 받아서 직접 찾는다. 이 API 도 없는 버전이면 '모른다'로 보고 진행한다 —
       확인을 못 한다고 작업 자체를 막으면 더 곤란하다.
    """
    try:
        return asset in subsystem.get_all_edited_assets()
    except Exception as err:
        unreal.log_warning("[Dissolve] 에디터 열림 여부를 확인하지 못했다 (%s) — 그냥 진행한다." % err)
        return False


def list_expressions(mat):
    """
    머테리얼이 들고 있는 노드 목록. 저장 위치가 엔진 버전마다 다르다 —
    5.1 부터 UMaterial::Expressions 가 MaterialExpressionCollection 안으로 옮겨졌다.
    """
    try:
        collection = mat.get_editor_property("expression_collection")
        return list(collection.get_editor_property("expressions"))
    except Exception:
        pass
    try:
        return list(mat.get_editor_property("expressions"))
    except Exception:
        return []


def purge_expressions(mat):
    """
    그래프를 비운다.

    ⚠️ delete_all_material_expressions 만 믿으면 안 된다. 실제로는 남는 경우가 있고,
       그러면 실행할 때마다 옛 노드가 쌓여 "Missing AppendVector input B" 처럼
       내가 만들지도 않은 노드의 컴파일 경고가 계속 나온다.
       목록을 직접 읽어 하나씩 지우고, 몇 개가 남았는지 로그로 남긴다.
    """
    before = len(list_expressions(mat))

    mel.delete_all_material_expressions(mat)

    #	남은 것을 직접 지운다. 지워지지 않는 것이 있으면 무한 루프에 빠지므로
    #	한 바퀴에 하나도 못 지우면 멈춘다.
    while True:
        remaining = list_expressions(mat)
        if not remaining:
            break
        removed = 0
        for expr in remaining:
            try:
                mel.delete_material_expression(mat, expr)
                removed += 1
            except Exception:
                pass
        if removed == 0:
            unreal.log_warning(
                "[Dissolve] 노드 %d 개가 지워지지 않는다 — 머테리얼을 새로 만들어야 할 수 있다."
                % len(remaining))
            break

    after = len(list_expressions(mat))
    unreal.log("[Dissolve] 그래프 정리 — 이전 %d 개, 남은 %d 개" % (before, after))
    return after


# ══════════════════════════════════════════════════════════════════════
#  노드 만들기 — 짧은 이름으로 감싼다
# ══════════════════════════════════════════════════════════════════════

class Graph(object):
    def __init__(self, material):
        self.mat = material
        self.x = -2400
        self.y = 0
        self.count = 0

    def node(self, cls, x=None, y=None):
        """격자에 대충 흩어 놓는다. 마지막에 layout 으로 정리한다."""
        self.count += 1
        px = self.x if x is None else x
        py = self.y if y is None else y
        self.y += 130
        if self.y > 2600:
            self.y = 0
            self.x += 320
        return mel.create_material_expression(self.mat, cls, px, py)

    def scalar(self, name, value):
        n = self.node(unreal.MaterialExpressionScalarParameter)
        n.set_editor_property("parameter_name", name)
        n.set_editor_property("default_value", value)
        n.set_editor_property("group", "Dissolve")
        return n

    def vector(self, name, color):
        n = self.node(unreal.MaterialExpressionVectorParameter)
        n.set_editor_property("parameter_name", name)
        n.set_editor_property("default_value", color)
        n.set_editor_property("group", "Dissolve")
        return n

    def tex_param(self, name, texture, sampler):
        n = self.node(unreal.MaterialExpressionTextureSampleParameter2D)
        n.set_editor_property("parameter_name", name)
        if texture:
            n.set_editor_property("texture", texture)
        n.set_editor_property("sampler_type", sampler)
        n.set_editor_property("group", "Dissolve")
        return n

    def const(self, value):
        n = self.node(unreal.MaterialExpressionConstant)
        n.set_editor_property("r", value)
        return n

    def const2(self, a, b):
        n = self.node(unreal.MaterialExpressionConstant2Vector)
        n.set_editor_property("r", a)
        n.set_editor_property("g", b)
        return n

    # ── 연결 ──
    @staticmethod
    def link(src, src_pin, dst, dst_pins):
        """
        입력 핀 이름이 노드마다 다르다. UE 에서 입력이 하나뿐인 노드(OneMinus · Abs ·
        Sign · Sine · Cosine · ComponentMask)는 **핀에 이름이 없어서** 빈 문자열이다.
        후보를 순서대로 시도하고, 전부 실패하면 조용히 넘어가지 말고 멈춘다 —
        연결이 빠지면 컴파일 경고만 남고 머테리얼이 기본값으로 대체되어 원인을 찾기 어렵다.
        """
        for pin in dst_pins:
            if mel.connect_material_expressions(src, src_pin, dst, pin):
                return pin
        raise RuntimeError("연결 실패: %s -> %s (핀 후보 %s)"
                           % (src.get_name(), dst.get_name(), dst_pins))

    # ── 연산 ──
    #	⚠️ 연결이 실제로 꽂혔는지 되읽어 확인할 수는 없다. 이항 노드의 A/B 는
    #	   protected 라 Python 에서 못 읽는다("Property 'A' ... is protected").
    #	   대신 그래프를 확실히 비우고 새로 만들어서 옛 노드가 안 섞이게 한다.
    def _bin(self, cls, a, b, pin_a="", pin_b=""):
        n = self.node(cls)
        self.link(a, pin_a, n, ["A"])
        self.link(b, pin_b, n, ["B"])
        return n

    def add(self, a, b, pa="", pb=""):
        return self._bin(unreal.MaterialExpressionAdd, a, b, pa, pb)

    def sub(self, a, b, pa="", pb=""):
        return self._bin(unreal.MaterialExpressionSubtract, a, b, pa, pb)

    def mul(self, a, b, pa="", pb=""):
        return self._bin(unreal.MaterialExpressionMultiply, a, b, pa, pb)

    def div(self, a, b, pa="", pb=""):
        return self._bin(unreal.MaterialExpressionDivide, a, b, pa, pb)

    def dot(self, a, b, pa="", pb=""):
        return self._bin(unreal.MaterialExpressionDotProduct, a, b, pa, pb)

    def minimum(self, a, b, pa="", pb=""):
        return self._bin(unreal.MaterialExpressionMin, a, b, pa, pb)

    def vec2(self, x_expr, y_expr):
        """
        스칼라 둘로 float2 를 만든다.

        ⚠️ AppendVector 를 쓰지 않는다. connect_material_expressions 가 B 핀에
           True 를 돌려주고도 실제로는 붙지 않아, 컴파일 때 "Missing AppendVector
           input B" 만 남고 원인을 찾기가 매우 어려웠다. 축 벡터에 스칼라를 곱해
           더하면 같은 float2 를 Add · Multiply 만으로 만들 수 있고, 이 둘은
           A/B 핀 이름이 확실히 동작한다.
        """
        return self.add(
            self.mul(self.const2(1.0, 0.0), x_expr),
            self.mul(self.const2(0.0, 1.0), y_expr),
        )

    def _unary(self, cls, src, pin=""):
        n = self.node(cls)
        self.link(src, pin, n, ["", "Input"])
        return n

    def one_minus(self, src, pin=""):
        return self._unary(unreal.MaterialExpressionOneMinus, src, pin)

    def absolute(self, src, pin=""):
        return self._unary(unreal.MaterialExpressionAbs, src, pin)

    def sign(self, src, pin=""):
        return self._unary(unreal.MaterialExpressionSign, src, pin)

    def sine(self, src, pin=""):
        n = self._unary(unreal.MaterialExpressionSine, src, pin)
        #	UE 의 Sine 은 라디안이 아니라 **회전 수**를 받는다(Period 기준).
        #	Period 1 이면 입력 1 이 한 바퀴다 — 그래서 각도를 360 으로 나눠 넣는다.
        n.set_editor_property("period", 1.0)
        return n

    def cosine(self, src, pin=""):
        n = self._unary(unreal.MaterialExpressionCosine, src, pin)
        n.set_editor_property("period", 1.0)
        return n

    def lerp(self, a, b, alpha, pa="", pb="", palpha=""):
        n = self.node(unreal.MaterialExpressionLinearInterpolate)
        self.link(a, pa, n, ["A"])
        self.link(b, pb, n, ["B"])
        self.link(alpha, palpha, n, ["Alpha"])
        return n

    def smoothstep(self, lo, hi, value, plo="", phi="", pv=""):
        n = self.node(unreal.MaterialExpressionSmoothStep)
        self.link(lo, plo, n, ["Min"])
        self.link(hi, phi, n, ["Max"])
        self.link(value, pv, n, ["Value"])
        return n

    def mask(self, src, r=False, g=False, b=False, a=False, pin=""):
        n = self.node(unreal.MaterialExpressionComponentMask)
        n.set_editor_property("r", r)
        n.set_editor_property("g", g)
        n.set_editor_property("b", b)
        n.set_editor_property("a", a)
        self.link(src, pin, n, ["", "Input"])
        return n


# ══════════════════════════════════════════════════════════════════════
#  실행
# ══════════════════════════════════════════════════════════════════════

def build():
    mat = unreal.load_asset(MATERIAL_PATH)
    if not mat:
        unreal.log_error("[Dissolve] 머테리얼이 없다: %s" % MATERIAL_PATH)
        return

    #	⚠️ 머테리얼 에디터가 열려 있으면 안 된다. 에디터는 /Engine/Transient 의 프리뷰 사본에서
    #	   작업하기 때문에, 원본 노드를 지워도 에디터가 들고 있던 그래프가 다시 얹힌다 —
    #	   두 그래프가 겹쳐서 파라미터가 두 벌씩 생기거나 연결이 끊긴 노드가 남는다.
    #
    #	닫기를 시도하되, 그래도 남아 있으면 **중단한다.** 반쯤 섞인 그래프를 만들어 놓고
    #	성공했다고 보고하면 원인을 찾는 데 시간을 다 쓰게 된다.
    subsystem = unreal.get_editor_subsystem(unreal.AssetEditorSubsystem)
    if subsystem:
        subsystem.close_all_editors_for_asset(mat)
        if editor_still_open(subsystem, mat):
            unreal.log_error(
                "[Dissolve] 머테리얼 에디터가 아직 열려 있다 — 닫고 다시 실행할 것. "
                "열린 채로 만들면 에디터 사본이 되살아나 그래프가 섞인다.")
            return

    ember_tex = import_mask("T_BGDissolve_Ember")
    soft_tex = import_mask("T_BGDissolve_Soft")
    slash_tex = import_mask("T_BGDissolve_Slash")

    #	UI 도메인이 아니면 리테이너가 못 쓴다. 반투명이 아니면 사라지질 못한다.
    mat.set_editor_property("material_domain", unreal.MaterialDomain.MD_UI)
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)

    purge_expressions(mat)
    g = Graph(mat)

    # ── 파라미터 ──────────────────────────────────────────────────────
    #	이름은 BGCardWidget.h 의 Dissolve|Param 과 반드시 같아야 한다.
    p_amount = g.scalar("DissolveAmount", 0.0)
    p_edge_w = g.scalar("EdgeWidth", 0.06)
    p_edge_i = g.scalar("EdgeIntensity", 4.0)
    p_sep = g.scalar("SeparateAmount", 0.0)
    p_cut = g.scalar("CutAngle", 0.0)
    p_soft = g.scalar("EdgeSoftness", 0.02)
    p_glow = g.scalar("SurfaceGlow", 0.0)
    p_rise = g.scalar("RiseAmount", 0.0)
    p_ember_i = g.scalar("EmberIntensity", 0.0)
    p_halo_w = g.scalar("HaloWidth", 0.0)
    p_color = g.vector("EdgeColor", unreal.LinearColor(1.0, 0.35, 0.05, 1.0))

    #	리테이너가 카드를 그린 결과를 여기로 넣어 준다.
    #	⚠️ 이름이 리테이너 디테일의 'Texture Parameter' 와 같아야 한다(기본 Texture).
    white = unreal.load_asset("/Engine/EngineResources/WhiteSquareTexture")
    t_card = g.tex_param("Texture", white, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    t_mask = g.tex_param("DissolveMask", slash_tex or ember_tex,
                         unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE)
    t_ember = g.tex_param("EmberMask", ember_tex,
                          unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE)

    # ── UV 변형 ──────────────────────────────────────────────────────
    uv = g.node(unreal.MaterialExpressionTextureCoordinate)

    #	잘린 조각이 벌어지는 방향(SeparateAmount 전용).
    #	각도를 회전 수로 바꿔 넣는다 — Sine/Cosine 은 라디안이 아니라 회전 수를 받는다.
    cut_turns = g.div(p_cut, g.const(360.0))
    cut_normal = g.vec2(g.cosine(cut_turns), g.sine(cut_turns))

    #	카드 중심 기준 좌표 → 칼자국 법선에 투영. 부호가 잘린 두 조각을 가른다.
    uv_centered = g.sub(uv, g.const2(0.5, 0.5))
    dp = g.dot(uv_centered, cut_normal)

    split_amount = g.mul(g.mul(p_sep, p_amount), g.sign(dp))
    split_vec = g.mul(cut_normal, split_amount)

    #	Y 를 더하면 아래쪽 픽셀을 읽어 그림이 위로 떠오른 것처럼 보인다.
    rise_vec = g.vec2(g.const(0.0), g.mul(p_rise, p_amount))

    uv_final = g.add(g.add(uv, split_vec), rise_vec)

    g.link(uv_final, "", t_card, ["UVs"])
    g.link(uv_final, "", t_mask, ["UVs"])

    # ── 마스크와 진행도 ───────────────────────────────────────────────
    #	마스크가 곧 잘리는 모양이다. 벤 자리가 0, 멀어질수록 1 이므로
    #	  DissolveAmount 가 커지면 그 선에서부터 바깥으로 벌어지며 사라진다.
    #
    #	⚠️ 절차적으로 만들지 않는다. 예전에는 CutAngle 로 직선 스윕을 만드는 길이
    #	   따로 있었는데, 텍스처와 역할이 겹치는 데다 직선이라 종이 자르는 것처럼
    #	   보였다. 모양은 텍스처 한 곳에서만 정한다.
    threshold = g.sub(t_mask, p_amount, pa="R")

    # ── 알파 ─────────────────────────────────────────────────────────
    #	⚠️ +0.001 이 없으면 EdgeSoftness 0 일 때 Min == Max 가 되어 0 으로 나눈다.
    soft_max = g.add(p_soft, g.const(0.001))
    opacity_raw = g.smoothstep(g.const(0.0), soft_max, threshold)
    opacity = g.mul(opacity_raw, t_card, pb="A")

    # ── 경계 띠 ──────────────────────────────────────────────────────
    #	잘리는 선 바로 안쪽이 1, 안으로 갈수록 0.
    #	opacity 를 곱해서 이미 사라진 자리에는 안 빛나게 한다.
    edge_raw = g.smoothstep(g.const(0.0), p_edge_w, threshold)
    edge_band = g.mul(g.one_minus(edge_raw), opacity)

    # ── 재 · 잔불 ────────────────────────────────────────────────────
    panner = g.node(unreal.MaterialExpressionPanner)
    panner.set_editor_property("speed_x", 0.08)
    panner.set_editor_property("speed_y", 0.05)
    g.link(uv, "", panner, ["Coordinate"])
    g.link(panner, "", t_ember, ["UVs"])
    ember = g.mul(t_ember, p_ember_i, pa="R")

    # ── 발광이 퍼지는 범위 ───────────────────────────────────────────
    #	SurfaceGlow 0 → 경계만(공격), 1 → 남은 면 전체(회복).
    glow = g.lerp(edge_band, opacity, p_glow)

    # ── 후광 ─────────────────────────────────────────────────────────
    #	⚠️ Fresnel 로 만들 수 없다. UI 도메인엔 뷰 벡터가 없어 항상 상수가 나온다.
    #	   카드 UV 에서 가장 가까운 테두리까지의 거리로 대신한다.
    uv_x = g.mask(uv, r=True)
    uv_y = g.mask(uv, g=True)
    border_d = g.minimum(
        g.minimum(uv_x, g.one_minus(uv_x)),
        g.minimum(uv_y, g.one_minus(uv_y)),
    )
    halo_max = g.add(p_halo_w, g.const(0.001))
    halo_raw = g.one_minus(g.smoothstep(g.const(0.0), halo_max, border_d))
    halo = g.mul(g.mul(halo_raw, opacity), p_halo_w)

    # ── 합성 ─────────────────────────────────────────────────────────
    total = g.add(g.add(glow, g.mul(edge_band, ember)), halo)
    colored = g.mul(g.mul(total, p_color), p_edge_i)
    emissive = g.add(t_card, colored, pa="RGB")

    mel.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY)

    mel.layout_material_expressions(mat)
    mel.recompile_material(mat)
    eal.save_asset(MATERIAL_PATH)

    #	겹친 그래프가 남아 있으면 여기서 개수가 안 맞는다. 세어 두면 다음에 바로 안다.
    unreal.log("[Dissolve] 완료 — 노드 %d 개, 파라미터 14 개" % g.count)
    unreal.log("[Dissolve] 다음: MI 에서 DissolveAmount 를 0→1 로 끌어 보고,"
               " WBP 의 DissolveByArchetype 에 공격 · 회복 항목을 넣을 것")


build()
