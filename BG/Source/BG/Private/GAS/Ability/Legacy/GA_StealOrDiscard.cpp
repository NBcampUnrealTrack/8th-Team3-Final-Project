// ⚠️ [비활성] AbilityByCardId 에 매핑하지 말 것 (GDD §13.1).
//   위협(Card.Id.CatBalou)은 UGA_BGCardEffects 의 DiscardTargetRandom/StealTargetRandom 로 간다.
//   이 GA 는 비활성 폴백인 EventMagnitude 를 읽으므로, 매핑하면 장수가 0 이 되어 아무 일도 없다.
//   정식 경로는 UGA_BGCardEffects + DT_BGCard 의 OutcomeEffects 하나뿐이다.

#include "GAS/Ability/Legacy/GA_StealOrDiscard.h"

#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Debug/BGCardLog.h"
#include "Card/BGCardType.h"
#include "Debug/BGDebug.h"
#include "Card/BGDiceComponent.h"
#include "Core/BGGameState.h"
#include "BGGameplayTags.h"
#include "Core/BGPlayerState.h"

UGA_StealOrDiscard::UGA_StealOrDiscard()
{
	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(BG::Ability::StealOrDiscard.GetTag());
	SetAssetTags(AssetTags);

	ActivationBlockedTags.AddTag(BG::State::Dead.GetTag());
}

void UGA_StealOrDiscard::ActivateAbility(
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

	AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;

	// ── 판정 결과 수신 (GDD §9.1) ──
	//	판정은 서버(HandlePlayCard)가 카드 데이터로 이미 끝냈다. 여기서는 실행만 한다.
	//	EventMagnitude = 버릴(또는 훔칠) 장수. 실패/대실패면 0 이다.
	static const FGameplayTag ResolutionRoot = FGameplayTag::RequestGameplayTag(TEXT("Resolution"));
	const bool bHasServerOutcome =
		TriggerEventData && TriggerEventData->InstigatorTags.HasTag(ResolutionRoot);

	const EBGDiceOutcome Outcome = bHasServerOutcome
		? UBGDiceComponent::TagsToOutcome(TriggerEventData->InstigatorTags, EBGDiceOutcome::Failure)
		: EBGDiceOutcome::Success;

	//	판정 없이 발동된 경우(폴백)에는 1 장으로 본다.
	const int32 RequestedCount = bHasServerOutcome
		? FMath::RoundToInt(TriggerEventData->EventMagnitude)
		: 1;

	if (RequestedCount <= 0)
	{
		//	실패/대실패 — 카드는 소비되지만 효과는 없다 (GDD §4.1).
		BGDebug::Screen(
			FString::Printf(TEXT("위협  %s → [%s] 효과 없음"),
				Avatar ? *Avatar->GetName() : TEXT("?"),
				*UBGDiceComponent::GetOutcomeText(Outcome).ToString()),
			FColor(160, 160, 160), /*Time=*/5.f);

		OnStealOrDiscardResolved(0, nullptr);
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	AActor* TargetActor = TriggerEventData ? const_cast<AActor*>(TriggerEventData->Target.Get()) : nullptr;
	ABGPlayerState* TargetPS = GetBGPlayerState(TargetActor);
	if (!TargetPS)
	{
		UE_LOG(LogBGCard, Warning,
			TEXT("[BG] GA_StealOrDiscard: 대상 PlayerState 를 찾지 못했습니다 (대상 지정 확인)."));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ABGPlayerState* SourcePS = GetBGPlayerState(Avatar);
	const UWorld* World = Avatar ? Avatar->GetWorld() : nullptr;
	ABGGameState* GS  = World ? World->GetGameState<ABGGameState>() : nullptr;
	UBGDiceComponent* Dice = UBGDiceComponent::Get(Avatar);

	if (!GS || !Dice)
	{
		UE_LOG(LogBGCard, Warning, TEXT("[BG] GA_StealOrDiscard: GameState / Dice 없음 — 중단."));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// ── 무작위 선택 후 이동 ──
	//	어느 카드를 버릴지는 서버가 정한다. 시드된 판정 스트림을 쓰므로 같은 판이 재현된다.
	TArray<FString> MovedNames;
	int32 Moved = 0;

	for (int32 i = 0; i < RequestedCount; ++i)
	{
		//	손패는 매 반복마다 다시 읽는다 — 앞 반복에서 한 장이 빠졌다.
		const TArray<FBGCardInstance>& Hand = TargetPS->GetHand();
		if (Hand.IsEmpty())
		{
			break;   // 더 버릴 카드가 없다
		}

		const int32 Index = Dice->RandomIndex(Hand.Num());
		if (!Hand.IsValidIndex(Index))
		{
			break;
		}

		//	제거 전에 InstanceId 를 값으로 복사해 둔다 — 제거하면 위 참조가 흔들린다.
		const int32 InstanceId = Hand[Index].InstanceId;

		FBGCardInstance Removed;
		if (!TargetPS->RemoveCardFromHand(InstanceId, Removed))
		{
			break;
		}

		if (bSteal && SourcePS)
		{
			SourcePS->AddCardToHand(Removed);
		}
		else
		{
			GS->PushToDiscard(Removed);
		}

		MovedNames.Add(FString::Printf(TEXT("%s#%d"), *Removed.CardId.ToString(), Removed.InstanceId));
		++Moved;
	}

	BGDebug::Screen(
		FString::Printf(TEXT("위협  %s → [%s] 좌석 %d 에서 %d 장 %s : %s"),
			Avatar ? *Avatar->GetName() : TEXT("?"),
			*UBGDiceComponent::GetOutcomeText(Outcome).ToString(),
			TargetPS->GetSeatIndex(),
			Moved,
			bSteal ? TEXT("훔침") : TEXT("버림"),
			MovedNames.IsEmpty() ? TEXT("(손패 없음)") : *FString::Join(MovedNames, TEXT(", "))),
		(Moved > 0) ? FColor(255, 180, 80) : FColor(160, 160, 160), /*Time=*/6.f);

	OnStealOrDiscardResolved(Moved, TargetActor);

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
