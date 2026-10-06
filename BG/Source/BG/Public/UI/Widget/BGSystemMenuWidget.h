// BANG! 시스템 메뉴 — 설정(그래픽 / 오디오)과 도움말(직업 / 역할 / 장비 설명).
// HUD 오른쪽 위 아이콘 두 개와 각각의 패널이 한 위젯 안에 같이 있다. 아이콘이 자기 패널을 토글하고,
// 한쪽을 열면 다른 쪽은 닫힌다. ESC 는 도움말이 열려 있으면 도움말을 닫고, 아니면 설정을 토글한다.
// WBP_BGSystemMenu 에 아래 이름으로 위젯을 올리면 C++ 이 이름으로 찾아 붙는다.
//
// ── 설정 ──
//   SettingsButton SettingsPanel Size_Popup CloseButton ResetButton MainMenuButton
//   WindowModeCombo ResolutionCombo QualityCombo FrameLimitCombo VSyncCheck
//   ShadowCombo TextureCombo EffectsCombo AntiAliasingCombo PostProcessCombo GlobalIlluminationCombo
//   MasterVolumeSlider MasterVolumeText
//   MusicVolumeSlider MusicVolumeText  SFXVolumeSlider SFXVolumeText
//   Image_Div1 Image_Div2 Image_Div3 Image_Div4
//
// ── 도움말 ──
//   본문은 코드에 박지 않고 열릴 때 DT 에서 읽어 한 행씩 만든다:
//     직업 → CharacterTable(DT_BGCharacterRow) 의 DisplayName / Description
//     역할 → InfoTable(DT_BGInfoTooltip) 의 Role.* Row 의 Title / Body
//     장비 → CardTable(DT_BGCard) 의 파란 카드(Card.Type.Blue) Row 의 DisplayName / Description
//   세 분류는 탭으로 나눠 한 번에 하나만 보인다. 분류마다 행 수가 크게 다르고(직업 12, 역할 4,
//   장비는 계속 는다) 한 화면에 나란히 두면 열 높이가 들쭉날쭉해진다. 탭 안에서는 행을
//   Columns 열 격자로 왼쪽→오른쪽, 위→아래 순서로 채운다 — 항목이 늘면 줄만 늘어난다.
//
//   HelpButton HelpPanel Help_Size_Popup HelpCloseButton
//   Tab_Jobs Tab_Roles Tab_Equips                 — 탭 버튼(Button)
//   Tab_Jobs_Text Tab_Roles_Text Tab_Equips_Text  — 탭 글자(TextBlock, 선택 색을 여기서 바꾼다)
//   Grid_Jobs Grid_Roles Grid_Equips — UniformGridPanel. 행이 여기에 쌓인다
//     (디자이너에서는 비어 있는 게 정상이다). 셋은 한 Overlay 에 겹쳐 둔다 — 고르지 않은
//     탭은 접지 않고 숨겨서 자리를 남긴다. 그래서 패널 크기가 항상 가장 큰 탭에 맞고,
//     탭을 바꿔도 창 크기가 흔들리지 않는다(WidgetSwitcher 는 보이는 페이지 크기만 따른다).
//   Scroll_Body       — 탭을 바꾸면 맨 위로 올린다
//   Help_Div1 Help_Div2

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "Templates/SubclassOf.h"
#include "UI/Widget/BGPopupOpenAnim.h"
#include "BGSystemMenuWidget.generated.h"

class UBGHelpRowWidget;
class UButton;
class UCheckBox;
class UComboBoxString;
class UDataTable;
class UImage;
class UScrollBox;
class USlider;
class UTextBlock;
class UTexture2D;
class UUniformGridPanel;
class UWidget;

UCLASS()
class BG_API UBGSystemMenuWidget : public UUserWidget
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

	// 지금 게임 창이 올라가 있는 모니터의 네이티브 해상도. 못 찾으면 (0,0).
	static FIntPoint GetCurrentMonitorResolution();

	UFUNCTION(BlueprintCallable, Category = "BG|Help")
	void OpenHelp();

	UFUNCTION(BlueprintCallable, Category = "BG|Help")
	void CloseHelp();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	// ESC 로 열고 닫는다. ESC 는 이미 대상 선택 취소에 물려 있어 두 핸들러가 같이 불린다.
	void TryBindToggleInput();
	void UnbindToggleInput();
	void HandleTogglePressed();

	// 콤보 목록을 채우고 현재 설정값으로 선택을 맞춘다.
	void PopulateOptions();
	void RefreshFromSettings();

	/**
	 * 저장된 창 모드·해상도를 지금 게임 창의 실제 상태로 맞춘다(저장은 하지 않는다).
	 * -windowed 같은 실행 인자는 창만 바꾸고 GameUserSettings 에는 옛 값을 남긴다. 그대로 두면
	 * 콤보가 실제와 다른 모드를 보여주고, 다른 항목을 바꿀 때 ApplySettings 가 옛 모드로 창을 튕긴다.
	 * 적용 직후에는 창이 다음 틱에야 바뀌므로 패널을 열 때만 부른다.
	 */
	void SyncSettingsToActualWindow();

	/**
	 * 구분선 두께를 DPI 스케일로 역보정한다.
	 * 얇은 선은 (레이아웃 높이 x 스케일)이 정수가 아니면 놓인 y 소수부에 따라 1px·2px 로
	 * 갈린다. 두께를 스케일로 나눠 두면 화면에서는 항상 같은 정수 픽셀로 떨어진다.
	 */
	void RefreshDividerThickness();

	// 목록에 없는 해상도(창을 끌어서 만든 크기 등)를 가장 가까운 칸으로 돌려준다.
	int32 FindClosestResolutionIndex(const FIntPoint& Target) const;

	// 패널을 열어 둔 채 창을 다른 모니터로 옮긴 경우 — 펼치기 직전에 목록을 다시 만든다.
	UFUNCTION()
	void HandleResolutionComboOpening();

	// 그래픽 설정은 즉시 적용하고 그대로 저장한다.
	void ApplyAndSaveGraphics();

	// 음량 저장과 Sound Mix 적용은 GameInstance 수명의 BGAudioSubsystem이 담당한다.
	void RefreshVolumesFromAudioSubsystem();
	void RefreshVolumeTexts() const;

	/** ComboBox/Slider의 현재 Hover 대상을 비교해 시작·종료음을 한 번씩만 재생한다. */
	void RefreshInteractiveAudioHover();

	UFUNCTION()
	void HandleComboOpeningAudio();
	UFUNCTION()
	void HandleComboSelectionAudio(FString SelectedItem, ESelectInfo::Type SelectionType);
	UFUNCTION()
	void HandleSliderCaptureBeginAudio();
	UFUNCTION()
	void HandleSliderCaptureEndAudio();

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

	UFUNCTION()
	void HandleMusicVolumeChanged(float Value);

	UFUNCTION()
	void HandleSFXVolumeChanged(float Value);

	// ─ 도움말 ─

	// DT 를 읽어 Grid_Jobs / Grid_Roles / Grid_Equips 를 채운다. 다시 불려도 쌓이지 않게 먼저 비운다.
	void BuildHelpRows();

	// 격자의 Index 번째 칸(왼쪽→오른쪽, 위→아래)에 한 행을 넣는다.
	void AddHelpRow(UUniformGridPanel& Grid, int32 Index, const FText& Title, const FText& Body, UTexture2D* Icon);

	/**
	 * 항목이 Columns 보다 적으면 남은 칸을 빈 칸으로 채운다. 균등 격자는 쓰인 열 수로 폭을
	 * 나누므로, 그대로 두면 장비 2개가 화면 폭을 반씩 나눠 가져 다른 탭보다 칸이 넓어진다.
	 */
	void PadFirstHelpRow(UUniformGridPanel& Grid, int32 Count);

	// 탭 Index(0 직업, 1 역할, 2 장비)를 보이고 탭 버튼 표시를 맞춘다.
	void SelectHelpTab(int32 Index);

	bool IsSettingsOpen() const;
	bool IsHelpOpen() const;

	UFUNCTION()
	void HandleHelpClicked();

	UFUNCTION()
	void HandleHelpCloseClicked();

	UFUNCTION()
	void HandleJobsTabClicked();

	UFUNCTION()
	void HandleRolesTabClicked();

	UFUNCTION()
	void HandleEquipsTabClicked();

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> SettingsButton;

	// 패널 루트. 어떤 위젯이든 되도록 UWidget 으로 받는다.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> SettingsPanel;

	// 블러 배경을 뺀 가운데 상자. 열 때 이것만 펼쳐진다(FBGPopupOpenAnim).
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> Size_Popup;

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

	// Hover/Open/Select 사운드가 필요한 모든 설정 ComboBox.
	UPROPERTY(Transient)
	TArray<TObjectPtr<UComboBoxString>> AudioCombos;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCheckBox> VSyncCheck;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<USlider> MasterVolumeSlider;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MasterVolumeText;

	// ─ BGM · 효과음 ─ 마스터에 곱해지는 갈래별 음량. 실제 분리는 SoundClass 가 한다.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<USlider> MusicVolumeSlider;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MusicVolumeText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<USlider> SFXVolumeSlider;

	// Hover/Capture 사운드가 필요한 모든 설정 Slider.
	UPROPERTY(Transient)
	TArray<TObjectPtr<USlider>> AudioSliders;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SFXVolumeText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_Div1;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_Div2;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_Div3;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_Div4;

	// 위 넷과 도움말 구분선 둘을 한 번에 돌리기 위한 목록.
	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> Dividers;

	// 마지막으로 두께를 맞춘 DPI 스케일. 달라졌을 때만 다시 계산한다.
	float LastViewportScale = -1.f;

	// 콤보 목록에 넣은 순서 그대로의 해상도. 선택 인덱스로 바로 찾는다.
	TArray<FIntPoint> ResolutionOptions;

	// 해상도 목록을 만들 때 기준으로 삼은 모니터 해상도. 바뀌었을 때만 다시 만든다.
	FIntPoint PopulatedMonitorResolution = FIntPoint::ZeroValue;

	// 설정·도움말은 한 번에 하나만 열리므로 연출도 하나로 돌린다.
	FBGPopupOpenAnim PopupAnim;

	float MasterVolume = 1.f;
	float MusicVolume  = 1.f;
	float SFXVolume    = 1.f;

	// 현재 값을 위젯에 되돌려 쓰는 중. 그 선택 통지로 다시 적용이 돌지 않게 막는다.
	bool bRefreshing = false;

	// 직전 프레임 Hover 대상. ComboBox와 Slider 사이 이동도 End→Begin 한 번씩만 발생시킨다.
	TWeakObjectPtr<UWidget> HoveredAudioWidget;

	bool bToggleInputBound = false;

	// ─ 도움말 ─

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> HelpButton;

	// 패널 루트. 어떤 위젯이든 되도록 UWidget 으로 받는다.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> HelpPanel;

	// 블러 배경을 뺀 가운데 상자. 열 때 이것만 펼쳐진다(FBGPopupOpenAnim).
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> Help_Size_Popup;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> HelpCloseButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Tab_Jobs;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Tab_Roles;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Tab_Equips;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Tab_Jobs_Text;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Tab_Roles_Text;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Tab_Equips_Text;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UScrollBox> Scroll_Body;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UUniformGridPanel> Grid_Jobs;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UUniformGridPanel> Grid_Roles;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UUniformGridPanel> Grid_Equips;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Help_Div1;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Help_Div2;

	/** 탭 안 격자의 열 수. 모든 탭이 같은 값을 써야 칸 폭이 탭마다 같다. */
	UPROPERTY(EditAnywhere, Category = "BG|Help", meta = (ClampMin = "1"))
	int32 Columns = 3;

	/** 선택된 탭 글자 색. 나머지 탭은 InactiveTabColor 로 가라앉힌다. */
	UPROPERTY(EditAnywhere, Category = "BG|Help|Tab")
	FSlateColor ActiveTabColor = FSlateColor(FLinearColor(0.92f, 0.82f, 0.58f));

	UPROPERTY(EditAnywhere, Category = "BG|Help|Tab")
	FSlateColor InactiveTabColor = FSlateColor(FLinearColor(0.55f, 0.52f, 0.46f));

	/** 선택되지 않은 탭 버튼 배경에 곱하는 색. 선택된 탭은 WBP 의 스타일 그대로 보인다. */
	UPROPERTY(EditAnywhere, Category = "BG|Help|Tab")
	FLinearColor InactiveTabTint = FLinearColor(0.6f, 0.6f, 0.6f, 1.f);

	/** 한 행을 그릴 위젯. 비워 두면 도움말이 빈 껍데기로 뜬다. */
	UPROPERTY(EditAnywhere, Category = "BG|Help")
	TSubclassOf<UBGHelpRowWidget> RowClass;

	/** 직업 설명 표. 행 순서는 이 DT 에 올라간 순서 그대로 나간다. */
	UPROPERTY(EditAnywhere, Category = "BG|Help", meta = (RequiredAssetDataTags = "RowStructure=/Script/BG.BGCharacterRow"))
	TObjectPtr<UDataTable> CharacterTable;

	/** 역할 설명 표. Row 이름 = 태그 문자열(Role.Sheriff)이다 — 턴 패널 툴팁과 같은 표다. */
	UPROPERTY(EditAnywhere, Category = "BG|Help", meta = (RequiredAssetDataTags = "RowStructure=/Script/BG.BGInfoTooltipRow"))
	TObjectPtr<UDataTable> InfoTable;

	/** 장비 설명 표. 파란 카드(Card.Type.Blue) 행만 이 DT 에 올라간 순서대로 나간다. */
	UPROPERTY(EditAnywhere, Category = "BG|Help", meta = (RequiredAssetDataTags = "RowStructure=/Script/BG.BGCardRow"))
	TObjectPtr<UDataTable> CardTable;

	/** 직업 태그(Character.Job.*) → 아이콘. WBP 에서 채운다. 없는 직업은 아이콘만 접힌다. */
	UPROPERTY(EditAnywhere, Category = "BG|Help|Icon")
	TMap<FGameplayTag, TObjectPtr<UTexture2D>> JobIcons;

	/** 역할 태그(Role.*) → 아이콘. JobIcons 와 같은 규칙이다. */
	UPROPERTY(EditAnywhere, Category = "BG|Help|Icon")
	TMap<FGameplayTag, TObjectPtr<UTexture2D>> RoleIcons;

	/**
	 * 장비 태그(Card.Id.*) → 아이콘. JobIcons 와 같은 규칙이다.
	 * 카드 그림(ArtTexture)은 쓰지 않는다 — 32px 정사각 칸에 넣으면 비율이 찌그러진다.
	 */
	UPROPERTY(EditAnywhere, Category = "BG|Help|Icon")
	TMap<FGameplayTag, TObjectPtr<UTexture2D>> EquipIcons;
};
