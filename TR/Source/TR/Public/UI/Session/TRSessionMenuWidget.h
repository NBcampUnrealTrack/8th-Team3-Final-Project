// TR 세션 메뉴 — 방 만들기 · 코드로 참가 · 목록에서 참가 · 친구 초대.
//
// 이 위젯은 세션 구현을 모른다. IInterface_TRSession 로만 이야기한다.
// WBP 에서 아래 이름으로 위젯을 만들면 자동으로 연결된다. 없어도 동작한다.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Session/TRSessionTypes.h"
#include "TRSessionMenuWidget.generated.h"

class IInterface_TRSession;
class UButton;
class UEditableTextBox;
class UScrollBox;
class UTextBlock;

UCLASS(Blueprintable)
class TR_API UTRSessionMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 목록의 한 줄을 눌렀다. WBP 가 만든 행 위젯이 이 인덱스로 알려 준다. */
	UFUNCTION(BlueprintCallable, Category = "TR|Session")
	void JoinByIndex(int32 ResultIndex);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/**
	 * 메뉴가 떠 있는 동안은 게임 입력을 막는다(UIOnly). 그러면 컨트롤러의 디버그 키가
	 * 안 들어오므로 닫는 키는 여기서 직접 받는다.
	 */
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	//	WBP 가 같은 이름으로 만들어 두면 자동으로 연결된다. 없어도 동작한다.

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Host;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_JoinByCode;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Refresh;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Invite;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Leave;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Close;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UEditableTextBox> Input_RoomName;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UEditableTextBox> Input_JoinCode;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Status;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_RoomCode;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Lobby;

	/** 준비 토글. 호스트에게는 숨긴다. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Ready;

	/** 판 시작. 호스트에게만 보이고, 전원 준비 전에는 잠긴다. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Start;

	/** "준비 2 / 3" 표시. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Ready;

	/** 준비 버튼 안의 글자. 준비 상태에 따라 "준비"와 "준비 취소"로 바뀐다. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Button_Ready_Text;

	/** 검색 결과를 담을 상자. 행 위젯을 WBP 가 직접 만들 때 쓴다. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UScrollBox> Box_Results;

	/**
	 * 목록 자리 — 검색 결과를 위에서부터 채운다. 결과가 적으면 남는 자리는 숨는다.
	 * 행을 동적으로 만들지 않아도 참가가 되게 하려는 것이다.
	 * 자리를 늘리려면 WBP 에 Text_Slot5 / Button_JoinSlot5 처럼 이어서 만들고 여기에 더한다.
	 */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Slot0;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Slot1;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Slot2;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Slot3;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_JoinSlot0;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_JoinSlot1;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_JoinSlot2;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_JoinSlot3;

	/** 방 목록이 갱신됐다. WBP 가 Box_Results 를 비우고 행을 다시 만든다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "TR|Session")
	void OnSessionListReady(const TArray<FTRSessionSearchResult>& Results);

	/** 단계가 바뀌었다. 버튼 잠금이나 스피너처럼 텍스트 말고 더 할 것이 있으면 여기서. */
	UFUNCTION(BlueprintImplementableEvent, Category = "TR|Session")
	void OnSessionPhaseChanged(ETRSessionPhase Phase, const FString& Message);

private:
	// 버튼 콜백. Dynamic 델리게이트라 UFUNCTION 이어야 한다.

	UFUNCTION()
	void HandleHostClicked();

	UFUNCTION()
	void HandleJoinByCodeClicked();

	UFUNCTION()
	void HandleRefreshClicked();

	UFUNCTION()
	void HandleInviteClicked();

	UFUNCTION()
	void HandleLeaveClicked();

	UFUNCTION()
	void HandleCloseClicked();

	UFUNCTION()
	void HandleReadyClicked();

	UFUNCTION()
	void HandleStartClicked();

	/** 로비 인원·준비 상황을 다시 그린다. 복제로 바뀌는 값이라 주기적으로 부른다. */
	void RefreshLobbyControls();

	FTimerHandle LobbyRefreshTimer;

	// 직전에 반영한 값. 바뀐 것이 없으면 위젯을 건드리지 않는다.
	bool bLastHost = false;
	bool bLastCanStart = false;
	bool bLastMatchStarted = false;
	bool bLastLocalReady = false;
	int32 LastReadyPlayers = -1;
	int32 LastRequiredPlayers = -1;

	// 판이 도는 중에 열린 메뉴인가. 이 경우 판 시작 자동 닫기를 하지 않는다.
	bool bOpenedDuringMatch = false;

	// 자동 닫기를 이미 걸었는가. 갱신이 여러 경로로 불려도 한 번만 예약한다.
	bool bClosePending = false;

	//	자리마다 콜백이 따로 필요하다 — 다이내믹 델리게이트는 인자를 실어 보낼 수 없다.

	UFUNCTION()
	void HandleJoinSlot0();

	UFUNCTION()
	void HandleJoinSlot1();

	UFUNCTION()
	void HandleJoinSlot2();

	UFUNCTION()
	void HandleJoinSlot3();

	// 세션 플로우 통지.

	UFUNCTION()
	void HandlePhaseChanged(ETRSessionPhase Phase, const FString& Message);

	UFUNCTION()
	void HandleListReady(const TArray<FTRSessionSearchResult>& Results);

	/** 세션 규약을 들고 있는 서브시스템. 없으면 nullptr. */
	IInterface_TRSession* GetSession() const;

	void BindButtons();
	void BindSessionEvents();
	void UnbindSessionEvents();

	/** 자리 하나를 결과로 채운다. Result 가 없으면 그 줄을 숨긴다. */
	void ApplySlot(int32 SlotIndex, const FTRSessionSearchResult* Result);

	/** 자리 번호를 검색 인덱스로 바꿔 참가한다. */
	void JoinBySlot(int32 SlotIndex);

	/** 방 코드와 로비 인원 표시를 지금 값으로 맞춘다. */
	void RefreshStatusTexts();

	void SetStatus(const FString& Message);

	/** 메뉴를 닫는다. 여닫는 판단은 컨트롤러 몫이라 그쪽에 넘긴다. */
	void RequestClose();

	/** 지금 화면에 올라간 목록. 자리 번호 → 검색 인덱스. */
	TArray<int32> SlotResultIndices;

	/** 열기 전 커서 상태. 닫을 때 그대로 되돌린다 — 게임 쪽 설정을 짐작하지 않는다. */
	bool bPrevShowMouseCursor = false;

	/** 시점·이동을 막아 두었는가. 카운터 방식이라 정확히 한 번만 되돌려야 한다. */
	bool bInputSuppressed = false;
};
