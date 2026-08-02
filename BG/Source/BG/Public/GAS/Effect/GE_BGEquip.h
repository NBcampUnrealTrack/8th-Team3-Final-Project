// 장비 지속 효과 (Infinite GE) — 파란 카드가 어트리뷰트 보정을 걸 때 쓴다.
//
// ⚠️ 반드시 Infinite 여야 한다. Instant 는 유효 핸들을 주지 않아
//    장착 해제(UnequipCard)에서 되돌릴 수 없다.
//
// Additional-Card-List 의 장비 2종을 C++ 클래스로 둔다 — DT 에서 바로 지정할 수 있어
// 에디터에서 GE 애셋을 따로 만들 필요가 없다.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GE_BGEquip.generated.h"

/**
 * 드워프 장갑 — 내 카드들이 실패하지 않는다.
 *   실패 비율에 큰 음수를 더해 사실상 0 으로 만든다(정규화 단계에서 0 클램프).
 *   ⚠️ "실패" 만 없앤다 — 대실패는 그대로 남는다(리스트 문구 그대로).
 */
UCLASS()
class BG_API UGE_Equip_DwarfGloves : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UGE_Equip_DwarfGloves();
};

/**
 * 마녀의 부적 — 대성공 확률과 대실패 확률이 두배.
 *   승산 어트리뷰트(기본 1.0)에 +1.0 을 더해 ×2 를 만든다.
 */
UCLASS()
class BG_API UGE_Equip_WitchCharm : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UGE_Equip_WitchCharm();
};
