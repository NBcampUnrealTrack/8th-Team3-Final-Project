#include "UI/Widget/BGPvELobbyWidget.h"

#include "Audio/BGAudioSubsystem.h"
#include "Components/Button.h"
#include "Core/BGGameState.h"
#include "Core/BGPlayerController.h"

void UBGPvELobbyWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (UBGAudioSubsystem* Audio = UBGAudioSubsystem::Get(this))
	{
		Audio->BindDefaultButtonsInWidget(this);
	}

	if (ensureMsgf(StartButton, TEXT("[BG] PvE 로비 위젯의 StartButton 이 비어있습니다. WBP 를 확인하세요.")))
	{
		StartButton->OnClicked.AddDynamic(this, &UBGPvELobbyWidget::HandleStartClicked);
	}

	//	바인딩 + 지금 이미 판이 시작됐거나(늦게 생성) 보상 선택이 걸려 있으면 바로 숨긴다.
	HandleTurnStateChanged();
}

void UBGPvELobbyWidget::NativeDestruct()
{
	if (ABGGameState* GS = BoundGameState.Get())
	{
		GS->OnTurnStateChanged.RemoveDynamic(this, &UBGPvELobbyWidget::HandleTurnStateChanged);
	}

	Super::NativeDestruct();
}

void UBGPvELobbyWidget::HandleStartClicked()
{
	if (ABGPlayerController* PC = GetOwningPlayer<ABGPlayerController>())
	{
		PC->RequestStartPvEMatch();
	}
}

void UBGPvELobbyWidget::TryBindTurnStateDelegate()
{
	ABGGameState* GS = GetWorld() ? GetWorld()->GetGameState<ABGGameState>() : nullptr;
	if (!GS || BoundGameState.Get() == GS)
	{
		return;
	}

	GS->OnTurnStateChanged.AddUniqueDynamic(this, &UBGPvELobbyWidget::HandleTurnStateChanged);
	BoundGameState = GS;
}

void UBGPvELobbyWidget::HandleTurnStateChanged()
{
	TryBindTurnStateDelegate();

	const ABGGameState* GS = BoundGameState.Get();
	if (GS && (GS->IsMatchRunning() || GS->GetPendingRewardCandidates().Num() > 0))
	{
		//	판이 시작됐거나, 보상 선택 화면이 대신 뜰 차례다 - 로비 버튼은 필요 없다.
		RemoveFromParent();
	}
}
