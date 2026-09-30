// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BG/Public/Chating/BGChatEntry.h"
#include "BGChatingWidget.generated.h"

class UScrollBox;
class UEditableTextBox;
class UButton;
class UBGChatingSubsystem;
class UTexture2D;

UCLASS(Abstract, Blueprintable)
class BG_API UBGChatingWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 입력창을 열고 닫는다. 버튼 말고 다른 경로로도 부를 수 있게 열어 둔다. */
	UFUNCTION(BlueprintCallable, Category = "BG|Chat")
	void ToggleChatInput();
	
	UFUNCTION(BlueprintCallable, Category = "BG|Chat")
	void OpenChatInput();
	
	UFUNCTION(BlueprintCallable, Category = "BG|Chat")
	void CloseChatInput();

	UFUNCTION(BlueprintPure, Category = "BG|Chat")
	bool IsInputOpen() const { return bIsInputOpen; }
	
	uint64 GetLastClosedFrame() const { return LastClosedFrame; }
	
	UFUNCTION(BlueprintCallable, Category = "BG|Chat")
	void SetChatFilter(EBGChatFilter NewFilter);

	UFUNCTION(BlueprintPure, Category = "BG|Chat")
	EBGChatFilter GetChatFilter() const { return ChatFilter; }

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UScrollBox> ChatScrollBox;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UEditableTextBox> ChatInputBox;

	/** 없으면 ToggleChatInput 을 BP 에서 직접 불러야 한다. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> ChatToggleButton;

	/** 입력창이 열렸다. 슬라이드 인 같은 연출은 BP 에서 여기에 붙인다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "BG|Chat")
	void OnInputBoxOpened();

	UFUNCTION(BlueprintImplementableEvent, Category = "BG|Chat")
	void OnInputBoxClosed();
	
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> FilterAllButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> FilterPlayerButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> FilterSystemButton;

	/** 필터가 바뀌었다. 탭 강조 표시는 BP 에서 여기에 붙인다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "BG|Chat")
	void OnChatFilterChanged(EBGChatFilter NewFilter);

	/** 로그에 남길 최대 줄 수. 넘으면 오래된 것부터 지운다. */
	UPROPERTY(EditDefaultsOnly, Category = "BG|Chat", meta = (ClampMin = "1"))
	int32 MaxVisibleMessages = 30;

	/** 로그 한 줄의 글자 크기. */
	UPROPERTY(EditDefaultsOnly, Category = "BG|Chat", meta = (ClampMin = "6"))
	int32 LogFontSize = 12;

	UPROPERTY(EditDefaultsOnly, Category = "BG|Chat")
	FLinearColor LogTextColor = FLinearColor(1.f, 1.f, 1.f, 0.9f);
	
	UPROPERTY(EditDefaultsOnly, Category = "BG|Chat")
	FLinearColor SystemTextColor = FLinearColor(1.f, 0.85f, 0.4f, 0.9f);

	/** 필터 탭 배경. 선택된 탭만 Active 를 쓴다. */
	UPROPERTY(EditDefaultsOnly, Category = "BG|Chat")
	TObjectPtr<UTexture2D> FilterTabActive;

	UPROPERTY(EditDefaultsOnly, Category = "BG|Chat")
	TObjectPtr<UTexture2D> FilterTabInactive;

private:
	UFUNCTION()
	void HandleMessageReceived(const FBGChatEntry& Entry);

	UFUNCTION()
	void HandleInputCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	UFUNCTION()
	void HandleInputChanged(const FText& Text);

	UFUNCTION()
	void HandleToggleButtonClicked();

	void AddEntryToLog(const FBGChatEntry& Entry);
	void TrimOldMessages();


	/** 이 게임의 기본 입력 모드로 되돌린다(BGSessionMenuWidget 과 같은 설정). */
	void RestoreGameInputMode();

	bool bIsInputOpen = false;

	TWeakObjectPtr<UBGChatingSubsystem> CachedSubsystem;
	
	uint64 LastClosedFrame = 0;
	double LastTypingSoundTime = -1.0;
	FString PreviousInputText;
	
	UFUNCTION() void HandleFilterAllClicked();
	UFUNCTION() void HandleFilterPlayerClicked();
	UFUNCTION() void HandleFilterSystemClicked();

	/** 지금 필터에서 이 줄을 보여줄 것인가. */
	bool PassesFilter(const FBGChatEntry& Entry) const;

	void RefreshFilterButtons();

	/** ScrollBox 를 비우고 History 전체를 필터에 맞춰 다시 그린다. */
	void RebuildLog();

	EBGChatFilter ChatFilter = EBGChatFilter::All;
};
