// BANG! 세션 UI — 방 생성 / 코드 조인 / 방 목록 / 준비 / 시작.
// WBP 에 아래 이름으로 위젯을 올리면 그쪽이 쓰이고, 없는 것만 C++ 이 만든다.
//   HostButton RoomNameInput JoinCodeButton JoinCodeInput RefreshButton
//   ReadyButton ReadyButtonText StartGameButton LeaveButton
//   StatusText RoomCodeText LobbyText ResultList

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Session/BGSessionTypes.h"
#include "BGSessionMenuWidget.generated.h"

class IBGSessionInterface;
class UBGSessionMenuWidget;
class UButton;
class UEditableTextBox;
class UScrollBox;
class UTextBlock;
class UVerticalBox;

// 방 목록 한 줄의 클릭 대상. UButton::OnClicked 가 인자를 넘기지 못해 행마다 하나씩 둔다.
UCLASS()
class UBGSessionRowHandler : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TObjectPtr<UBGSessionMenuWidget> Menu;

	UPROPERTY()
	int32 ResultIndex = INDEX_NONE;

	UFUNCTION()
	void HandleClicked();
};

UCLASS()
class BG_API UBGSessionMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 목록 행에서 호출된다(UBGSessionRowHandler).
	void JoinByIndex(int32 ResultIndex);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	IBGSessionInterface* GetSession() const;

	// 디자이너가 안 올린 위젯만 만들어 붙인다.
	void BuildMissingWidgets();
	bool HasAllWidgets() const;
	UTextBlock* AddLabel(UVerticalBox* Parent, const FString& Text, int32 FontSize = 12);
	UButton* AddButton(UVerticalBox* Parent, const FString& Label, FName WidgetName);
	UEditableTextBox* AddInput(UVerticalBox* Parent, const FString& HintText, FName WidgetName);

	// 버튼 클릭 연결 — 디자이너/C++ 어느 트리든 여기서 한 번에 붙인다.
	void BindButtonEvents();

	void RefreshHeader();

	// 접속 인원 표시는 통지가 없어 짧은 주기로 갱신한다.
	void RefreshLobbyStatus();

	UFUNCTION()
	void HandleHostClicked();
	UFUNCTION()
	void HandleJoinCodeClicked();
	UFUNCTION()
	void HandleRefreshClicked();
	UFUNCTION()
	void HandleReadyClicked();
	UFUNCTION()
	void HandleStartGameClicked();
	UFUNCTION()
	void HandleLeaveClicked();

	UFUNCTION()
	void HandlePhaseChanged(EBGSessionPhase Phase, const FString& Message);
	UFUNCTION()
	void HandleSessionListReady(const TArray<FBGSessionSearchResult>& Results);

	// 디자이너가 올린 위젯이 있으면 이름으로 붙고, 없으면 BuildDefaultTree 가 채운다.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StatusText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> RoomCodeText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UEditableTextBox> RoomNameInput;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UEditableTextBox> JoinCodeInput;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UScrollBox> ResultList;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> LobbyText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> HostButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> JoinCodeButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> RefreshButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> ReadyButton;

	// 준비/준비 해제로 라벨이 바뀐다. 버튼 안에 이 이름으로 텍스트를 넣어둔다.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ReadyButtonText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> StartGameButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> LeaveButton;

	FTimerHandle LobbyRefreshTimer;

	// 로비를 벗어난 페이즈를 본 뒤 한 번만 닫는다. 다시 F9 로 열면 그대로 둔다.
	bool bClosedOnMatchStart = false;

	// 목록 행 핸들러 — 목록을 다시 만들 때까지 살아 있어야 클릭이 동작한다.
	UPROPERTY(Transient)
	TArray<TObjectPtr<UBGSessionRowHandler>> RowHandlers;
};
