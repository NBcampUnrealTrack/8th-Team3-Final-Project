// ⚠️ [비활성] AbilityByCardId 에 매핑하지 말 것 (GDD §13.1).
//   BAAM 에는 "빗나감" 카드가 없다 — 방어는 요정 갑옷의 자동 방어 판정(GDD §14 미결)이 맡을 예정이다.
//   본문의 응답 제출도 TODO 스텁이라 판정 결과가 어디에도 전달되지 않는다.
//   또한 구형 주사위 경로(UBGDiceComponent::RollForOutcome)의 유일한 호출처다 —
//   이 GA 를 정리하면 구형 경로 전체를 함께 걷어낼 수 있다.

#include "GAS/Ability/Legacy/GA_Missed.h"
#include "GAS/BGAttributeSet.h"
#include "BGGameplayTags.h"
#include "Card/BGDiceComponent.h"
#include "Debug/BGDebug.h"
#include "AbilitySystemComponent.h"

UGA_Missed::UGA_Missed()
{
	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(BG::Ability::Missed.GetTag());
	// Reaction 성질 — 응답 창에서만 사용 (능동 사용 검증에서 걸러진다).
	AssetTags.AddTag(BG::Card::Trait::Reaction.GetTag());
	SetAssetTags(AssetTags);

	ActivationBlockedTags.AddTag(BG::State::Dead.GetTag());
}

void UGA_Missed::ActivateAbility(
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

	UAbilitySystemComponent* SourceASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const UBGAttributeSet* SourceAS = SourceASC ? SourceASC->GetSet<UBGAttributeSet>() : nullptr;
	AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	UBGDiceComponent* Dice = UBGDiceComponent::Get(Avatar);

	if (!SourceAS || !Dice)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BG] GA_Missed — 서버 컨텍스트 없음(AS/Dice). 중단."));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 회피는 행운 + 민첩 을 굴림 보정으로 넣는다.
	const int32 DodgeBonus = FMath::RoundToInt(SourceAS->GetLuck() + SourceAS->GetAgility());
	int32 Roll = 0;
	const EBGDiceOutcome Outcome = Dice->RollForOutcome(Roll, /*Faces=*/0, /*RollBonus=*/DodgeBonus);

	// 회피는 성공/실패로만 압축: 성공·대성공 → 회피 성공.
	const bool bDodged =
		(Outcome == EBGDiceOutcome::Success || Outcome == EBGDiceOutcome::CriticalSuccess);

	// TODO(§3): bDodged 를 진행 중인 응답 요청에 제출 → (응답 창 타입은 아직 만들지 않았다)
	//           성공이면 해당 BANG! 을 무효화한다. (MissedRequired 만큼 필요할 수 있음)
	BGDebug::Screen(
		FString::Printf(TEXT("회피  %s → %s  (판정 %s, 눈%d, 행운+민첩%+d)"),
			Avatar ? *Avatar->GetName() : TEXT("?"),
			bDodged ? TEXT("성공") : TEXT("실패"),
			*UBGDiceComponent::GetOutcomeText(Outcome).ToString(), Roll, DodgeBonus),
		bDodged ? FColor::Cyan : FColor::Red, /*Time=*/5.f);

	OnDodgeResolved(bDodged);

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
