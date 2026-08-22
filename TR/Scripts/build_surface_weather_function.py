# -*- coding: utf-8 -*-
"""MF_TR_SurfaceWeather — 표면에 눈과 물을 덧씌우는 머테리얼 함수.

실행: 에디터 > Tools > Execute Python Script..., 또는 Remote Execution.
여러 번 실행해도 안전하다.

**에셋을 지우지 않는다.** 노드만 하나씩 지우고 다시 만든다. 에셋을 지우면
M_TR_FloorBiome 의 함수 참조가 끊어져서 땅이 새까매진다. 한 번 겪었다.

**입출력 이름과 개수를 바꾸지 말 것.** M_TR_FloorBiome 의 호출 노드가 이름으로
붙어 있어서, 이름이 바뀌면 그 연결이 조용히 끊어진다.

  IN  BaseColor / Normal / Roughness   원래 머테리얼이 만들던 값
      SnowAmount / Wetness             0~1
      NoisePosition                    얼룩을 뿌릴 좌표
  OUT BaseColor / Normal / Roughness
"""

import unreal

MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary

PACKAGE_PATH = "/Game/Developers/start/Material"
NAME = "MF_TR_SurfaceWeather"
TAG = "SurfaceWeather"
GROUP = "Weather"

V3 = unreal.FunctionInputType.FUNCTION_INPUT_VECTOR3
S = unreal.FunctionInputType.FUNCTION_INPUT_SCALAR

IN_BASE_COLOR, IN_NORMAL, IN_ROUGHNESS = "BaseColor", "Normal", "Roughness"
IN_SNOW, IN_WET, IN_NOISE_POSITION = "SnowAmount", "Wetness", "NoisePosition"

# ── 룩 기본값 ───────────────────────────────────────────────────────────────
SNOW_NOISE_SCALE = 0.0004
SNOW_SHARPNESS = 4.0
SNOW_NOISE_WEIGHT = 0.45
SNOW_COLOR = (0.90, 0.93, 1.00)
SNOW_ROUGHNESS = 0.80
SNOW_FLATTEN = 0.85

# 높이 블렌드: 베이스 컬러의 밝기를 높이로 본다. 어두운 곳 = 골짜기 = 먼저 찬다.
SNOW_HEIGHT_BLEND = 0.60     # 0 = 예전대로 위를 보는 면 기준, 1 = 완전히 높이 기준
PUDDLE_HEIGHT_BLEND = 0.75   # 물은 무조건 낮은 곳에 고인다

# 매크로 변주: 아주 낮은 주파수로 눈의 양 자체를 지역마다 바꾼다.
SNOW_MACRO_SCALE = 0.00004
SNOW_MACRO_WEIGHT = 0.35

# 가장자리 침식: 덮개 경계에서만 고주파 노이즈로 갉아낸다.
EDGE_NOISE_SCALE = 0.004
SNOW_EDGE_EROSION = 0.50
PUDDLE_EDGE_EROSION = 0.35

PUDDLE_NOISE_SCALE = 0.0008
PUDDLE_SHARPNESS = 6.0
PUDDLE_NOISE_WEIGHT = 0.45
WET_DARKEN = 0.55
PUDDLE_DARKEN = 0.35
WET_ROUGHNESS = 0.45
PUDDLE_ROUGHNESS = 0.05
PUDDLE_FLATTEN = 0.95

# 핀 이름은 C++ 프로퍼티 이름 그대로가 아니다. 엔진이 몇 개를 줄인다.
PIN_ALIASES = {
    "Input": "", "VectorInput": "", "Coordinates": "UVs",
    "Position": "World Position", "Exponent": "Exp",
}


def log(message):
    unreal.log("[{}] {}".format(TAG, message))


class Graph(object):
    """머테리얼 함수 안에 노드를 만들고 잇는다. 못 이으면 예외를 던진다.

    조용히 넘기면 컴파일은 멀쩡히 되고 화면만 이상해져서, 나중에 값이나 텍스처를
    의심하게 된다. 찾는 데 한나절이 든다.
    """

    def __init__(self, function):
        self.function = function

    def node(self, cls, x, y):
        return MEL.create_material_expression_in_function(self.function, cls, x, y)

    def link(self, src, dst, dst_in, src_out=""):
        names = [dst_in]
        alias = PIN_ALIASES.get(dst_in)
        if alias is not None:
            names.append(alias)
        for candidate in names:
            if MEL.connect_material_expressions(src, src_out, dst, candidate):
                return dst
        raise RuntimeError("연결 실패: {}.{} -> {}.{}".format(
            src.get_class().get_name(), src_out or "(기본)",
            dst.get_class().get_name(), dst_in))

    # ── 상수와 파라미터 ─────────────────────────────────────────────────────

    def scalar(self, name, default, x, y):
        n = self.node(unreal.MaterialExpressionScalarParameter, x, y)
        n.set_editor_property("parameter_name", unreal.Name(name))
        n.set_editor_property("default_value", default)
        n.set_editor_property("group", unreal.Name(GROUP))
        return n

    def vector_param(self, name, value, x, y):
        n = self.node(unreal.MaterialExpressionVectorParameter, x, y)
        n.set_editor_property("parameter_name", unreal.Name(name))
        n.set_editor_property("default_value",
                              unreal.LinearColor(value[0], value[1], value[2], 1.0))
        n.set_editor_property("group", unreal.Name(GROUP))
        return n

    def constant(self, value, x, y):
        n = self.node(unreal.MaterialExpressionConstant, x, y)
        n.set_editor_property("r", value)
        return n

    def constant3(self, value, x, y):
        n = self.node(unreal.MaterialExpressionConstant3Vector, x, y)
        n.set_editor_property("constant",
                              unreal.LinearColor(value[0], value[1], value[2], 1.0))
        return n

    # ── 연산 ────────────────────────────────────────────────────────────────

    def binop(self, cls, a, b, x, y, const_b=None, a_out="", b_out=""):
        n = self.node(cls, x, y)
        self.link(a, n, "A", a_out)
        if b is not None:
            self.link(b, n, "B", b_out)
        elif const_b is not None:
            n.set_editor_property("const_b", const_b)
        return n

    def mul(self, a, b, x, y, const_b=None, a_out="", b_out=""):
        return self.binop(unreal.MaterialExpressionMultiply, a, b, x, y, const_b, a_out, b_out)

    def add(self, a, b, x, y, const_b=None, a_out="", b_out=""):
        return self.binop(unreal.MaterialExpressionAdd, a, b, x, y, const_b, a_out, b_out)

    def sub(self, a, b, x, y, const_b=None, a_out="", b_out=""):
        return self.binop(unreal.MaterialExpressionSubtract, a, b, x, y, const_b, a_out, b_out)

    def dot(self, a, b, x, y, a_out="", b_out=""):
        return self.binop(unreal.MaterialExpressionDotProduct, a, b, x, y, None, a_out, b_out)

    def lerp(self, a, b, alpha, x, y, a_out="", b_out="", alpha_out=""):
        n = self.node(unreal.MaterialExpressionLinearInterpolate, x, y)
        self.link(a, n, "A", a_out)
        self.link(b, n, "B", b_out)
        self.link(alpha, n, "Alpha", alpha_out)
        return n

    def append(self, a, b, x, y, a_out="", b_out=""):
        n = self.node(unreal.MaterialExpressionAppendVector, x, y)
        self.link(a, n, "A", a_out)
        self.link(b, n, "B", b_out)
        return n

    # ── 이름 없는 핀을 피해 가는 것들 ───────────────────────────────────────
    #
    # Saturate / OneMinus / ComponentMask 는 입력이 하나뿐이라 그래프에 핀 이름이
    # 안 뜬다. 함수 안에서는 붙었다고 응답만 하고 실제로는 안 붙는다. 그러면
    # 컴파일 단계에서 "Missing Saturate input" 이 뜬다. A/B 핀만 쓰는 노드로 다시 짠다.

    def clamp01(self, a, x, y, a_out=""):
        lower = self.binop(unreal.MaterialExpressionMax, a, None, x, y,
                           const_b=0.0, a_out=a_out)
        return self.binop(unreal.MaterialExpressionMin, lower, None, x + 170, y,
                          const_b=1.0)

    def invert(self, a, x, y, a_out=""):
        return self.sub(self.constant(1.0, x, y - 120), a, x + 170, y, b_out=a_out)

    def component_z(self, vector, x, y, src_out=""):
        return self.dot(vector, self.constant3((0.0, 0.0, 1.0), x, y + 120),
                        x + 170, y, a_out=src_out)

    def luminance(self, color, x, y, src_out=""):
        """색의 밝기. 높이맵이 없어서 이걸 높이 대신 쓴다."""
        return self.dot(color, self.constant3((0.3333, 0.3333, 0.3333), x, y + 120),
                        x + 170, y, a_out=src_out)

    def noise(self, position, x, y, levels=2, turbulence=False):
        n = self.node(unreal.MaterialExpressionNoise, x, y)
        self.link(position, n, "Position")
        n.set_editor_property("scale", 1.0)
        n.set_editor_property("output_min", 0.0)
        n.set_editor_property("output_max", 1.0)
        n.set_editor_property("levels", levels)
        n.set_editor_property("turbulence", turbulence)
        return n

    # ── 함수 입출력 ─────────────────────────────────────────────────────────

    def function_input(self, name, input_type, x, y, sort_priority=0,
                       default=None, description=""):
        n = self.node(unreal.MaterialExpressionFunctionInput, x, y)
        n.set_editor_property("input_name", unreal.Name(name))
        n.set_editor_property("input_type", input_type)
        n.set_editor_property("sort_priority", sort_priority)
        n.set_editor_property("description", description)

        if default is not None:
            # Vector4f 는 생성자로 값을 못 넣는다. 빈 걸 만들어 필드를 채운다.
            try:
                value = unreal.Vector4f()
                if isinstance(default, (tuple, list)):
                    for axis, v in zip(("x", "y", "z"), default):
                        value.set_editor_property(axis, float(v))
                else:
                    value.set_editor_property("x", float(default))
                n.set_editor_property("preview_value", value)
                n.set_editor_property("use_preview_value_as_default", True)
            except Exception as error:
                unreal.log_warning(
                    "[{}] '{}' 기본값을 못 넣었다: {}. 이 입력은 반드시 연결해야 한다."
                    .format(TAG, name, error))
        return n

    def function_output(self, name, src, x, y, sort_priority=0, src_out=""):
        n = self.node(unreal.MaterialExpressionFunctionOutput, x, y)
        n.set_editor_property("output_name", unreal.Name(name))
        n.set_editor_property("sort_priority", sort_priority)
        # 출력의 입력은 이름 없는 핀이다. "Input" -> "" 별칭을 타야 붙는다.
        self.link(src, n, "Input", src_out)
        return n


# ── 조각들 ──────────────────────────────────────────────────────────────────

def build_upward_facing(g, normal_input):
    """이 픽셀이 얼마나 위를 보는가(0~1). 두 층을 곱한다.

      1. 메시가 월드에서 위를 보는 정도 — 바위 옆구리에 눈이 발리는 것을 막는다.
      2. 노멀맵이 말하는 국소 평탄도.

    **바닥에서는 1번이 항상 1 이라 2번만 남는다.** 타일링 노멀맵의 Z 는 대부분
    0.9~1.0 에 몰려 있어서 값이 거의 균일하다. 그래서 바닥은 이 값만으로는
    눈이 "차오르지" 않고 화면 전체가 동시에 하얘진다. 높이 블렌드를 넣은 이유다.
    """
    geometry_up = g.clamp01(
        g.component_z(g.node(unreal.MaterialExpressionVertexNormalWS, -2100, -480),
                      -1920, -480),
        -1580, -480)
    detail_up = g.clamp01(g.component_z(normal_input, -1920, -240), -1580, -240)
    return g.mul(geometry_up, detail_up, -1240, -370)


def build_blotches(g, noise_position, scale_param, weight_param, bias, x, y):
    """얼룩 노이즈를 0~1 덮개 순서값으로 만든다.

    bias 는 '어디부터 덮이는가'다. 얼룩을 weight 만큼 섞어서 경계가 자로 잰 듯
    반듯해지지 않게 한다.
    """
    # Noise 의 Scale 은 프로퍼티라 인스턴스에서 못 만진다. 좌표에 파라미터를 곱해서
    # 넣는다 — noise(p * s) 는 scale=s 인 noise(p) 와 같다.
    scaled = g.mul(noise_position, scale_param, x, y)
    blotch = g.noise(scaled, x + 200, y)
    mixed = g.add(
        g.mul(blotch, weight_param, x + 400, y),
        g.mul(bias, g.invert(weight_param, x + 400, y + 200), x + 750, y + 160),
        x + 920, y + 60)
    return g.clamp01(mixed, x + 1100, y + 60)


def build_coverage(g, amount, field, sharpness_param, x, y):
    """임계값 덮개.

        coverage = saturate((field - (1 - amount)) * sharpness)

    amount 가 0 이면 임계값이 1 이라 어디도 안 덮이고, 1 이면 0 이라 전부 덮인다.
    그 사이에서는 field 가 큰 곳부터 순서대로 덮인다.
    """
    threshold = g.invert(amount, x, y + 160)
    return g.clamp01(
        g.mul(g.sub(field, threshold, x + 400, y), sharpness_param, x + 570, y),
        x + 740, y)


def build_edge_erosion(g, coverage, fine_noise, strength_param, x, y):
    """덮개 경계에서만 고주파 노이즈로 갉아낸다.

        band = coverage * (1 - coverage) * 4      경계에서만 1 인 띠
        out  = saturate(coverage - band * noise * strength)

    실제 눈 가장자리는 얇고 부서져 있다. saturate 한 방으로 자르면 스티커가 된다.
    band 를 쓰면 완전히 덮인 곳과 안 덮인 곳은 안 건드리고 경계만 부서진다.
    """
    band = g.mul(
        g.mul(coverage, g.invert(coverage, x, y + 200), x + 340, y),
        None, x + 510, y, const_b=4.0)
    bite = g.mul(g.mul(band, fine_noise, x + 680, y), strength_param, x + 850, y)
    return g.clamp01(g.sub(coverage, bite, x + 1020, y), x + 1190, y)


def build_network(g, pins):
    upward = build_upward_facing(g, pins[IN_NORMAL])

    # ── 높이 ────────────────────────────────────────────────────────────────
    #
    # 높이맵이 없어서 베이스 컬러의 밝기를 높이로 쓴다. 자갈·바위 텍스처는 골짜기에
    # AO 가 구워져 있어서 어두운 곳이 낮은 곳이다. 눈과 물은 낮은 곳부터 찬다.
    cavity = g.invert(g.luminance(pins[IN_BASE_COLOR], -1920, 0), -1580, 0)

    snow_bias = g.lerp(upward, cavity,
                       g.scalar("SnowHeightBlend", SNOW_HEIGHT_BLEND, -1240, 140),
                       -1000, 40)
    puddle_bias = g.lerp(g.invert(upward, -1240, 400), cavity,
                         g.scalar("PuddleHeightBlend", PUDDLE_HEIGHT_BLEND, -1240, 560),
                         -1000, 440)

    # ── 매크로 변주 ─────────────────────────────────────────────────────────
    #
    # 아주 낮은 주파수로 눈의 양 자체를 지역마다 바꾼다. 이게 없으면 어디나 같은
    # 두께로 깔려서 담요를 덮은 것처럼 보인다.
    #
    # SnowAmount 에 곱하므로 0 이면 여전히 0 이다 — "눈 없음"이 깨지지 않는다.
    macro = g.noise(
        g.mul(pins[IN_NOISE_POSITION],
              g.scalar("SnowMacroScale", SNOW_MACRO_SCALE, -1920, 900), -1580, 820),
        -1380, 820, levels=2)
    macro_mul = g.lerp(g.constant(1.0, -1380, 1040),
                       g.mul(macro, None, -1380, 1160, const_b=2.0),
                       g.scalar("SnowMacroWeight", SNOW_MACRO_WEIGHT, -1380, 1280),
                       -1100, 1080)
    snow_amount = g.clamp01(g.mul(pins[IN_SNOW], macro_mul, -880, 1000), -700, 1000)

    # 경계 침식에 쓸 고주파 노이즈. 눈과 물이 같이 쓴다 — Noise 는 비싸다.
    fine = g.noise(
        g.mul(pins[IN_NOISE_POSITION],
              g.scalar("EdgeNoiseScale", EDGE_NOISE_SCALE, -1920, 1500), -1580, 1420),
        -1380, 1420, levels=3, turbulence=True)

    # ── 덮개 ────────────────────────────────────────────────────────────────
    snow_raw = build_coverage(
        g, snow_amount,
        build_blotches(
            g, pins[IN_NOISE_POSITION],
            g.scalar("SnowNoiseScale", SNOW_NOISE_SCALE, -520, 200),
            g.scalar("SnowNoiseWeight", SNOW_NOISE_WEIGHT, -520, 320),
            snow_bias, -300, 100),
        g.scalar("SnowSharpness", SNOW_SHARPNESS, 900, -20), 900, 100)
    snow_coverage = build_edge_erosion(
        g, snow_raw, fine,
        g.scalar("SnowEdgeErosion", SNOW_EDGE_EROSION, 1700, 300), 1900, 100)

    puddle_raw = build_coverage(
        g, pins[IN_WET],
        build_blotches(
            g, pins[IN_NOISE_POSITION],
            g.scalar("PuddleNoiseScale", PUDDLE_NOISE_SCALE, -520, 1900),
            g.scalar("PuddleNoiseWeight", PUDDLE_NOISE_WEIGHT, -520, 2020),
            puddle_bias, -300, 1800),
        g.scalar("PuddleSharpness", PUDDLE_SHARPNESS, 900, 1680), 900, 1800)
    puddle = build_edge_erosion(
        g, puddle_raw, fine,
        g.scalar("PuddleEdgeErosion", PUDDLE_EDGE_EROSION, 1700, 2000), 1900, 1800)

    # ── 베이스 컬러 ─────────────────────────────────────────────────────────
    #
    # 젖은 표면은 색이 어두워진다. 젖은 곳을 회색으로 덮는 게 아니라 원래 색을 눌러야
    # 젖어 보인다.
    wet_darken = g.lerp(g.constant(1.0, 3300, -700),
                        g.scalar("WetDarken", WET_DARKEN, 3300, -580),
                        pins[IN_WET], 3500, -650)
    puddle_darken = g.lerp(g.constant(1.0, 3300, -440),
                           g.scalar("PuddleDarken", PUDDLE_DARKEN, 3300, -320),
                           puddle, 3500, -390)
    wet_color = g.mul(pins[IN_BASE_COLOR],
                      g.mul(wet_darken, puddle_darken, 3700, -520), 3900, -640)

    # VectorParameter 의 기본 출력은 알파까지 붙은 4채널이다. 3채널인 색과 Lerp 하면
    # 채널 수가 안 맞아 컴파일이 막힌다. R/G/B 를 Append 로 다시 묶는다 — Append 는
    # A/B 핀만 써서 함수 안에서도 확실히 붙는다.
    snow_param = g.vector_param("SnowColor", SNOW_COLOR, 3300, -960)
    snow_color = g.append(
        g.append(snow_param, snow_param, 3500, -960, a_out="R", b_out="G"),
        snow_param, 3700, -900, b_out="B")

    # 눈은 물 위에 덮는다. 순서를 뒤집으면 눈밭 위에 웅덩이가 비쳐 진창이 된다.
    out_color = g.lerp(wet_color, snow_color, snow_coverage, 4100, -700)

    # ── 노멀 ────────────────────────────────────────────────────────────────
    #
    # 눈도 물도 요철을 메운다. 덮인 곳의 노멀을 평평한 (0,0,1) 쪽으로 당긴다.
    flat_normal = g.constant3((0.0, 0.0, 1.0), 3600, -100)
    wet_normal = g.lerp(
        pins[IN_NORMAL], flat_normal,
        g.mul(puddle, g.scalar("PuddleFlatten", PUDDLE_FLATTEN, 3600, 100), 3800, 40),
        4100, -100)
    out_normal = g.lerp(
        wet_normal, flat_normal,
        g.mul(snow_coverage, g.scalar("SnowFlatten", SNOW_FLATTEN, 4100, 240), 4300, 180),
        4500, 0)

    # ── 러프니스 ────────────────────────────────────────────────────────────
    #
    # 여기가 젖은 느낌의 대부분을 만든다. 색만 어둡게 하면 그냥 어두운 땅이고,
    # 러프니스가 떨어져야 빛이 늘어지며 젖어 보인다.
    wet_rough = g.lerp(pins[IN_ROUGHNESS],
                       g.scalar("WetRoughness", WET_ROUGHNESS, 3600, 700),
                       pins[IN_WET], 3800, 620)
    puddle_rough = g.lerp(wet_rough,
                          g.scalar("PuddleRoughness", PUDDLE_ROUGHNESS, 3800, 860),
                          puddle, 4100, 700)
    out_rough = g.lerp(puddle_rough,
                       g.scalar("SnowRoughness", SNOW_ROUGHNESS, 4100, 940),
                       snow_coverage, 4300, 780)

    return out_color, out_normal, out_rough


def build_inputs(g):
    return {
        IN_BASE_COLOR: g.function_input(
            IN_BASE_COLOR, V3, -2500, -500, 0,
            description="눈과 물을 얹기 전의 색. 밝기를 높이 대신 쓴다."),
        IN_NORMAL: g.function_input(
            IN_NORMAL, V3, -2500, -320, 1, default=(0.0, 0.0, 1.0),
            description="탄젠트 공간 노멀. 배수가 곱해지지 않은 것이어야 한다."),
        IN_ROUGHNESS: g.function_input(
            IN_ROUGHNESS, S, -2500, -140, 2, default=0.5,
            description="눈과 물을 얹기 전의 러프니스."),
        IN_SNOW: g.function_input(
            IN_SNOW, S, -2500, 40, 3, default=0.0,
            description="쌓인 눈의 양(0~1)."),
        IN_WET: g.function_input(
            IN_WET, S, -2500, 220, 4, default=0.0,
            description="젖은 정도(0~1)."),
        # 여기만 기본값이 없다. 지면은 흐르는 Panner 좌표, 소품은 LocalPosition 을
        # 줘야 한다. 틀리면 무늬가 표면 위를 기어다닌다 — 조용히 넘기지 않는다.
        IN_NOISE_POSITION: g.function_input(
            IN_NOISE_POSITION, V3, -2500, 400, 5,
            description="얼룩을 뿌릴 좌표. 지면은 Panner, 소품은 LocalPosition."),
    }


def clear_expressions(function):
    """노드를 하나씩 지운다.

    delete_all_material_expressions_in_function 은 조용히 아무것도 안 지우는 경우가
    있다. 그러면 실행할 때마다 노드가 한 벌씩 쌓이고, 같은 이름의 파라미터가 여러 개
    생겨서 어느 쪽에 값이 붙을지 알 수 없게 된다. 세어 가며 지우고 확인한다.
    """
    before = len(MEL.get_material_function_expressions(function))
    for expression in list(MEL.get_material_function_expressions(function)):
        MEL.delete_material_expression_in_function(function, expression)

    remaining = len(MEL.get_material_function_expressions(function))
    if remaining:
        raise RuntimeError(
            "노드를 다 못 지웠다({}개 남음). 그대로 만들면 중복이 쌓인다.".format(remaining))

    log("기존 노드 {}개를 지웠다.".format(before))


# ── 호출 쪽 다시 잇기 ───────────────────────────────────────────────────────

FLOOR_MATERIAL_PATH = "/Game/Developers/start/Material/M_TR_FloorBiome"

# 호출 노드의 핀은 그래프에서 타입 꼬리표를 달고 보인다("BaseColor (V3)").
PIN_TYPE_SUFFIXES = ("", " (V3)", " (S)", " (V2)", " (V4)", " (B)")


def relink_floor_material():
    """M_TR_FloorBiome 의 함수 호출 노드를 다시 잇는다.

    **이 함수의 노드를 새로 만들면 호출 쪽 연결이 전부 끊어진다.** 호출 노드는 입력을
    이름이 아니라 ExpressionInputId(GUID)로 물고 있어서, FunctionInput 을 새로 만들면
    이름이 같아도 GUID 가 달라져 다 떨어진다. 이름만 지키면 된다고 믿고 한 번 통째로
    끊어 먹었다 — 셰이더 명령이 581 에서 186 으로 떨어져서야 알았다.

    그래서 함수를 새로 만든 직후 여기서 다시 잇는다. 이걸 빼면 이 스크립트는 돌릴
    때마다 땅을 망가뜨린다.

    노드는 **구조로 찾는다.** 위치로 찾으면 레이아웃을 고칠 때마다 조용히 어긋난다.
    """
    if not EAL.does_asset_exist(FLOOR_MATERIAL_PATH):
        unreal.log_warning("[{}] {} 이 없다. 다시 잇기를 건너뛴다."
                           .format(TAG, FLOOR_MATERIAL_PATH))
        return

    material = EAL.load_asset(FLOOR_MATERIAL_PATH)
    exprs = list(MEL.get_material_expressions(material))

    def kind(e):
        return e.get_class().get_name().replace("MaterialExpression", "")

    def param(e):
        try:
            return str(e.get_editor_property("parameter_name"))
        except Exception:
            return ""

    def inputs(e):
        return [i for i in MEL.get_inputs_for_material_expression(material, e) if i]

    def lerp_of(a_name, b_name):
        """A/B 가 그 이름의 파라미터인 Lerp. 바이옴 A/B 를 섞는 노드다."""
        for e in exprs:
            if kind(e) == "LinearInterpolate":
                names = {param(i) for i in inputs(e)}
                if a_name in names and b_name in names:
                    return e
        return None

    def multiply_over_textures(a_tex, b_tex):
        """텍스처 A/B 를 섞는 Lerp 를 받는 Multiply. 매크로 변주 x 텍스처."""
        for e in exprs:
            if kind(e) != "Multiply":
                continue
            for i in inputs(e):
                if kind(i) != "LinearInterpolate":
                    continue
                names = {param(t) for t in inputs(i)
                         if kind(t) == "TextureSampleParameter2D"}
                if a_tex in names and b_tex in names:
                    return e
        return None

    calls = [e for e in exprs if kind(e) == "MaterialFunctionCall"]
    if len(calls) != 1:
        raise RuntimeError("{} 의 함수 호출 노드가 {}개다. 하나여야 한다."
                           .format(FLOOR_MATERIAL_PATH, len(calls)))
    call = calls[0]

    sources = {
        IN_BASE_COLOR: multiply_over_textures("TextureBase", "TextureBaseB"),
        IN_NORMAL: multiply_over_textures("TextureNormal", "TextureNormalB"),
        IN_ROUGHNESS: next((e for e in exprs if kind(e) == "ScalarParameter"
                            and param(e) == "Roughness"), None),
        IN_SNOW: lerp_of("SnowAmount", "SnowAmountB"),
        IN_WET: lerp_of("Wetness", "WetnessB"),
        # Panner 를 받는 Append 가 여럿이지만 전부 Append(Panner.xy, 0) 으로 값이 같다.
        IN_NOISE_POSITION: next((e for e in exprs if kind(e) == "AppendVector"
                                 and any(kind(i) == "Panner" for i in inputs(e))), None),
    }

    missing = sorted(name for name, node in sources.items() if node is None)
    if missing:
        raise RuntimeError(
            "먹일 노드를 못 찾았다: {}. 땅이 끊긴 채로 남는다 — 손으로 이어야 한다."
            .format(missing))

    for name, src in sources.items():
        if not any(MEL.connect_material_expressions(src, "", call, name + suffix)
                   for suffix in PIN_TYPE_SUFFIXES):
            raise RuntimeError("입력 '{}' 을 다시 잇지 못했다.".format(name))

    # Normal 은 함수를 거친 뒤에 NormalMultiplier 를 곱해서 루트로 간다. 배수를 함수
    # 앞에서 곱하면 노멀의 Z 가 1 을 넘어 '어디나 완전히 평평'이 되고 눈이 균일해진다.
    normal_mul = next((e for e in exprs if kind(e) == "Multiply"
                       and any(param(i) == "NormalMultiplier" for i in inputs(e))), None)
    if normal_mul is None:
        raise RuntimeError("NormalMultiplier 를 곱하는 Multiply 를 못 찾았다.")

    steps = (
        (lambda: MEL.connect_material_expressions(call, "Normal", normal_mul, "A"),
         "MF.Normal -> Multiply.A"),
        (lambda: MEL.connect_material_property(call, "BaseColor",
                                               unreal.MaterialProperty.MP_BASE_COLOR),
         "MF.BaseColor -> BaseColor"),
        (lambda: MEL.connect_material_property(call, "Roughness",
                                               unreal.MaterialProperty.MP_ROUGHNESS),
         "MF.Roughness -> Roughness"),
        (lambda: MEL.connect_material_property(normal_mul, "",
                                               unreal.MaterialProperty.MP_NORMAL),
         "Multiply -> Normal"),
    )
    for action, label in steps:
        if not action():
            raise RuntimeError("{} 연결 실패.".format(label))

    # 출력에 안 닿는 노드를 지운다. 위를 다 이은 뒤라야 쓰는 노드가 안 지워진다.
    MEL.delete_unused_expressions(material)
    MEL.recompile_material(material)
    EAL.save_loaded_asset(material)

    connected = len(inputs(call))
    if connected != len(sources):
        raise RuntimeError("다시 이었는데 입력이 {}/{} 개만 붙었다."
                           .format(connected, len(sources)))

    stats = MEL.get_statistics(material)
    log("M_TR_FloorBiome 다시 이었다: 입력 {}/{}, 노드 {}개, 픽셀 셰이더 {}명령"
        .format(connected, len(sources),
                len(MEL.get_material_expressions(material)),
                stats.num_pixel_shader_instructions))


def main():
    full_path = "{}/{}".format(PACKAGE_PATH, NAME)
    if not EAL.does_asset_exist(full_path):
        raise RuntimeError(
            "{} 이 없다. 이 스크립트는 에셋을 새로 만들지 않는다 — 새로 만들면 "
            "M_TR_FloorBiome 의 참조가 끊어진다.".format(full_path))

    function = EAL.load_asset(full_path)
    clear_expressions(function)

    g = Graph(function)
    pins = build_inputs(g)
    out_color, out_normal, out_rough = build_network(g, pins)

    g.function_output("BaseColor", out_color, 4800, -700, 0)
    g.function_output("Normal", out_normal, 4800, 0, 1)
    g.function_output("Roughness", out_rough, 4800, 780, 2)

    MEL.update_material_function(function)
    EAL.save_loaded_asset(function)
    log("함수 완료: 노드 {}개".format(len(MEL.get_material_function_expressions(function))))

    relink_floor_material()


main()
