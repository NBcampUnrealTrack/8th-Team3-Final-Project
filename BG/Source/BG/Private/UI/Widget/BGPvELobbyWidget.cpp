#include "UI/Widget/BGPvELobbyWidget.h"

#include "Audio/BGAudioSubsystem.h"
#include "Components/Button.h"
#include "Core/BGGameInstance.h"
#include "Core/BGGameMode.h"
#include "Core/BGGameState.h"
#include "Core/BGPlayerController.h"

void UBGPvELobbyWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (UBGAudioSubsystem* Audio = UBGAudioSubsystem::Get(this))
	{
		Audio->BindDefaultButtonsInWidget(this);
		Audio->BindButtonSoundGroup(StartButton, EBGUIButtonSoundGroup::Confirm);
	}

	//	HUD 는 모드를 가리지 않고 뜨므로 여기서 걸러낸다 — PvP 에 "게임 시작" 버튼이
	//	나오면 안 된다.
	if (!ShouldShowLobby())
	{
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	if (ensureMsgf(StartButton, TEXT("[BG] PvE 로비 위젯의 StartButton 이 비어있습니다. WBP 를 확인하세요.")))
	{
		StartButton->OnClicked.AddDynamic(this, &UBGPvELobbyWidget::HandleStartClicked);
	}

	//	바인딩 + 지금 이미 판이 시작됐거나(늦게 생성) 보상 선택이 걸려 있으면 바로 숨긴다.
	HandleTurnStateChanged();
}

bool UBGPvELobbyWidget::ShouldShowLobby() const
{
	if (ABGGameMode::GetPlayMode(this) != EBGPlayMode::PvE)
	{
		return false;
	}

	//	직전 조우를 이겨서 이어서 온 판은 사람 눈에 로비 스킵처럼 보여야 한다 —
	//	버튼을 아예 띄우지 않는다. 플래그를 지우는 건 서버의 UBGMatchStartComponent 다.
	const UBGGameInstance* GI = Cast<UBGGameInstance>(GetGameInstance());
	return !(GI && GI->ShouldAutoStartNextPvEMatch());
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
		//	떼어내지 않고 접는다 — HUD 캔버스의 자식이라, 이 함수가 NativeConstruct 에서
		//	불릴 때 RemoveFromParent 를 하면 부모가 슬롯을 만들고 있는 중에 슬롯이 사라진다.
		SetVisibility(ESlateVisibility::Collapsed);
	}
}
