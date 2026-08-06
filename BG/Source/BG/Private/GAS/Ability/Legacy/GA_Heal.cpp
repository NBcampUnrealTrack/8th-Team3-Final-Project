#include "GAS/Ability/Legacy/GA_Heal.h"
#include "Card/BGDiceComponent.h"
#include "GAS/Effect/GE_BGHeal.h"
#include "GAS/BGAttributeSet.h"
#include "BGGameplayTags.h"
#include "Debug/BGDebug.h"
#include "AbilitySystemComponent.h"

UGA_Heal::UGA_Heal()
{
	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(BG::Ability::Heal.GetTag());
	SetAssetTags(AssetTags);

	// 사망한 플레이어는 카드를 쓸 수 없다.
	ActivationBlockedTags.AddTag(BG::State::Dead.GetTag());
}

void UGA_Heal::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// TODO(§M4): 생존 2명이면 Beer 무효 — GameState 의 생존 수 확인 후 스킵.
	const AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;

	// 자신 ASC 에 GE_BGHeal(SetByCaller.Heal = 회복량) 적용. MaxHealth 클램프는 AttributeSet 담당.
	UAbilitySystemComponent* SourceASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;

	// ── 판정 결과 수신 (GDD §9.1) ──
	//	판정은 서버(HandlePlayCard)가 카드 DT 로 끝냈다. 비율은 DT_BGCard 의 OutcomeWeights,
	//	등급별 회복량은 OutcomeMagnitudes 다. 이 GA 는 결과 실행만 한다.
	//	회복 확률·회복량에 붙는 스탯 보정은 두지 않는다 — §7.1 의 "회복 보정" 은 확정 스탯이지만
	//	그 자리를 채울 어트리뷰트가 아직 없고, 이 GA 자체가 비활성이다(GDD §13.1).
	//
	//	[비활성] HealAmount 폴백 (GDD §13.1 — 선언만 유지, 참조 제거).
	//	  사유: 판정 없이 성공 취급하고 GA 값으로 회복하면, 카드 DT 를 고쳐도 회복량이 안 바뀌는
	//	        경로가 남아 어느 값이 적용됐는지 알 수 없다. 판정 결과가 없다는 건 데이터/매핑 문제다.
	//	  복구: FinalHeal 의 else 분기에 HealAmount 를 다시 넣는다.
	static const FGameplayTag ResolutionRoot = FGameplayTag::RequestGameplayTag(TEXT("Resolution"));
	const bool bHasServerOutcome =
		TriggerEventData && TriggerEventData->InstigatorTags.HasTag(ResolutionRoot);

	EBGDiceOutcome Outcome = EBGDiceOutcome::Failure;
	int32 FinalHeal = 0;   // 판정 결과가 없으면 실패(회복 0) — §4.1 "실패는 추가 불이익 없음".

	if (bHasServerOutcome)
	{
		Outcome   = UBGDiceComponent::TagsToOutcome(TriggerEventData->InstigatorTags, EBGDiceOutcome::Failure);
		FinalHeal = FMath::RoundToInt(TriggerEventData->EventMagnitude);
	}
	else
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[BG] GA_Heal: 서버 판정 결과(Resolution.*)가 없습니다 — "
			     "DT_BGCard 의 OutcomeWeights/OutcomeMagnitudes 와 AbilityByCardId 매핑을 확인하세요. 회복 없이 종료합니다."));
	}

	if (SourceASC && FinalHeal > 0)
	{
		FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
		Context.AddSourceObject(this);

		const FGameplayEffectSpecHandle Spec =
			SourceASC->MakeOutgoingSpec(UGE_BGHeal::StaticClass(), 1.f, Context);
		if (Spec.IsValid())
		{
			Spec.Data->SetSetByCallerMagnitude(BG::SetByCaller::Heal.GetTag(), static_cast<float>(FinalHeal));
			SourceASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
		}
	}

	// 적용 후 현재 체력을 함께 표시(회복이 실제로 붙었는지 눈으로 확인).
	const UBGAttributeSet* AS = SourceASC ? SourceASC->GetSet<UBGAttributeSet>() : nullptr;
	const float HP    = AS ? AS->GetHealth()    : 0.f;
	const float MaxHP = AS ? AS->GetMaxHealth() : 0.f;

	BGDebug::Screen(
		FString::Printf(TEXT("맥주  %s → [%s] 회복 +%d  (HP %.0f/%.0f)"),
			Avatar ? *Avatar->GetName() : TEXT("?"),
			*UBGDiceComponent::GetOutcomeText(Outcome).ToString(), FinalHeal, HP, MaxHP),
		(FinalHeal > 0) ? FColor::Green : FColor(160, 160, 160), /*Time=*/5.f);

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
