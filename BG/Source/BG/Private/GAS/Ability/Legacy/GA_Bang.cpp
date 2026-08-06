#include "GAS/Ability/Legacy/GA_Bang.h"
#include "GAS/Effect/GE_BGDamage.h"
#include "GAS/BGAttributeSet.h"
#include "BGGameplayTags.h"
#include "Card/BGDiceComponent.h"
#include "Debug/BGDebug.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"

UGA_Bang::UGA_Bang()
{
	// Asset 태그 = Ability.Bang (메커니즘 식별).
	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(BG::Ability::Bang.GetTag());
	SetAssetTags(AssetTags);
}

// [비활성] 자체 판정 폴백용 등급 피해표 (GDD §13.1) — 호출처 없음.
//   지금 등급별 수치는 카드 DT(OutcomeMagnitudes)에서 온다. 사유·복구는 ActivateAbility 주석 참고.
int32 UGA_Bang::TierBaseDamage(EBGDiceOutcome Outcome) const
{
	switch (Outcome)
	{
	case EBGDiceOutcome::CriticalSuccess: return CriticalSuccessDamage;
	case EBGDiceOutcome::Success:         return SuccessDamage;
	case EBGDiceOutcome::Failure:         return FailureDamage;
	case EBGDiceOutcome::CriticalFailure: return CriticalFailureDamage;
	default:                                return 0;
	}
}

void UGA_Bang::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/true);
		return;
	}

	UAbilitySystemComponent* SourceASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const UBGAttributeSet* SourceAS = SourceASC ? SourceASC->GetSet<UBGAttributeSet>() : nullptr;
	AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;

	if (!SourceASC || !SourceAS)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BG] GA_Bang — 서버 컨텍스트 없음(ASC/AS). 중단."));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// ── 판정 결과 수신 (GDD §9.1) ──
	//	판정은 서버(ABGPlayerController::HandlePlayCard)가 카드 DT 로 이미 끝냈다.
	//	비율은 DT_BGCard 의 OutcomeWeights, 등급별 수치는 OutcomeMagnitudes 다.
	//	여기서 다시 굴리지 않는다 — GA 는 "결과 실행" 만 한다(§9.2 같은 판정 한 번만 적용).
	//
	//	굴림 보정(행운)은 두지 않는다. 행운은 §7.1 의 확정 스탯 목록에 없는 RPG 능력치이고,
	//	확률 보정은 WeightBonus*/WeightMult* 축이 담당한다. 카드 DT 의 비율이 곧 확률이어야
	//	§10 확률 표시와 §11 "설정 확률 vs 실측" 비교가 성립한다.
	static const FGameplayTag ResolutionRoot = FGameplayTag::RequestGameplayTag(TEXT("Resolution"));
	const bool bHasServerOutcome =
		TriggerEventData && TriggerEventData->InstigatorTags.HasTag(ResolutionRoot);

	EBGDiceOutcome Outcome = EBGDiceOutcome::Failure;
	int32 TierBase = 0;

	if (bHasServerOutcome)
	{
		Outcome  = UBGDiceComponent::TagsToOutcome(TriggerEventData->InstigatorTags, EBGDiceOutcome::Failure);
		TierBase = FMath::RoundToInt(TriggerEventData->EventMagnitude);   // 카드 DT 의 등급별 수치
	}
	else
	{
		//	[비활성] 자체 판정 폴백 (GDD §13.1 — 호출 경로 차단, 아래 등급 피해 표는 선언만 유지).
		//	사유: GA 가 스스로 굴리면 카드 DT 와 다른 확률로 갈라져 어느 값이 적용됐는지 알 수 없다.
		//	     판정 결과가 없다는 건 데이터/매핑 문제이므로 조용히 굴리지 말고 드러낸다.
		//	복구: 폴백이 다시 필요하면 TierBaseDamage 호출과 Dice->RollForOutcome 을 되살린다.
		//	현재 처리: 실패(수치 0)로 두고 경고만 남긴다 — §4.1 "실패는 추가 불이익 없음".
		UE_LOG(LogTemp, Warning,
			TEXT("[BG] GA_Bang: 서버 판정 결과(Resolution.*)가 없습니다 — "
			     "DT_BGCard 의 OutcomeWeights/OutcomeMagnitudes 와 AbilityByCardId 매핑을 확인하세요. 피해 없이 종료합니다."));
	}

	// ── 피해 계산: 등급 기본 피해 × 힘 배율 − 대상 지능 경감 ──

	int32 FinalDamage = 0;
	if (TierBase > 0)
	{
		const float StrengthMult = 1.f + SourceAS->GetStrength() * StrengthDamageMult;
		FinalDamage = FMath::RoundToInt(TierBase * StrengthMult);
	}

	// 대상 확보(응답 창/타깃팅은 §3에서 확장). 지금은 이벤트로 넘어온 대상만 사용.
	AActor* TargetActor = TriggerEventData ? const_cast<AActor*>(TriggerEventData->Target.Get()) : nullptr;
	UAbilitySystemComponent* TargetASC =
		UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);

	// 대상 지능만큼 피해 경감.
	if (FinalDamage > 0 && TargetASC)
	{
		if (const UBGAttributeSet* TargetAS = TargetASC->GetSet<UBGAttributeSet>())
		{
			const int32 Mitigation = FMath::RoundToInt(TargetAS->GetIntelligence() * IntelligenceMitigation);
			FinalDamage = FMath::Max(0, FinalDamage - Mitigation);
		}
	}

	// 판정 등급별 색: 대성공=주황, 성공=노랑, 실패=회색, 대실패=빨강.
	FColor OutcomeColor = FColor::Yellow;
	switch (Outcome)
	{
	case EBGDiceOutcome::CriticalSuccess: OutcomeColor = FColor(255, 140, 0); break; // 주황
	case EBGDiceOutcome::Success:         OutcomeColor = FColor::Yellow;       break;
	case EBGDiceOutcome::Failure:         OutcomeColor = FColor(160, 160, 160); break; // 회색
	case EBGDiceOutcome::CriticalFailure: OutcomeColor = FColor::Red;          break;
	}

	BGDebug::Screen(
		FString::Printf(TEXT("뱅  %s → [%s]  기본%d ×힘 → 피해 %d  대상:%s"),
			Avatar ? *Avatar->GetName() : TEXT("?"),
			*UBGDiceComponent::GetOutcomeText(Outcome).ToString(), TierBase, FinalDamage,
			TargetActor ? *TargetActor->GetName() : TEXT("없음")),
		OutcomeColor, /*Time=*/5.f);

	// ── 적용: 대상이 있고 피해가 남으면 GE_BGDamage(음수)로 Health 차감 ──
	if (FinalDamage > 0 && TargetASC)
	{
		FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
		Context.AddSourceObject(this);
		Context.AddInstigator(Avatar, Avatar);

		const FGameplayEffectSpecHandle Spec =
			SourceASC->MakeOutgoingSpec(UGE_BGDamage::StaticClass(), 1.f, Context);
		if (Spec.IsValid())
		{
			// 피해는 음수로 넣는다(Health 가산 = 감소).
			Spec.Data->SetSetByCallerMagnitude(BG::SetByCaller::Damage.GetTag(), -static_cast<float>(FinalDamage));
			SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
		}
	}

	OnBangResolved(Outcome, FinalDamage, TargetActor);

	EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/false);
}
