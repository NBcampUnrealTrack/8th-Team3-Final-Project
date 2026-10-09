// BANG! 머리 위 이름표 — 캐릭터 위에 뜨는 좌석 정보.
//
// 평소에는 이름과 체력만 보이고, 커서를 올리면 같은 패널이 직업·역할·손패·장비 줄까지 펼쳐진다.
// C++ 는 값만 넘기고 어떻게 그릴지는 WBP 가 정한다. 위젯 이름을 코드가 모르므로
// 배치를 바꿔도 코드는 손댈 일이 없다.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/View/BGSeatView.h"
#include "BGSeatInfoWidget.generated.h"

class UImage;
class USizeBox;
class UTextBlock;
class UTexture2D;

/**
 * WBP 에서 OnSeatInfoChanged 를 구현해 자기 텍스트를 채운다.
 *
 * 이름은 항상 보여주고, 스탯은 ShouldShowStats() 가 참일 때만 보여줄 것.
 * 판이 시작되기 전에도 어트리뷰트는 이미 있어서 체력 값만으로는 구분되지 않는다.
 * 역할은 공개된 것만 들어온다 — 비어 있으면 "???" 로 숨겨져 있다는 표시를 남긴다.
 */
UCLASS(Blueprintable)
class BG_API UBGSeatInfoWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 좌석 정보를 밀어넣는다. 매 프레임 불러도 안전하다. */
	UFUNCTION(BlueprintCallable, Category = "BG|SeatInfo")
	void SetSeatInfo(const FBGSeatView& InView);

	/** 상세 줄(Panel_Detail)을 펼치거나 접는다. 커서가 올라간 좌석 하나만 펼친다. */
	UFUNCTION(BlueprintCallable, Category = "BG|SeatInfo")
	void SetDetailExpanded(bool bExpanded);

	UFUNCTION(BlueprintPure, Category = "BG|SeatInfo")
	bool IsDetailExpanded() const { return bDetailExpanded; }

	UFUNCTION(BlueprintPure, Category = "BG|SeatInfo")
	const FBGSeatView& GetSeatView() const { return View; }

	/** 체력 같은 판 중 정보를 보여줄 때인가. 판정 규칙을 BP 가 다시 짜지 않게 한다. */
	UFUNCTION(BlueprintPure, Category = "BG|SeatInfo")
	bool ShouldShowStats() const { return View.bMatchRunning && View.MaxHealth > 0; }

	// 그래프에서 구조체를 쪼개지 않도록 바로 쓸 수 있는 형태로 준다.

	/** 사망·이탈 표시까지 붙인 이름. */
	UFUNCTION(BlueprintPure, Category = "BG|SeatInfo")
	FText GetNameText() const;

	/** "4 / 5". 판이 시작되기 전에는 빈 텍스트. */
	UFUNCTION(BlueprintPure, Category = "BG|SeatInfo")
	FText GetHealthText() const;

	UFUNCTION(BlueprintPure, Category = "BG|SeatInfo")
	FText GetJobText() const { return View.JobName; }

	/** "6". 무슨 숫자인지는 옆 아이콘이 말해 준다. */
	UFUNCTION(BlueprintPure, Category = "BG|SeatInfo")
	FText GetHandText() const;

	/** 공개된 역할만 온다. 숨겨져 있으면 "???". */
	UFUNCTION(BlueprintPure, Category = "BG|SeatInfo")
	FText GetRoleText() const;

	/** 장비 이름을 한 줄로 이어 준다. 없으면 "장비 없음". */
	UFUNCTION(BlueprintPure, Category = "BG|SeatInfo")
	FText GetEquipmentLine() const;

	/** 지금 차례인 사람을 다른 색으로 칠할 때 쓴다. */
	UFUNCTION(BlueprintPure, Category = "BG|SeatInfo")
	bool IsCurrentTurn() const { return View.bIsCurrentTurn; }

protected:
	//	WBP 가 같은 이름으로 만들어 두면 자동으로 연결된다. 없어도 동작한다.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Name;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Health;

	/**
	 * 커서를 올렸을 때만 보이는 상세 줄 묶음. 접으면 자리까지 빠져 패널이 이름·체력 크기로 줄어든다.
	 * 없으면 아래 상세 줄들이 늘 보인다.
	 */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> Panel_Detail;

	/**
	 * Panel_Detail 을 감싼 상자. 높이를 0 에서 내용 높이까지 늘려 펼침을 보여준다(ClipToBounds 필수).
	 * 없으면 애니메이션 없이 바로 펼쳐진다.
	 */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<USizeBox> Size_Detail;

	/** 상세가 다 펼쳐지는(접히는) 데 걸리는 시간(초). 0 이면 바로 바뀐다. */
	UPROPERTY(EditAnywhere, Category = "BG|SeatInfo", meta = (ClampMin = "0.0", Units = "s"))
	float DetailExpandSeconds = 0.18f;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Job;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Hand;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Role;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Equipment;

	/** 직업 아이콘. 태그가 아래 표에 없으면 WBP 가 깔아 둔 범용 아이콘으로 둔다. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Img_JobIcon;

	/** 역할 아이콘. Img_JobIcon 과 같은 규칙이다. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Img_RoleIcon;

	/** 직업 태그(Character.Job.*) → 아이콘. WBP 에서 채운다. 턴 패널의 같은 표와 내용을 맞출 것. */
	UPROPERTY(EditAnywhere, Category = "BG|SeatInfo|Icon")
	TMap<FGameplayTag, TObjectPtr<UTexture2D>> JobIcons;

	/** 역할 태그(Role.*) → 아이콘. JobIcons 와 같은 규칙이다. */
	UPROPERTY(EditAnywhere, Category = "BG|SeatInfo|Icon")
	TMap<FGameplayTag, TObjectPtr<UTexture2D>> RoleIcons;

	/** 값이 들어왔다. 색·애니메이션처럼 텍스트 말고 더 할 것이 있으면 여기서 한다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "BG|SeatInfo")
	void OnSeatInfoChanged(const FBGSeatView& InView);

	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// WBP 가 배치를 주지 않았을 때만 최소 배치를 만든다. WBP 를 만들면 이 경로는 죽는다.
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	void BuildFallbackLayout();

	/** 태그에 맞는 아이콘을 깐다. 못 찾으면 WBP 의 범용 아이콘으로 되돌린다. */
	void RefreshIcons();

	/** DetailExpandAlpha 를 화면에 입힌다 — 상자 높이와 상세 줄 투명도. */
	void ApplyDetailExpand();

	//	WBP 가 깔아 둔 범용 아이콘. 태그를 못 찾았을 때 되돌릴 곳이라 처음 한 번 기억해 둔다.
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> JobIconDefault;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> RoleIconDefault;

	UPROPERTY()
	FBGSeatView View;

	bool bDetailExpanded = false;

	//	0 = 접힘, 1 = 다 펼침. bDetailExpanded 쪽으로 매 프레임 다가간다.
	float DetailExpandAlpha = 0.f;
};
