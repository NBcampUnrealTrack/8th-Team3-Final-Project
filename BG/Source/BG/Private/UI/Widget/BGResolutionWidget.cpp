#include "UI/Widget/BGResolutionWidget.h"

#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "TimerManager.h"

void UBGResolutionWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	//	디자이너가 ResolutionView 에 값을 넣어두면 에디터 프리뷰에서 바로 보인다.
	RefreshVisual();
}

void UBGResolutionWidget::NativeDestruct()
{
	//	타이머가 남으면 파괴된 위젯을 향해 발사된다.
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HideTimerHandle);
	}

	Super::NativeDestruct();
}

void UBGResolutionWidget::ShowResolution(const FBGResolutionView& InResolutionView)
{
	ResolutionView = InResolutionView;

	RefreshVisual();
	BeginShowing();
	StartAutoHideTimer();
}

void UBGResolutionWidget::BeginShowing()
{
	SetVisibility(ESlateVisibility::HitTestInvisible);   // 판정 표시가 카드 클릭을 먹지 않도록

	bShowing = true;

	//	연출 훅은 값이 다 채워진 뒤에 부른다 — BP 가 GetResolutionView 로 최종값을 읽을 수 있다.
	//	위젯 안(BP 훅) 먼저, 바깥(HUD) 나중이다. 순서를 뒤집으면 HUD 가 패널을 바꾸는 동안
	//	이 위젯의 등장 연출이 아직 시작되지 않은 프레임이 생긴다.
	OnResolutionShown(ResolutionView);
	OnResolutionShownEvent.Broadcast(ResolutionView);
}

void UBGResolutionWidget::StartAutoHideTimer()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	//	이전 표시가 남아 있으면 타이머를 다시 잡는다. 안 그러면 먼저 잡힌 타이머가
	//	새로 띄운 결과를 이르게 지운다(카드를 연달아 낼 때 두 번째 결과가 깜빡이고 사라졌다).
	World->GetTimerManager().ClearTimer(HideTimerHandle);

	if (DisplaySeconds > 0.f)
	{
		World->GetTimerManager().SetTimer(
			HideTimerHandle, this, &UBGResolutionWidget::HideResolution, DisplaySeconds, /*bLoop=*/false);
	}
}

void UBGResolutionWidget::HideResolution()
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HideTimerHandle);
	}

	SetVisibility(ESlateVisibility::Collapsed);

	//	보여 준 적이 없으면 퇴장을 알리지 않는다. SetResolutionWidget 이 등록 직후
	//	빈 패널을 감추려고 여기를 부르는데, 그때 통지가 나가면 HUD 가 게임 시작과 동시에
	//	복귀 연출을 한 번 돌린다(평상시 패널이 한 번 깜빡인다).
	//	⚠️ SetVisibility 는 이 가드보다 **위**여야 한다 — 감추는 것 자체는 그때도 해야 한다.
	if (!bShowing)
	{
		return;
	}
	bShowing = false;

	OnResolutionHidden();
	OnResolutionHiddenEvent.Broadcast();
}

FLinearColor UBGResolutionWidget::GetTierColor(EBGResolutionTier Tier) const
{
	switch (Tier)
	{
	case EBGResolutionTier::CriticalFailure: return CriticalFailureColor;
	case EBGResolutionTier::Failure:         return FailureColor;
	case EBGResolutionTier::Success:         return SuccessColor;
	case EBGResolutionTier::CriticalSuccess: return CriticalSuccessColor;
	default:                                 return UnknownTierColor;
	}
}

FText UBGResolutionWidget::GetTierText(EBGResolutionTier Tier)
{
	switch (Tier)
	{
	case EBGResolutionTier::CriticalFailure: return NSLOCTEXT("BG", "TierCriticalFailure", "대실패");
	case EBGResolutionTier::Failure:         return NSLOCTEXT("BG", "TierFailure",         "실패");
	case EBGResolutionTier::Success:         return NSLOCTEXT("BG", "TierSuccess",         "성공");
	case EBGResolutionTier::CriticalSuccess: return NSLOCTEXT("BG", "TierCriticalSuccess", "대성공");
	default:                                 return NSLOCTEXT("BG", "TierNone",            "―");
	}
}

void UBGResolutionWidget::RefreshVisual()
{
	const FLinearColor TierColor = GetTierColor(ResolutionView.Tier);

	if (Text_Outcome)
	{
		Text_Outcome->SetText(GetTierText(ResolutionView.Tier));
		Text_Outcome->SetColorAndOpacity(FSlateColor(TierColor));
	}

	if (Border_Root)
	{
		Border_Root->SetBrushColor(TierColor);
	}

	if (Text_Roll)
	{
		//	강제된 판정에서는 굴린 눈이 결과와 어긋난다(준비/함정이 결과를 덮어썼다).
		//	그대로 "눈 3 → 대성공" 을 띄우면 버그로 보이므로 강제였다는 사실을 대신 적는다.
		if (ResolutionView.bForced)
		{
			Text_Roll->SetText(NSLOCTEXT("BG", "ResolutionForced", "판정 강제됨"));
		}
		else if (ResolutionView.Roll != INDEX_NONE)
		{
			Text_Roll->SetText(FText::Format(
				NSLOCTEXT("BG", "ResolutionRoll", "눈 {0} / 성공 {1}%"),
				FText::AsNumber(ResolutionView.Roll),
				FText::AsNumber(FMath::RoundToInt(ResolutionView.Chance.GetSuccessOrBetter()))));
		}
		else
		{
			Text_Roll->SetText(FText::GetEmpty());
		}

		Text_Roll->SetColorAndOpacity(FSlateColor(TierColor));
	}

	if (Text_Card)
	{
		//	누가 썼는지까지 적는다 — GDD §4.1 "모든 플레이어에게 동일한 결과를 표시".
		//	남의 판정도 뜨므로 카드 이름만 있으면 누구 것인지 알 수 없다.
		Text_Card->SetText(
			ResolutionView.SeatIndex == INDEX_NONE
				? ResolutionView.CardName
				: FText::Format(NSLOCTEXT("BG", "ResolutionCardBySeat", "{0}  ·  {1}"),
					ResolutionView.CardName, ResolutionView.SeatName));
	}

	if (Text_Chance)
	{
		const FBGCardChanceView& C = ResolutionView.Chance;
		Text_Chance->SetText(
			C.IsSet()
				? FText::Format(
					NSLOCTEXT("BG", "ResolutionChanceDetail", "대실패 {0} · 실패 {1} · 성공 {2} · 대성공 {3}"),
					FText::AsNumber(FMath::RoundToInt(C.CriticalFailure)),
					FText::AsNumber(FMath::RoundToInt(C.Failure)),
					FText::AsNumber(FMath::RoundToInt(C.Success)),
					FText::AsNumber(FMath::RoundToInt(C.CriticalSuccess)))
				: FText::GetEmpty());
	}
}
