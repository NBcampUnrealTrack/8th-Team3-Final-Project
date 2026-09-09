// TRVoiceSettingsWidget.h
// 임시 음성 설정 패널의 C++ 베이스.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TRVoiceSettingsWidget.generated.h"

class UButton;
class UCheckBox;
class UProgressBar;
class USlider;
class UTextBlock;
class UVerticalBox;
class UTRVoiceSubsystem;

/** 접속자 한 줄의 위젯과 NetId. */
UCLASS()
class UTRVoicePlayerRow : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION()
	void HandleVolumeChanged(float Value);

	UPROPERTY()
	FString NetId;

	UPROPERTY()
	TObjectPtr<UTextBlock> Text_Name;

	UPROPERTY()
	TObjectPtr<UProgressBar> Bar_Level;

	UPROPERTY()
	TObjectPtr<USlider> Slider_Volume;

	UPROPERTY()
	TObjectPtr<UTextBlock> Text_Volume;

	float ShownLevel = 0.f;
};

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

	/** 게이지를 갱신하고 접속자 집합 변화를 확인한다. */
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	UFUNCTION()
	void HandlePushToTalkChanged(bool bIsChecked);

	UFUNCTION()
	void HandleOutputMutedChanged(bool bIsChecked);

	UFUNCTION()
	void HandleSpatialChanged(bool bIsChecked);

	UFUNCTION()
	void HandleMicGainChanged(float Value);

	UFUNCTION()
	void HandleMicThresholdChanged(float Value);

	UFUNCTION()
	void HandleMicNoiseGateChanged(float Value);

	UFUNCTION()
	void HandleJitterDelayChanged(float Value);

	UFUNCTION()
	void HandleOutputVolumeChanged(float Value);

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

	/** 접속자 집합이 바뀌었을 때만 줄을 다시 만든다. */
	void RebuildPlayerRows();

	void UpdateGauges(float DeltaTime);

	/** 가로 박스 안 게이지·값 칸에 Fill 크기를 주고 배경 패널을 내용 크기에 맞춘다. */
	void FixRowSlots();

	UTRVoiceSubsystem* GetVoice() const;

	//--- 선택 슬롯. 이름이 맞는 위젯이 WBP 에 있으면 연결된다 ---
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCheckBox> Check_PushToTalk;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCheckBox> Check_OutputMuted;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCheckBox> Check_Spatial;

	// 0~1 슬라이더. 실제 범위 환산은 위젯이 한다(마이크 증폭은 0~2, 수신 음량도 0~2).
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<USlider> Slider_MicGain;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<USlider> Slider_MicThreshold;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<USlider> Slider_MicNoiseGate;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<USlider> Slider_JitterDelay;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<USlider> Slider_OutputVolume;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_MicGain;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_MicThreshold;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_MicNoiseGate;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_JitterDelay;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_OutputVolume;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Status;

	// 내 마이크 입력 게이지. 무전 키를 누르는 동안만 움직인다.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> Bar_MicLevel;

	// 접속자 줄이 들어갈 자리. 줄은 C++ 가 만든다.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UVerticalBox> Box_Players;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Save;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Reset;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Close;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTRVoicePlayerRow>> PlayerRows;

	// 슬라이더 콜백이 다시 슬라이더를 쓰는 것을 막는다.
	bool bRefreshing = false;

	float ShownMicLevel = 0.f;

	float ListCheckAccum = 0.f;
};
