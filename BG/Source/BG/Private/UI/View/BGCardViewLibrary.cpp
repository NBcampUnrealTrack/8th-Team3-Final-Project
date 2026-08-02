#include "UI/View/BGCardViewLibrary.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Debug/BGCardLog.h"
#include "Card/BGCardType.h"
#include "Card/BGDataSubsystem.h"
#include "Card/BGGameDataTypes.h"
#include "BGGameplayTags.h"
#include "Core/BGGameState.h"
#include "Core/BGPlayerState.h"

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
}

FBGCardView UBGCardViewLibrary::MakeCardView(const UObject* WorldContextObject, const FBGCardInstance& Card)
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
		Views.Add(MakeCardView(PlayerState, Card));
	}

	return Views;
}
