// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/View/BGCardView.h"
#include "BGHandWidget.generated.h"

class UBGCardWidget;
class UCanvasPanel;
class UPanelWidget;

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
 *  나중에 UBGCardFXComponent 가 SetDeckAnchor / SetDiscardAnchor 로 채워 준다.
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
	//	나중에 UBGCardFXComponent 가 덱/버린패 위젯의 지오메트리를 손패 로컬 좌표로
	//	변환해 여기에 넣어 준다. 안 넣으면 아래 Fallback 값이 쓰인다.

	/** 카드가 등장할 출발점(손패 패널 로컬 좌표). */
	UFUNCTION(BlueprintCallable, Category = "BG|Hand|Motion")
	void SetDeckAnchor(const FVector2D& InLocalPosition);

	/** 카드가 퇴장할 도착점(손패 패널 로컬 좌표). */
	UFUNCTION(BlueprintCallable, Category = "BG|Hand|Motion")
	void SetDiscardAnchor(const FVector2D& InLocalPosition);

	/** 사용한 카드가 잠시 머물 자리(손패 패널 로컬 좌표). 보통 플레이 존 중앙이다. */
	UFUNCTION(BlueprintCallable, Category = "BG|Hand|Motion")
	void SetPlayZoneAnchor(const FVector2D& InLocalPosition);

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

	/** ⚠️ 구형 HorizontalBox 경로에서만 쓰인다. 캔버스 배치는 CardSpacing 을 본다. */
	UPROPERTY(EditAnywhere, Category = "BG|Hand")
	FMargin CardPadding = FMargin(4.f, 0.f);

	// ── 배치 튜닝 ────────────────────────────────────────────────────

	// ── 간격 ─────────────────────────────────────────────────────────
	//
	//	카드 중심 사이의 거리 = CardWidth + CardSpacing.
	//	일직선이든 부채꼴이든 **같은 값**이 쓰인다 — 모드를 바꿔도 간격 감각이 유지된다.
	//
	//	  CardSpacing > 0  →  카드 사이가 벌어진다
	//	  CardSpacing = 0  →  옆면끼리 딱 붙는다
	//	  CardSpacing < 0  →  그만큼 겹친다

	/**
	 * 카드 한 장의 폭.
	 * ⚠️ 카드 WBP 의 실제 폭과 맞춰야 한다. 이 값이 실제보다 작으면 겹쳐 보이고,
	 *    크면 사이가 벌어져 보인다 — 간격이 안 맞는 원인은 대개 여기다.
	 */
	UPROPERTY(EditAnywhere, Category = "BG|Hand|Layout", meta = (ClampMin = "1"))
	float CardWidth = 140.f;

	/** 카드 옆면 사이의 여백. 음수면 그만큼 겹친다. */
	UPROPERTY(EditAnywhere, Category = "BG|Hand|Layout")
	float CardSpacing = 12.f;

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

	/** 압축의 하한. 이보다 좁아지지 않는다 — 완전히 겹쳐 한 장처럼 보이는 것을 막는다. */
	UPROPERTY(EditAnywhere, Category = "BG|Hand|Layout", meta = (ClampMin = "1"))
	float MinCardStep = 28.f;

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
	 * 회전 중심까지의 거리(픽셀). **곡률만** 정한다 — 간격은 CardSpacing 이 정한다.
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

	/** SetDeckAnchor 가 아직 안 불렸을 때 쓸 출발점. 화면 왼쪽 바깥쯤. */
	UPROPERTY(EditAnywhere, Category = "BG|Hand|Motion")
	FVector2D DeckAnchorFallback = FVector2D(-220.f, 60.f);

	/** SetDiscardAnchor 가 아직 안 불렸을 때 쓸 도착점. 화면 위쪽 가운데쯤. */
	UPROPERTY(EditAnywhere, Category = "BG|Hand|Motion")
	FVector2D DiscardAnchorFallback = FVector2D(0.f, -180.f);

	/**
	 * SetPlayZoneAnchor 가 아직 안 불렸을 때 쓸 중앙 자리.
	 * 손패 가운데를 기준으로 한 오프셋이다 — Y 를 음수로 줄수록 위로 올라간다.
	 */
	UPROPERTY(EditAnywhere, Category = "BG|Hand|Motion")
	FVector2D PlayZoneAnchorFallback = FVector2D(0.f, -260.f);

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

	/** 슬롯 정렬과 회전 축을 CardAlignment 로 맞춘다. 둘은 항상 같아야 한다. */
	void ApplyCardAlignment(UBGCardWidget* CardWidget) const;

	/** 손패 전체에 새 목표 자리를 지정한다. bSnap 이면 보간 없이 즉시 배치한다. */
	void RelayoutCards(bool bSnap = false);

	/** 손패에서 빠진 카드를 퇴장 목록으로 옮긴다. */
	void BeginExitFor(UBGCardWidget* CardWidget);

	/**
	 * 이 카드를 지금 당장 손패에서 떼어내 중앙 연출로 넘긴다(서버 응답을 기다리지 않는다).
	 * LocalStartPosition 에서 출발한다. 카드를 찾지 못하면 false.
	 */
	bool BeginPlayPresentationAt(int32 InstanceId, const FVector2D& LocalStartPosition);

	/** 이미 손패를 떠나 연출 중인 카드인가. 갱신이 겹칠 때 복제본이 생기는 것을 막는다. */
	bool IsOutgoing(int32 InstanceId) const;

	/** 배치 계산에 쓸 패널 폭. 지오메트리가 아직 없으면 MaxHandWidth 로 폴백한다. */
	float GetLayoutWidth() const;
	float GetLayoutHeight() const;

	FVector2D GetDeckAnchor() const;
	FVector2D GetDiscardAnchor() const;
	FVector2D GetPlayZoneAnchor() const;

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

	/** InstanceId → 표시가 만료되기까지 남은 시간. 틱에서 줄어든다. */
	TMap<int32, float> PlayedMarks;

	/** 중앙에 잡아 두기 전 손패에서의 위치. 되돌릴 때 원래 자리로 넣는다. */
	TMap<int32, int32> HeldOriginalIndex;

	/** InstanceId → 중앙에 잡혀 있은 시간. 안전장치 타임아웃에 쓴다. */
	TMap<int32, float> HeldElapsed;

	//	구형 패널 경고는 한 번만 — 매 SetHand 마다 찍으면 로그가 도배된다.
	bool bLegacyPanelWarned = false;

	//	지난 프레임에 배분 중이던 카드가 있었는가. 끝나는 순간을 잡아 Z 순서를 되돌린다.
	bool bWasDealing = false;

	//	지난 프레임에 마우스가 올라가 있던 카드. 바뀔 때만 Z 순서를 다시 잡는다.
	int32 LastHoveredInstanceId = INDEX_NONE;
};
