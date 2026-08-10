// TR 세션 메뉴 — 방 만들기 · 코드로 참가 · 목록에서 참가 · 친구 초대.
//
// 이 위젯은 세션 구현을 모른다. ITRSessionInterface 로만 이야기한다.
// WBP 에서 아래 이름으로 위젯을 만들면 자동으로 연결된다. 없어도 동작한다.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Session/TRSessionTypes.h"
#include "TRSessionMenuWidget.generated.h"

class ITRSessionInterface;
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

	//	자리마다 콜백이 따로 필요하다 — 다이내믹 델리게이트는 인자를 실어 보낼 수 없다.

	UFUNCTION()
	void HandleJoinSlot0();

	UFUNCTION()
	void HandleJoinSlot1();

	UFUNCTION()
	void HandleJoinSlot2();

	UFUNCTION()
	void HandleJoinSlot3();

	/** 자리 하나를 결과로 채운다. Result 가 없으면 그 줄을 숨긴다. */
	void ApplySlot(int32 SlotIndex, const FTRSessionSearchResult* Result);

	/** 자리 번호를 검색 인덱스로 바꿔 참가한다. */
	void JoinBySlot(int32 SlotIndex);

	/** 지금 화면에 올라간 목록. 자리 번호 → 검색 인덱스. */
	TArray<int32> SlotResultIndices;

	// 세션 플로우 통지.

	UFUNCTION()
	void HandlePhaseChanged(ETRSessionPhase Phase, const FString& Message);

	UFUNCTION()
	void HandleListReady(const TArray<FTRSessionSearchResult>& Results);

	/** 세션 규약을 들고 있는 서브시스템. 없으면 nullptr. */
	ITRSessionInterface* GetSession() const;

	void BindButtons();
	void BindSessionEvents();
	void UnbindSessionEvents();

	/** 방 코드와 로비 인원 표시를 지금 값으로 맞춘다. */
	void RefreshStatusTexts();

	void SetStatus(const FString& Message);
};
