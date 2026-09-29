// BANG! 도움말 UI — 직업 / 역할 / 장비 설명.
//
// 설정 창(UBGSettingsWidget)과 같은 구조다. HUD 오른쪽 위 도움말 버튼과 도움말 패널이
// 한 위젯 안에 같이 있고, 버튼이 패널을 토글한다. 버튼은 설정 버튼 바로 왼쪽에 놓는다.
// ESC 는 물지 않는다 — 그 키는 설정 창이 이미 쓰고 있어, 여기서 또 물면 도움말을 닫는
// ESC 가 설정 창을 여는 ESC 가 된다. 도움말은 버튼과 "닫기" 로만 닫는다.
//
// 본문은 코드에 박지 않고 열릴 때 DT 에서 읽어 한 행씩 만든다:
//   직업 → CharacterTable(DT_BGCharacterRow) 의 DisplayName / Description
//   역할 → InfoTable(DT_BGInfoTooltip) 의 Role.* Row 의 Title / Body
//   장비 → CardTable(DT_BGCard) 의 파란 카드(Card.Type.Blue) Row 의 DisplayName / Description
// 그래서 DT 문구를 고치면 도움말도 따라온다.
//
// 세 분류는 탭으로 나눠 한 번에 하나만 보인다. 분류마다 행 수가 크게 다르고(직업 12, 역할 4,
// 장비는 계속 는다) 한 화면에 나란히 두면 열 높이가 들쭉날쭉해진다. 탭 안에서는 행을
// Columns 열 격자로 왼쪽→오른쪽, 위→아래 순서로 채운다 — 항목이 늘면 줄만 늘어난다.
//
// WBP_BGHelp 에 아래 이름으로 위젯을 올리면 C++ 이 이름으로 찾아 붙는다.
//   HelpButton HelpPanel CloseButton
//   Tab_Jobs Tab_Roles Tab_Equips                 — 탭 버튼(Button)
//   Tab_Jobs_Text Tab_Roles_Text Tab_Equips_Text  — 탭 글자(TextBlock, 선택 색을 여기서 바꾼다)
//   Grid_Jobs Grid_Roles Grid_Equips — UniformGridPanel. 행이 여기에 쌓인다
//     (디자이너에서는 비어 있는 게 정상이다). 셋은 한 Overlay 에 겹쳐 둔다 — 고르지 않은
//     탭은 접지 않고 숨겨서 자리를 남긴다. 그래서 패널 크기가 항상 가장 큰 탭에 맞고,
//     탭을 바꿔도 창 크기가 흔들리지 않는다(WidgetSwitcher 는 보이는 페이지 크기만 따른다).
//   Scroll_Body       — 탭을 바꾸면 맨 위로 올린다
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
class UScrollBox;
class UTextBlock;
class UTexture2D;
class UUniformGridPanel;
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
	// DT 를 읽어 Grid_Jobs / Grid_Roles / Grid_Equips 를 채운다. 다시 불려도 쌓이지 않게 먼저 비운다.
	void BuildRows();

	// 격자의 Index 번째 칸(왼쪽→오른쪽, 위→아래)에 한 행을 넣는다.
	void AddRow(UUniformGridPanel& Grid, int32 Index, const FText& Title, const FText& Body, UTexture2D* Icon);

	/**
	 * 항목이 Columns 보다 적으면 남은 칸을 빈 칸으로 채운다. 균등 격자는 쓰인 열 수로 폭을
	 * 나누므로, 그대로 두면 장비 2개가 화면 폭을 반씩 나눠 가져 다른 탭보다 칸이 넓어진다.
	 */
	void PadFirstRow(UUniformGridPanel& Grid, int32 Count);

	// 탭 Index(0 직업, 1 역할, 2 장비)를 보이고 탭 버튼 표시를 맞춘다.
	void SelectTab(int32 Index);

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
	UFUNCTION()
	void HandleJobsTabClicked();
	UFUNCTION()
	void HandleRolesTabClicked();
	UFUNCTION()
	void HandleEquipsTabClicked();

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> HelpButton;

	// 패널 루트. 어떤 위젯이든 되도록 UWidget 으로 받는다.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> HelpPanel;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> CloseButton;

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
	TObjectPtr<UImage> Image_Div1;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_Div2;

	// 위 둘을 한 번에 돌리기 위한 목록.
	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> Dividers;

	// 마지막으로 두께를 맞춘 DPI 스케일. 달라졌을 때만 다시 계산한다.
	float LastViewportScale = -1.f;

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
