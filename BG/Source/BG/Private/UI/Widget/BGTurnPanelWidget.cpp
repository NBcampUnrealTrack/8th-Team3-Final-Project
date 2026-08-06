#include "UI/Widget/BGTurnPanelWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Lobby/BGRopeComponent.h"
#include "Engine/World.h"

void UBGTurnPanelWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	RefreshVisual();
}

void UBGTurnPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (Button_EndTurn)
	{
		//	중복 구독 방지 — NativeConstruct 는 위젯이 다시 뷰포트에 붙을 때마다 불릴 수 있다.
		Button_EndTurn->OnClicked.RemoveDynamic(this, &UBGTurnPanelWidget::HandleEndTurnClicked);
		Button_EndTurn->OnClicked.AddDynamic(this, &UBGTurnPanelWidget::HandleEndTurnClicked);
	}

	RefreshVisual();
}

bool UBGTurnPanelWidget::TryBindRope()
{
	if (Rope.IsValid())
	{
		return true;
	}

	UBGRopeComponent* Found = UBGRopeComponent::Find(GetWorld());
	if (!Found)
	{
		return false;   //	GameState 가 아직 안 왔다(클라 접속 직후). 다음 틱에 다시 본다.
	}

	Rope = Found;
	Found->OnRopeChanged.AddUniqueDynamic(this, &UBGTurnPanelWidget::HandleRopeChanged);

	//	위젯이 밧줄보다 늦게 만들어졌으면 점화 통지를 이미 놓쳤다 — 지금 상태로 한 번 맞춘다.
	RefreshRopeText();
	return true;
}

void UBGTurnPanelWidget::HandleRopeChanged(int32 Seat, float SecondsRemaining, float TotalSeconds)
{
	RefreshRopeText();
}

void UBGTurnPanelWidget::RefreshRopeText()
{
	const UBGRopeComponent* R = Rope.Get();

	FString Label = TEXT("밧줄 -");
	if (R && R->IsBurning())
	{
		//	내 밧줄인지 남의 밧줄인지 구분해 둔다 — 지금은 텍스트뿐이라 이 표시가 유일한 단서다.
		const bool bMine = R->IsSeatBurning(TurnView.MySeat);
		Label = FString::Printf(TEXT("밧줄 %s%.0f초 (좌석 %d)"),
			bMine ? TEXT("[내 차례] ") : TEXT(""), R->GetSecondsRemaining(), R->GetRopeSeat());
	}

	if (Text_Rope)
	{
		Text_Rope->SetText(FText::FromString(Label));
		return;
	}

	//	WBP 에 Text_Rope 가 아직 없다 — 화면에 직접 찍어 흐름만이라도 확인할 수 있게 한다.
	//	정식 위젯을 붙이면 위에서 반환되어 이 경로는 자연히 죽는다.
#if !UE_BUILD_SHIPPING
	if (GEngine)
	{
		//	표시 시간은 틱 간격보다 넉넉해야 한다 — 0 이면 그리기 전에 만료돼 아무것도 안 보인다.
		GEngine->AddOnScreenDebugMessage(/*Key=*/ 0xB60DE, /*Time=*/ 1.f, FColor::Yellow, Label);
	}
#endif
}

void UBGTurnPanelWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	//	Text_Rope 가 없어도 부른다 — 그때는 화면 표시로 폴백하기 때문이다.
	if (TryBindRope())
	{
		RefreshRopeText();   //	남은 초는 매 프레임 줄어든다 — 델리게이트로는 표현할 수 없다.
	}
}

void UBGTurnPanelWidget::NativeDestruct()
{
	if (UBGRopeComponent* R = Rope.Get())
	{
		R->OnRopeChanged.RemoveDynamic(this, &UBGTurnPanelWidget::HandleRopeChanged);
	}
	Rope = nullptr;

	Super::NativeDestruct();
}

void UBGTurnPanelWidget::SetTurnView(const FBGTurnView& InTurnView)
{
	TurnView = InTurnView;
	RefreshVisual();
}

void UBGTurnPanelWidget::HandleEndTurnClicked()
{
	//	버튼이 눌렸다는 사실만 알린다. 실제 검증은 서버가 한다.
	OnEndTurnRequested.Broadcast();
}

void UBGTurnPanelWidget::RefreshVisual()
{
	if (Text_Status)
	{
		Text_Status->SetText(TurnView.StatusText);
	}

	if (Text_HandLimit)
	{
		//	한도가 없으면(설정 문제로 Health 를 못 읽은 경우) 숫자 대신 '-' 로 표시한다.
		Text_HandLimit->SetText(TurnView.HandLimit == INDEX_NONE
			? FText::FromString(FString::Printf(TEXT("손패 %d"), TurnView.HandCount))
			: FText::FromString(FString::Printf(TEXT("손패 %d / 한도 %d"), TurnView.HandCount, TurnView.HandLimit)));
	}

	if (Text_Deck)
	{
		Text_Deck->SetText(FText::FromString(FString::Printf(TEXT("덱 %d"), TurnView.DeckCount)));
	}

	if (Text_CardUse)
	{
		//	GDD §10: 턴 UI 에 "사용한 카드 수 / 현재 카드 사용 한도" 를 표시한다.
		Text_CardUse->SetText(FText::FromString(
			FString::Printf(TEXT("카드 %d / %d"), TurnView.CardsUsed, TurnView.CardUseLimit)));
	}

	if (Button_EndTurn)
	{
		//	버리기 페이즈에서는 턴 종료를 누를 수 없다 — 카드를 버려야 넘어간다.
		//	그래서 활성 조건은 bIsMyTurn(= Play 페이즈) 뿐이다.
		Button_EndTurn->SetIsEnabled(TurnView.bIsMyTurn);
	}

	if (bHideWhenNotMyTurn)
	{
		SetVisibility((TurnView.bIsMyTurn || TurnView.bIsMyDiscard)
			? ESlateVisibility::SelfHitTestInvisible
			: ESlateVisibility::Collapsed);
	}
}
