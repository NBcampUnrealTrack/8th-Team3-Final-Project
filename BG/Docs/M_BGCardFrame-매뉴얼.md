# M_BGCardFrame — 카드 테두리 외곽선 글로우 매뉴얼

**대상 애셋**: `/Game/UI/Material/M_BGCardFrame` · `/Game/UI/Material/MI_BGCardFrame_Default`
**생성 스크립트**: `BG/Scripts/Python/build_frame_material.py`
**붙는 곳**: `WBP_BGCard` 의 `Border_Root` 브러시

---

## 한 줄

> 텍스처 한 장을 받아 그대로 그리고, **그 텍스처의 알파에서** 바깥으로 옅어지는 외곽선 글로우를 뽑는다. 코드가 테두리 모양을 알 필요가 없다.

카드 종류가 늘어도 `FrameTex` 에 그림만 꽂으면 된다. 글로우 모양은 그림이 스스로 정한다.

---

## 0. 왜 리테이너가 아니라 여기인가

| 효과 | 필요한 재료 | 있어야 할 곳 |
|---|---|---|
| 퇴장 디졸브 | 카드 전체(글씨까지 같이 타야 함) | `Retainer_Card` (`M_BGCardDissolve`) |
| **호버 외곽선 글로우** | **프레임 실루엣만** | **`Border_Root` (이 머테리얼)** |
| 등급 글로우 | 프레임 실루엣만 | `Border_Root` (이 머테리얼) |
| 아트 홀로 · 포일 | 그 이미지 한 장 | `Img_Art` |

리테이너는 자식 트리를 렌더 타겟에 한 번 그린 뒤 그 결과에 머테리얼을 먹인다. **인스턴스마다 RT 를 하나씩 잡으므로** 손패 7 장이면 RT 7 개다. 그래서 평상시에는 꺼 두고(`SetRetainRendering(false)`) 퇴장할 때만 켜는데, 호버 글로우처럼 **상시로 도는 연출**을 거기 얹으면 그 절약이 통째로 사라진다.

외곽선은 프레임 실루엣만 있으면 그릴 수 있고 그 실루엣은 `Border_Root` 브러시 텍스처에 이미 들어 있다. 그래서 RT 없이 여기서 처리한다 — 다른 UI 와 배칭되고, 상시로 켜 둬도 공짜다.

---

## 1. 만들기

에디터에서 **Tools ▸ Execute Python Script** 로 `BG/Scripts/Python/build_frame_material.py` 를 지정한다. 커맨드라인이면:

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe" "C:/Users/start/Documents/Unreal Projects/8th-Team3-Final-Project/BG/BG.uproject" -ExecutePythonScript="BG/Scripts/Python/build_frame_material.py"
```

`M_BGCardFrame` 이 없으면 만들고, 있으면 **그래프를 통째로 비우고 다시 만든다.** `MI_BGCardFrame_Default` 도 같이 생긴다.

> ⚠️ **머테리얼 에디터를 닫고 실행할 것.** 에디터는 `/Engine/Transient` 의 프리뷰 사본에서 작업하기 때문에, 열어 둔 채로 돌리면 원본 노드를 지워도 에디터가 들고 있던 그래프가 다시 얹혀 파라미터가 두 벌씩 생긴다. 스크립트가 닫기를 시도하고, 그래도 열려 있으면 **아무것도 하지 않고 멈춘다.**

출력 로그에 `[Frame] 완료 — 노드 N 개, 표본 21 개, 파라미터 10 개` 가 뜨면 성공이다. 노드 수가 실행할 때마다 늘면 옛 그래프가 안 지워진 것이다 — 머테리얼을 지우고 다시 돌린다.

---

## 2. 붙이기

1. `WBP_BGCard` 를 연다.
2. `Border_Root` 선택 → **Appearance ▸ Brush ▸ Image** 에 `MI_BGCardFrame_Default` 를 물린다.
   - `M_BGCardFrame` 을 직접 물리지 말 것. 값을 만질 때마다 셰이더를 다시 컴파일한다.
3. `Border_Root` 바깥에 **여백**이 남아 있는지 본다. 글로우는 이 위젯 사각형 밖으로 못 나간다 — Overlay 안에서 프레임이 카드 칸을 꽉 채우고 있으면 외곽선이 잘린다.
4. MI 를 열어 `HoverAmount` 를 0 → 1 로 끌어 본다. 외곽선이 나오면 배선이 맞은 것이다.

### ⚠️ 텍스처에 투명 여백이 필요하다

이 방식은 **텍스처 UV 밖으로는 못 번진다.** `T_Card_Attack` · `T_Card_Equipment` · `T_Card_Magic` 의 그림이 텍스처 가장자리까지 꽉 차 있으면 외곽선이 경계에서 싹둑 잘린다.

가로폭의 `GlowWidth`(기본 5%) 만큼은 사방에 투명 여백을 두고 다시 뽑을 것. 512 폭이면 약 26px 이다.

### ⚠️ `RefreshFrame()` 을 고쳐야 한다

`BGCardWidget.cpp` 의 `Border_Root->SetBrushFromTexture(Frame)` 는 브러시를 **통째로 텍스처로 덮어쓴다.** 그대로 두면 카드가 갱신되는 순간 머테리얼이 날아간다. 둘 중 하나를 고른다.

| | 경로 A — MI 하나 | 경로 B — 타입별 MI 3 개 |
|---|---|---|
| C++ 이 하는 일 | `FrameTex` 파라미터만 민다 | `SetBrushResourceObject(MI)` 로 갈아 끼운다 |
| 애셋 | `MI_BGCardFrame_Default` 1 개 | `MI_BGCardFrame_Attack/Equipment/Magic` |
| 장점 | 애셋이 하나 | 코드가 텍스처를 아예 모른다 |
| 단점 | 코드가 텍스처를 안다 | 타입이 늘 때마다 MI 를 만든다 |

경로 A 라면 DMI 를 **브러시에서** 얻어야 한다. 자기가 따로 만든 MID 에 값을 밀면 UMG 애니메이션이 잡은 DMI 와 갈라져서 한쪽 값이 안 먹는다.

```cpp
// SetBrushFromTexture 대신 — 브러시는 머테리얼 그대로 두고 파라미터만 갈아끼운다
if (UMaterialInstanceDynamic* FrameMID = Border_Root->GetDynamicMaterial())
{
    FrameMID->SetTextureParameterValue(TEXT("FrameTex"), Frame);
}
```

---

## 3. 파라미터

전부 그룹 `Frame` 에 모여 있다. 이름은 `BGCardWidget.h` 의 `*ParamName` 기본값과 같다 — **한 글자라도 다르면 DMI 가 조용히 무시한다.**

| 이름 | 형 | 기본 | 뜻 | 누가 조절 |
|---|---|---|---|---|
| `FrameTex` | Texture | `T_Card_Attack` | 테두리 그림. 알파가 곧 실루엣 | 코드 (`RefreshFrame`) |
| `HoverAmount` | Scalar | `0` | 호버 세기 0~1 | **UMG 애니메이션** |
| `GlowRingStrength` | Scalar | `0` | 등급 연출 세기 0~1 | **UMG 애니메이션** |
| `GlowColor` | Vector | `(1, 0.82, 0.35)` 금빛 | 글로우 색 | UMG 애니메이션 |
| `GlowIntensity` | Scalar | `2.5` | 밝기 배수. 1 이하면 밋밋, 4 넘으면 흰색으로 탄다 | 룩 고정값 |
| `GlowWidth` | Scalar | `0.05` | 번지는 거리. **카드 가로폭 대비 비율** | 룩 고정값 |
| `GlowFalloff` | Scalar | `1.6` | 클수록 실루엣에 바짝, 1 에 가까울수록 멀리 옅게 | 룩 고정값 |
| `WaveAmount` | Scalar | `0.35` | 파동 세기. 0 이면 매끈한 그라데이션만 | 룩 고정값 |
| `WaveFreq` | Scalar | `3.0` | 글로우 폭 안에 마루가 몇 개 | 룩 고정값 |
| `WaveSpeed` | Scalar | `0.6` | 파동이 바깥으로 흐르는 속도 | 룩 고정값 |

**구동값은 `max(HoverAmount, GlowRingStrength)` 다.** 더하지 않는다 — 호버 중에 대성공이 나면 두 배로 타 버린다.

`GlowWidth` 가 비율인 덕에 해상도를 안 탄다. 512 짜리 텍스처를 1024 로 다시 뽑아도 두께가 그대로다. 세로는 텍스처 가로세로 비를 곱해 보정하므로 카드가 세로로 길어도 타원으로 눌리지 않는다.

> `WidgetSize` 파라미터는 **일부러 없앴다.** 위젯 픽셀 크기를 C++ 이 매 프레임 먹여야 하는 값이라 UMG 애니메이션만으로는 굴릴 수 없는데, 여기서 필요한 것은 가로세로 비율뿐이고 그건 텍스처에서 바로 읽는다. 리테이너의 `PushWidgetSize` 는 디졸브용이라 그대로 둔다.

---

## 4. UMG 애니메이션으로 굴리기

브러시에 머테리얼을 **먼저 물린 뒤에** 트랙을 만들어야 파라미터 목록이 뜬다.

1. `WBP_BGCard` 의 **Animations** 에서 애니메이션을 하나 만든다 (예: `Anim_CardHover`).
2. 트랙 `+` → `Border_Root` 를 추가한다.
3. `Border_Root` 트랙의 `+` → **Brush** → 파라미터 목록에서 `HoverAmount` 를 고른다.
4. 키를 찍는다.

### 추천 키

| 애니메이션 | 파라미터 | 키 |
|---|---|---|
| `Anim_CardHover` | `HoverAmount` | 0.00s → `0` · 0.12s → `1` |
| `Anim_CardTier` (대성공) | `GlowRingStrength` | 0.00s → `0` · 0.08s → `1` · 0.60s → `0` |
| | `GlowColor` | 등급색 고정 또는 흰색 → 등급색 |

호버 해제는 같은 애니메이션을 역재생하면 된다.

> ⚠️ **C++ 의 `TickHoverGlow()` 와 겹치지 않게 할 것.** 지금 코드는 `HoverBlend` 를 매 프레임 리테이너 MID 에 밀고 있다. 애니메이션이 `HoverAmount` 를 맡으면 그 호출은 지워야 한다 — 두 곳에서 같은 이름을 밀면 마지막에 쓴 쪽이 이긴다.
>
> 겸사겸사 확인할 것: 호버 글로우가 리테이너에 붙어 있는 한, `SetRetainRendering` 이 꺼진 평상시에는 Effect Material 자체가 적용되지 않아 **애초에 보이지 않는다.** 이 머테리얼로 옮기는 이유가 그것이다.

---

## 5. 원리

거리장(SDF)을 텍스처로 굽지 않는다. 굽는 순간 카드 종류마다 한 장씩 관리해야 하고, 그림이 바뀌면 다시 구워야 한다.

대신 프레임 알파를 **나선으로 흩어 20 번 더 찍어 평균**을 낸다.

```
blur   = (A×2 + Σ tap_i) / 22            흐린 알파 — 실루엣 밖으로 번져 있다
shape  = saturate(blur × (1−A) × 1.8)    원래 알파가 0 인 자리에서만 남긴다
shape  = shape ^ GlowFalloff             감쇠 모양

phase  = blur × WaveFreq + Time × WaveSpeed
wave   = 0.5 + 0.5 × sin(phase)
ripple = lerp(1, wave, WaveAmount)

drive  = max(HoverAmount, GlowRingStrength)
glow   = shape × ripple × GlowIntensity × drive

Emissive = FrameTex.rgb + GlowColor × glow
Opacity  = saturate(FrameTex.a + glow)
```

`blur` 은 바깥으로 갈수록 작아지므로 **거리 대용**이다. 파동 위상에 그대로 넣으면 시간이 흐를수록 마루가 낮은 `blur` 쪽 — 즉 바깥으로 흘러간다.

알파에 `glow` 를 더하는 것이 핵심이다. 이게 없으면 실루엣 밖은 알파 0 이라 아무리 밝아도 화면에 안 나온다.

### 왜 나선인가

동심 링(바깥 8 + 안쪽 4)으로 짰다가 갈아엎었다. 표본 반지름이 두 종류뿐이라 흐린 알파가 서너 값으로 양자화되어 **눈에 보이는 띠**가 생긴다. CPU 로 프로파일을 뽑아 확인한 값:

256×384 카드에 `GlowWidth` 14px 를 넣고 잰 글로우 밝기다.

| 경계 밖 거리 | 링 12 탭 | 나선 20 탭 |
|---|---|---|
| 0 px | 0.74 | 0.61 |
| 2 px | 0.74 | 0.51 |
| 4 px | 0.24 | 0.24 |
| 6 px | 0.24 | 0.17 |
| 8 px | 0.24 | 0.11 |
| 10 px | 0.04 | 0.02 |
| **인접 단계 최대 낙차** | **0.49** | **0.19** |

Vogel 나선(황금각 137.5°, 반지름 √)은 표본마다 반지름이 다 달라 거리의 매끈한 함수가 된다. 가중치를 반지름에 따라 기울여 봐야 낙차만 커져서(0.25) 균등 가중으로 뒀다.

---

## 6. 안 보일 때

| 증상 | 원인 | 고치는 곳 |
|---|---|---|
| 아무 변화 없음 | 브러시에 MI 가 안 물렸다 | `Border_Root` ▸ Brush ▸ Image |
| 값을 바꿔도 무반응 | 파라미터 이름 오타 | MI 의 이름과 `BGCardWidget.h` 의 `*ParamName` 대조 |
| 카드 갱신되면 사라짐 | `SetBrushFromTexture` 가 브러시를 덮어씀 | 2 장의 경로 A / B |
| 외곽선이 네모나게 잘림 | 텍스처에 투명 여백이 없다 | `T_Card_*` 다시 뽑기 |
| 외곽선이 위젯에서 잘림 | `Border_Root` 가 칸을 꽉 채움 | WBP 여백 |
| 띠가 보인다 | `GlowWidth` 가 너무 크다 | 0.05 부근으로 |
| 세로로만 두껍다 | `TextureProperty` 대체 경로를 탐 | 로그에 `FrameTexSize` 경고가 있는지 확인 후 실제 해상도 입력 |
| 카드가 새까맣다 | `GlowIntensity` 폭주 | 4 이하로 |

---

## 7. 비용

표본 21 개(= 텍스처 페치 21 회). `Border_Root` 픽셀에서만 돌고 손패는 7 장이므로 문제 되는 규모가 아니다. 렌더 타겟은 하나도 안 쓴다.

샘플러는 **1 개**만 먹는다 — Sample 노드가 각자 샘플러를 잡으면 머테리얼당 상한 16 개를 넘겨 컴파일부터 실패하므로, 전부 공유 샘플러(`SSM_Clamp_WorldGroupSettings`)로 묶었다. 이게 UV 를 0~1 로 가두는 역할도 겸한다 — 안 그러면 표본이 텍스처 밖으로 나가 반대쪽 그림이 딸려 온다.

더 줄여야 하면 `TAP_COUNT` 를 12 로 낮춘다. 대신 위 표의 띠가 돌아온다.

---

## 관련 문서

- `BG/Scripts/Python/build_dissolve_material.py` — 퇴장 디졸브(리테이너)
- `BG/Source/BG/Public/UI/Widget/BGCardWidget.h` — 파라미터 이름의 출처
