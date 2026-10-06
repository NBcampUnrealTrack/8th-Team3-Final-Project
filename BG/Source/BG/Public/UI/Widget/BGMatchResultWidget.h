// BANG! 엔딩 ③ — 전원 정체·결과 화면.
//
// 판이 끝나면 전원의 역할이 공개되므로 남의 정체도 정상적으로 읽힌다(CheckWinCondition).
// 기본 표는 네이티브 OnResultReady가 채우며, WBP에서 레이아웃과 스타일을 조절한다.
//
// 띄우고 내리는 것은 ABGPlayerController 가 종료 단계를 보고 한다.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/View/BGSeatView.h"
#include "BGMatchResultWidget.generated.h"

/** 결과 표의 한 줄. 좌석 정보에 "이겼는가" 만 얹은 것이다. */
USTRUCT(BlueprintType)
struct FBGMatchResultRow
{
	GENERATED_BODY()

	/** 이름·직업·정체·생사·본인 여부가 전부 여기 있다. */
	UPROPERTY(BlueprintReadOnly, Category = "BG|Match")
	FBGSeatView Seat;

	/** 이 좌석의 진영이 이겼는가. 죽었어도 진영이 이기면 승리다(뱅 원작 규칙). */
	UPROPERTY(BlueprintReadOnly, Category = "BG|Match")
	bool bWinner = false;
};

UCLASS(Abstract, Blueprintable)
class BG_API UBGMatchResultWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 결과를 채워 화면에 올린다. 값이 아직 안 왔으면 도착하는 대로 다시 채운다. */
	UFUNCTION(BlueprintCallable, Category = "BG|Match")
	void ShowResult();

	/** 좌석 순서대로 정렬된 결과 줄. WBP 가 이것으로 표를 그린다. */
	UFUNCTION(BlueprintPure, Category = "BG|Match")
	const TArray<FBGMatchResultRow>& GetRows() const { return Rows; }

	/** 이번 판의 승리 진영(Role.*). 제목 문구를 고를 때 쓴다. */
	UFUNCTION(BlueprintPure, Category = "BG|Match")
	FGameplayTag GetWinningRole() const;

	/** "로비로 돌아가기". 서버가 전원을 함께 로비로 보낸다. */
	UFUNCTION(BlueprintCallable, Category = "BG|Match")
	void RequestReturnToLobby();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** 줄이 채워졌다. 표를 만들거나 등장 애니메이션을 재생할 곳. */
	UFUNCTION(BlueprintNativeEvent, Category = "BG|Match")
	void OnResultReady(const TArray<FBGMatchResultRow>& InRows);
	virtual void OnResultReady_Implementation(const TArray<FBGMatchResultRow>& InRows);
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UWidget> Scale_Result;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UWidget> Background_Dim;

	//	⚠️ 승리 배너는 WBP_Victory/WBP_Defeat 로 옮겨 갔다. WBP 에 남은 배너가 결과 표 위에
	//	   뜨지 않도록 숨기기만 한다 — WBP 에서 Canvas_Victory 를 지우면 이 바인딩도 지울 것.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UWidget> Banner_Victory;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UTextBlock> Text_ResultTitle;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UTextBlock> Text_ResultReason;

	//	표 헤더 5칸(플레이어, 직업, 정체, 상태, 결과). PvE 에서는 라벨만 바꾼다. 없으면 건드리지 않는다.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UTextBlock> Header_0;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UTextBlock> Header_1;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UTextBlock> Header_2;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UTextBlock> Header_3;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UTextBlock> Header_4;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UVerticalBox> Rows_Container;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UButton> Button_Return;

	UPROPERTY(EditDefaultsOnly, Category = "BG|Match|Style")
	FSlateFontInfo ResultFont;

	//	결과 표 등장(페이드 + 슬라이드) 경과 시간. 음수면 재생 중이 아니다.
	float ResultElapsed = -1.f;


	/**
	 * 값이 아직 안 왔을 때 다시 보는 주기(초).
	 *
	 * ⚠️ 역할 공개(PlayerState)와 종료 단계(GameState)는 액터가 달라 도착 순서가 보장되지 않는다.
	 *    단계가 먼저 와서 정체 칸이 빈 채로 뜨는 경우가 정상적으로 있다.
	 */
	UPROPERTY(EditAnywhere, Category = "BG|Match", meta = (ClampMin = "0.05", Units = "s"))
	float RetryInterval = 0.2f;

private:
	/** PvE 런 기록으로 제목, 부제, 헤더, 표를 채운다. 같은 WBP 에 내용만 바꿔 담는다. */
	void ShowPvERunRecords(const class ABGGameState& GS, const TArray<FBGMatchResultRow>& InRows);

	/** 헤더 5칸을 열 폭에 맞춰 놓는다. 행을 만들 때 쓰는 폭과 같은 배열을 넘길 것. */
	void LayoutHeaders(const float* Widths, bool bCenter);

	/** 직업 태그의 표시 이름. DT 의 DisplayName 이 우선이다. */
	FText GetJobDisplayName(const FGameplayTag& Job) const;

	/** 지금 읽을 수 있는 값으로 줄을 다시 만든다. 전원의 정체까지 채워졌으면 true. */
	bool RefreshRows();

	/** 표에 그리는 값이 같은가. 같으면 줄을 다시 만들지 않는다. */
	static bool RowsEqual(const TArray<FBGMatchResultRow>& A, const TArray<FBGMatchResultRow>& B);

	UPROPERTY()
	TArray<FBGMatchResultRow> Rows;

	//	아직 못 채운 값이 있는가. 다 차면 폴링을 멈춘다.
	bool bWaitingForValues = true;

	float TimeSinceRetry = 0.f;
};
