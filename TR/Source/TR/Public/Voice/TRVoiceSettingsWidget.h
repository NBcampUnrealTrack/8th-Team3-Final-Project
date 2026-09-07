// TRVoiceSettingsWidget.h
// 임시 음성 설정 패널의 C++ 베이스.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TRVoiceSettingsWidget.generated.h"

class UButton;
class UCheckBox;
class USlider;
class UTextBlock;
class UTRVoiceSubsystem;

/**
 * WBP 를 이 클래스로 파생시키고 아래 이름 그대로 위젯을 두면 자동으로 연결된다.
 * 슬롯은 전부 선택이라 일부만 두어도 되고, 하나도 없어도 패널은 뜬다.
 */
UCLASS()
class TR_API UTRVoiceSettingsWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 서브시스템 값으로 모든 슬롯을 다시 채운다. */
	UFUNCTION(BlueprintCallable, Category = "TR|Voice")
	void RefreshFromSettings();

protected:
	virtual void NativeConstruct() override;

	virtual void NativeDestruct() override;

private:
	UFUNCTION()
	void HandlePushToTalkChanged(bool bIsChecked);

	UFUNCTION()
	void HandleOutputMutedChanged(bool bIsChecked);

	UFUNCTION()
	void HandleMicGainChanged(float Value);

	UFUNCTION()
	void HandleMicThresholdChanged(float Value);

	UFUNCTION()
	void HandleMicNoiseGateChanged(float Value);

	UFUNCTION()
	void HandleJitterDelayChanged(float Value);

	UFUNCTION()
	void HandleSaveClicked();

	UFUNCTION()
	void HandleResetClicked();

	UFUNCTION()
	void HandleCloseClicked();

	/** 누군가 말하기 시작/중지했다. 서브시스템이 엔진 델리게이트를 옮겨 준다. */
	UFUNCTION()
	void HandleTalkingChanged(const FString& PlayerId, bool bIsTalking);

	void RefreshStatus();

	UTRVoiceSubsystem* GetVoice() const;

	//--- 선택 슬롯. 이름이 맞는 위젯이 WBP 에 있으면 연결된다 ---
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCheckBox> Check_PushToTalk;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCheckBox> Check_OutputMuted;

	// 0~1 슬라이더. 실제 범위 환산은 위젯이 한다(마이크 증폭은 0~2).
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<USlider> Slider_MicGain;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<USlider> Slider_MicThreshold;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<USlider> Slider_MicNoiseGate;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<USlider> Slider_JitterDelay;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_MicGain;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_MicThreshold;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_MicNoiseGate;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_JitterDelay;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Status;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Save;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Reset;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Close;

	// 슬라이더 콜백이 다시 슬라이더를 쓰는 것을 막는다.
	bool bRefreshing = false;
};
