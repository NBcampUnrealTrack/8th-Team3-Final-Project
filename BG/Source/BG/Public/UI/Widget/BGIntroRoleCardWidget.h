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

	/** 직업이 무엇을 하는지 한 줄 설명(DT_BGCharacterRow 의 Description). */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_JobAbility;

	//	카드가 한 번에 다 보이지 않고 이 두 칸이 차례로 떠오른다(UpdateReveal).
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> Border_JobAbility;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> Border_SecretRole;

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

	/**
	 * 직업 능력 → 비밀 역할 순으로 칸을 띄운다. 한꺼번에 보여주면 읽을 순서가 없다.
	 * ⚠️ 카드 그림이 도착한 뒤부터 센다 — 빈 카드 위에서 글자만 먼저 떠오르면 어색하다.
	 */
	void UpdateReveal(float DeltaTime);

	//	카드가 화면에 떠 있는가 / 내려가는 중인가. 떠 있는 동안만 위 연출이 돈다.
	bool bCardShown = false;
	bool bHiding = false;
	float RevealElapsed = 0.f;

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
