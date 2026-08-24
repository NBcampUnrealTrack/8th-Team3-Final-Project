// TRMainMenuWidget.h

#pragma once

#include "CommonUserWidget.h"
#include "Session/TRSessionTypes.h"
#include "TRMainMenuWidget.generated.h"

class IInterface_TRSession;
class UCommonButtonBase;
class UEditableTextBox;
class UTextBlock;
class UWidget;
class UTRMenuButton;
class UTRSettingsWidget;

UCLASS(Abstract)
class TR_API UTRMainMenuWidget : public UCommonUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonButtonBase> Btn_FindGame;

	/** 방/로비 패널. 처음에는 접어 두고 Btn_Host 로 펼친다. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> Border_Session;

	/** 캔버스에서 Border_Session 과 같은 자리를 쓴다. 한 번에 하나만 보인다. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> Border_Find;

	// BindWidget 을 쓰면 WBP 가 들고 있는 Btn_Settings 변수와 이름이 충돌해
	// 위젯 컴파일이 깨진다. 그래서 이름으로 찾아 둔다.
	UPROPERTY(Transient)
	TObjectPtr<UWidget> SettingsButton;

	// Btn_Settings 로 띄울 설정 WBP. WBP_TRMainMenu 의 클래스 디폴트에서 고른다.
	UPROPERTY(EditDefaultsOnly, Category = "TR|UI")
	TSoftClassPtr<UTRSettingsWidget> SettingsWidgetClass;

	// 아래는 세션 패널. WBP 가 만들지 않은 것은 그냥 없는 기능이 된다.

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTRMenuButton> Btn_Host;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTRMenuButton> Btn_JoinByCode;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTRMenuButton> Btn_Refresh;

	/** 입력한 이름으로 방을 다시 만든다. 방 코드가 새로 나오고 들어와 있던 사람은 끊긴다. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTRMenuButton> Btn_RenameRoom;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTRMenuButton> Btn_Invite;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTRMenuButton> Btn_Leave;

	/** 준비 토글. 호스트에게는 숨긴다. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTRMenuButton> Btn_Ready;

	/** 판 시작. 호스트에게만 보이고, 전원 준비 전에는 잠긴다. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTRMenuButton> Btn_Start;

	/** 게임을 끝낸다. 세션에 들어가 있으면 먼저 정리하고 종료한다. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTRMenuButton> Btn_Quit;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UEditableTextBox> Input_RoomName;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UEditableTextBox> Input_JoinCode;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Status;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_RoomCode;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Lobby;

	/** "준비 2 / 3" 표시. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Ready;

	/**
	 * 목록 자리 — 검색 결과를 위에서부터 채운다. 결과가 적으면 남는 자리는 숨는다.
	 * 자리를 늘리려면 WBP 에 Txt_Slot4 / Btn_JoinSlot4 를 이어서 만들고 여기에 더한다.
	 */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Slot0;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Slot1;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Slot2;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Slot3;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTRMenuButton> Btn_JoinSlot0;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTRMenuButton> Btn_JoinSlot1;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTRMenuButton> Btn_JoinSlot2;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTRMenuButton> Btn_JoinSlot3;

private:
	static constexpr int32 SlotCount = 4;

	void HandleFindGameClicked();

	void HandleSettingsClicked();

	// 위젯이 스스로 비활성화될 때도 불린다.
	void CloseSettings();

	// 세션 버튼.

	void HandleHostClicked();
	void HandleRenameRoomClicked();
	void HandleJoinByCodeClicked();
	void HandleRefreshClicked();
	void HandleInviteClicked();
	void HandleLeaveClicked();
	void HandleReadyClicked();
	void HandleStartClicked();
	void HandleQuitClicked();

	// 세션 플로우 통지. Dynamic 델리게이트라 UFUNCTION 이어야 한다.

	UFUNCTION()
	void HandlePhaseChanged(ETRSessionPhase Phase, const FString& Message);

	UFUNCTION()
	void HandleListReady(const TArray<FTRSessionSearchResult>& Results);

	/** 세션 규약을 들고 있는 서브시스템. 없으면 nullptr. */
	IInterface_TRSession* GetSession() const;

	void BindSessionButtons();
	void BindSessionEvents();
	void UnbindSessionEvents();

	/** 자리 하나를 결과로 채운다. Result 가 없으면 그 줄을 숨긴다. */
	void ApplySlot(int32 SlotIndex, const FTRSessionSearchResult* Result);

	/** 자리 번호를 검색 인덱스로 바꿔 참가한다. */
	void JoinBySlot(int32 SlotIndex);

	/** 방 코드와 로비 인원 표시를 지금 값으로 맞춘다. */
	void RefreshStatusTexts();

	/** 로비 인원·준비 상황을 다시 그린다. 복제로 바뀌는 값이라 주기적으로 부른다. */
	void RefreshLobbyControls();

	void SetStatus(const FString& Message);

	/** 요청이 도는 중에는 그 요청을 다시 걸 수 있는 버튼을 잠근다. */
	void ApplyPhaseLocks(ETRSessionPhase Phase);

	void ShowOnlyPanel(UWidget* PanelToShow);

	bool IsInSession() const;

	// PIE를 리슨 서버로 띄우면 넷모드는 처음부터 서버다. 방 코드로 봐야 방 만들기를 실제로 했는지 구분된다.
	bool HasHostedRoom() const;

	void RefreshSettingsVisibility();

	void RefreshSessionPresence();

	/** 지금 화면에 올라간 목록. 자리 번호 → 검색 인덱스. */
	TArray<int32> SlotResultIndices;

	UPROPERTY(Transient)
	TObjectPtr<UTRSettingsWidget> SettingsPopup;

	FTimerHandle LobbyRefreshTimer;

	// 직전에 반영한 값. 바뀐 것이 없으면 위젯을 건드리지 않는다.
	// 처음 한 번은 반드시 적용되도록 비워 둔다.
	TOptional<bool> LastInSession;

	bool bLastHost = false;
	bool bLastCanStart = false;
	bool bLastMatchStarted = false;
	bool bLastLocalReady = false;
	int32 LastReadyPlayers = -1;
	int32 LastRequiredPlayers = -1;
	bool bLastInSessionControls = false;
	bool bLastHasHostedRoom = false;
	bool bLastFindPanelOpen = false;
	bool bPanelOpen = false;

	// 목록 패널 위에 겹쳐 보이는 버튼을 접는 데 쓴다.
	bool bFindPanelOpen = false;
};
