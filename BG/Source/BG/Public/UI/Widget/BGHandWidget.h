// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/View/BGCardView.h"
#include "UI/View/BGCardFXTypes.h"   // FBGCardDealContext / FBGCardPlayContext
#include "UI/Widget/BGCardWidget.h"  // UBGCardWidget — 손패가 카드 위젯을 직접 만들고 들고 있다
#include "BGHandWidget.generated.h"

class UBGCardWidget;
class UBGPlayZoneWidget;
class UCanvasPanel;
class UPanelWidget;

/**
 * 카드가 손패에 나타났다. 카드 자신이 아니라 **바깥**에서 반응할 것들이 구독한다 —
 * 사운드 매니저, 카메라 흔들림, 덱 카운터 애니메이션 등.
 *
 * 카드 위젯의 OnDealt 훅과 역할이 다르다: 저쪽은 "그 카드의 연출", 이쪽은 "화면의 반응".
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBGCardDealtDelegate, int32, InstanceId, const FBGCardDealContext&, Context);

/** 사용한 카드의 효과가 확정됐다. 화면 전체 연출(흔들림·플래시)이 여기 붙는다. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBGCardPlayResolvedDelegate, int32, InstanceId, const FBGCardPlayContext&, Context);

/** 사용한 카드(내 카드·관전 카드 모두)가 퇴장을 시작했다. 판정 게이지의 시작 신호. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBGCardPlayExitStartedDelegate, int32, InstanceId);


/** 카드 한 장이 있어야 할 자리. ComputeCardSlot 이 계산한다. */
USTRUCT(BlueprintType)
struct FBGCardSlotLayout
{
	GENERATED_BODY()

	/** 손패 패널 로컬 좌표. 카드 슬롯의 정렬 기준은 (0.5, 1.0) — 아래 가운데다. */
	UPROPERTY(BlueprintReadOnly, Category = "BG|Hand")
	FVector2D Position = FVector2D::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "BG|Hand")
	float AngleDegrees = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "BG|Hand")
	int32 ZOrder = 0;
};

/**
 * 손패 위젯. FBGCardView 목록을 받아 카드 위젯을 채운다.
 *
 * WBP 서브클래스에 아래가 있어야 한다:
 *   Box_Cards (PanelWidget, 필수)
 * 그리고 디테일 패널에서 CardWidgetClass 에 카드 WBP 를 지정해야 한다.
 *
 * ── 게임 로직과의 연결 지점 ──
 *   들어오는 방향: SetHand()          — 로직이 손패를 밀어넣는다
 *   나가는 방향:   OnCardPlayRequested — UI 가 사용 의도를 알린다
 * 로직 파트는 이 두 개만 알면 된다.
 *
 * ── 배치 방식 ────────────────────────────────────────────────────────
 *
 *  Box_Cards 가 **CanvasPanel 이면** 자리를 직접 계산해 카드를 보간 이동시킨다
 *  (중앙 정렬 · 자동 압축 · 부채꼴 · 덱에서 등장 · 날아가며 퇴장).
 *
 *  CanvasPanel 이 아니면(구형 HorizontalBox) 예전처럼 패널에 맡기고 연출을 끈다.
 *  ⚠️ 기존 WBP 를 고치지 않아도 컴파일과 실행이 그대로 유지되어야 하므로 남긴 경로다.
 *     연출을 쓰려면 WBP 에서 Box_Cards 를 CanvasPanel 로 바꿔야 한다(로그로 안내한다).
 *
 *  덱/버린 패의 화면 위치는 손패가 알 수 없다(손패 바깥이다). 기본값으로 두고,
	*  나중에 HUD 조정자가 SetDeckAnchor / SetDiscardAnchor 로 채워 준다.
 */
UCLASS(Abstract, Blueprintable)
class BG_API UBGHandWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 손패 전체를 교체한다. 서버에서 손패가 복제될 때마다 호출하면 된다. */
	UFUNCTION(BlueprintCallable, Category = "BG|Hand")
	void SetHand(const TArray<FBGCardView>& InCards);

	UFUNCTION(BlueprintCallable, Category = "BG|Hand")
	void ClearHand();

	/** 카드의 실제 출발 통지이며 배분 연출당 한 번만 효과음을 재생한다. */
	void NotifyCardDealStarted();

	/** 낙관적 제거용. 서버 확인 후 SetHand 로 덮어쓰는 것이 정석이다. */
	UFUNCTION(BlueprintCallable, Category = "BG|Hand")
	bool RemoveCardByInstanceId(int32 InstanceId);

	UFUNCTION(BlueprintPure, Category = "BG|Hand")
	int32 GetCardCount() const { return CardWidgets.Num(); }

	UFUNCTION(BlueprintPure, Category = "BG|Hand")
	UBGCardWidget* FindCardWidget(int32 InstanceId) const;

	/**
	 * 카드가 플레이 존에 드롭되어 사용 요청이 발생했다.
	 * PlayerController 가 여기에 바인딩해 ServerRequestPlayCard 를 호출하면 된다.
	 */
	UPROPERTY(BlueprintAssignable, Category = "BG|Hand")
	FOnBGCardPlayRequested OnCardPlayRequested;

	// ── 연출 관찰용 ──────────────────────────────────────────────────
	//
	//	카드 위젯은 자기 연출만 한다. 화면 전체에 걸리는 연출(카메라 흔들림, 전체 플래시,
	//	사운드)은 카드가 알 필요가 없으므로 여기 구독해서 처리한다.

	UPROPERTY(BlueprintAssignable, Category = "BG|Hand|FX")
	FOnBGCardDealtDelegate OnCardDealt;

	UPROPERTY(BlueprintAssignable, Category = "BG|Hand|FX")
	FOnBGCardPlayResolvedDelegate OnCardPlayResolved;

	UPROPERTY(BlueprintAssignable, Category = "BG|Hand|FX")
	FOnBGCardPlayExitStartedDelegate OnCardPlayExitStarted;

	//	⚠️ 카드 사용 연출은 여기서 나가지 않는다. 구독은 연출 큐에 직접 할 것 —
	//	   UBGPresentationDirector::Get → OnPresentationStarted / OnPresentationFinished.

	/** 플레이 존이 드롭을 받으면 호출한다. 직접 호출로 드래그 없이 테스트할 수도 있다. */
	UFUNCTION(BlueprintCallable, Category = "BG|Hand")
	void NotifyCardPlayRequested(const FBGCardView& Card);

	/**
	 * 플레이 존에 카드를 놓았다. 좌표까지 함께 받는 버전이다.
	 *
	 * ⚠️ 서버 응답을 기다리지 않고 **드롭한 그 자리에서** 중앙 연출을 시작한다.
	 *    드래그 비주얼(커서를 따라다니던 복제본)은 드롭 순간 슬레이트가 없애 버리므로,
	 *    여기서 이어받지 않으면 카드가 손패 자리로 돌아갔다가 서버 응답이 온 뒤에
	 *    다시 중앙으로 날아간다 — "놨는데 사라졌다 다시 나타나는" 그림이 된다.
	 *
	 *    대상 지정이 필요한 카드(bNeedsTarget)는 예외다. 아직 낸 것이 아니라
	 *    좌석을 고르는 중이므로 손패에 그대로 둔다.
	 */
	UFUNCTION(BlueprintCallable, Category = "BG|Hand")
	void NotifyCardDroppedOnPlayZone(const FBGCardView& Card, const FGeometry& ZoneGeometry, const FVector2D& DropScreenPosition);

	/**
	 * 손패 전체의 드래그를 잠근다. 대상 좌석 선택을 기다리는 동안 쓴다.
	 *
	 * 잠금은 위젯이 아니라 손패 상태로 기억되므로, 잠긴 동안 SetHand() 로 손패가
	 * 갱신돼도 새로 만들어진 카드에 그대로 적용된다. (서버 손패 복제와 대상 선택은
	 * 동시에 일어날 수 있어서, 이게 없으면 갱신 한 번에 잠금이 풀린다.)
	 */
	UFUNCTION(BlueprintCallable, Category = "BG|Hand")
	void SetInteractionLocked(bool bInLocked);

	UFUNCTION(BlueprintPure, Category = "BG|Hand")
	bool IsInteractionLocked() const { return bInteractionLocked; }

	/** 게임 로직 없이 UI 만 테스트하기 위한 더미 손패. */
	UFUNCTION(BlueprintCallable, Category = "BG|Hand|Debug")
	void FillWithDebugCards(int32 Count = 5);

	// ── 외부 앵커 (손패 바깥의 좌표) ─────────────────────────────────
	//
	//	손패는 덱이 화면 어디에 있는지 알 수 없다 — 알게 되면 HUD 구조에 묶여
	//	재사용도 테스트도 못 하게 된다. 그래서 좌표만 주입받는다.
	//	나중에 HUD 조정자가 덱/버린패 위젯의 지오메트리를 손패 로컬 좌표로
	//	변환해 여기에 넣어 준다. 안 넣으면 아래 Fallback 값이 쓰인다.

	/** 카드가 등장할 출발점(손패 패널 로컬 좌표). */
	UFUNCTION(BlueprintCallable, Category = "BG|Hand|Motion")
	void SetDeckAnchor(const FVector2D& InLocalPosition);

	/** 카드가 퇴장할 도착점(손패 패널 로컬 좌표). */
	UFUNCTION(BlueprintCallable, Category = "BG|Hand|Motion")
	void SetDiscardAnchor(const FVector2D& InLocalPosition);

	/**
	 * 사용한 카드가 잠시 머물 자리(손패 패널 로컬 좌표). 보통 플레이 존 중앙이다.
	 *
	 * ⚠️ 한 번 넣으면 그 좌표가 **고정**된다 — 플레이 존이 움직여도 따라가지 않는다.
	 *    보통은 부를 필요가 없다: 플레이 존 위젯이 화면에 있으면 그 중앙을 매번 직접 읽는다.
	 */
	UFUNCTION(BlueprintCallable, Category = "BG|Hand|Motion")
	void SetPlayZoneAnchor(const FVector2D& InLocalPosition);

	/**
	 * 대상 선택 중인 카드가 설 자리를 못 박는다(손패 패널 로컬 좌표).
	 *
	 * 보통은 부를 필요가 없다 — 기본값은 플레이 존 중앙 + TargetingAnchorOffset 이라
	 * 오프셋만 만져도 자리가 잡힌다. HUD 안의 특정 슬롯에 정확히 세워야 할 때만 쓴다.
	 *
	 * ⚠️ SetPlayZoneAnchor 와 같은 주의 — 한 번 넣으면 **고정**이다. 플레이 존이 움직여도,
	 *    해상도가 바뀌어도 따라가지 않는다.
	 */
	UFUNCTION(BlueprintCallable, Category = "BG|Hand|Motion")
	void SetTargetingAnchor(const FVector2D& InLocalPosition);

	/**
	 * 중앙 연출의 기준이 될 플레이 존 위젯을 지정한다.
	 *
	 * 보통은 부를 필요가 없다 — 처음 필요해질 때 화면에서 알아서 찾는다.
	 * 플레이 존이 화면에 둘 이상 있을 때만 어느 쪽인지 못 고르므로 여기서 못 박는다.
	 */
	UFUNCTION(BlueprintCallable, Category = "BG|Hand|Motion")
	void SetPlayZoneWidget(UBGPlayZoneWidget* InPlayZone);

	/**
	 * "이 카드는 내가 냈다" 고 표시한다. 손패에서 빠질 때 곧장 사라지지 않고
	 * 중앙에 올라가 머무는 연출을 탄다.
	 *
	 * ⚠️ 왜 표시가 필요한가: 손패 갱신은 "카드가 없어졌다" 만 알려 줄 뿐
	 *    사유(내가 사용 / 남이 훔침 / 버리기)를 알려 주지 않는다. 사유를 모르면
	 *    훔쳐 간 카드까지 중앙에 올라가 버린다. 그래서 로컬에서 아는 것만 표시해 둔다.
	 *
	 *    NotifyCardPlayRequested 가 자동으로 불러 주므로 보통은 직접 부를 일이 없다.
	 *    나중에 서버가 이동 사유를 이벤트로 보내면 이 표시는 필요 없어진다.
	 */
	UFUNCTION(BlueprintCallable, Category = "BG|Hand|Motion")
	void MarkCardPlayed(int32 InstanceId);

	/**
	 * "내가 냈다" 표시를 거둔다. 사용이 무산돼 카드가 손패에 남을 때 부른다(대상 선택 취소 등).
	 *
	 * 안 거두면 PlayedMarkLifetime 동안 표시가 남아, 그 사이 같은 카드가 다른 사유로
	 * 빠지면(도둑맞기 등) 내지도 않은 카드가 중앙 연출을 탄다.
	 */
	UFUNCTION(BlueprintCallable, Category = "BG|Hand|Motion")
	void ClearPlayedMark(int32 InstanceId);

	/**
	 * **남이 낸 카드**를 중앙에 띄운다(관전 연출). 판정이 이미 끝난 상태로만 부른다.
	 *
	 * 손패 연출과 같은 길(중앙 등장 → 판정 연출 → 디졸브)을 타지만 손패에는 속하지 않는다 —
	 * CardWidgets 에 넣지 않으므로 부채꼴 배치·호버·드래그와 무관하다.
	 *
	 * ⚠️ 내 카드에는 쓰지 말 것. 내 카드는 손패에 실물 위젯이 있고 그쪽이 정식 경로다
	 *    (여기로 오면 같은 카드가 둘 보인다).
	 */
	UFUNCTION(BlueprintCallable, Category = "BG|Hand|Motion")
	void ShowSpectatorCard(const FBGCardView& Card, const FBGCardPlayContext& Context);

	/**
	 * 중앙에 잡혀 있던 카드를 손패로 되돌린다. 대상 선택을 취소했을 때 부른다.
	 * 이미 사라지기 시작했으면 false.
	 */
	UFUNCTION(BlueprintCallable, Category = "BG|Hand|Motion")
	bool ReturnHeldCardToHand(int32 InstanceId);

	/**
	 * 지금 중앙에 잡혀 있는(확정도 취소도 안 된) 카드들의 InstanceId.
	 * 되돌릴지 말지는 부르는 쪽이 정한다 — 서버가 받아들인 카드도 손패가 올 때까지는 잡혀 있다.
	 */
	UFUNCTION(BlueprintPure, Category = "BG|Hand|Motion")
	TArray<int32> GetHeldCardIds() const;

	/**
	 * 판정 결과가 도착했다. 중앙에 잡혀 있는 카드를 놓아 주고 맥락을 넘긴다.
	 *
	 * ⚠️ 이 통지가 "효과가 발동했다" 의 정식 신호다. 손패에서 카드가 사라지는 것보다
	 *    정확하고 등급까지 실어 올 수 있다. 다만 장비(파란 카드)는 판정을 거치지 않아
	 *    이 통지가 오지 않는다 — 그쪽은 SetHand 의 폴백 경로로 풀린다(bResolvedWithoutRoll).
	 *
	 * 내가 낸 카드가 아니면(다른 좌석의 판정) 아무것도 하지 않는다.
	 */
	UFUNCTION(BlueprintCallable, Category = "BG|Hand|Motion")
	void NotifyCardResolved(const FBGCardPlayContext& Context, int32 InstanceId);

	/**
	 * Index 번째 카드가 있어야 할 자리를 계산한다. 상태를 읽지 않는 순수 함수라
	 * 언제 불러도 같은 답이 나온다 — 연출 디버깅에도 그대로 쓸 수 있다.
	 */
	UFUNCTION(BlueprintPure, Category = "BG|Hand|Motion")
	FBGCardSlotLayout ComputeCardSlot(int32 Index, int32 Count) const;

protected:
	/**
	 * 카드들의 보간을 여기서 한 번에 돌린다.
	 *
	 * 네이티브 부모가 NativeTick 을 재정의하면 WBP 컴파일 시 ClassRequiresNativeTick 이
	 * 켜지므로(WidgetBlueprint.cpp), BP 에 Tick 이벤트가 없어도 호출된다.
	 * 끄고 싶으면 클래스에 DisableNativeTick 메타를 달면 된다.
	 */
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPanelWidget> Box_Cards;

	UPROPERTY(EditAnywhere, Category = "BG|Hand")
	TSubclassOf<UBGCardWidget> CardWidgetClass;

	/** ⚠️ 구형 HorizontalBox 경로에서만 쓰인다. 캔버스 배치는 CardOverlapRatio 를 본다. */
	UPROPERTY(EditAnywhere, Category = "BG|Hand")
	FMargin CardPadding = FMargin(4.f, 0.f);

	// ── 배치 튜닝 ────────────────────────────────────────────────────

	// ── 간격 ─────────────────────────────────────────────────────────
	//
	//	카드 중심 사이의 거리 = 카드 폭 × (1 - CardOverlapRatio).
	//	일직선이든 부채꼴이든 **같은 값**이 쓰인다 — 모드를 바꿔도 간격 감각이 유지된다.
	//
	//	⚠️ 간격을 픽셀이 아니라 **비율**로 잡는 이유: 픽셀로 두면 카드 크기를 바꿀 때마다
	//	   간격도 같이 고쳐야 하고, CardWidth 가 실제 카드와 어긋나면 조용히 틀어진다.
	//	   비율이면 카드가 커지든 작아지든 "얼마나 겹쳐 보이는가" 가 유지된다.

	/**
	 * 카드 한 장의 폭.
	 * ⚠️ 카드 WBP 의 실제 폭과 맞춰야 한다. 이 값이 실제보다 작으면 겹쳐 보이고,
	 *    크면 사이가 벌어져 보인다 — 간격이 안 맞는 원인은 대개 여기다.
	 *
	 * ⚠️ 실제 크기는 WBP 가 정한다(슬롯이 AutoSize 다). 그래서 카드 **안의 내용물**이
	 *    크기를 밀어낼 수 있다 — 줄바꿈이 꺼진 긴 설명 텍스트가 대표적이고, 그러면
	 *    텍스트가 긴 카드만 넓어져 이 값과 어긋난다. 카드 폭이 제각각으로 보이면
	 *    WBP 의 Auto Wrap Text 부터 확인할 것.
	 */
	UPROPERTY(EditAnywhere, Category = "BG|Hand|Layout", meta = (ClampMin = "1"))
	float CardWidth = 200.f;

	/**
	 * 옆 카드에 **가려지는 비율**. 0.3 이면 카드의 30% 가 다음 카드에 덮인다.
	 *
	 *   0.0  →  옆면끼리 딱 붙는다 (겹침 없음)
	 *   0.3  →  슬레이 더 스파이어 정도의 손패 밀도  ← 기본
	 *   0.5  →  절반이 가려진다. 장수가 많을 때의 느낌
	 *
	 * 손패는 겹쳐 쥐는 것이라 0 에 가까우면 진열대처럼 보인다.
	 */
	UPROPERTY(EditAnywhere, Category = "BG|Hand|Layout", meta = (ClampMin = "0.0", ClampMax = "0.9"))
	float CardOverlapRatio = 0.30f;

	/**
	 * 실제 카드 폭을 위젯에서 재서 쓸지. 켜 두는 것을 권한다.
	 *
	 * ⚠️ 슬롯이 AutoSize 라 카드의 진짜 폭은 WBP 가 정한다. 그래서 CardWidth 가 실제와
	 *    어긋나면 간격이 조용히 틀어진다 — 카드가 이유 없이 겹치거나 벌어지는 원인이
	 *    대부분 이것이었다. 재서 쓰면 WBP 에서 카드 크기를 바꿔도 간격이 알아서 따라온다.
	 *
	 * 첫 프레임처럼 아직 크기를 모를 때만 CardWidth 로 폴백한다.
	 */
	UPROPERTY(EditAnywhere, Category = "BG|Hand|Layout")
	bool bMeasureCardWidth = true;

	/**
	 * 손패가 퍼질 수 있는 최대 폭. 패널이 더 넓어도 이 폭을 넘지 않는다
	 * (넓은 화면에서 카드가 양끝으로 흩어지는 것을 막는다).
	 *
	 * ⚠️ 간격을 아무리 올려도 안 넓어지면 대개 여기에 걸린 것이다. 이 상한이
	 *    조용히 간격을 깎는다 — 손패가 많을 때는 겹치는 것이 정상이지만,
	 *    적은 장수에서도 안 벌어지면 이 값을 올려야 한다.
	 */
	UPROPERTY(EditAnywhere, Category = "BG|Hand|Layout", meta = (ClampMin = "1"))
	float MaxHandWidth = 1100.f;

	/**
	 * 압축의 하한. 이보다 좁아지지 않는다 — 완전히 겹쳐 한 장처럼 보이는 것을 막는다.
	 *
	 * 카드 폭의 25~30% 아래로 내리지 말 것. 그보다 좁으면 남는 부분이 테두리뿐이라
	 * 카드 장수는 세어져도 무슨 카드인지 못 읽는다.
	 */
	UPROPERTY(EditAnywhere, Category = "BG|Hand|Layout", meta = (ClampMin = "1"))
	float MinCardStep = 60.f;

	/**
	 * 카드의 정렬 기준점이자 회전 축.
	 *
	 *   (0.5, 0.5) → 카드 한가운데. 패널 세로 중앙에 놓인다. ← 기본
	 *   (0.5, 1.0) → 카드 밑동. 패널 바닥에 붙고, 부채꼴이 밑동을 축으로 돈다.
	 *
	 * ⚠️ Y 값이 곧 "패널 높이의 몇 지점에 놓을지" 도 결정한다.
	 *    0.5 면 패널 세로 중앙, 1.0 이면 패널 바닥이다 — 정렬과 배치가 항상 맞물린다.
	 */
	UPROPERTY(EditAnywhere, Category = "BG|Hand|Layout")
	FVector2D CardAlignment = FVector2D(0.5f, 0.5f);

	/** 세로 미세 조정(픽셀). 음수면 위로, 양수면 아래로. */
	UPROPERTY(EditAnywhere, Category = "BG|Hand|Layout")
	float VerticalOffset = 0.f;

	// ── 부채꼴 ───────────────────────────────────────────────────────
	//
	//	카드를 **원호 위에 놓고** 각도를 그 접선에 맞춘다. 손으로 부채를 쥔 것처럼
	//	손패 아래 한 점(회전 중심)을 향해 카드들이 모인다.
	//
	//	⚠️ 직선 위에 놓고 제자리에서만 회전시키면 안 된다. 위치는 일직선인데 각도만
	//	   커지므로 바깥쪽 카드가 좌우로 삐죽 튀어나온다 — 부채가 아니라 부러진 모양이 된다.
	//
	//	⚠️ "최대 각도를 장수로 나눈다" 로도 하면 안 된다. 2장일 때도 부채가 활짝 펴진다.
	//	   장당 각도를 고정하고 전체에 상한을 두어야 장수가 늘수록 자연스럽게 펼쳐진다.

	/** 끄면 회전 없이 일직선으로 배치한다. 간격 계산은 양쪽이 같다. */
	UPROPERTY(EditAnywhere, Category = "BG|Hand|Layout")
	bool bUseFanLayout = true;

	/**
	 * 회전 중심까지의 거리(픽셀). **곡률만** 정한다 — 간격은 CardOverlapRatio 가 정한다.
	 *   크게  → 완만하게 휜다 (거의 일직선)
	 *   작게  → 급하게 휜다
	 *
	 * 각도는 간격에서 역산한다(θ = asin(간격 / 반지름)). 그래서 반지름을 바꿔도
	 * 카드 사이 거리는 그대로 유지되고 휘는 정도만 달라진다.
	 */
	UPROPERTY(EditAnywhere, Category = "BG|Hand|Layout", meta = (ClampMin = "1", EditCondition = "bUseFanLayout"))
	float FanRadius = 1000.f;

	/** 부채가 벌어질 수 있는 전체 각도(도). 장수가 많아져도 이 이상은 안 펴진다. */
	UPROPERTY(EditAnywhere, Category = "BG|Hand|Layout", meta = (ClampMin = "0", EditCondition = "bUseFanLayout"))
	float MaxTotalFanAngle = 60.f;

	/**
	 * 카드 한 장이 기울 수 있는 최대 각도(도). **위치는 그대로 두고 각도만** 깎는다.
	 *
	 * FanRadius 로도 기울기를 낮출 수 있지만, 실제 각도는 카드 폭과 장수에서 역산되기
	 * 때문에 손패가 길어지거나 카드 WBP 가 넓어지면 조용히 다시 커진다. 여기서 상한을
	 * 걸면 그 두 가지와 무관하게 보장된다.
	 *
	 * ⚠️ 위치와 각도를 떼어 놓는 것은 원래 금기다(일직선 위에 각도만 주면 바깥 카드가
	 *    삐죽 튀어나온다). 여기서 안전한 이유는 **깎기만 하기 때문**이다 — 원호가 주는
	 *    각도보다 더 세우기만 하므로 부채가 부러져 보이지 않는다. 다만 FanRadius 가
	 *    작아 원호가 가파른데 이 값만 낮추면 그 금기에 걸린다. 둘을 같이 올릴 것.
	 */
	UPROPERTY(EditAnywhere, Category = "BG|Hand|Layout", meta = (ClampMin = "0", EditCondition = "bUseFanLayout"))
	float MaxCardTiltDegrees = 2.f;

	// ── 연출 튜닝 ────────────────────────────────────────────────────

	/**
	 * 한 장이 다 안착한 뒤에 다음 장을 보낸다.
	 *
	 * 끄면 DealStaggerSeconds 만큼만 어긋나 여러 장이 겹쳐 날아온다(빠르지만 뭉쳐 보인다).
	 * 켜면 한 장씩 또렷하게 들어온다 — 대신 장수만큼 시간이 걸린다.
	 */
	UPROPERTY(EditAnywhere, Category = "BG|Hand|Motion")
	bool bSequentialDeal = true;

	/** 순차 배분에서 한 장이 안착한 뒤 다음 장까지의 간격(초). */
	UPROPERTY(EditAnywhere, Category = "BG|Hand|Motion", meta = (ClampMin = "0", EditCondition = "bSequentialDeal"))
	float SequentialDealGap = 0.08f;

	/**
	 * 겹쳐 배분할 때 장당 지연(초). bSequentialDeal 이 꺼졌을 때만 쓰인다.
	 * 0 이면 전부 겹쳐 날아와 한 장처럼 보인다.
	 */
	UPROPERTY(EditAnywhere, Category = "BG|Hand|Motion", meta = (ClampMin = "0", EditCondition = "!bSequentialDeal"))
	float DealStaggerSeconds = 0.12f;

	/**
	 * 아직 나타나지 않은(stagger 대기 중인) 카드를 뺀 장수로 자리를 잡는다.
	 *
	 * ⚠️ 끄면 SetHand 시점에 5장 자리가 **한꺼번에** 열리고, 카드는 이미 벌어져 있는
	 *    틈으로 날아와 박힌다 — 부채가 펼쳐지는 것이 아니라 카드가 튀어나오는 것으로
	 *    보인다("그냥 점프해 온다" 의 원인이 이것이다).
	 *
	 * 켜면 한 장이 나타날 때마다 손패가 그만큼씩 벌어진다. 이미 놓인 카드들이
	 * 옆으로 비켜 주면서 자리를 내주므로 손패가 살아 있는 것처럼 보인다.
	 */
	UPROPERTY(EditAnywhere, Category = "BG|Hand|Motion")
	bool bProgressiveFanOpen = true;

	// ── 호버 시 이웃 비키기 ──────────────────────────────────────────
	//
	//	마우스가 올라간 카드를 키우기만 하면 양옆 카드를 덮어 버린다. 이웃이 옆으로
	//	비켜 줘야 "집어 들 수 있는 한 장" 으로 읽힌다(슬레이 더 스파이어 방식).
	//
	//	⚠️ 이동은 여기서 시키지 않는다. 목표 자리만 바꾸고 실제 이동은 카드의
	//	   MoveInterpSpeed 보간이 한다 — 그래야 배분·퇴장 이동과 감각이 같다.

	/**
	 * 바로 옆 카드가 좌우로 비키는 거리(픽셀). 0 이면 이 연출을 끈다.
	 *
	 * 카드 폭의 15~25% 가 무난하다. 너무 크면 손패가 통째로 출렁여서
	 * 어느 카드에 마우스가 올라갔는지 오히려 안 읽힌다.
	 */
	UPROPERTY(EditAnywhere, Category = "BG|Hand|Motion|Hover", meta = (ClampMin = "0"))
	float HoverSpreadAmount = 38.f;

	/**
	 * 한 칸 멀어질 때마다 비키는 거리에 곱해지는 비율.
	 * 1 에 가까우면 손패 전체가 평행이동하듯 밀린다 — 0.4~0.5 가 자연스럽다.
	 */
	UPROPERTY(EditAnywhere, Category = "BG|Hand|Motion|Hover", meta = (ClampMin = "0", ClampMax = "1"))
	float HoverSpreadFalloff = 0.45f;

	/** 비켜 주는 이웃의 범위(칸). 1 이면 바로 옆 한 장씩만 비킨다. */
	UPROPERTY(EditAnywhere, Category = "BG|Hand|Motion|Hover", meta = (ClampMin = "0"))
	int32 HoverSpreadRange = 2;

	/** SetDeckAnchor 가 아직 안 불렸을 때 쓸 출발점. 화면 왼쪽 바깥쯤. */
	UPROPERTY(EditAnywhere, Category = "BG|Hand|Motion")
	FVector2D DeckAnchorFallback = FVector2D(-220.f, 60.f);

	/** SetDiscardAnchor 가 아직 안 불렸을 때 쓸 도착점. 화면 위쪽 가운데쯤. */
	UPROPERTY(EditAnywhere, Category = "BG|Hand|Motion")
	FVector2D DiscardAnchorFallback = FVector2D(0.f, -180.f);

	/**
	 * 플레이 존을 못 찾았고 SetPlayZoneAnchor 도 안 불렸을 때 쓸 중앙 자리.
	 * 손패 가운데를 기준으로 한 오프셋이다 — Y 를 음수로 줄수록 위로 올라간다.
	 *
	 * ⚠️ 여기까지 내려오면 내 카드와 관전 카드가 같은 자리에 서기는 하지만 실제
	 *    플레이 존과는 어긋난다. 존이 화면에 있는데 이 값이 쓰였다면 그 프레임에
	 *    존의 지오메트리가 아직 0 이었다는 뜻이다.
	 */
	UPROPERTY(EditAnywhere, Category = "BG|Hand|Motion")
	FVector2D PlayZoneAnchorFallback = FVector2D(0.f, -260.f);

	// ── 대상 선택 중인 카드 (뱅 등) ──────────────────────────────────
	//
	//	대상 좌석을 고르는 동안 그 카드를 어디에 세울 것인가.
	//	카드를 손패에 그대로 두면 무엇을 내는 중인지 화면에서 사라지고, 그렇다고 플레이 존
	//	한복판에 세우면 아직 내지도 않은 카드가 판을 가린다 — 그래서 자리를 따로 둔다.

	/**
	 * 대상 선택 중에 카드를 세울 것인가.
	 *
	 * 끄면 카드는 손패에 그대로 남고, 서버가 확정해 손패에서 빠질 때 비로소 중앙 연출을 탄다
	 * (BeginExitFor). 화면을 최대한 비우고 싶을 때 쓴다.
	 */
	UPROPERTY(EditAnywhere, Category = "BG|Hand|Motion|Targeting")
	bool bPresentCardWhileTargeting = true;

	/**
	 * 대상 선택 중인 카드가 설 자리 — **플레이 존 중앙 기준 오프셋**이다.
	 * 0 이면 낸 카드와 같은 자리에 선다. Y 를 키우면 아래로, X 를 줄이면 왼쪽으로 간다.
	 *
	 * 오프셋으로 둔 이유: 절대 좌표로 잡으면 플레이 존이 움직였을 때 따라가지 않아,
	 * 해상도가 다른 화면에서 카드만 엉뚱한 곳에 선다(PlayZoneAnchor 주석과 같은 이유).
	 * 자리를 완전히 못 박아야 하면 SetTargetingAnchor 를 쓴다.
	 */
	UPROPERTY(EditAnywhere, Category = "BG|Hand|Motion|Targeting")
	FVector2D TargetingAnchorOffset = FVector2D(-420.f, 180.f);

	/**
	 * 관전 카드(ShowSpectatorCard)가 중앙에 서기 전에 출발하는 자리. 중앙 기준 오프셋이다.
	 *
	 * 남의 카드는 손패가 없어 출발점이 없다 — 중앙에서 그냥 튀어나오면 "언제 나타났는지"
	 * 가 안 읽히므로 조금 떨어진 곳에서 날아오게 한다. 0 으로 두면 중앙에서 그대로 커진다.
	 */
	UPROPERTY(EditAnywhere, Category = "BG|Hand|Motion")
	FVector2D SpectatorCardEntryOffset = FVector2D(0.f, 120.f);

	/**
	 * "내가 냈다" 표시가 유지되는 시간(초).
	 *
	 * 서버가 거절하면 카드는 손패에 그대로 남는다 — 그때 표시를 지우지 않으면
	 * 나중에 그 카드가 다른 사유로 빠질 때 엉뚱하게 중앙 연출이 나온다.
	 */
	UPROPERTY(EditAnywhere, Category = "BG|Hand|Motion", meta = (ClampMin = "0.1"))
	float PlayedMarkLifetime = 3.f;

	/**
	 * 중앙에 잡아 둘 수 있는 최대 시간(초). 넘으면 손패로 되돌린다.
	 *
	 * 순수한 안전장치다 — 서버 확정도 취소 통지도 오지 않는 경우(연결 문제, 놓친 이벤트)에
	 * 카드가 중앙에 영원히 박혀 있는 것을 막는다. 대상 선택을 느긋하게 해도 걸리지 않도록
	 * 넉넉하게 잡을 것.
	 */
	UPROPERTY(EditAnywhere, Category = "BG|Hand|Motion", meta = (ClampMin = "1"))
	float HoldTimeoutSeconds = 30.f;

private:
	/** Box_Cards 가 캔버스면 그 포인터, 아니면 nullptr(= 연출 없는 구형 경로). */
	UCanvasPanel* GetCardCanvas() const;

	/** 카드 위젯 하나를 만들어 패널에 붙인다. 슬롯 설정까지 여기서 끝낸다. */
	UBGCardWidget* CreateCardWidget(const FBGCardView& Card);

	// ── 카드 연출 신호 중계 ──────────────────────────────────────────
	//	CreateCardWidget 에서 카드마다 바인딩한다 — 손패 카드와 관전 카드가 모두 거길 지난다.
	//	HUD 가 카드에 직접 구독하지 않는 이유는 수명이다: 카드는 판마다 생겼다 사라진다.

	UFUNCTION()
	void HandleCardPlayFXStarted(UBGCardWidget* Card, const FBGCardPlayContext& Context);

	UFUNCTION()
	void HandleCardPlayExitStarted(UBGCardWidget* Card);

	/**
	 * 연출 큐를 찾고, 아직이면 종료 통지를 구독한다. 없으면 nullptr(PC 가 아직 없음).
	 * ⚠️ 이름이 Get 이 아닌 이유: 구독을 건다. 단순 조회가 필요하면
	 *    UBGPresentationDirector::Get 을 쓸 것.
	 */
	class UBGPresentationDirector* EnsureDirectorBound();

	/** 디렉터가 연출을 끝냈다 — 중앙에 서 있던 카드에 전달한다. */
	UFUNCTION()
	void HandlePresentationFinished(const struct FBGPresentationRequest& Request);

	/**
	 * 연출을 기다리는 카드들. 디렉터 큐와 **같은 순서**로 쌓고 앞에서 뺀다.
	 * 소유가 아니라 지목이므로 약참조 — 연출 중 카드가 파괴될 수 있다.
	 */
	TArray<TWeakObjectPtr<UBGCardWidget>> PresentingCards;

	/** 슬롯 정렬과 회전 축을 CardAlignment 로 맞춘다. 둘은 항상 같아야 한다. */
	void ApplyCardAlignment(UBGCardWidget* CardWidget) const;

	/** 손패 전체에 새 목표 자리를 지정한다. bSnap 이면 보간 없이 즉시 배치한다. */
	void RelayoutCards(bool bSnap = false);

	/**
	 * Index 카드가 호버 때문에 옆으로 비켜야 할 거리(픽셀). 왼쪽이 음수다.
	 * HoveredIndex 가 INDEX_NONE 이면 0 — 아무도 비키지 않는다.
	 *
	 * ComputeCardSlot 에 넣지 않고 따로 두는 이유: 저쪽은 (Index, Count) 만 보는
	 * 순수 함수여야 한다. "지금 어디에 마우스가 있는가" 는 상태라서 섞으면 안 된다.
	 */
	float ComputeHoverSpread(int32 Index, int32 HoveredIndex) const;

	/** 이미 나타난(stagger 대기가 끝난) 카드 장수. 부채가 펼쳐지는 속도를 정한다. */
	int32 CountRevealedCards() const;

	/** 손패에서 빠진 카드를 퇴장 목록으로 옮긴다. */
	void BeginExitFor(UBGCardWidget* CardWidget);

	/**
	 * 이 카드를 지금 당장 손패에서 떼어내 중앙 연출로 넘긴다(서버 응답을 기다리지 않는다).
	 * LocalStartPosition 에서 출발한다. 카드를 찾지 못하면 false.
	 *
	 * bTargeting 이면 카드가 설 자리가 다르다(GetTargetingAnchor) — 아직 확정된 카드가
	 * 아니므로 플레이 존 한복판을 차지하지 않는다.
	 */
	bool BeginPlayPresentationAt(int32 InstanceId, const FVector2D& LocalStartPosition,
		bool bTargeting = false);

	/** 이미 손패를 떠나 연출 중인 카드인가. 갱신이 겹칠 때 복제본이 생기는 것을 막는다. */
	bool IsOutgoing(int32 InstanceId) const;

	/**
	 * 중앙에 잡혀 대기 중인 카드를 찾는다. InstanceId 가 INDEX_NONE 이면 아무거나 —
	 * 중앙에 잡히는 카드는 한 번에 하나뿐이라 모호하지 않다.
	 */
	UBGCardWidget* FindHeldCard(int32 InstanceId) const;

	/**
	 * 잡아 둔 카드를 놓아 주는 **유일한 경로**.
	 *
	 * ⚠️ 해제 경로가 둘(판정 통지 / 손패 갱신 폴백)이라 예전에는 한쪽만 델리게이트를
	 *    쏘고 있었다 — 장비 카드는 OnCardPlayResolved 가 영영 안 나갔다.
	 *    양쪽이 반드시 여기로 모이게 한다.
	 */
	void ReleaseHeldCard(UBGCardWidget* CardWidget, const FBGCardPlayContext& Context);

	/** 이번 묶음의 총 장수를 채워 넣고 OnCardDealt 를 쏜다. SetHand 배분 루프 직후. */
	void FinalizeDealBatch(const TArray<UBGCardWidget*>& Batch);

	/**
	 * 중앙에 잡혀 있는데 새 손패 목록에 없는 카드를 놓아 준다 — 판정 통지가
	 * 오지 않는(또는 늦게 오는) 경우의 폴백이다.
	 */
	void ReleaseHeldCardsMissingFrom(const TArray<FBGCardView>& InCards);

	/** 배치 계산에 쓸 패널 폭. 지오메트리가 아직 없으면 MaxHandWidth 로 폴백한다. */
	float GetLayoutWidth() const;
	float GetLayoutHeight() const;

	/**
	 * 배치 계산에 쓸 카드 폭. bMeasureCardWidth 면 실제 위젯에서 재고,
	 * 아직 크기를 모르면 CardWidth 로 폴백한다.
	 */
	float GetEffectiveCardWidth() const;

	FVector2D GetDeckAnchor() const;
	FVector2D GetDiscardAnchor() const;

	/**
	 * 카드가 서는 중앙 자리. **내가 낸 카드와 남이 낸 카드가 모두 이 값을 쓴다** —
	 * 두 경로가 따로 계산하면 같은 판인데 화면마다 카드가 다른 자리에 선다.
	 */
	FVector2D GetPlayZoneAnchor() const;

	/**
	 * 대상 선택 중인 카드가 설 자리. SetTargetingAnchor 로 못 박았으면 그 값,
	 * 아니면 플레이 존 중앙 + TargetingAnchorOffset.
	 */
	FVector2D GetTargetingAnchor() const;

	/** 중앙 연출의 기준이 될 플레이 존. 지정이 없으면 화면에서 한 번 찾아 기억한다. */
	UBGPlayZoneWidget* ResolvePlayZoneWidget() const;

	/** 캔버스가 아닌 패널에 붙었을 때의 옛 동작(전부 다시 만들기). */
	void SetHandLegacy(const TArray<FBGCardView>& InCards);

	UPROPERTY()
	TArray<TObjectPtr<UBGCardWidget>> CardWidgets;

	/**
	 * 퇴장 연출 중인 카드. 손패가 아니므로 배치 계산에서 빠진다 —
	 * 그래서 한 장이 날아가기 시작하는 즉시 나머지가 알아서 중앙 재정렬된다.
	 */
	UPROPERTY()
	TArray<TObjectPtr<UBGCardWidget>> OutgoingWidgets;

	/** 위젯이 아니라 손패에 붙는 상태. SetHand 로 카드가 새로 만들어져도 유지된다. */
	bool bInteractionLocked = false;

	//	지오메트리는 첫 프레임에 0 이다. 유효해지는 순간 한 번 스냅해야 카드가 튀지 않는다.
	FVector2D CachedPanelSize = FVector2D::ZeroVector;
	bool bHasValidPanelSize = false;

	TOptional<FVector2D> DeckAnchor;
	TOptional<FVector2D> DiscardAnchor;
	TOptional<FVector2D> PlayZoneAnchor;
	TOptional<FVector2D> TargetingAnchor;

	//	찾기가 비싸지는 않지만 카드마다 할 이유도 없다. 약참조라 존이 사라지면 다시 찾는다.
	mutable TWeakObjectPtr<UBGPlayZoneWidget> PlayZoneWidget;

	/** InstanceId → 표시가 만료되기까지 남은 시간. 틱에서 줄어든다. */
	TMap<int32, float> PlayedMarks;

	/**
	 * 아직 중앙에 올라오지 않은 카드의 판정 통지를 잠깐 들고 있는 자리.
	 *
	 * 대상 지정 카드는 서버가 확정한 뒤에야 중앙 연출을 시작하므로 판정 통지가 손패 복제를
	 * 앞지를 수 있다. 그냥 버리면 카드가 중앙에서 풀리지 못하고 타임아웃까지 박힌다 —
	 * BeginExitFor 가 중앙 연출을 태우는 즉시 풀어 준다.
	 */
	FBGCardPlayContext PendingResolution;
	int32 PendingResolutionId = INDEX_NONE;
	float PendingResolutionRemaining = 0.f;

	/**
	 * 중앙에 잡혀 있는 카드의 부기(簿記).
	 *
	 * 예전에는 원래 위치와 경과 시간을 각각 다른 맵에 들고 있었다 — 수명이 같은 값을
	 * 두 곳에서 지우다 보니 한쪽만 남는 누수가 생기기 쉬웠다. 한 구조체로 묶는다.
	 */
	struct FHeldCardRecord
	{
		/** 잡아 두기 전 손패에서의 인덱스. 되돌릴 때 원래 자리로 넣는다. */
		int32 OriginalIndex = 0;

		/** 중앙에 잡혀 있은 시간. 안전장치 타임아웃에 쓴다. */
		float Elapsed = 0.f;
	};

	TMap<int32, FHeldCardRecord> HeldCards;

	//	구형 패널 경고는 한 번만 — 매 SetHand 마다 찍으면 로그가 도배된다.
	bool bLegacyPanelWarned = false;

	//	지난 프레임에 배분 중이던 카드가 있었는가. 끝나는 순간을 잡아 Z 순서를 되돌린다.
	bool bWasDealing = false;
	bool bDealAudioPlayed = false;

	//	지난 프레임에 마우스가 올라가 있던 카드. 바뀔 때만 Z 순서를 다시 잡는다.
	int32 LastHoveredInstanceId = INDEX_NONE;

	//	지난 프레임에 나타나 있던 카드 장수. 한 장 늘어날 때마다 손패를 다시 벌린다.
	int32 LastRevealedCount = 0;
};
