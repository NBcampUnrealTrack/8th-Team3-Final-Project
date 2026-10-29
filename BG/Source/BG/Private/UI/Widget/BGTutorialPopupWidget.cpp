#include "UI/Widget/BGTutorialPopupWidget.h"

#include "Audio/BGAudioSubsystem.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Core/BGGameInstance.h"
#include "Core/BGGameState.h"
#include "Core/BGPlayerController.h"
#include "Debug/BGCardLog.h"
#include "Engine/DataTable.h"
#include "Tutorial/BGTutorialStepRow.h"

void UBGTutorialPopupWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (UBGAudioSubsystem* Audio = UBGAudioSubsystem::Get(this))
	{
		Audio->BindDefaultButtonsInWidget(this);
		Audio->BindButtonSoundGroup(Btn_Next, EBGUIButtonSoundGroup::Confirm);
	}

	if (Btn_Next)
	{
		Btn_Next->OnClicked.AddDynamic(this, &UBGTutorialPopupWidget::HandleNextClicked);
	}

	//	바인딩 + 지금 이미 단계가 떠 있으면(늦게 생성) 바로 채운다.
	HandleTurnStateChanged();
}

void UBGTutorialPopupWidget::NativeDestruct()
{
	if (ABGGameState* GS = BoundGameState.Get())
	{
		GS->OnTurnStateChanged.RemoveDynamic(this, &UBGTutorialPopupWidget::HandleTurnStateChanged);
	}

	Super::NativeDestruct();
}

void UBGTutorialPopupWidget::HandleNextClicked()
{
	if (ABGPlayerController* PC = GetOwningPlayer<ABGPlayerController>())
	{
		PC->RequestTutorialNext();
	}
}

void UBGTutorialPopupWidget::TryBindTurnStateDelegate()
{
	ABGGameState* GS = GetWorld() ? GetWorld()->GetGameState<ABGGameState>() : nullptr;
	if (!GS || BoundGameState.Get() == GS)
	{
		return;
	}

	GS->OnTurnStateChanged.AddUniqueDynamic(this, &UBGTutorialPopupWidget::HandleTurnStateChanged);
	BoundGameState = GS;
}

void UBGTutorialPopupWidget::HandleTurnStateChanged()
{
	TryBindTurnStateDelegate();

	const ABGGameState* GS = BoundGameState.Get();
	const FName StepId = GS ? GS->GetActiveTutorialStep() : NAME_None;
	if (StepId.IsNone())
	{
		//	떼어내지 않고 접는다 - HUD 캔버스의 자식이라 슬롯이 만들어지는 중에 사라지면 안 된다.
		SetVisibility(ESlateVisibility::Collapsed);
		ShownStep = NAME_None;
		return;
	}

	//	턴 상태 통지는 한 단계가 떠 있는 동안에도 여러 번 온다.
	if (StepId == ShownStep)
	{
		return;
	}

	const UBGGameInstance* GI = GetGameInstance<UBGGameInstance>();
	const UDataTable* Table = GI ? GI->GetTutorialStepTable() : nullptr;
	const FBGTutorialStepRow* Row = Table ? Table->FindRow<FBGTutorialStepRow>(StepId, TEXT("TutorialPopup")) : nullptr;
	if (!Row)
	{
		UE_LOG(LogBGCard, Warning, TEXT("[Tutorial] 팝업이 단계 %s 의 문구를 찾지 못했다 - 표 지정을 확인할 것."),
			*StepId.ToString());
		SetVisibility(ESlateVisibility::Collapsed);
		ShownStep = NAME_None;
		return;
	}

	ShownStep = StepId;

	if (Text_Title)
	{
		Text_Title->SetText(Row->Title);
	}
	if (Text_Body)
	{
		Text_Body->SetText(Row->Body);
	}
	if (Btn_Next)
	{
		//	행동형 단계는 버튼 없이 안내만 띄운다. 끝내는 건 사용자의 행동이다.
		Btn_Next->SetVisibility(Row->CompleteOn == EBGTutorialComplete::NextButton
			? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	//	빈 곳은 아래 HUD 로 클릭이 통과해야 한다. 버튼처럼 눌리는 자식은 따로 클릭을 받는다.
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}
