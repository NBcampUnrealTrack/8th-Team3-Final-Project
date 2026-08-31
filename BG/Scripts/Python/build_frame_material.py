"""
M_BGCardFrame 의 그래프를 통째로 만든다.

에디터에서 실행:
    Tools ▸ Execute Python Script ▸ 이 파일
  또는 출력 로그 하단 명령창을 Python 으로 바꾸고
    exec(open(r"...\\BG\\Scripts\\Python\\build_frame_material.py").read())

⚠️ 기존 노드를 전부 지우고 새로 만든다. 손으로 만든 것이 있으면 먼저 백업할 것.

하는 일
  1. /Game/UI/Material/M_BGCardFrame 이 없으면 만든다 (UI 도메인 · 반투명)
  2. 텍스처 한 장(FrameTex)을 받아 그대로 그리고, 그 알파에서 외곽선 글로우를 뽑는다
  3. MI_BGCardFrame_Default 를 같이 만들어 둔다

원리 — 거리장을 굽지 않는다
  프레임 알파를 나선으로 흩어 20 번 더 찍어 평균 낸다(= 흐린 알파). 흐린 알파는 실루엣
  바깥으로 조금 번져 있고 멀어질수록 옅어지므로, **원래 알파가 0 인 자리에서만** 남기면
  그것이 곧 바깥으로 향하는 그라데이션이다. 값 자체가 거리 대용이라 파동의 위상으로도
  그대로 쓴다.

  ⚠️ 이 방식은 텍스처 UV 밖으로는 못 번진다. T_Card_* 에 **투명 여백**이 없으면
     외곽선이 텍스처 경계에서 잘린다. 가로폭의 GlowWidth(기본 5%) 만큼은 여백을
     두고 뽑을 것.

자세한 사용법은 BG/Docs/M_BGCardFrame-매뉴얼.md 를 볼 것.

파라미터 이름은 BGCardWidget.h 의 것과 **한 글자도 달라선 안 된다** —
틀리면 DMI 가 조용히 무시해서 값을 바꿔도 아무 반응이 없다.
"""

import math

import unreal

MATERIAL_PATH = "/Game/UI/Material/M_BGCardFrame"
INSTANCE_PATH = "/Game/UI/Material/MI_BGCardFrame_Default"
ASSET_DIR = "/Game/UI/Material"

#	미리보기용 기본 텍스처. 코드가 RefreshFrame 에서 실제 카드 텍스처로 갈아 끼운다.
PREVIEW_TEX = "/Game/UI/Texture/Card/T_Card_Attack"

mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary

#	외곽선 표본 배치.
#
#	⚠️ 동심 링(바깥 8 + 안쪽 4)으로 두지 말 것. 표본 반지름이 두 종류뿐이라 흐린 알파가
#	   서너 값으로 양자화되어 **눈에 보이는 띠**가 생긴다. CPU 로 프로파일을 뽑아 보면
#	   0.74 → 0.24 → 0.04 처럼 계단이 진다.
#
#	그래서 Vogel 나선으로 흩는다 — 표본마다 반지름이 다 달라 거리의 매끈한 함수가 된다.
#	반지름에 sqrt 를 씌우는 것은 원판을 고르게 덮기 위한 것이고, 황금각(137.5°)은
#	적은 표본으로도 방향이 뭉치지 않게 한다.
#
#	20 개 · 균등 가중이 가장 낫다. 가중치를 반지름에 따라 기울여 봐야 낙차만 커진다
#	(측정: 낙차 0.25 → 균등 0.19).
TAP_COUNT = 20
GOLDEN_ANGLE = math.radians(137.50776)

#	중심 표본에만 가중을 준다. 실루엣 바로 안쪽이 너무 옅어지는 것을 막는다.
W_CENTER = 2.0
W_TOTAL = W_CENTER + TAP_COUNT

#	평균을 내면서 값이 통째로 낮아지므로 되올린다. 이 값도 위 프로파일로 맞춘 것이다.
SHAPE_BOOST = 1.8

#	안쪽 구멍을 가려내는 기준점(UV). 여기서 멀어지는 쪽을 '바깥' 으로 본다.
#
#	프레임은 가운데가 뚫린 고리다. 알파가 0 인 곳이 **바깥**만이 아니라 **아트 창**
#	이기도 해서, 1-A 만 쓰면 창 둘레에도 글로우가 낀다.
#
#	가르는 방법: 불투명한 덩어리가 **어느 쪽에 쏠려 있는가**를 본다.
#	  · 바깥 픽셀 — 프레임이 기준점 **쪽**에 있다        → 쏠림이 음수
#	  · 창 안 픽셀 — 프레임이 기준점 **반대쪽**에 있다   → 쏠림이 양수
#	외곽선용으로 이미 뜬 표본 20 개를 그대로 쓰므로 텍스처를 더 읽지 않는다.
#
#	기준점은 **창 안에 있기만 하면** 된다. 카드 그림은 창이 가운데 있으므로 0.5, 0.5 로
#	충분하다(실측 여유 0.119). 창이 한쪽으로 치우친 그림이면 fit_frame_material.py 가
#	창 중심을 재서 여기에 써 넣는다.
GLOW_CENTER = (0.5, 0.5)

#	쏠림이 0 을 넘는 지점을 부드럽게 넘긴다. 실측 여유가 바깥 0.195 · 창 0.119 이므로
#	그보다 한참 작게 잡아야 양쪽 다 안 깎인다.
HOLE_CUT_SOFT = 0.050


def spiral_taps():
    """
    표본 20 개. (dx, dy, ux, uy) — 앞의 둘은 UV 를 밀 오프셋, 뒤의 둘은 같은 방향의
    **단위 벡터**다. 쏠림을 재려면 거리가 아니라 방향만 필요해서 따로 둔다.
    """
    taps = []
    for i in range(TAP_COUNT):
        radius = math.sqrt((i + 0.5) / TAP_COUNT)
        angle = i * GOLDEN_ANGLE
        dx, dy = radius * math.cos(angle), radius * math.sin(angle)
        taps.append((dx, dy, math.cos(angle), math.sin(angle)))
    return taps


# ══════════════════════════════════════════════════════════════════════
#  애셋 준비
# ══════════════════════════════════════════════════════════════════════

def ensure_material():
    """머테리얼을 얻는다. 없으면 만든다."""
    mat = unreal.load_asset(MATERIAL_PATH)
    if mat:
        return mat

    tools = unreal.AssetToolsHelpers.get_asset_tools()
    mat = tools.create_asset("M_BGCardFrame", ASSET_DIR,
                             unreal.Material, unreal.MaterialFactoryNew())
    if not mat:
        unreal.log_error("[Frame] 머테리얼을 만들지 못했다: %s" % MATERIAL_PATH)
    else:
        unreal.log("[Frame] 머테리얼을 새로 만들었다: %s" % MATERIAL_PATH)
    return mat


def ensure_instance(mat):
    """
    MI 를 만들어 둔다. WBP 의 Border_Root 브러시에는 M 이 아니라 **이것을** 물린다 —
    M 을 직접 물리면 값을 조정할 때마다 셰이더를 다시 컴파일한다.
    """
    if eal.does_asset_exist(INSTANCE_PATH):
        return unreal.load_asset(INSTANCE_PATH)

    tools = unreal.AssetToolsHelpers.get_asset_tools()
    factory = unreal.MaterialInstanceConstantFactoryNew()
    mi = tools.create_asset("MI_BGCardFrame_Default", ASSET_DIR,
                            unreal.MaterialInstanceConstant, factory)
    if mi:
        mel.set_material_instance_parent(mi, mat)
        eal.save_asset(INSTANCE_PATH)
        unreal.log("[Frame] 인스턴스를 만들었다: %s" % INSTANCE_PATH)
    return mi


def editor_still_open(subsystem, asset):
    """
    에디터가 아직 이 애셋을 들고 있는가.

    ⚠️ find_editor_for_asset 은 5.8 Python 에 노출돼 있지 않다. 편집 중인 애셋
       목록을 받아서 직접 찾는다. 이 API 도 없는 버전이면 '모른다'로 보고 진행한다.
    """
    try:
        return asset in subsystem.get_all_edited_assets()
    except Exception as err:
        unreal.log_warning("[Frame] 에디터 열림 여부를 확인하지 못했다 (%s) — 그냥 진행한다." % err)
        return False


def list_expressions(mat):
    """
    머테리얼이 들고 있는 노드 목록. **읽을 수 없으면 None** 을 준다(빈 목록이 아니다).

    ⚠️ 5.8 에서는 둘 다 막혀 있다 — expression_collection 은 프로퍼티가 없고,
       expressions 는 protected 라 못 읽는다. 예전에는 실패를 빈 목록으로 돌려주는
       바람에 노드가 179 개 있는데도 "이전 0 개, 남은 0 개" 라고 찍혀서, 그래프가
       깨끗이 비워진 줄 알았다. 모르는 것은 0 이 아니라 모른다고 해야 한다.
    """
    for getter in (lambda: mat.get_editor_property("expression_collection")
                             .get_editor_property("expressions"),
                   lambda: mat.get_editor_property("expressions")):
        try:
            return list(getter())
        except Exception:
            continue
    return None


def purge_expressions(mat):
    """
    그래프를 비운다.

    ⚠️ delete_all_material_expressions 만 믿으면 안 된다. 남는 경우가 있고, 그러면
       실행할 때마다 옛 노드가 쌓여 내가 만들지도 않은 노드의 컴파일 경고가 계속 나온다.
    """
    before = list_expressions(mat)
    mel.delete_all_material_expressions(mat)

    if before is None:
        #	목록을 못 읽는 버전. 지워졌는지는 **실행할 때마다 찍히는 노드 수**로 판단한다 —
        #	옛 노드가 남으면 그 수가 계속 불어난다.
        unreal.log("[Frame] 그래프를 비웠다 (노드 수는 확인 불가 — 5.8 은 Expressions 가 "
                   "protected 다. 아래 '완료' 줄의 노드 수가 실행마다 늘면 안 지워진 것이다)")
        return

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
                "[Frame] 노드 %d 개가 지워지지 않는다 — 머테리얼을 새로 만들어야 할 수 있다."
                % len(remaining))
            break

    unreal.log("[Frame] 그래프 정리 — 이전 %d 개, 남은 %d 개"
               % (len(before), len(list_expressions(mat) or [])))


# ══════════════════════════════════════════════════════════════════════
#  노드 만들기 — build_dissolve_material.py 와 같은 감싸개
# ══════════════════════════════════════════════════════════════════════

class Graph(object):
    def __init__(self, material):
        self.mat = material
        self.x = -2600
        self.y = 0
        self.count = 0
        self._shared = {}
        self.shared_hits = 0   # 상수를 새로 안 만들고 재사용한 횟수

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

    # ── 파라미터 ──
    def scalar(self, name, value, group="Frame"):
        n = self.node(unreal.MaterialExpressionScalarParameter)
        n.set_editor_property("parameter_name", name)
        n.set_editor_property("default_value", value)
        n.set_editor_property("group", group)
        return n

    def vector(self, name, color, group="Frame"):
        n = self.node(unreal.MaterialExpressionVectorParameter)
        n.set_editor_property("parameter_name", name)
        n.set_editor_property("default_value", color)
        n.set_editor_property("group", group)
        return n

    def tex_object(self, name, texture, group="Frame"):
        """
        표본을 21 번 뜬다. TextureSampleParameter2D 를 21 개 두면 파라미터가 21 벌
        생기므로, **오브젝트 파라미터 하나**를 두고 Sample 노드들이 나눠 쓴다.
        """
        n = self.node(unreal.MaterialExpressionTextureObjectParameter)
        n.set_editor_property("parameter_name", name)
        if texture:
            n.set_editor_property("texture", texture)
        n.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
        n.set_editor_property("group", group)
        return n

    #	상수는 **같은 값이면 노드 하나를 나눠 쓴다.** 출력 하나를 여러 입력에 물려도
    #	되기 때문이다. 안 그러면 0 · 1 · 0.5 같은 값이 표본마다 새로 생겨 그래프가
    #	똑같은 노드로 뒤덮인다 — Clamp 20 개만 해도 상수가 40 개 붙는다.
    def const(self, value):
        key = ("c1", value)
        if key in self._shared:
            self.shared_hits += 1
        else:
            n = self.node(unreal.MaterialExpressionConstant)
            n.set_editor_property("r", value)
            self._shared[key] = n
        return self._shared[key]

    def const2(self, a, b):
        key = ("c2", a, b)
        if key in self._shared:
            self.shared_hits += 1
        else:
            n = self.node(unreal.MaterialExpressionConstant2Vector)
            n.set_editor_property("r", a)
            n.set_editor_property("g", b)
            self._shared[key] = n
        return self._shared[key]

    # ── 연결 ──
    @staticmethod
    def link(src, src_pin, dst, dst_pins):
        """
        입력이 하나뿐인 노드(OneMinus · Abs · Sine · ComponentMask)는 핀에 이름이 없어
        빈 문자열이다. 후보를 순서대로 시도하고, 전부 실패하면 조용히 넘어가지 말고 멈춘다 —
        연결이 빠지면 컴파일 경고만 남고 머테리얼이 기본값으로 대체되어 원인을 찾기 어렵다.
        """
        for pin in dst_pins:
            if mel.connect_material_expressions(src, src_pin, dst, pin):
                return pin
        raise RuntimeError("연결 실패: %s -> %s (핀 후보 %s)"
                           % (src.get_name(), dst.get_name(), dst_pins))

    # ── 연산 ──
    #	⚠️ 연결이 실제로 꽂혔는지 되읽어 확인할 수 없다. 이항 노드의 A/B 는 protected 라
    #	   Python 에서 못 읽는다. 대신 그래프를 확실히 비우고 새로 만든다.
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

    def minimum(self, a, b, pa="", pb=""):
        return self._bin(unreal.MaterialExpressionMin, a, b, pa, pb)

    def maximum(self, a, b, pa="", pb=""):
        return self._bin(unreal.MaterialExpressionMax, a, b, pa, pb)

    def dot(self, a, b, pa="", pb=""):
        return self._bin(unreal.MaterialExpressionDotProduct, a, b, pa, pb)

    def mul_c(self, a, value, pa=""):
        """상수를 곱한다. Constant 노드를 따로 만드는 줄을 줄이려고 둔다."""
        return self.mul(a, self.const(value), pa)

    def _unary(self, cls, src, pin=""):
        n = self.node(cls)
        self.link(src, pin, n, ["", "Input"])
        return n

    def one_minus(self, src, pin=""):
        return self._unary(unreal.MaterialExpressionOneMinus, src, pin)

    def sine(self, src, pin=""):
        n = self._unary(unreal.MaterialExpressionSine, src, pin)
        #	UE 의 Sine 은 라디안이 아니라 **회전 수**를 받는다(Period 기준).
        n.set_editor_property("period", 1.0)
        return n

    def power(self, base, exponent, pb="", pe=""):
        n = self.node(unreal.MaterialExpressionPower)
        self.link(base, pb, n, ["Base"])
        self.link(exponent, pe, n, ["Exp", "Exponent"])
        return n

    def smoothstep(self, lo, hi, value, plo="", phi="", pv=""):
        n = self.node(unreal.MaterialExpressionSmoothStep)
        self.link(lo, plo, n, ["Min"])
        self.link(hi, phi, n, ["Max"])
        self.link(value, pv, n, ["Value"])
        return n

    def lerp(self, a, b, alpha, pa="", pb="", palpha=""):
        n = self.node(unreal.MaterialExpressionLinearInterpolate)
        self.link(a, pa, n, ["A"])
        self.link(b, pb, n, ["B"])
        self.link(alpha, palpha, n, ["Alpha"])
        return n

    def vec2(self, x_expr, y_expr):
        """
        스칼라 둘로 float2 를 만든다.

        ⚠️ AppendVector 를 쓰지 않는다. connect_material_expressions 가 B 핀에 True 를
           돌려주고도 실제로는 붙지 않아, 컴파일 때 "Missing AppendVector input B" 만
           남고 원인을 찾기가 매우 어려웠다(build_dissolve_material.py 에서 겪은 것).
           축 벡터에 스칼라를 곱해 더하면 Add · Multiply 만으로 같은 float2 가 나온다.
        """
        return self.add(
            self.mul(self.const2(1.0, 0.0), x_expr),
            self.mul(self.const2(0.0, 1.0), y_expr),
        )

    def texture_size(self, tex_obj):
        """
        텍스처의 픽셀 크기(float2). 표본 간격을 가로세로 같은 길이로 맞추는 데만 쓴다.

        ⚠️ 노드가 없거나 Python 에 안 열려 있는 버전이 있다. 그때는 조용히 넘어가지 말고
           **파라미터로 대체**한다 — 비율이 틀리면 외곽선이 눌린 타원이 되는데, 원인을
           모르면 머테리얼을 한참 뒤진다.
        """
        try:
            n = self.node(unreal.MaterialExpressionTextureProperty)
            n.set_editor_property(
                "property", unreal.MaterialExposedTextureProperty.TMTM_TEXTURE_SIZE)
            self.link(tex_obj, "", n, ["Texture", "TextureObject", ""])
            return n
        except Exception as err:
            unreal.log_warning(
                "[Frame] TextureProperty 를 못 만들었다 (%s) — FrameTexSize 파라미터로 "
                "대체한다. 카드 텍스처 해상도에 맞춰 직접 넣을 것." % err)
            return self.vector("FrameTexSize", unreal.LinearColor(512.0, 768.0, 0.0, 0.0))

    def clamp01(self, src, pin=""):
        """
        Saturate 노드는 엔진 버전마다 Python 노출이 들쭉날쭉하다. Min/Max 로 만든다 —
        이 둘은 A/B 핀 이름이 확실히 동작한다.
        """
        return self.minimum(self.maximum(src, self.const(0.0), pa=pin),
                            self.const(1.0))

    def clamp_uv(self, src, pin=""):
        """
        UV 를 0~1 로 가둔다. 표본이 텍스처 밖으로 나가면 반대쪽 그림이 딸려 온다.

        ⚠️ const_min · const_max 는 5.8 Python 에 안 열려 있다("Failed to find property
           'const_min'"). 핀에 상수를 물려야 한다. 입력 핀은 이름이 없다 — 'Input' 은
           실패하고 빈 문자열이 붙는다.

        ⚠️ 이 노드를 빼고 샘플러 주소 지정에 맡길 수 없다. sampler_source 역시 Python 에
           노출돼 있지 않아 Clamp 계열 공유 샘플러로 못 바꾼다.
        """
        n = self.node(unreal.MaterialExpressionClamp)
        self.link(self.const(0.0), "", n, ["Min"])
        self.link(self.const(1.0), "", n, ["Max"])
        self.link(src, pin, n, ["", "Input"])
        return n

    def mask(self, src, r=False, g=False, b=False, a=False, pin=""):
        n = self.node(unreal.MaterialExpressionComponentMask)
        n.set_editor_property("r", r)
        n.set_editor_property("g", g)
        n.set_editor_property("b", b)
        n.set_editor_property("a", a)
        self.link(src, pin, n, ["", "Input"])
        return n

    def sample(self, tex_obj, uv):
        """
        오브젝트 파라미터를 UV 한 곳에서 찍는다.

        표본이 21 개나 되지만 **텍스처 오브젝트가 하나**라 셰이더가 쓰는 텍스처와
        샘플러도 하나다. 표본마다 TextureSampleParameter2D 를 두면 그때는 21 벌이 되어
        머테리얼당 상한 16 을 넘긴다.
        """
        n = self.node(unreal.MaterialExpressionTextureSample)
        n.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
        self.link(tex_obj, "", n, ["Tex", "TextureObject"])
        self.link(uv, "", n, ["UVs"])
        return n


# ══════════════════════════════════════════════════════════════════════
#  실행
# ══════════════════════════════════════════════════════════════════════

def build():
    mat = ensure_material()
    if not mat:
        return

    #	⚠️ 머테리얼 에디터가 열려 있으면 안 된다. 에디터는 /Engine/Transient 의 프리뷰
    #	   사본에서 작업하기 때문에, 원본 노드를 지워도 에디터가 들고 있던 그래프가 다시
    #	   얹힌다 — 파라미터가 두 벌씩 생기거나 연결이 끊긴 노드가 남는다.
    subsystem = unreal.get_editor_subsystem(unreal.AssetEditorSubsystem)
    if subsystem:
        subsystem.close_all_editors_for_asset(mat)
        if editor_still_open(subsystem, mat):
            unreal.log_error(
                "[Frame] 머테리얼 에디터가 아직 열려 있다 — 닫고 다시 실행할 것.")
            return

    #	UI 도메인이 아니면 UMG 브러시가 못 쓴다. 반투명이 아니면 글로우가 배경과 안 섞인다.
    mat.set_editor_property("material_domain", unreal.MaterialDomain.MD_UI)
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)

    purge_expressions(mat)
    g = Graph(mat)

    preview_tex = unreal.load_asset(PREVIEW_TEX)
    if not preview_tex:
        unreal.log_warning("[Frame] 미리보기 텍스처가 없다: %s — 비워 둔 채로 만든다." % PREVIEW_TEX)

    # ── 파라미터 ──────────────────────────────────────────────────────
    #	이름은 BGCardWidget.h 의 것과 반드시 같아야 한다 — 코드가 미는 경로를 열어 둔다.
    #	  FrameTex          RefreshFrame 이 대분류별 테두리로 갈아 끼운다
    #	  HoverAmount       HoverParamName      — 호버 보간값 0~1
    #	  GlowColor         ColorParamName
    #	  GlowIntensity     IntensityParamName
    #	  GlowRingStrength  RingParamName       — 등급 연출이 쓰는 별도 구동값
    #
    #	⚠️ WidgetSize 는 두지 않는다. 위젯 픽셀 크기를 매 프레임 먹여야 하는 파라미터라
    #	   C++ 없이는 못 쓰는데, 여기서 필요한 것은 **가로세로 비율**뿐이고 그건
    #	   텍스처에서 바로 읽을 수 있다. UMG 애니메이션만으로 굴리려면 이 결합을 끊어야 한다.
    t_frame = g.tex_object("FrameTex", preview_tex)

    p_hover = g.scalar("HoverAmount", 0.0)
    p_ring = g.scalar("GlowRingStrength", 0.0)
    p_intensity = g.scalar("GlowIntensity", 2.5)
    p_width = g.scalar("GlowWidth", 0.05)
    p_falloff = g.scalar("GlowFalloff", 1.6)
    p_wave_amt = g.scalar("WaveAmount", 0.35)
    p_wave_freq = g.scalar("WaveFreq", 3.0)
    p_wave_speed = g.scalar("WaveSpeed", 0.6)
    p_center = g.vector("GlowCenter",
                        unreal.LinearColor(GLOW_CENTER[0], GLOW_CENTER[1], 0.0, 0.0))
    p_cut_soft = g.scalar("HoleCutSoft", HOLE_CUT_SOFT)

    p_color = g.vector("GlowColor", unreal.LinearColor(1.0, 0.82, 0.35, 1.0))

    # ── 원본 ─────────────────────────────────────────────────────────
    uv = g.node(unreal.MaterialExpressionTextureCoordinate)
    base = g.sample(t_frame, uv)

    # ── 표본 간격 ────────────────────────────────────────────────────
    #	GlowWidth 는 **카드 가로폭 대비 비율**이다(0.05 = 5%). 해상도를 안 타므로
    #	512 짜리 텍스처를 1024 로 다시 뽑아도 두께가 그대로다.
    #
    #	세로는 같은 비율로 두면 안 된다 — 카드가 세로로 길어서 위아래가 더 두껍게
    #	번진다. 가로세로 픽셀 수의 비를 곱해 **텍셀 기준으로 같은 거리**가 되게 맞춘다.
    #	  du = GlowWidth,  dv = GlowWidth × (W / H)
    tex_size = g.texture_size(t_frame)
    aspect = g.div(g.mask(tex_size, r=True), g.mask(tex_size, g=True))
    step = g.vec2(p_width, g.mul(p_width, aspect))

    def tap(dx, dy):
        """(dx, dy) 방향으로 step 만큼 밀어 찍은 표본."""
        offset = g.mul(step, g.const2(dx, dy))
        return g.sample(t_frame, g.clamp_uv(g.add(uv, offset)))

    #	표본 알파의 합. 가중이 전부 1 이라 곱셈 노드가 필요 없다 — 링 배치를 버린 덕이다.
    #
    #	⚠️ 첫 표본을 ComponentMask 로 스칼라화하지 말 것. TextureSample 의 **이름 없는
    #	   출력은 RGB(float3)** 라서 알파 마스크가 안 붙는다("Not enough components in
    #	   float3 for component mask 0001"). 처음 둘을 A 핀끼리 더해 시작한다.
    spiral = spiral_taps()
    taps = [tap(dx, dy) for dx, dy, _, _ in spiral]
    tap_sum = g.add(taps[0], taps[1], pa="A", pb="A")
    for s in taps[2:]:
        tap_sum = g.add(tap_sum, s, pb="A")

    #	불투명한 덩어리가 어느 쪽에 쏠려 있는지 — 방향 벡터를 알파로 가중해 더한 것.
    #	Σ A_i·(u_i·n) = (Σ A_i·u_i)·n 이므로 표본마다 내적하지 않고 float2 하나로 모은다.
    mass = None
    for s, (_, _, ux, uy) in zip(taps, spiral):
        term = g.mul(g.const2(ux, uy), s, pb="A")
        mass = term if mass is None else g.add(mass, term)

    # ── 흐린 알파 ────────────────────────────────────────────────────
    #	중심에 가중 2, 나머지 20 개는 1 씩. 합 22 로 나눈다.
    weighted = g.add(g.mul_c(base, W_CENTER, pa="A"), tap_sum)
    blur = g.div(weighted, g.const(W_TOTAL))

    # ── 외곽선 모양 ──────────────────────────────────────────────────
    #	흐린 알파를 **원래 알파가 0 인 자리에서만** 남긴다. 안쪽은 프레임 그림이
    #	덮으므로 빛낼 필요가 없고, 남겨 두면 테두리 안쪽이 뿌옇게 뜬다.
    #
    #	평균을 내면서 값이 통째로 낮아졌으므로 되올린 뒤 0~1 로 가둔다.
    outside = g.one_minus(base, pin="A")
    shape_raw = g.clamp01(g.mul_c(g.mul(blur, outside), SHAPE_BOOST))

    #	GlowFalloff 가 클수록 실루엣에 바짝 붙고, 1 에 가까울수록 멀리까지 옅게 퍼진다.
    shape = g.power(shape_raw, p_falloff)

    # ── 안쪽 구멍 죽이기 ─────────────────────────────────────────────
    #	위까지만 하면 **아트 창 둘레에도** 글로우가 낀다. 프레임이 가운데 뚫린 고리라
    #	알파 0 인 자리가 바깥만이 아니라 창이기도 하기 때문이다.
    #
    #	바깥 픽셀은 프레임이 기준점 쪽에 있고, 창 안 픽셀은 프레임이 기준점 반대쪽에
    #	있다. 그래서 쏠림(mass)을 기준점에서 멀어지는 방향에 투영한 부호로 가른다.
    #
    #	⚠️ 여기서 텍스처를 더 읽지 않는다. 외곽선용 표본을 그대로 재활용한다.
    to_out = g.sub(uv, g.mask(p_center, r=True, g=True))

    #	길이로 나눠 방향만 남긴다. Normalize 노드는 기준점에서 0/0 이 되므로 직접
    #	나눈다 — 기준점은 창 한복판이라 blur 가 0 이고 어차피 글로우가 없지만,
    #	NaN 이 한 픽셀이라도 나면 그 픽셀이 새까맣게 뚫린다.
    to_out_len = g.node(unreal.MaterialExpressionSquareRoot)
    g.link(g.dot(to_out, to_out), "", to_out_len, ["", "Input"])
    dir_out = g.div(to_out, g.add(to_out_len, g.const(0.0001)))

    #	-1 ~ 1 로 맞춘다. 그래야 HoleCutSoft 가 그림 밝기와 무관해진다.
    lean = g.div(g.dot(mass, dir_out), g.add(tap_sum, g.const(0.0001)))

    #	음수(프레임이 기준점 쪽) = 바깥이라 살린다. 양수 = 창이라 죽인다.
    shape = g.mul(shape, g.one_minus(
        g.smoothstep(g.const(0.0), p_cut_soft, lean)))

    # ── 파동 ─────────────────────────────────────────────────────────
    #	blur 는 바깥으로 갈수록 작아지므로 거리 대용으로 그대로 쓴다.
    #	위상 = blur*WaveFreq + Time*WaveSpeed 이면 시간이 갈수록 마루가 낮은 blur
    #	쪽으로, 즉 **바깥으로** 흘러간다.
    #
    #	⚠️ Sine 은 회전 수를 받는다(period=1). 라디안으로 넣으면 2π 배 빨라진다.
    time = g.node(unreal.MaterialExpressionTime)
    phase = g.add(g.mul(blur, p_wave_freq), g.mul(time, p_wave_speed))
    wave = g.add(g.mul_c(g.sine(phase), 0.5), g.const(0.5))

    #	WaveAmount 0 이면 파동 없이 매끈한 그라데이션, 1 이면 마루와 골이 다 드러난다.
    ripple = g.lerp(g.const(1.0), wave, p_wave_amt)

    # ── 구동값 ───────────────────────────────────────────────────────
    #	호버와 등급 연출이 각각 이 머테리얼을 켠다. 둘을 더하면 호버 중에 대성공이
    #	나면 두 배로 타므로 큰 쪽만 쓴다.
    drive = g.maximum(p_hover, p_ring)

    glow = g.mul(g.mul(g.mul(shape, ripple), p_intensity), drive)

    # ── 합성 ─────────────────────────────────────────────────────────
    #	색은 프레임 그림 위에 글로우를 더한다. 알파는 프레임 실루엣 + 글로우가
    #	번진 만큼 — 이게 없으면 실루엣 밖은 알파 0 이라 아무리 밝아도 안 보인다.
    emissive = g.add(base, g.mul(p_color, glow), pa="RGB")
    opacity = g.clamp01(g.add(base, glow, pa="A"))

    mel.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY)

    mel.layout_material_expressions(mat)
    mel.recompile_material(mat)
    eal.save_asset(MATERIAL_PATH)

    ensure_instance(mat)

    #	겹친 그래프가 남아 있으면 여기서 개수가 안 맞는다. 세어 두면 다음에 바로 안다.
    unreal.log("[Frame] 완료 — 노드 %d 개, 표본 %d 개(원본 1 · 외곽선 %d), 파라미터 12 개"
               % (g.count, 1 + TAP_COUNT, TAP_COUNT))
    unreal.log("[Frame] 상수 노드 %d 개 — 같은 값 요청 %d 회를 재사용해 그만큼 안 만들었다"
               % (len(g._shared), g.shared_hits))
    unreal.log("[Frame] 다음: MI_BGCardFrame_Default 에서 HoverAmount 를 0→1 로 끌어 "
               "외곽선을 확인하고, WBP_BGCard 의 Border_Root 브러시에 이 MI 를 물릴 것")
    unreal.log("[Frame] ⚠️ T_Card_* 에 투명 여백이 없으면 외곽선이 텍스처 경계에서 잘린다 "
               "— 가로폭의 GlowWidth(기본 5%) 만큼 여백을 두고 다시 뽑을 것")


build()
