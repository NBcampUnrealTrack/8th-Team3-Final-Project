#include "UI/View/BGCardViewLibrary.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Debug/BGCardLog.h"
#include "Card/BGCardType.h"
#include "Card/BGDataSubsystem.h"
#include "Card/BGDiceComponent.h"
#include "Card/BGGameDataTypes.h"
#include "BGGameplayTags.h"
#include "Core/BGGameState.h"
#include "Core/BGPlayerState.h"
#include "AbilitySystemGlobals.h"

namespace
{
	const UBGDataSubsystem* GetDataSubsystem(const UObject* WorldContextObject)
	{
		const UWorld* World = GEngine
			? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull)
			: nullptr;
		const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
		return GI ? GI->GetSubsystem<UBGDataSubsystem>() : nullptr;
	}

	//	비율 → 확률(%). 정규화 규칙이 두 곳에 생기지 않도록 판정 컴포넌트의 변환을 그대로 쓴다.
	FBGCardChanceView ToChanceView(const FBGOutcomeWeights& Weights)
	{
		FBGCardChanceView Out;
		UBGDiceComponent::GetOutcomeChances(Weights,
			Out.CriticalFailure, Out.Failure, Out.Success, Out.CriticalSuccess);
		return Out;
	}

	//	보정이 실제로 확률을 바꿨는가. 표시 단위가 % 라 0.05%p 미만은 같은 값으로 본다
	//	(정규화 부동소수 오차로 "보정됨" 표시가 깜빡이는 것을 막는다).
	bool ChancesDiffer(const FBGCardChanceView& A, const FBGCardChanceView& B)
	{
		constexpr float Epsilon = 0.05f;
		return !FMath::IsNearlyEqual(A.CriticalFailure, B.CriticalFailure, Epsilon)
			|| !FMath::IsNearlyEqual(A.Failure,         B.Failure,         Epsilon)
			|| !FMath::IsNearlyEqual(A.Success,         B.Success,         Epsilon)
			|| !FMath::IsNearlyEqual(A.CriticalSuccess, B.CriticalSuccess, Epsilon);
	}
}

FBGCardView UBGCardViewLibrary::MakeCardView(const UObject* WorldContextObject, const FBGCardInstance& Card,
	const UAbilitySystemComponent* SourceASC)
{
	FBGCardView View;
	View.InstanceId = Card.InstanceId;
	View.CardId     = Card.CardId;

	const UBGDataSubsystem* Data = GetDataSubsystem(WorldContextObject);
	const FBGCardRow* Row = Data ? Data->GetCardRow(Card.CardId) : nullptr;

	if (!Row)
	{
		// 조용히 빈 카드가 되면 "왜 이름이 안 보이지" 로 시간을 버린다. 화면에 원인을 띄운다.
		View.DisplayName = FText::FromString(FString::Printf(TEXT("%s (Row 없음)"), *Card.CardId.ToString()));
		View.Description = FText::FromString(TEXT("DT_BGCard 에 이 CardId 의 Row 가 없습니다."));
		View.bPlayable   = false;
		UE_LOG(LogBGCard, Warning, TEXT("MakeCardView: '%s' Row 를 찾지 못했습니다."), *Card.CardId.ToString());
		return View;
	}

	View.DisplayName  = Row->DisplayName;
	View.Description  = Row->Description;
	View.bNeedsTarget = Row->TraitTags.HasTag(BG::Card::Trait::NeedsTargetSeat.GetTag());

	// TODO(5.x): 사거리·턴당 BANG 한도·현재 페이즈를 반영한 실제 판정으로 교체.
	//            어디까지나 표시용 힌트이고, 최종 판정은 언제나 서버가 한다.
	View.bPlayable = true;

	// ── 4단계 확률 (GDD §4.4 / §6.1 / §8 / §10) ──────────────────────────
	//	파란(장비) 카드는 판정 없이 장착 경로로 빠진다 — 판별자는 카드를 실제로 내는 쪽
	//	(ABGPlayerController::ServerRequestPlayCard)과 같은 TypeTag 를 쓴다. 두 곳이 갈리면
	//	확률이 표시되는데 판정은 일어나지 않는 카드가 생긴다.
	const bool bIsEquip = (Row->TypeTag == BG::Card::Type::Blue.GetTag());

	if (!bIsEquip && Row->OutcomeWeights.Sum() > 0)
	{
		View.bHasChances = true;
		View.BaseChance  = ToChanceView(Row->OutcomeWeights);

		//	보정 합산은 순수 계산이라 클라에서 돌려도 안전하다.
		//	GDD §9.2 — 확률 "표시" 는 클라, 확률 "판정" 은 서버. 여기는 표시다.
		const UBGDiceComponent* Dice = UBGDiceComponent::Get(WorldContextObject);
		View.FinalChance = (Dice && SourceASC)
			? ToChanceView(Dice->ApplyModifiers(Row->OutcomeWeights, SourceASC))
			: View.BaseChance;

		View.bChanceModified = ChancesDiffer(View.BaseChance, View.FinalChance);
	}

	return View;
}

TArray<FBGCardView> UBGCardViewLibrary::MakeHandViews(const ABGPlayerState* PlayerState)
{
	TArray<FBGCardView> Views;
	if (!PlayerState)
	{
		return Views;
	}

	const TArray<FBGCardInstance>& Hand = PlayerState->GetHand();
	Views.Reserve(Hand.Num());

	//	내 손패이므로 시전자는 나다 — 장비·직업 보정을 반영한 최종 확률을 채울 수 있다.
	//	ASC 는 폰에 붙어 있다(ABGGameState::GetSeatHealth 와 같은 경로).
	//	⚠️ 보정이 바뀌면 이 뷰는 낡는다. 손패가 바뀔 때만 다시 만들어지기 때문이다.
	//	   장착 직후 갱신은 ABGPlayerController::RefreshHandWidget 호출로 보장한다.
	const UAbilitySystemComponent* SourceASC =
		UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PlayerState->GetPawn());

	//	⚠️ 여기서 턴/페이즈를 반영하지 말 것.
	//	   bPlayable 은 SetHand 시점에 뷰에 박히는 스냅샷이라, 손패 "내용" 이 바뀔 때만 갱신된다.
	//	   턴 진행은 Draw 페이즈에서 카드를 뽑은 뒤 Play 로 넘어가므로, 마지막 갱신이 Draw 시점에
	//	   일어나 "내 차례인데 카드가 안 나가는" 교착이 생겼다(카드를 내야 손패가 바뀌는데
	//	   카드를 낼 수 없으므로 영구히 풀리지 않는다).
	//
	//	   턴/페이즈 잠금은 UBGHandWidget::SetInteractionLocked 가 담당한다 —
	//	   그쪽은 카드 위젯을 재생성하지 않고 상태만 바꾸므로 주기 갱신해도 드래그가 끊기지 않는다.
	//	   (ABGPlayerController::UpdateHandInteractionLock)
	for (const FBGCardInstance& Card : Hand)
	{
		Views.Add(MakeCardView(PlayerState, Card, SourceASC));
	}

	return Views;
}
