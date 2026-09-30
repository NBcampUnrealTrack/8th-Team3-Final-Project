// BANG! 턴 패널 — 현재 턴 상태를 보여주고 "턴 종료" 버튼을 제공한다.
// 이게 있으면 콘솔에 BG_EndTurn 을 치지 않고 플레이할 수 있다.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/View/BGSeatView.h"
#include "UI/View/BGTurnView.h"
#include "UI/Widget/BGHeartSlotWidget.h"
#include "BGTurnPanelWidget.generated.h"

class UBGInfoTooltipWidget;
class UBorder;
class UButton;
class UDataTable;
class UImage;
class UTextBlock;
class UTexture2D;

/**
 * WBP 서브클래스에 아래 이름의 위젯이 있어야 한다:
 *   Button_EndTurn (Button,    필수)
 *   Text_Status    (TextBlock, 선택 — "내 차례" 등 안내문)
 *   Text_HandLimit (TextBlock, 선택 — "5 / 4". 손패 장수 / 한도)
 *   Text_Deck      (TextBlock, 선택 — 남은 덱 장수)
 *   Text_CardUse   (TextBlock, 선택 — "카드 사용 1 / 2")
 *   Text_Rope      (TextBlock, 선택 — "밧줄 37초" 턴 제한 시간)
 *   Image_Rope     (Image, 선택 — 밧줄이 타는 그림. 지금 아트는 깃털이다)
 *
 * 좌하단 본인 정보도 이 패널이 맡는다. 머리 위 이름표는 남만 띄우기 때문이다:
 *   Text_Name      (TextBlock, 선택 — 내 이름)
 *   Text_Job       (TextBlock, 선택 — 캐릭터 이름)
 *   Text_Health    (TextBlock, 선택 — "체력 5 / 5")
 *   Heart_1..6     (WBP_BGHeartSlot, 선택 — 체력을 하트로. Text_Health 대신 쓰는 표시다)
 *   Text_Role      (TextBlock, 선택 — 내 역할. 본인 것이라 숨기지 않는다)
 *   Img_JobIcon    (Image,     선택 — 직업 아이콘. JobIcons 에서 태그로 고른다)
 *   Img_RoleIcon   (Image,     선택 — 역할 아이콘. RoleIcons 에서 태그로 고른다)
 *   Text_Equipment (TextBlock, 선택 — 장비 목록)
 *   손패 장수는 Text_HandLimit 이 이미 보여주므로 따로 두지 않는다.
 *
 * 커서를 올리면 설명이 뜨는 줄(선택). Visibility 가 Visible 이어야 커서를 받는다:
 *   Border_Job / Border_Equipment / Border_HandLimit / Border_Role
 *
 * ── 게임 로직과의 연결 지점 ──
 *   들어오는 방향: SetTurnView()      — PlayerController 가 주기적으로 밀어넣는다
 *   나가는 방향:   OnEndTurnRequested — PlayerController 가 구독해 서버로 보낸다
 */
UCLASS(Abstract, Blueprintable)
class BG_API UBGTurnPanelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "BG|Turn")
	void SetTurnView(const FBGTurnView& InTurnView);

	UFUNCTION(BlueprintPure, Category = "BG|Turn")
	const FBGTurnView& GetTurnView() const { return TurnView; }

	/** 턴 종료 버튼이 눌렸다. PlayerController 가 구독한다. */
	UPROPERTY(BlueprintAssignable, Category = "BG|Turn")
	FOnBGEndTurnRequested OnEndTurnRequested;

	/** 좌하단에 띄울 내 좌석 정보. BGSeatInfoDisplayComponent 가 밀어넣는다. */
	UFUNCTION(BlueprintCallable, Category = "BG|Turn")
	void SetLocalSeatInfo(const FBGSeatView& InView);

	UFUNCTION(BlueprintPure, Category = "BG|Turn")
	const FBGSeatView& GetLocalSeatView() const { return LocalSeatView; }

	/** 체력·장비처럼 판 중에만 뜻이 있는 값을 보여줄 때인가. */
	UFUNCTION(BlueprintPure, Category = "BG|Turn")
	bool ShouldShowLocalStats() const { return LocalSeatView.bMatchRunning && LocalSeatView.MaxHealth > 0; }

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativePreConstruct() override;

	UFUNCTION()
	void HandleEndTurnClicked();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Turn", meta = (ExposeOnSpawn = true))
	FBGTurnView TurnView;

	/**
	 * 내 차례가 아닐 때 패널을 통째로 숨길지.
	 * false 면 남의 차례에도 상태 표시는 남고 버튼만 비활성화된다(진행 상황을 보기엔 이쪽이 낫다).
	 */
	UPROPERTY(EditAnywhere, Category = "BG|Turn")
	bool bHideWhenNotMyTurn = false;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button_EndTurn;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Status;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_HandLimit;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Deck;

	/** "카드 사용 1 / 2" — GDD §10 이 요구하는 사용한 카드 수 / 한도 표시. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_CardUse;

	/** 밧줄(턴 제한 시간). 남은 초는 매 틱 줄어들므로 델리게이트가 아니라 여기서 직접 읽는다. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Rope;

	//	밧줄이 타는 그림. 아트는 깃털이지만 부르는 이름은 밧줄 그대로다 —
	//	게임 규칙 쪽이 전부 Rope 라서, 여기만 Feather 로 부르면 둘을 잇는 사람이 헷갈린다.
	//
	//	브러시 머티리얼의 Remaining(0~1) 스칼라를 매 틱 밀어 넣는다. 머티리얼이 그 값을
	//	기준으로 오른쪽 끝부터 왼쪽으로 태운다(M_Feather_Burn).
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_Rope;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Name;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Job;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Health;

	/**
	 * 최대 체력 여섯 칸(WBP_BGHeartSlot)을 미리 두고 남는 것은 접는다.
	 * 하나도 없으면 Text_Health 만으로 표시된다. 색·연출은 하트 칸 WBP 가 정한다.
	 */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBGHeartSlotWidget> Heart_1;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBGHeartSlotWidget> Heart_2;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBGHeartSlotWidget> Heart_3;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBGHeartSlotWidget> Heart_4;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBGHeartSlotWidget> Heart_5;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBGHeartSlotWidget> Heart_6;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Role;

	/** 직업 아이콘. 직업이 배정되기 전에는 접힌다. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Img_JobIcon;

	/** 역할 아이콘. Text_Role 과 같은 조건 — 역할을 알기 전에는 접힌다. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Img_RoleIcon;

	/**
	 * 직업 태그(Character.Job.*) → 아이콘. WBP 에서 채운다.
	 * 태그가 없거나 표에 없는 직업이면 WBP 가 깔아 둔 범용 아이콘으로 되돌아간다.
	 */
	UPROPERTY(EditAnywhere, Category = "BG|Turn|Icon")
	TMap<FGameplayTag, TObjectPtr<UTexture2D>> JobIcons;

	/** 역할 태그(Role.*) → 아이콘. JobIcons 와 같은 규칙이다. */
	UPROPERTY(EditAnywhere, Category = "BG|Turn|Icon")
	TMap<FGameplayTag, TObjectPtr<UTexture2D>> RoleIcons;

	//	WBP 가 깔아 둔 범용 아이콘. 태그를 못 찾았을 때 되돌릴 곳이라 처음 한 번 기억해 둔다.
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> JobIconDefault;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> RoleIconDefault;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Equipment;

	//	── 설명 툴팁 ── 위치는 슬레이트가 잡는다. 여기서는 내용만 만든다.

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> Border_Job;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> Border_Equipment;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> Border_HandLimit;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> Border_Role;

	/** 툴팁을 그릴 위젯. 비워 두면 툴팁이 아예 뜨지 않는다. */
	UPROPERTY(EditAnywhere, Category = "BG|Turn|Info")
	TSubclassOf<UBGInfoTooltipWidget> InfoTooltipClass;

	/**
	 * 툴팁 문구 표. 비워 두면 살아 있는 값만 뜨고 설명은 빠진다.
	 *   Info.Job / Info.Equipment / Info.HandLimit / Info.Role — 각 줄의 "값이 없을 때" 문구
	 *   Role.Sheriff / Role.Deputy / ...                       — 역할별 설명(Row 이름 = 태그 문자열)
	 *
	 * 제목 = 살아 있는 이름·값(직업명 / 역할명 / "손패 3 / 5"), 본문 = 그에 대한 설명.
	 * 본문은 살아 있는 값이 있으면 표의 문구를 대체한다:
	 *   직업 배정 전 → Info.Job.Body,       배정 후 → DT_BGCharacterRow 의 직업 설명
	 *   장비 없음    → Info.Equipment.Body, 있음    → DT_BGCard 설명을 붙인 장비 목록
	 * 손패는 값이 없는 상태가 없어 숫자가 제목으로 가고 본문은 표의 설명이 그대로 남는다.
	 */
	UPROPERTY(EditAnywhere, Category = "BG|Turn|Info", meta = (RequiredAssetDataTags = "RowStructure=/Script/BG.BGInfoTooltipRow"))
	TObjectPtr<UDataTable> InfoTable;

	//	슬레이트가 툴팁을 띄우기 직전에 부른다. 어느 줄인지 알려주지 않아 줄마다 하나씩 둔다.

	UFUNCTION()
	UWidget* MakeJobTooltip();

	UFUNCTION()
	UWidget* MakeEquipmentTooltip();

	UFUNCTION()
	UWidget* MakeHandLimitTooltip();

	UFUNCTION()
	UWidget* MakeRoleTooltip();

	UPROPERTY()
	FBGSeatView LocalSeatView;

	/** 값이 들어왔다. 초상화·하트 아이콘처럼 텍스트 말고 더 할 것이 있으면 여기서 한다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "BG|Turn")
	void OnLocalSeatInfoChanged(const FBGSeatView& InView);

	//	밧줄이 붙고 꺼지는 순간만 델리게이트로 받는다. 남은 초는 NativeTick 이 읽는다.
	UFUNCTION()
	void HandleRopeChanged(int32 Seat, float SecondsRemaining, float TotalSeconds);

	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeDestruct() override;

private:
	void RefreshVisual();

	UWidget* MakeInfoTooltip(const FText& InTitle, const FText& InBody);

	//	표가 없거나 Row 가 없으면 nullptr.
	const struct FBGInfoTooltipRow* FindInfoRow(FName RowName) const;

	//	장비 목록을 "이름 — 설명" 여러 줄로 만든다.
	FText MakeEquipmentLines() const;

	//	LocalSeatView 의 체력을 하트 줄에 반영한다.
	void RefreshHealthHearts();

	FBGHeartRowState HeartRow;

	// 직업·역할 아이콘을 LocalSeatView 의 태그에 맞춘다.
	void RefreshIcons();

	// 밧줄 컴포넌트를 찾아 구독한다. 아직 없으면 false — 다음 틱에 다시 시도한다.
	bool TryBindRope();

	void RefreshRopeText();

	/** 남은 비율을 밧줄 그림에 밀어 넣는다. 타는 중이 아니면 그림을 접는다. */
	void RefreshRopeFeather();

	/** Image_Rope 브러시의 머티리얼로 MID 를 한 번 만들어 둔다. 없으면 nullptr. */
	UMaterialInstanceDynamic* GetRopeMaterial();

	TWeakObjectPtr<class UBGRopeComponent> Rope;

	/** Image_Rope 브러시에 물린 동적 머티리얼. 매 틱 Remaining 만 바꾼다. */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> RopeMaterial;
};
