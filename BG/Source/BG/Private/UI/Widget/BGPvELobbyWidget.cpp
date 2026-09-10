#include "UI/Widget/BGPvELobbyWidget.h"

#include "Components/Button.h"
#include "Core/BGGameState.h"
#include "Core/BGPlayerController.h"

void UBGPvELobbyWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (StartButton)
	{
		StartButton->OnClicked.AddDynamic(this, &UBGPvELobbyWidget::HandleStartClicked);
	}

	TryBindTurnStateDelegate();
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
	if (GS && GS->IsMatchRunning())
	{
		RemoveFromParent();   // 판이 시작됐다. 로비 위젯은 더 필요 없다.
	}
}
