// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/View/BGCardView.h"
#include "UI/View/BGCardFXTypes.h"   // FBGCardDealContext / FBGCardPlayContext
#include "BGCardWidget.generated.h"

class UTextBlock;
class URichTextBlock;
class UBorder;
class UImage;
class UTexture2D;
class UBGHandWidget;
class UMaterialInstanceDynamic;
class UCurveFloat;

/**
 * 판정 등급 하나에 대한 글로우 연출값.
 *
 * ⚠️ 색만으로 등급을 가르지 말 것. 색각이상(남성 약 8%)에서 빨강·노랑·주황이 한 덩어리로
 *    뭉쳐서 대실패·성공·대성공이 구분되지 않는다. Seconds 와 Intensity 를 등급마다 다르게
 *    줘서 **밝기와 속도로도** 읽히게 한다 — 대성공은 세고 길게, 실패는 약하고 짧게.
 */
USTRUCT(BlueprintType)
struct FBGCardTierFX
{
	GENERATED_BODY()

	/** 이 등급의 글로우 색. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Card|TierFX")
	FLinearColor Color = FLinearColor::White;

	/** 터지는 순간의 세기. 여기서 평상시 값으로 잦아든다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Card|TierFX", meta = (ClampMin = "0.0"))
	float Intensity = 3.f;

	/** 테두리 링의 세기. 머테리얼에 링 파라미터가 없으면 무시된다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Card|TierFX", meta = (ClampMin = "0.0"))
	float RingStrength = 1.f;

	/**
	 * 잦아드는 데 걸리는 시간(초).
	 *
	 * 실패 계열을 길게 끌지 말 것 — 실패는 빨리 넘어가야 답답하지 않다.
	 * 판이 뒤집히는 대성공·대실패만 길게 준다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Card|TierFX", meta = (ClampMin = "0.01"))
	float Seconds = 0.6f;
};

/**
 * 카드 위젯의 연출 상태.
 *
 * ⚠️ 게임 상태가 아니라 화면 상태다 — 복제하지 않고 서버는 알 필요도 없다.
 *    손패에서 빠진 카드도 Exiting 동안은 화면에 남는다(게임 상태와 시간축이 다르다).
 */
UENUM(BlueprintType)
enum class EBGCardMotionState : uint8
{
	Idle       UMETA(DisplayName = "정지"),      // 목표에 도달했다. 더 굴릴 것이 없다.
	Pending    UMETA(DisplayName = "등장 대기"),  // stagger 대기 중. 아직 숨어 있다.
	Dealing    UMETA(DisplayName = "덱에서 비행"), // 덱 → 손패. 곡선 · 회전 · 오버슈트.
	Moving     UMETA(DisplayName = "이동 중"),    // 손패 자리를 향해 보간 중.

	// ── 사용한 카드가 손패를 떠나는 3단계 ──
	//
	//	낸 즉시 사라지면 "무슨 카드를 냈는지" 를 볼 시간이 없다. 중앙으로 올려
	//	보여 준 뒤(Presenting) 효과가 적용되는 동안 머물고(Dwelling) 사라진다(Exiting).
	Presenting UMETA(DisplayName = "중앙으로"),   // 손패 → 플레이 존 중앙
	Dwelling   UMETA(DisplayName = "중앙 대기"),  // 효과가 보이는 동안 머문다
	Exiting    UMETA(DisplayName = "퇴장 중")     // 끝나면 손패 위젯이 파괴한다.
};

/**
 * 카드 1장 위젯. 이름 + 설명을 표시하고, 좌클릭 드래그를 시작한다.
 *
 * WBP 서브클래스에 아래 이름의 위젯이 있어야 한다:
 *   Text_CardName        (TextBlock, 필수)
 *   Text_CardDescription (TextBlock, 필수)
 *   Border_Root          (Border, 선택 - 드래그 중 디밍에 사용)
 *   Text_ChanceModified   (TextBlock, 선택 - 호버 중 "확률 보정중" 문구)
 *   Text_ChanceCriticalFailure  (RichTextBlock, 선택 - 대실패)
 *   Text_ChanceFailure          (RichTextBlock, 선택 - 실패)
 *   Text_ChanceSuccess          (RichTextBlock, 선택 - 성공)
 *   Text_ChanceCriticalSuccess  (RichTextBlock, 선택 - 대성공)
 *
 * 확률 텍스트는 둘 다 선택이다. WBP 에 없으면 그냥 표시되지 않는다 —
 * 기존 카드 WBP 를 고치지 않아도 컴파일과 실행이 그대로 유지되어야 하기 때문이다.
 */
UCLASS(Abstract, Blueprintable)
class BG_API UBGCardWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "BG|Card")
	void SetCardView(const FBGCardView& InCardView);

	/**
	 * 판정 등급 연출을 터뜨린다. 판정이 확정되는 순간 자동으로 불린다.
	 *
	 * BlueprintCallable 인 이유는 튜닝 때문이다 — 판을 돌리지 않고 BP 에서 등급을 골라
	 * 바로 재생해 볼 수 있어야 색·세기를 맞출 수 있다.
	 *
	 * 등급이 None 이면(장비처럼 판정하지 않는 카드) 아무 일도 하지 않는다.
	 */
	UFUNCTION(BlueprintCallable, Category = "BG|Card|TierFX")
	void PlayTierFX(EBGResolutionTier Tier);

	/**
	 * 중앙 대기를 지금 끝낸다 — 카드가 퇴장을 시작한다.
	 *
	 * bWaitForDwellAnimation 을 켰을 때 **BP 가 반드시 불러야 하는 신호**다.
	 * UMG 애니메이션의 Finished 이벤트에 물려 두면 연출 길이가 애셋을 따라간다.
	 *
	 * 대기 중이 아니면 아무 일도 하지 않는다 — 여러 번 불러도 안전하다.
	 */
	UFUNCTION(BlueprintCallable, Category = "BG|Card|Motion")
	void FinishDwell();

	UFUNCTION(BlueprintPure, Category = "BG|Card")
	const FBGCardView& GetCardView() const { return CardView; }

	UFUNCTION(BlueprintPure, Category = "BG|Card")
	int32 GetInstanceId() const { return CardView.InstanceId; }

	/** 이 카드를 소유한 핸드 위젯. 핸드가 카드를 만들 때 설정한다. */
	// 정의는 .cpp 에 있다 - TWeakObjectPtr 대입은 완전한 타입을 요구한다.
	void SetOwningHand(UBGHandWidget* InHand);

	/** 드래그 중인 원본 카드를 흐리게 표시한다. 드롭/취소 시 false 로 복구된다. */
	UFUNCTION(BlueprintCallable, Category = "BG|Card")
	void SetBeingDragged(bool bInBeingDragged);

	/**
	 * 드래그를 잠근다. 대상 좌석 선택을 기다리는 동안 카드를 또 던지는 것을 막는다.
	 * 잠금은 손패 위젯이 일괄로 건다 — 개별 카드에서 직접 부르지 말 것.
	 */
	UFUNCTION(BlueprintCallable, Category = "BG|Card")
	void SetDragLocked(bool bInLocked);

	UFUNCTION(BlueprintPure, Category = "BG|Card")
	bool IsDragLocked() const { return bDragLocked; }

	/**
	 * true 면 드래그 비주얼용 복제본이므로 입력을 받지 않는다.
	 * 복제본이 다시 드래그를 시작하는 무한 재귀를 막는다.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "BG|Card")
	bool bIsDragVisual = false;

	// ══════════════════════════════════════════════════════════════════
	//  연출 (Presentation)
	//
	//  ⚠️ 이 구간은 전부 클라 로컬이다. 게임 규칙과 무관하므로 연출을 통째로
	//     들어내도 게임은 그대로 굴러가야 한다.
	//
	//  틱은 이 위젯이 스스로 돌지 않는다 — 손패 위젯이 TickMotion() 을 불러준다.
	//  카드가 각자 틱하면 순서가 보장되지 않고, 손패가 레이아웃을 다시 잡는
	//  프레임에 어떤 카드는 옛 목표로 한 프레임 움직인다.
	// ══════════════════════════════════════════════════════════════════
public:
	/**
	 * 이 카드가 있어야 할 자리. 손패가 재정렬될 때마다 불린다.
	 *
	 * 도착 전에 목표가 바뀌어도 그 자리에서 이어진다 — 고정 시간 트윈이 아니라
	 * 보간이라서, 카드가 추가돼 자리가 밀리는 도중에도 끊기지 않는다.
	 */
	void SetLayoutTarget(const FVector2D& InPosition, float InAngleDegrees);

	/** 보간 없이 즉시 목표로. 첫 프레임(패널 크기를 아직 모를 때)에만 쓴다. */
	void SnapToLayoutTarget();

	/**
	 * 현재 위치를 강제로 옮긴다(목표는 건드리지 않는다).
	 *
	 * 드롭 순간에 쓴다 — 커서를 놓은 자리에서 연출을 이어받아야 드래그와 끊기지 않는다.
	 * 이걸 안 하면 카드가 손패 자리로 돌아갔다가 다시 중앙으로 날아간다.
	 */
	void SetMotionPosition(const FVector2D& InPosition);

	/**
	 * 덱에서 날아오는 등장 연출을 시작한다.
	 * Delay 동안은 숨어 있다가 나타난다 — 여러 장을 동시에 뽑을 때 겹쳐 보이지 않게 한다.
	 */
	void BeginDeal(const FVector2D& FromPosition, float Delay, const FBGCardDealContext& Context);

	/**
	 * 곧장 사라지는 퇴장. 남이 훔쳐 갔거나 버려진 카드처럼 "보여 줄 필요가 없는" 경우다.
	 *
	 * ⚠️ 보간이 아니라 고정 시간이다 — 퇴장은 반드시 끝나야 하는데
	 *    InterpTo 는 목표에 정확히 도달하지 않아 영원히 안 끝난다.
	 */
	void BeginExit(const FVector2D& ToPosition);

	/**
	 * 내가 사용한 카드의 퇴장. 중앙으로 올라가 **잡혀 있다가** 풀리면 사라진다.
	 *
	 * 중앙에 도착하면 ReleaseHold() 가 올 때까지 가만히 기다린다 —
	 * 대상 좌석을 고르는 동안 카드가 중앙에 서 있어야 무엇을 내고 있는지 보인다.
	 * 서버가 손패에서 카드를 빼면 그것이 곧 "효과가 발동했다" 는 신호이고,
	 * 그때 ReleaseHold() 가 불려 잠깐 더 보여 준 뒤 사라진다.
	 */
	void BeginPlayExit(const FVector2D& CenterPosition, const FVector2D& ToPosition);

	/**
	 * 중앙에 잡아 두던 카드를 놓아 준다. DwellDuration 만큼 더 보여 준 뒤 사라진다.
	 * 여기서 OnPlayResolved 가 발사된다 — 그 순간이 곧 "효과가 발동했다" 이다.
	 *
	 * 부르는 쪽이 모르는 칸(연출 원형)은 **카드가 스스로 채운다** — 자기 데이터니까.
	 * 채워진 맥락을 돌려주므로, 손패는 그것을 그대로 관찰자에게 흘려보내면 된다.
	 */
	FBGCardPlayContext ReleaseHold(const FBGCardPlayContext& Context);

	/** 중앙에 잡혀 대기 중인가(아직 확정되지 않았다). */
	UFUNCTION(BlueprintPure, Category = "BG|Card|Motion")
	bool IsHoldingAtCenter() const { return bHoldingAtCenter; }

	/**
	 * 떠나던 카드를 손패로 되돌린다(대상 선택 취소 등).
	 * 이미 사라지기 시작했으면(Exiting) 되돌리지 않고 false 를 돌려준다.
	 */
	bool ReturnToHand();

	UFUNCTION(BlueprintPure, Category = "BG|Card|Motion")
	EBGCardMotionState GetMotionState() const { return MotionState; }

	/** 손패를 떠나는 중인가(중앙 이동 · 중앙 대기 · 퇴장 전부 포함). */
	UFUNCTION(BlueprintPure, Category = "BG|Card|Motion")
	bool IsLeavingHand() const;

	/** 퇴장 연출이 끝났다. 손패 위젯이 이걸 보고 파괴한다. */
	bool IsExitFinished() const;

	/** 손패 위젯이 매 프레임 불러준다. */
	void TickMotion(float DeltaTime);

	/** 아직 덱에서 날아오는 중인가(대기 포함). 순차 배분과 Z 순서 판단에 쓴다. */
	UFUNCTION(BlueprintPure, Category = "BG|Card|Motion")
	bool IsDealInProgress() const { return bDealInProgress; }

	/** 지금 마우스가 올라와 연출이 걸려 있는가. 손패가 Z 순서를 올릴 때 쓴다. */
	UFUNCTION(BlueprintPure, Category = "BG|Card|Motion")
	bool IsHoverActive() const;

	/** 한 장을 배분하는 데 걸리는 시간. 손패가 순차 배분 지연을 계산할 때 읽는다. */
	float GetDealFlightDuration() const { return DealFlightDuration; }

	/**
	 * 이번 묶음의 총 장수를 뒤늦게 채운다.
	 * BeginDeal 시점에는 몇 장이 들어올지 아직 모른다 — 손패가 다 훑은 뒤에 알려 준다.
	 */
	void SetDealBatchTotal(int32 InTotal) { DealContext.TotalInBatch = InTotal; }

	UFUNCTION(BlueprintPure, Category = "BG|Card|Motion")
	const FBGCardDealContext& GetDealContext() const { return DealContext; }

	// ── BP 훅 ────────────────────────────────────────────────────────
	//
	//	C++ 은 "언제 / 어디로" 만 정하고, "어떻게 보이는가" 는 BP 가 붙인다.
	//	이 경계를 지키면 연출을 바꿀 때 C++ 재컴파일이 필요 없다
	//	(BGResolutionWidget::OnResolutionShown 과 같은 규약).

	/** stagger 대기가 끝나 카드가 실제로 나타나는 순간. 등장 애니메이션·사운드를 여기에. */
	UFUNCTION(BlueprintImplementableEvent, Category = "BG|Card|Motion")
	void OnDealt(const FBGCardDealContext& Context);

	/**
	 * 사용한 카드가 중앙에 도착한 순간. 아직 확정 전이라 여기서 대기가 시작된다.
	 * "이 카드를 내는 중" 을 알리는 연출(테두리 강조 등)을 붙이면 된다.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "BG|Card|Motion")
	void OnPlayPresented();

	/**
	 * 서버가 카드를 확정해 효과가 발동한 순간(대기 해제).
	 * 발동 연출(빛·파티클·화면 흔들림)은 여기에 붙이는 것이 맞다.
	 *
	 * Context.Tier 로 등급별 분기, Context.Archetype 으로 카드 성격별 분기를 한다.
	 * ⚠️ 장비는 판정이 없어 Tier 가 None 이다 — 분기할 때 반드시 처리할 것.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "BG|Card|Motion")
	void OnPlayResolved(const FBGCardPlayContext& Context);

	/**
	 * 마우스가 올라와 카드가 들리기 시작한 순간 / 내려놓인 순간.
	 *
	 * ⚠️ 마우스가 올라왔다고 항상 발사되지 않는다. 손패가 잠겼거나(내 턴이 아님)
	 *    드래그·등장·퇴장 중이면 호버 연출 자체가 억제되므로 bActive 는 false 다.
	 *    BP 에서 On Mouse Enter 를 직접 쓰면 상대 턴에도 빛나므로 이쪽을 쓸 것.
	 *
	 * 상태가 **바뀔 때만** 온다 — 매 프레임 오지 않는다.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "BG|Card|Motion")
	void OnHoverChanged(bool bActive);

	/** 손패로 되돌아가는 순간(대상 선택 취소 등). */
	UFUNCTION(BlueprintImplementableEvent, Category = "BG|Card|Motion")
	void OnReturnedToHand();

	/** 실제로 사라지기 시작하는 순간. 퇴장 애니메이션·사운드를 여기에. */
	UFUNCTION(BlueprintImplementableEvent, Category = "BG|Card|Motion")
	void OnExitStarted();

protected:
	virtual void NativePreConstruct() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;

	// ── 호버 (하스스톤/슬더스 식 손패 반응) ──────────────────────────
	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	/** 표시할 카드 데이터. 디자이너에서 값을 넣으면 프리뷰에 그대로 반영된다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Card", meta = (ExposeOnSpawn = true))
	FBGCardView CardView;

	/** 비워두면 자기 자신의 클래스로 드래그 비주얼을 만든다. */
	UPROPERTY(EditAnywhere, Category = "BG|Card")
	TSubclassOf<UBGCardWidget> DragVisualClass;

	UPROPERTY(EditAnywhere, Category = "BG|Card")
	FLinearColor NormalTint = FLinearColor::White;

	UPROPERTY(EditAnywhere, Category = "BG|Card")
	FLinearColor DraggedTint = FLinearColor(1.f, 1.f, 1.f, 0.25f);

	/** 사용할 수 없는 카드(bPlayable == false)의 색조. */
	UPROPERTY(EditAnywhere, Category = "BG|Card")
	FLinearColor UnplayableTint = FLinearColor(0.4f, 0.4f, 0.4f, 1.f);

	/** 대상 선택 대기 등으로 손패 전체가 잠긴 상태의 색조. */
	UPROPERTY(EditAnywhere, Category = "BG|Card")
	FLinearColor LockedTint = FLinearColor(0.3f, 0.3f, 0.35f, 1.f);

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_CardName;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_CardDescription;

	UPROPERTY(BlueprintReadWrite,meta = (BindWidgetOptional))
	TObjectPtr<UBorder> Border_Root;

	/**
	 * 테두리 안쪽 그림. WBP 의 Overlay 에서 테두리 **아래** 칸에 둔다.
	 *
	 * Optional 인 이유: 이 클래스는 손패 카드와 드래그 비주얼이 함께 쓴다(BeginDrag 참고).
	 * 필수로 걸면 그림 칸이 없는 WBP 가 전부 바인딩 에러로 죽는다.
	 */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Img_Art;

	/**
	 * DT 에 그림이 없는 카드에 대신 그릴 이미지.
	 *
	 * 비워 두면 그림 칸을 Hidden 으로 접는다 — 브러시를 null 로 두면 카드 한복판이
	 * 투명해져서 "카드가 안 보인다" 로 시간을 버린다. 아트 작업 중에도 카드 모양은 유지돼야 한다.
	 */
	UPROPERTY(EditAnywhere, Category = "BG|Card")
	TObjectPtr<UTexture2D> FallbackArtTexture;

	// ── 등급 연출 (글로우 머테리얼) ──────────────────────────────────
	//
	//	브러시에 머테리얼이 물린 이미지를 잡아 **이 카드 전용 DMI** 로 바꿔 쓴다.
	//
	//	⚠️ DMI 없이 MI 를 그대로 쓰면 손패 전원이 같은 인스턴스를 공유한다 —
	//	   한 장이 대성공 나면 손패가 통째로 번쩍인다.

	/**
	 * C++ 글로우로 등급을 표현할지. **기본은 꺼짐** — 등급 연출은 WBP 의 UMG 애니메이션이 맡는다.
	 *
	 * 코드를 지우지 않고 스위치로 둔 이유는 §13.1 비활성 정책과 같다. 애니메이션으로
	 * 원하는 그림이 안 나오면 켜서 같이 쓰거나, 애니메이션 대신 되돌릴 수 있어야 한다.
	 * 꺼져 있으면 PlayTierFX 가 즉시 빠져나가므로 DMI 도 만들지 않는다(비용 0).
	 */
	UPROPERTY(EditAnywhere, Category = "BG|Card|TierFX")
	bool bUseTierGlow = false;

	/** 글로우 머테리얼이 물려 있는 이미지. 없으면 등급 연출만 조용히 꺼진다. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Img_Glow;

	//	파라미터 이름을 값으로 두는 이유: 이 위젯에 어떤 머테리얼이 물릴지 코드가 모른다.
	//	M_BGCardGlow 면 아래 기본값이 맞고, M_BGCardFX 면 RimColor / RimIntensity 로 바꾸면 된다.
	//	이름이 틀리면 UMaterialInstanceDynamic 이 조용히 무시하므로 BP 에서 확인할 것.
	UPROPERTY(EditAnywhere, Category = "BG|Card|TierFX")
	FName ColorParamName = TEXT("GlowColor");

	UPROPERTY(EditAnywhere, Category = "BG|Card|TierFX")
	FName IntensityParamName = TEXT("GlowIntensity");

	UPROPERTY(EditAnywhere, Category = "BG|Card|TierFX")
	FName RingParamName = TEXT("GlowRingStrength");

	/**
	 * 위젯 크기를 받는 파라미터. SDF 글로우가 테두리 두께를 균일하게 그리려면 필요하다.
	 * 안 먹이면 카드 비율이 바뀔 때 글로우가 찌그러진다. 없는 머테리얼이면 그냥 무시된다.
	 */
	UPROPERTY(EditAnywhere, Category = "BG|Card|TierFX")
	FName WidgetSizeParamName = TEXT("WidgetSize");

	/**
	 * 잦아드는 곡선(가로 0~1 → 세로 1~0). 비우면 선형으로 잦아든다.
	 *
	 * 커브를 애셋으로 빼 두면 아티스트가 코드 빌드 없이 감쇠를 고칠 수 있다.
	 * 앞부분을 평평하게 만들면 "확 터졌다가 천천히" 가 된다.
	 */
	UPROPERTY(EditAnywhere, Category = "BG|Card|TierFX")
	TObjectPtr<UCurveFloat> TierFXCurve;

	UPROPERTY(EditAnywhere, Category = "BG|Card|TierFX")
	FBGCardTierFX CriticalFailureFX = { FLinearColor(0.85f, 0.15f, 0.15f), 3.5f, 1.4f, 0.9f };

	UPROPERTY(EditAnywhere, Category = "BG|Card|TierFX")
	FBGCardTierFX FailureFX = { FLinearColor(0.63f, 0.63f, 0.63f), 1.2f, 0.4f, 0.35f };

	UPROPERTY(EditAnywhere, Category = "BG|Card|TierFX")
	FBGCardTierFX SuccessFX = { FLinearColor(1.f, 0.92f, 0.23f), 2.2f, 0.9f, 0.5f };

	UPROPERTY(EditAnywhere, Category = "BG|Card|TierFX")
	FBGCardTierFX CriticalSuccessFX = { FLinearColor(1.f, 0.55f, 0.f), 4.5f, 2.f, 1.1f };

	//	보정이 걸렸다는 것만 알리는 문구. 카드 맨 위층에 두고 호버 중에만 띄운다 —
	//	손패 전체에 항상 떠 있으면 정작 확률 숫자를 읽는 데 방해가 된다.
	//	문구와 서식은 WBP 가 정한다. 여기서는 보이고 숨기는 것만 한다.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_ChanceModified;

	// 4단계 상세 확률 (GDD §10)

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<URichTextBlock> Text_ChanceCriticalFailure;

	/** 보정이 걸린 확률에 쓰는 색. 장비 효과가 붙었다는 것을 한눈에 보이게 한다. */
	UPROPERTY(EditAnywhere, Category = "BG|Card")
	FLinearColor ModifiedChanceColor = FLinearColor(0.45f, 0.85f, 1.f, 1.f);
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<URichTextBlock> Text_ChanceFailure;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<URichTextBlock> Text_ChanceSuccess;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<URichTextBlock> Text_ChanceCriticalSuccess;

	UPROPERTY(EditAnywhere, Category = "BG|Card")
	FLinearColor NormalChanceColor = FLinearColor::White;

	// ── 연출 튜닝 ────────────────────────────────────────────────────
	//
	//	보간 속도는 "초당 남은 거리의 비율" 이다(FInterpTo 규약).
	//	값이 클수록 빠르고 딱딱하며, 작을수록 느리고 물렁하다. 10~20 이 무난하다.

	UPROPERTY(EditAnywhere, Category = "BG|Card|Motion", meta = (ClampMin = "0.1"))
	float MoveInterpSpeed = 14.f;

	UPROPERTY(EditAnywhere, Category = "BG|Card|Motion", meta = (ClampMin = "0.1"))
	float AngleInterpSpeed = 14.f;

	UPROPERTY(EditAnywhere, Category = "BG|Card|Motion", meta = (ClampMin = "0.1"))
	float ScaleInterpSpeed = 12.f;

	// ── 덱에서 뽑히는 비행 (Slay the Spire 류의 과장된 등장) ──────────
	//
	//	세 가지가 겹쳐야 "뽑았다" 는 느낌이 난다:
	//	  1) 직선이 아니라 활처럼 휘는 궤적   → DealArcHeight
	//	  2) 기울어 있다가 제자리에서 펴짐     → DealSpinDegrees
	//	  3) 작게 출발해 살짝 지나쳤다 안착   → DealStartScale + DealScaleOvershoot
	//
	//	⚠️ 이 단계는 고정 시간이다. 보간(InterpTo)으로는 궤적을 그릴 수 없다 —
	//	   진행도(0~1)를 알아야 곡선·회전·오버슈트를 계산할 수 있다.

	/** 덱에서 손패까지 날아오는 시간(초). 0 이면 비행을 건너뛰고 곧장 보간으로 간다. */
	UPROPERTY(EditAnywhere, Category = "BG|Card|Motion|Deal", meta = (ClampMin = "0"))
	float DealFlightDuration = 0.2f;

	/**
	 * 궤적이 휘는 정도(픽셀). 0 이면 직선.
	 * 진행 방향의 수직으로, 항상 위로 볼록하게 휜다.
	 */
	UPROPERTY(EditAnywhere, Category = "BG|Card|Motion|Deal", meta = (ClampMin = "0"))
	float DealArcHeight = 170.f;

	/**
	 * 출발할 때의 기울기(도). 날아오면서 제자리 각도로 펴진다.
	 * -360 이나 360 을 주면 한 바퀴 도는 등장이 된다.
	 */
	UPROPERTY(EditAnywhere, Category = "BG|Card|Motion|Deal")
	float DealSpinDegrees = -160.f;

	/**
	 * 안착 직전에 제 크기를 얼마나 지나치는가. 0 이면 오버슈트 없음.
	 * 1.7 이 고전적인 값이고, 3 을 넘기면 눈에 띄게 튀어오른다.
	 */
	UPROPERTY(EditAnywhere, Category = "BG|Card|Motion|Deal", meta = (ClampMin = "0"))
	float DealScaleOvershoot = 3.f;

	/** 위치 감속 곡선의 지수. 클수록 초반이 빠르고 끝이 부드럽다. */
	UPROPERTY(EditAnywhere, Category = "BG|Card|Motion|Deal", meta = (ClampMin = "1"))
	float DealEaseExponent = 2.5f;

	/**
	 * 비행 시작에 서서히 나타나는 시간(초). 0 이면 처음부터 불투명하다.
	 *
	 * 덱 앵커가 화면 안이면 카드가 덱 위에서 불쑥 튀어나오는 것처럼 보인다 —
	 * 짧게라도 넣으면 "덱에서 뽑혀 나온다" 로 읽힌다.
	 */
	UPROPERTY(EditAnywhere, Category = "BG|Card|Motion|Deal", meta = (ClampMin = "0"))
	float DealFadeInDuration = 0.12f;

	/** 덱에서 출발할 때의 크기. 1 보다 작으면 "멀리서 다가오는" 느낌이 난다. */
	UPROPERTY(EditAnywhere, Category = "BG|Card|Motion", meta = (ClampMin = "0.01"))
	float DealStartScale = 0.35f;

	// ── 관성 기울기 (secondary motion) ───────────────────────────────
	//
	//	주 움직임에 자동으로 따라붙는 부수 효과다. 카드가 이동하는 방향으로 살짝 눕는다 —
	//	직접 키를 찍지 않아도 "무게가 있는 물체" 처럼 보이게 만드는 가장 싼 방법이다.

	/** 가로 속도 1px/s 당 기울어지는 각도. 0 이면 관성 기울기를 끈다. */
	UPROPERTY(EditAnywhere, Category = "BG|Card|Motion|Inertia")
	float VelocityTiltPerSpeed = 0.018f;

	/** 관성 기울기의 상한(도). 빠르게 움직여도 이 이상 눕지 않는다. */
	UPROPERTY(EditAnywhere, Category = "BG|Card|Motion|Inertia", meta = (ClampMin = "0"))
	float MaxVelocityTilt = 16.f;

	/** 기울기가 붙고 풀리는 속도. 낮을수록 흐느적거린다. */
	UPROPERTY(EditAnywhere, Category = "BG|Card|Motion|Inertia", meta = (ClampMin = "0.1"))
	float VelocityTiltInterpSpeed = 9.f;

	// ── 호버 ─────────────────────────────────────────────────────────
	//
	//	⚠️ 이 값들은 루트의 렌더 트랜스폼을 쓴다. WBP 에서 루트에 호버 애니메이션을
	//	   걸면 서로 덮어쓴다 — WBP 쪽 연출은 자식 컨테이너에 걸 것.

	/** 마우스가 올라갔을 때의 크기 배율. */
	UPROPERTY(EditAnywhere, Category = "BG|Card|Motion|Hover", meta = (ClampMin = "0.01"))
	float HoverScale = 1.18f;

	/** 마우스가 올라갔을 때 떠오르는 높이(픽셀). 음수가 위쪽이다. */
	UPROPERTY(EditAnywhere, Category = "BG|Card|Motion|Hover")
	float HoverLift = -34.f;

	/** 커서가 카드 좌우 끝에 있을 때의 기울기(도). 커서를 따라 기운다. */
	UPROPERTY(EditAnywhere, Category = "BG|Card|Motion|Hover")
	float HoverTiltMax = 9.f;

	/**
	 * 커서 위치에 따른 비틀림(shear). 2D 위젯에 입체감을 주는 값이다 —
	 * 카드가 커서 쪽으로 기울어 누운 것처럼 보인다. 0 이면 끈다.
	 */
	UPROPERTY(EditAnywhere, Category = "BG|Card|Motion|Hover")
	float HoverShearMax = 5.f;

	/** 호버 연출이 붙고 풀리는 속도. */
	UPROPERTY(EditAnywhere, Category = "BG|Card|Motion|Hover", meta = (ClampMin = "0.1"))
	float HoverInterpSpeed = 15.f;

	/** 퇴장할 때 줄어드는 최종 크기. */
	UPROPERTY(EditAnywhere, Category = "BG|Card|Motion", meta = (ClampMin = "0.01"))
	float ExitEndScale = 0.65f;

	/** 퇴장에 걸리는 시간(초). 이 시간이 지나면 위젯이 파괴된다. */
	UPROPERTY(EditAnywhere, Category = "BG|Card|Motion", meta = (ClampMin = "0.01"))
	float ExitDuration = 0.35f;

	// ── 사용한 카드의 중앙 연출 ──────────────────────────────────────

	/** 손패에서 중앙까지 올라가는 시간(초). */
	UPROPERTY(EditAnywhere, Category = "BG|Card|Motion|Play", meta = (ClampMin = "0.01"))
	float PresentDuration = 0.25f;

	/** 중앙에 있을 때의 크기. 1 보다 크면 앞으로 튀어나온 느낌이 난다. */
	UPROPERTY(EditAnywhere, Category = "BG|Card|Motion|Play", meta = (ClampMin = "0.01"))
	float PresentScale = 1.25f;

	/**
	 * 효과가 발동된 뒤(ReleaseHold) 중앙에서 더 머무는 시간(초).
	 *
	 * 판정 결과 위젯의 표시 시간과 맞추는 것이 좋다 — 카드가 먼저 사라지면
	 * "무엇의 결과인지" 를 놓친다.
	 *
	 * ⚠️ 대상을 고르는 동안의 대기는 여기에 포함되지 않는다. 그쪽은 시간이 아니라
	 *    ReleaseHold 가 올 때까지 무한히 기다린다.
	 *
	 * ⚠️ bWaitForDwellAnimation 을 켜면 이 값은 **안전 상한**으로만 쓰인다.
	 */
	UPROPERTY(EditAnywhere, Category = "BG|Card|Motion|Play", meta = (ClampMin = "0"))
	float DwellDuration = 2.2f;

	/**
	 * 중앙 대기의 길이를 **UMG 애니메이션이 정하게** 한다.
	 *
	 * 켜면 DwellRemaining 이 저절로 줄지 않고, BP 가 FinishDwell() 을 부를 때까지 머문다.
	 * 애니메이션 끝 이벤트에 FinishDwell 을 물리면 연출 길이를 애셋에서 잡을 수 있다 —
	 * 길이를 바꿀 때마다 C++ 값을 따라 고칠 필요가 없어진다.
	 *
	 * ⚠️ 그래도 DwellDuration 이 상한으로 계속 돈다. BP 가 FinishDwell 을 빠뜨리거나
	 *    애니메이션이 없으면 카드가 중앙에 영원히 박혀서 손패가 잠기기 때문이다 —
	 *    상한이 없으면 그 상태에서 아무 에러도 안 난다.
	 */
	UPROPERTY(EditAnywhere, Category = "BG|Card|Motion|Play")
	bool bWaitForDwellAnimation = false;

private:
	void RefreshVisual();

	/** 현재 보간값을 실제 슬롯/렌더 트랜스폼에 적용한다. */
	void ApplyMotionTransform();

	/** 지금 위치에서 사라지기 시작한다. 곧장 퇴장과 중앙 대기 후 퇴장이 여기서 만난다. */
	void EnterExitPhase(const FVector2D& ToPosition);

	/**
	 * 주 움직임에 얹히는 부수 효과 — 관성 기울기와 호버.
	 * 정지 상태에서도 굴러야 기울기가 0 으로 되돌아온다.
	 */
	void TickSecondaryMotion(float DeltaTime, const FVector2D& PositionBeforeTick);

	/** 등급별 확률 숫자를 채운다. 판정하지 않는 카드면 숨긴다. */
	void RefreshChanceText();

	/** "확률 보정중" 문구를 보이고 숨긴다. 보정이 걸렸고 호버 중일 때만 보인다. */
	void RefreshChanceModifiedBanner();

	/** 테두리 안쪽 그림을 채운다. 뷰에 그림이 없으면 폴백으로, 그것도 없으면 칸을 접는다. */
	void RefreshArt();

	// ── 등급 연출 ────────────────────────────────────────────────────

	/**
	 * 이 카드 전용 DMI 를 얻는다(처음 부를 때 만든다).
	 * 브러시에 머테리얼이 없으면(그냥 텍스처면) nullptr — 등급 연출은 조용히 꺼진다.
	 */
	UMaterialInstanceDynamic* GetGlowMID();

	/** 등급별 연출값을 고른다. 등급이 없으면(장비 등) nullptr. */
	const FBGCardTierFX* FindTierFX(EBGResolutionTier Tier) const;

	/**
	 * 잦아드는 진행도(1 = 방금 터짐, 0 = 평상시)를 머테리얼에 반영한다.
	 * 평상시 값은 DMI 를 만들 때 붙잡아 둔 것으로 되돌아간다 — 0 으로 밀어 버리면
	 * 판정 한 번 뒤에 카드 글로우가 영영 꺼진다.
	 */
	void ApplyTierFX(float Shape);

	/** 감쇠를 굴린다. TickMotion 이 매 프레임 부른다. */
	void TickTierFX(float DeltaTime);

	// ── 등급 연출 런타임 상태 ────────────────────────────────────────

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> GlowMID;

	//	DMI 를 만든 시점의 값 — 연출이 끝나면 여기로 돌아간다.
	//	0 으로 되돌리면 아티스트가 잡아 둔 평상시 글로우까지 죽는다.
	FLinearColor RestColor = FLinearColor::White;
	float RestIntensity = 0.f;
	float RestRing = 0.f;
	bool bRestCaptured = false;

	FBGCardTierFX ActiveTierFX;
	float TierFXElapsed = 0.f;
	bool bTierFXActive = false;

	UPROPERTY()
	TWeakObjectPtr<UBGHandWidget> OwningHand;

	bool bBeingDragged = false;
	bool bDragLocked = false;

	//	직전 프레임의 호버 상태. 바뀌는 순간에만 BP 훅을 쏘기 위한 것이다 —
	//	매 프레임 쏘면 BP 에서 애니메이션이 계속 처음부터 다시 재생된다.
	bool bWasHoverActive = false;

	// ── 연출 내부 상태 ──
	EBGCardMotionState MotionState = EBGCardMotionState::Idle;

	FVector2D CurrentPosition = FVector2D::ZeroVector;
	FVector2D TargetPosition  = FVector2D::ZeroVector;
	float CurrentAngle = 0.f;
	float TargetAngle  = 0.f;
	float CurrentScale = 1.f;

	//	Pending 동안 남은 대기 시간.
	float DealDelayRemaining = 0.f;

	//	등장 맥락. 대기가 끝나 실제로 나타나는 시점에 BP 로 넘어간다.
	FBGCardDealContext DealContext;

	//	등장 연출이 자리에 도착할 때까지 true. 스냅으로 연출이 잘리는 것을 막는다.
	bool bDealInProgress = false;

	//	비행 시작점. 목표는 날아오는 중에도 바뀔 수 있어(다른 카드가 추가되면 자리가 밀린다)
	//	매 프레임 TargetPosition 을 다시 읽는다 — 시작점만 고정이다.
	FVector2D DealStartPosition = FVector2D::ZeroVector;
	float DealElapsed = 0.f;

	//	stagger 로 숨기기 전의 가시성. 되돌릴 때 Visible 로 하드코딩하면
	//	WBP 가 정한 히트테스트 설정을 덮어쓴다.
	ESlateVisibility PreDealVisibility = ESlateVisibility::Visible;

	//	퇴장은 고정 시간 보간이라 시작점을 기억해야 한다.
	FVector2D ExitStartPosition = FVector2D::ZeroVector;
	float ExitStartScale = 1.f;
	float ExitElapsed = 0.f;

	// ── 중앙 연출 내부 상태 ──
	FVector2D PresentStartPosition = FVector2D::ZeroVector;
	FVector2D PresentTargetPosition = FVector2D::ZeroVector;
	float PresentStartScale = 1.f;
	float PresentStartAngle = 0.f;
	float PresentElapsed = 0.f;

	//	중앙 대기가 끝난 뒤 날아갈 곳. 대기 중에는 아직 쓰이지 않는다.
	FVector2D PendingExitPosition = FVector2D::ZeroVector;
	float DwellRemaining = 0.f;

	//	true 면 DwellRemaining 이 줄지 않는다 — ReleaseHold 가 올 때까지 무한 대기.
	bool bHoldingAtCenter = false;

	//	떠나기 전의 가시성. 손패로 되돌릴 때 복원한다.
	ESlateVisibility PreLeaveVisibility = ESlateVisibility::Visible;

	// ── 부수 효과 내부 상태 ──
	//	속도는 위치 차분으로 구한다. 별도로 들고 다니면 스냅·순간이동 때 어긋난다.
	float VelocityTilt = 0.f;

	bool bHovered = false;

	//	커서의 카드 안 위치. (-1,-1) 좌상단 ~ (1,1) 우하단.
	FVector2D HoverLocalNormalized = FVector2D::ZeroVector;

	//	호버 연출의 강도 0~1. 마우스가 들고 날 때 이 값이 보간된다.
	float HoverBlend = 0.f;
};
