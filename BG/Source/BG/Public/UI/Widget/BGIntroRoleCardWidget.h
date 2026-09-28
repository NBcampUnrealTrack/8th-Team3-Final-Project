// BANG! 도입 연출 ② — 내 역할 확인. 봉인된 편지가 뒤집히며 직업과 비밀 역할을 보여준다.
//
// **이 화면 주인에게만 보이는 위젯이다.** 값은 자기 PlayerController 에서 읽으므로
// 남의 역할이 섞여 들어올 여지가 없다.
//
// 띄우고 내리는 것은 ABGPlayerController 가 도입 단계를 보고 한다 — 위젯은 시키는 대로만 한다.
//
// WBP 에 둘 것 (모두 선택):
//   Text_Job           (TextBlock) — "사제"
//   Text_Role          (TextBlock) — "무법자"
//   Text_WinCondition  (TextBlock) — 내 승리 조건 한 문장
//   Anim_In            (애니메이션) — 카드가 뒤집히며 등장
//   Anim_Out           (애니메이션) — 내 정보창으로 작아지며 퇴장
//
// ⚠️ "이 정보는 나에게만 보입니다" 같은 고정 문구는 WBP 에 직접 적을 것 — 코드가 채우는 값이 아니다.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/View/BGSeatView.h"
#include "BGIntroRoleCardWidget.generated.h"

class UBorder;
class UTexture2D;
class UDataTable;
class UTextBlock;
class UWidgetAnimation;

UCLASS(Abstract, Blueprintable)
class BG_API UBGIntroRoleCardWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * 값만 먼저 읽어 카드 그림을 **미리 받아 둔다.** 화면에는 아직 띄우지 않는다.
	 *
	 * ⚠️ 이 준비가 없으면 그림이 카드가 뜨는 순간에야 도착해 한 박자 늦게 채워진다.
	 *    도입 연출은 한 단계 앞(야영지 진입)에서 이것을 불러 둔다.
	 */
	UFUNCTION(BlueprintCallable, Category = "BG|Intro")
	void PrepareCard();

	/** 카드를 띄운다. 값이 아직 안 왔으면 도착하는 대로 채운다. */
	UFUNCTION(BlueprintCallable, Category = "BG|Intro")
	void ShowCard();

	/** 카드를 내린다. Anim_Out 이 있으면 그것이 끝난 뒤 화면에서 뗀다. */
	UFUNCTION(BlueprintCallable, Category = "BG|Intro")
	void HideCard();

	UFUNCTION(BlueprintPure, Category = "BG|Intro")
	const FBGSeatView& GetSeatView() const { return View; }

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeDestruct() override;

	/** 값이 채워졌다. 직업별 초상화처럼 텍스트 말고 더 할 것이 있으면 여기서 한다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "BG|Intro")
	void OnRoleInfoChanged(const FBGSeatView& InView);

	//	⚠️ 선택이 아니라 필수다. 카드 그림이 이 위젯의 본체라, 이름이 어긋나면 글자만 뜨고
	//	   그림만 조용히 빠진다 — WBP 컴파일 에러로 바로 드러나게 한다.
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UBorder> Border_Root;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Job;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Role;

	/** 내 승리 조건. InfoTable 의 역할 Row(Role.Outlaw 등) 본문을 그대로 쓴다. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_WinCondition;

	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> Anim_In;

	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> Anim_Out;

	/**
	 * 역할 설명 표. 턴 패널의 InfoTable 과 같은 것을 물리면 된다 —
	 * 역할별 Row 이름은 태그 문자열 그대로다(Role.Sheriff). 비워 두면 승리 조건 줄이 접힌다.
	 */
	UPROPERTY(EditAnywhere, Category = "BG|Intro",
		meta = (RequiredAssetDataTags = "RowStructure=/Script/BG.BGInfoTooltipRow"))
	TObjectPtr<UDataTable> InfoTable;

	/**
	 * 값이 아직 안 왔을 때 다시 보는 주기(초).
	 *
	 * ⚠️ 폴링이 필요한 이유: 직업(폰)과 역할(PlayerState)과 도입 단계(GameState)는 액터가
	 *    달라 도착 순서가 보장되지 않는다. 단계가 먼저 와서 값이 빈 채로 뜨는 경우가 정상적으로 있다.
	 */
	UPROPERTY(EditAnywhere, Category = "BG|Intro", meta = (ClampMin = "0.05", Units = "s"))
	float RetryInterval = 0.2f;

	// ── Anim_Out 이 없을 때의 퇴장 연출 ──
	//
	//  기획의 "역할 카드가 내 정보창으로 작아지며 들어감" 을 코드로 낸다.
	//  ⚠️ WBP 에 Anim_Out 을 만들어 두면 그쪽이 우선이다 — 여기 값들은 쓰이지 않는다.

	/** 퇴장에 걸리는 시간(초). */
	UPROPERTY(EditAnywhere, Category = "BG|Intro|Fallback", meta = (ClampMin = "0.05", Units = "s"))
	float FallbackOutSeconds = 0.5f;

	/** 줄어드는 최종 배율. */
	UPROPERTY(EditAnywhere, Category = "BG|Intro|Fallback", meta = (ClampMin = "0.01"))
	float FallbackOutScale = 0.15f;

	/**
	 * 빨려 들어갈 지점까지의 화면 이동량(픽셀). 기본값은 좌하단 — 내 정보창(턴 패널)이 있는 쪽이다.
	 * 정보창을 다른 자리에 뒀다면 이 값을 그쪽으로 바꿀 것.
	 */
	UPROPERTY(EditAnywhere, Category = "BG|Intro|Fallback")
	FVector2D FallbackOutOffset = FVector2D(-620.f, 380.f);

private:
	/** 지금 읽을 수 있는 값으로 카드를 채운다. 직업과 역할이 모두 채워졌으면 true. */
	bool RefreshFromGame();

	/** 카드 그림을 비동기로 받아 둔다. 이미 같은 그림을 요청했으면 아무것도 하지 않는다. */
	void RequestCardArt();

	/** 받아 둔 그림을 배경에 입힌다. 아직 안 왔으면 배경을 비워 둔다. */
	void ApplyCardArt();

	UPROPERTY()
	FBGSeatView View;

	//	요청해 둔 그림. 로드가 끝나면 여기에서 꺼내 쓴다.
	TSharedPtr<struct FStreamableHandle> CardArtHandle;

	//	마지막으로 요청한 그림. 같은 것을 다시 요청하지 않게 한다.
	FSoftObjectPath RequestedCardArt;

	//	마지막으로 화면에 반영한 값. 폴링이 도는 동안 같은 값을 반복해 칠하지 않게 한다.
	FGameplayTag AppliedJobTag;
	FGameplayTag AppliedRoleTag;

	//	아직 못 채운 값이 있는가. 채워지면 폴링을 멈춘다.
	bool bWaitingForValues = true;

	/** Anim_Out 이 없을 때 쓰는 퇴장 연출을 한 프레임 진행한다. 끝나면 화면에서 뗀다. */
	void TickFallbackOut(float DeltaTime);

	//	Anim_Out 이 끝나면 화면에서 뗀다.
	FTimerHandle RemoveTimer;

	float TimeSinceRetry = 0.f;

	//	코드 퇴장 연출이 도는 중인가와 그 경과 시간.
	bool bFallbackOut = false;
	float FallbackOutElapsed = 0.f;
};
