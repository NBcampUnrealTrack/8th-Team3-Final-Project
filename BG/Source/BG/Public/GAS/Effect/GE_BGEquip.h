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

/**
 * 요정 갑옷 — 나에게 들어오는 공격이 25% 확률로 무효화된다.
 *
 * 앞의 둘과 성격이 다르다: 저 둘은 **내가 굴리는 판정**의 비율을 바꾸지만,
 * 이것은 **남이 나에게 넣는 피해**를 막는다. 그래서 판정 가중치가 아니라
 * DamageNegateChance 어트리뷰트를 올리고, 실제 굴림은 피해가 들어가기 직전
 * UGA_BGCardEffects::ApplyDamage 에서 서버가 한 번 한다.
 */
UCLASS()
class BG_API UGE_Equip_FairyArmor : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UGE_Equip_FairyArmor();
};
