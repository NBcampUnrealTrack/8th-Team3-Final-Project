// BANG! 설정 UI — 그래픽 / 오디오.
// HUD 오른쪽 위 설정 버튼과 설정 패널이 한 위젯 안에 같이 있다. 버튼이 패널을 토글한다.
// WBP_BGSettings 에 아래 이름으로 위젯을 올리면 C++ 이 이름으로 찾아 붙는다.
//   SettingsButton SettingsPanel CloseButton ResetButton MainMenuButton
//   WindowModeCombo ResolutionCombo QualityCombo FrameLimitCombo VSyncCheck
//   ShadowCombo TextureCombo EffectsCombo AntiAliasingCombo PostProcessCombo GlobalIlluminationCombo
//   MasterVolumeSlider MasterVolumeText

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BGSettingsWidget.generated.h"

class UButton;
class UCheckBox;
class UComboBoxString;
class USlider;
class UTextBlock;
class UWidget;

UCLASS()
class BG_API UBGSettingsWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "BG|Settings")
	void OpenSettings();

	UFUNCTION(BlueprintCallable, Category = "BG|Settings")
	void CloseSettings();

	/**
	 * HUD 가 없는 곳(메인메뉴)에서 띄울 때 부른다. 오른쪽 위 토글 버튼과
	 * 메인메뉴 복귀 버튼은 그 자리에서 의미가 없으므로 감춘다.
	 */
	UFUNCTION(BlueprintCallable, Category = "BG|Settings")
	void SetStandaloneMode(bool bStandalone);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	// ESC 로 열고 닫는다. ESC 는 이미 대상 선택 취소에 물려 있어 두 핸들러가 같이 불린다.
	void TryBindToggleInput();
	void UnbindToggleInput();
	void HandleTogglePressed();

	// 콤보 목록을 채우고 현재 설정값으로 선택을 맞춘다.
	void PopulateOptions();
	void RefreshFromSettings();

	// 지금 게임 창이 올라가 있는 모니터의 네이티브 해상도. 못 찾으면 (0,0).
	static FIntPoint GetCurrentMonitorResolution();

	// 목록에 없는 해상도(창을 끌어서 만든 크기 등)를 가장 가까운 칸으로 돌려준다.
	int32 FindClosestResolutionIndex(const FIntPoint& Target) const;

	// 그래픽 설정은 즉시 적용하고 그대로 저장한다.
	void ApplyAndSaveGraphics();

	// 음량은 GameUserSettings 에 자리가 없어 GameUserSettings.ini 에 직접 넣는다.
	void LoadAudio();
	void SaveAudio() const;
	void ApplyAudioToDevice() const;
	void RefreshVolumeText() const;

	UFUNCTION()
	void HandleSettingsClicked();
	UFUNCTION()
	void HandleCloseClicked();
	UFUNCTION()
	void HandleResetClicked();
	UFUNCTION()
	void HandleMainMenuClicked();

	UFUNCTION()
	void HandleWindowModeChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
	UFUNCTION()
	void HandleResolutionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
	UFUNCTION()
	void HandleQualityChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

	// 세부 품질 6개. UFUNCTION 델리게이트는 인자를 더 받을 수 없어 행마다 하나씩 둔다.
	UFUNCTION()
	void HandleShadowChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
	UFUNCTION()
	void HandleTextureChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
	UFUNCTION()
	void HandleEffectsChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
	UFUNCTION()
	void HandleAntiAliasingChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
	UFUNCTION()
	void HandlePostProcessChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
	UFUNCTION()
	void HandleGlobalIlluminationChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

	// DetailCombos[RowIndex] 의 선택을 같은 자리의 세터로 넘긴다.
	void ApplyDetailQuality(int32 RowIndex);
	UFUNCTION()
	void HandleFrameLimitChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
	UFUNCTION()
	void HandleVSyncChanged(bool bIsChecked);

	UFUNCTION()
	void HandleMasterVolumeChanged(float Value);

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> SettingsButton;

	// 패널 루트. 어떤 위젯이든 되도록 UWidget 으로 받는다.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> SettingsPanel;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> CloseButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> ResetButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> MainMenuButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UComboBoxString> WindowModeCombo;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UComboBoxString> ResolutionCombo;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UComboBoxString> QualityCombo;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UComboBoxString> FrameLimitCombo;

	// ─ 세부 품질 ─ 아래 6개의 선언 순서가 .cpp 의 게터/세터 표 순서와 같아야 한다.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UComboBoxString> ShadowCombo;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UComboBoxString> TextureCombo;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UComboBoxString> EffectsCombo;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UComboBoxString> AntiAliasingCombo;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UComboBoxString> PostProcessCombo;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UComboBoxString> GlobalIlluminationCombo;

	// 위 6개를 순서대로 담는다. 채우기·갱신·적용을 하나의 반복문으로 돌리기 위한 것이다.
	UPROPERTY(Transient)
	TArray<TObjectPtr<UComboBoxString>> DetailCombos;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCheckBox> VSyncCheck;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<USlider> MasterVolumeSlider;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MasterVolumeText;

	// 콤보 목록에 넣은 순서 그대로의 해상도. 선택 인덱스로 바로 찾는다.
	TArray<FIntPoint> ResolutionOptions;

	float MasterVolume = 1.f;

	// 현재 값을 위젯에 되돌려 쓰는 중. 그 선택 통지로 다시 적용이 돌지 않게 막는다.
	bool bRefreshing = false;

	bool bToggleInputBound = false;
};
