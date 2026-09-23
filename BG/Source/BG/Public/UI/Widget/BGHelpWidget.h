// BANG! 도움말 UI — 직업 / 역할 설명.
//
// 설정 창(UBGSettingsWidget)과 같은 구조다. HUD 오른쪽 위 도움말 버튼과 도움말 패널이
// 한 위젯 안에 같이 있고, 버튼이 패널을 토글한다. 버튼은 설정 버튼 바로 왼쪽에 놓는다.
// ESC 는 물지 않는다 — 그 키는 설정 창이 이미 쓰고 있어, 여기서 또 물면 도움말을 닫는
// ESC 가 설정 창을 여는 ESC 가 된다. 도움말은 버튼과 "닫기" 로만 닫는다.
//
// 본문은 코드에 박지 않고 열릴 때 DT 에서 읽어 한 행씩 만든다:
//   직업 → CharacterTable(DT_BGCharacterRow) 의 DisplayName / Description
//   역할 → InfoTable(DT_BGInfoTooltip) 의 Role.* Row 의 Title / Body
// 그래서 DT 문구를 고치면 도움말도 따라온다.
//
// WBP_BGHelp 에 아래 이름으로 위젯을 올리면 C++ 이 이름으로 찾아 붙는다.
//   HelpButton HelpPanel CloseButton
//   VBox_Jobs VBox_Jobs2 VBox_Roles  — 행이 여기에 쌓인다(디자이너에서는 비어 있는 게 정상이다)
//   Image_Div1 Image_Div2

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "Templates/SubclassOf.h"
#include "BGHelpWidget.generated.h"

class UBGHelpRowWidget;
class UButton;
class UDataTable;
class UImage;
class UTexture2D;
class UVerticalBox;
class UWidget;

UCLASS()
class BG_API UBGHelpWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "BG|Help")
	void OpenHelp();

	UFUNCTION(BlueprintCallable, Category = "BG|Help")
	void CloseHelp();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	// DT 를 읽어 VBox_Jobs / VBox_Roles 를 채운다. 다시 불려도 쌓이지 않게 먼저 비운다.
	void BuildRows();
	void AddRow(UVerticalBox& Box, const FText& Title, const FText& Body, UTexture2D* Icon);

	/**
	 * 구분선 두께를 DPI 스케일로 역보정한다 — 설정 창과 같은 이유다.
	 * 얇은 선은 (레이아웃 높이 x 스케일)이 정수가 아니면 놓인 y 소수부에 따라 1px·2px 로
	 * 갈린다. 두께를 스케일로 나눠 두면 화면에서는 항상 같은 정수 픽셀로 떨어진다.
	 */
	void RefreshDividerThickness();

	UFUNCTION()
	void HandleHelpClicked();
	UFUNCTION()
	void HandleCloseClicked();

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> HelpButton;

	// 패널 루트. 어떤 위젯이든 되도록 UWidget 으로 받는다.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> HelpPanel;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> CloseButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UVerticalBox> VBox_Jobs;

	/**
	 * 직업 둘째 열. 직업이 12종이라 한 열로 세우면 너무 길다 — 있으면 반씩 나눠 담는다.
	 * 없으면 VBox_Jobs 한 열에 전부 들어간다(한 열짜리 WBP 도 그대로 동작한다).
	 */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UVerticalBox> VBox_Jobs2;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UVerticalBox> VBox_Roles;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_Div1;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_Div2;

	// 위 둘을 한 번에 돌리기 위한 목록.
	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> Dividers;

	// 마지막으로 두께를 맞춘 DPI 스케일. 달라졌을 때만 다시 계산한다.
	float LastViewportScale = -1.f;

	/** 한 행을 그릴 위젯. 비워 두면 도움말이 빈 껍데기로 뜬다. */
	UPROPERTY(EditAnywhere, Category = "BG|Help")
	TSubclassOf<UBGHelpRowWidget> RowClass;

	/** 직업 설명 표. 행 순서는 이 DT 에 올라간 순서 그대로 나간다. */
	UPROPERTY(EditAnywhere, Category = "BG|Help", meta = (RequiredAssetDataTags = "RowStructure=/Script/BG.BGCharacterRow"))
	TObjectPtr<UDataTable> CharacterTable;

	/** 역할 설명 표. Row 이름 = 태그 문자열(Role.Sheriff)이다 — 턴 패널 툴팁과 같은 표다. */
	UPROPERTY(EditAnywhere, Category = "BG|Help", meta = (RequiredAssetDataTags = "RowStructure=/Script/BG.BGInfoTooltipRow"))
	TObjectPtr<UDataTable> InfoTable;

	/** 직업 태그(Character.Job.*) → 아이콘. WBP 에서 채운다. 없는 직업은 아이콘만 접힌다. */
	UPROPERTY(EditAnywhere, Category = "BG|Help|Icon")
	TMap<FGameplayTag, TObjectPtr<UTexture2D>> JobIcons;

	/** 역할 태그(Role.*) → 아이콘. JobIcons 와 같은 규칙이다. */
	UPROPERTY(EditAnywhere, Category = "BG|Help|Icon")
	TMap<FGameplayTag, TObjectPtr<UTexture2D>> RoleIcons;
};
