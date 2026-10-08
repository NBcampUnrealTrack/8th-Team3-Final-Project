// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/View/BGCardView.h"
#include "UI/View/BGCardFXTypes.h"   // FBGCardDealContext / FBGCardPlayContext
#include "UI/Widget/BGCardMotionRuntime.h"   // FBGCardDealRuntime / FBGCardCenterRuntime
#include "BGCardWidget.generated.h"

class UTextBlock;
class URichTextBlock;
class UBorder;
class UImage;
class UTexture2D;
class UBGHandWidget;
class UMaterialInstanceDynamic;
class UCurveFloat;
class URetainerBox;
class UBGCardWidget;

/**
 * 카드가 중앙에서 발동 연출을 터뜨리는 순간. 화면 전체 연출(HUD)이 여기 맞춰 켜진다.
 * BP 훅 OnPlayResolved 와 같은 순간이지만 그쪽은 WBP 서브클래스 전용이라 바깥이 못 쓴다.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBGCardPlayFXStarted,
	UBGCardWidget*, Card, const FBGCardPlayContext&, Context);

/** 사용한 카드가 퇴장(디졸브)을 시작한 순간. 판정 게이지가 이 신호로 굴러가기 시작한다. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBGCardPlayExitStarted, UBGCardWidget*, Card);

/** 판정 등급 하나의 글로우 값. ⚠️ 색만으로 가르지 말 것(색각이상) — Intensity·Seconds 로도 구분되게. */
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

	/** 잦아드는 시간(초). 실패 계열은 짧게, 판이 뒤집히는 대성공·대실패만 길게. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Card|TierFX", meta = (ClampMin = "0.01"))
	float Seconds = 0.6f;
};

/**
 * 카드가 사라지는 방식 — 원형(Archetype)마다 한 벌. 퇴장 중 리테이너 머테리얼 파라미터로 흘러간다.
 *
 * ⚠️ 마스터 머테리얼은 하나로 두고 값만 바꾼다 — 갈아끼우면 DMI 가 새로 생기고 퍼뮤테이션이 늘어난다.
 * ⚠️ 모양은 Static Switch 가 아니라 MaskTexture 로 가른다 — Static Switch 는 DMI 로 안 바뀐다(에러도 안 남).
 */
USTRUCT(BlueprintType)
struct FBGCardDissolveFX
{
	GENERATED_BODY()

	/**
	 * 잘리는 모양 마스크. **이것 하나가 연출의 성격을 정한다.** 검은 곳이 먼저 사라진다.
	 * 칼자국 거리장(Slash) · 불규칙 노이즈(Ember) · 부드러운 셀(Soft). 비우면 머테리얼 기본값.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Card|Dissolve")
	TObjectPtr<UTexture2D> MaskTexture = nullptr;

	/** 타들어가는 경계에 얹히는 색. 불이면 주황, 칼자국이면 흰색. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Card|Dissolve")
	FLinearColor EdgeColor = FLinearColor(1.f, 0.35f, 0.05f, 1.f);

	/** 경계의 두께(0~1 UV). 불은 넓게, 칼자국은 얇게. 0 이면 경계 없이 사라진다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Card|Dissolve", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float EdgeWidth = 0.06f;

	/** 경계 색의 밝기. 1 을 넘겨야 발광으로 읽힌다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Card|Dissolve", meta = (ClampMin = "0.0"))
	float EdgeIntensity = 4.f;

	/** 잘린 두 조각이 벌어지는 정도(UV). 실제로 쪼개는 것이 아니라 컷 라인 기준 UV 밀기다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Card|Dissolve", meta = (ClampMin = "0.0"))
	float SeparateAmount = 0.f;

	/** 잘리는 각도(도). SeparateAmount 가 0 이면 의미가 없다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Card|Dissolve")
	float CutAngleDegrees = 0.f;

	/**
	 * 경계선의 흐림(0~1 UV). 0 에 가까우면 칼로 자른 듯(공격), 크면 뭉근하게(회복).
	 * ⚠️ EdgeWidth(빛나는 띠의 두께)와 다른 축이다 — 같이 키우면 경계가 뭉개진다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Card|Dissolve", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float EdgeSoftness = 0.02f;

	/** 발광이 면 전체로 퍼지는 정도. 0 이면 경계만(공격), 1 이면 남은 면 전체가 빛난다(회복). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Card|Dissolve", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SurfaceGlow = 0.f;

	/** 사라지며 위로 밀려 올라가는 정도(UV). 회복은 조금, 공격은 0(잘린 무게감이 사라진다). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Card|Dissolve", meta = (ClampMin = "0.0"))
	float RiseAmount = 0.f;

	/** 경계에 얹는 재·잔불 노이즈. 회복은 비울 것 — 성스러운 빛에 그을음이 섞이면 탁해진다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Card|Dissolve")
	TObjectPtr<UTexture2D> EmberMask = nullptr;

	/** 위 노이즈의 세기. 0 이면 텍스처를 넣어도 안 보인다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Card|Dissolve", meta = (ClampMin = "0.0"))
	float EmberIntensity = 0.f;

	/** 카드 외곽 후광의 두께(0~1 UV). ⚠️ Fresnel 금지 — UI 도메인엔 뷰 벡터가 없다. UV 비네트로 낼 것. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Card|Dissolve", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HaloWidth = 0.f;

	/** 이 원형 전용 진행 곡선. 비우면 위젯의 DissolveCurve. 공격은 뒤가 가파르고 회복은 고르게. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Card|Dissolve")
	TObjectPtr<UCurveFloat> Curve = nullptr;
};

/** 카드 위젯의 연출 상태. ⚠️ 게임 상태가 아니라 화면 상태다 — 복제하지 않는다. */
UENUM(BlueprintType)
enum class EBGCardMotionState : uint8
{
	Idle       UMETA(DisplayName = "정지"),      // 목표에 도달했다. 더 굴릴 것이 없다.
	Pending    UMETA(DisplayName = "등장 대기"),  // stagger 대기 중. 아직 숨어 있다.
	Dealing    UMETA(DisplayName = "덱에서 비행"), // 덱 → 손패. 곡선 · 회전 · 오버슈트.
	Moving     UMETA(DisplayName = "이동 중"),    // 손패 자리를 향해 보간 중.

	//	사용한 카드가 손패를 떠나는 3단계 — 낸 즉시 사라지면 무슨 카드인지 볼 시간이 없다.
	Presenting UMETA(DisplayName = "중앙으로"),   // 손패 → 플레이 존 중앙
	Dwelling   UMETA(DisplayName = "중앙 대기"),  // 효과가 보이는 동안 머문다
	Exiting    UMETA(DisplayName = "퇴장 중")     // 끝나면 손패 위젯이 파괴한다.
};

/**
 * 카드 1장 위젯. 이름 + 설명을 표시하고, 좌클릭 드래그를 시작한다.
 *
 * WBP 필수: Text_CardName, Text_CardDescription.
 * WBP 선택: Border_Root, Img_Art, Retainer_Card, Text_ChanceModified, Text_Chance*(4단계 확률).
 * 선택 바인딩은 없으면 그냥 표시되지 않는다 — 기존 WBP 를 고치지 않아도 컴파일·실행이 유지된다.
 */
UCLASS(Abstract, Blueprintable)
class BG_API UBGCardWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "BG|Card")
	void SetCardView(const FBGCardView& InCardView);

	/**
	 * 판정 등급 연출을 터뜨린다. 확정 순간 자동으로 불린다. 등급이 None 이면 아무 일도 없다.
	 * BlueprintCallable 인 이유는 튜닝 — 판을 돌리지 않고 등급을 골라 재생해 볼 수 있어야 한다.
	 */
	UFUNCTION(BlueprintCallable, Category = "BG|Card|TierFX")
	void PlayTierFX(EBGResolutionTier Tier);

	/**
	 * 중앙 대기를 지금 끝낸다(퇴장 시작). bWaitForDwellAnimation 을 켰을 때 BP 가 반드시 부를 신호다.
	 * 대기 중이 아니면 아무 일도 하지 않는다 — 여러 번 불러도 안전하다.
	 */
	UFUNCTION(BlueprintCallable, Category = "BG|Card|Motion")
	void FinishDwell();

	/**
	 * 공격·피격 연출이 끝났다 — OnPlayFXFinished 를 지금 보낸다.
	 * 여러 번 불러도, 켠 적이 없어도 안전하다(두 번째부터 아무 일도 없다).
	 */
	UFUNCTION(BlueprintCallable, Category = "BG|Card|Motion")
	void NotifyPlayFXFinished();

	/**
	 * 중앙 대기 자리를 지금 위치에서 새 자리로 옮긴다. 이미 그 자리면 아무 일도 없다.
	 * 대상 선택 중 비켜서 있던 카드를 확정 시점에 중앙으로 데려오는 데 쓴다 —
	 * 안 그러면 낸 사람 화면과 남의 화면에서 카드가 다른 곳에 선다.
	 */
	void MoveHoldTo(const FVector2D& NewCenterPosition);

	UFUNCTION(BlueprintPure, Category = "BG|Card")
	const FBGCardView& GetCardView() const { return CardView; }

	UFUNCTION(BlueprintPure, Category = "BG|Card")
	int32 GetInstanceId() const { return CardView.InstanceId; }

	/** 이 카드를 소유한 핸드 위젯. 정의가 .cpp 인 이유: TWeakObjectPtr 대입은 완전한 타입을 요구한다. */
	void SetOwningHand(UBGHandWidget* InHand);

	/** 드래그 중인 원본 카드를 흐리게 표시한다. 드롭/취소 시 false 로 복구된다. */
	UFUNCTION(BlueprintCallable, Category = "BG|Card")
	void SetBeingDragged(bool bInBeingDragged);

	/** 드래그를 잠근다(대상 선택 대기 중 중복 사용 방지). 손패가 일괄로 건다 — 개별 호출 금지. */
	UFUNCTION(BlueprintCallable, Category = "BG|Card")
	void SetDragLocked(bool bInLocked);

	UFUNCTION(BlueprintPure, Category = "BG|Card")
	bool IsDragLocked() const { return bDragLocked; }

	/** true 면 드래그 비주얼용 복제본이라 입력을 받지 않는다(드래그 무한 재귀 방지). */
	UPROPERTY(BlueprintReadOnly, Category = "BG|Card")
	bool bIsDragVisual = false;

	// ══════════════════════════════════════════════════════════════════
	//  연출 (Presentation)
	//
	//  ⚠️ 전부 클라 로컬이다. 통째로 들어내도 게임은 그대로 굴러가야 한다.
	//  ⚠️ 틱은 스스로 돌지 않는다 — 손패가 TickMotion() 을 불러준다.
	//     각자 틱하면 손패가 재배치하는 프레임에 어떤 카드는 옛 목표로 한 프레임 움직인다.
	// ══════════════════════════════════════════════════════════════════
public:
	/** 이 카드가 있어야 할 자리. 트윈이 아니라 보간이라 도착 전에 목표가 바뀌어도 끊기지 않는다. */
	void SetLayoutTarget(const FVector2D& InPosition, float InAngleDegrees);

	/** 보간 없이 즉시 목표로. 첫 프레임(패널 크기를 아직 모를 때)에만 쓴다. */
	void SnapToLayoutTarget();

	/** 현재 위치만 강제로 옮긴다(목표는 그대로). 드롭 순간에 써서 드래그와 연출을 잇는다. */
	void SetMotionPosition(const FVector2D& InPosition);

	/** 덱에서 날아오는 등장 시작. Delay 동안 숨어 있다 나타난다(여러 장 동시 뽑기 대비). */
	void BeginDeal(const FVector2D& FromPosition, float Delay, const FBGCardDealContext& Context);

	/**
	 * 곧장 사라지는 퇴장(훔쳐감·버려짐 등 보여 줄 필요가 없는 경우).
	 * ⚠️ 보간이 아니라 고정 시간이다 — InterpTo 는 목표에 정확히 도달하지 않아 영원히 안 끝난다.
	 */
	void BeginExit(const FVector2D& ToPosition);

	/**
	 * 내가 사용한 카드의 퇴장. 중앙으로 올라가 ReleaseHold() 가 올 때까지 잡혀 있다가 사라진다.
	 * 서버가 손패에서 카드를 빼는 것이 곧 "효과가 발동했다" 는 신호다.
	 */
	void BeginPlayExit(const FVector2D& CenterPosition, const FVector2D& ToPosition);

	/**
	 * 중앙에 잡아 두던 카드를 놓아 준다(= 효과 발동). 여기서 OnPlayResolved 가 발사된다.
	 * 부르는 쪽이 모르는 칸(연출 원형)은 카드가 스스로 채워 돌려준다 — 자기 데이터니까.
	 */
	FBGCardPlayContext ReleaseHold(const FBGCardPlayContext& Context);

	/** 중앙에 잡혀 대기 중인가(아직 확정되지 않았다). */
	UFUNCTION(BlueprintPure, Category = "BG|Card|Motion")
	bool IsHoldingAtCenter() const { return Center.bHolding; }

	/** 떠나던 카드를 손패로 되돌린다(대상 선택 취소 등). 이미 Exiting 이면 false. */
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
	bool IsDealInProgress() const { return Deal.bInProgress; }

	/** 지금 마우스가 올라와 연출이 걸려 있는가. 손패가 Z 순서를 올릴 때 쓴다. */
	UFUNCTION(BlueprintPure, Category = "BG|Card|Motion")
	bool IsHoverActive() const;

	/** 한 장을 배분하는 데 걸리는 시간. 손패가 순차 배분 지연을 계산할 때 읽는다. */
	float GetDealFlightDuration() const { return DealFlightDuration; }

	/** 이번 묶음의 총 장수를 뒤늦게 채운다 — BeginDeal 시점에는 몇 장인지 아직 모른다. */
	void SetDealBatchTotal(int32 InTotal) { Deal.Context.TotalInBatch = InTotal; }

	UFUNCTION(BlueprintPure, Category = "BG|Card|Motion")
	const FBGCardDealContext& GetDealContext() const { return Deal.Context; }

	// ── BP 훅 ────────────────────────────────────────────────────────
	//
	//	C++ 은 "언제 / 어디로" 만 정하고 "어떻게 보이는가" 는 BP 가 붙인다 — 연출을 바꿔도 재컴파일이 없다.

	/** stagger 대기가 끝나 카드가 실제로 나타나는 순간. 등장 애니메이션·사운드를 여기에. */
	UFUNCTION(BlueprintImplementableEvent, Category = "BG|Card|Motion")
	void OnDealt(const FBGCardDealContext& Context);

	/** 사용한 카드가 중앙에 도착한 순간(아직 확정 전). "내는 중" 연출을 붙인다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "BG|Card|Motion")
	void OnPlayPresented();

	/**
	 * 서버가 확정해 효과가 발동한 순간. 발동 연출(빛·파티클·화면 흔들림)은 여기에.
	 * ⚠️ 장비는 판정이 없어 Tier 가 None 이다 — 등급으로 분기할 때 반드시 처리할 것.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "BG|Card|Motion")
	void OnPlayResolved(const FBGCardPlayContext& Context);

	/**
	 * 호버 연출이 붙고 풀리는 순간. 상태가 **바뀔 때만** 온다(매 프레임 아님).
	 * ⚠️ 잠금·드래그·등장/퇴장 중이면 bActive 는 false 다. BP 의 On Mouse Enter 를
	 *    직접 쓰면 상대 턴에도 빛나므로 이쪽을 쓸 것.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "BG|Card|Motion")
	void OnHoverChanged(bool bActive);

	/** 손패로 되돌아가는 순간(대상 선택 취소 등). */
	UFUNCTION(BlueprintImplementableEvent, Category = "BG|Card|Motion")
	void OnReturnedToHand();

	/** 실제로 사라지기 시작하는 순간. 퇴장 애니메이션·사운드를 여기에. */
	UFUNCTION(BlueprintImplementableEvent, Category = "BG|Card|Motion")
	void OnExitStarted();

	// ── 바깥 구독용 ──────────────────────────────────────────────────
	//	손패가 받아 다시 쏘고 HUD 는 손패만 구독한다 — 카드는 HUD 를 모른다.

	UPROPERTY(BlueprintAssignable, Category = "BG|Card|Motion")
	FOnBGCardPlayFXStarted OnPlayFXStarted;

	/** ⚠️ 사용한 카드만 쏜다. 버려지는 카드까지 쏘면 게이지가 엉뚱한 퇴장에 굴러간다. */
	UPROPERTY(BlueprintAssignable, Category = "BG|Card|Motion")
	FOnBGCardPlayExitStarted OnPlayExitStarted;

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeDestruct() override;
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

	/**
	 * 드래그 중 손패에 남는 실루엣의 불투명도. 0 이면 자리에 구멍이 난다.
	 * ⚠️ DraggedTint 로는 부족하다 — 그 틴트는 Border_Root 에만 걸려 텍스트·아트가 안 흐려진다.
	 */
	UPROPERTY(EditAnywhere, Category = "BG|Card", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DragGhostOpacity = 0.28f;

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

	/** 테두리 안쪽 그림(Overlay 에서 테두리 아래 칸). Optional 인 이유: 드래그 비주얼도 이 클래스를 쓴다. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Img_Art;

	/** DT 에 그림이 없는 카드의 대체 이미지. 비우면 칸을 Hidden 으로 접는다(브러시 null 은 카드가 투명해진다). */
	UPROPERTY(EditAnywhere, Category = "BG|Card")
	TObjectPtr<UTexture2D> FallbackArtTexture;

	/** 대분류별 테두리. 비워 두면 그 타입은 WBP 에 넣어 둔 테두리를 그대로 쓴다. */
	UPROPERTY(EditAnywhere, Category = "BG|Card|Frame")
	TObjectPtr<UTexture2D> EquipmentFrameTexture;

	UPROPERTY(EditAnywhere, Category = "BG|Card|Frame")
	TObjectPtr<UTexture2D> AttackFrameTexture;

	UPROPERTY(EditAnywhere, Category = "BG|Card|Frame")
	TObjectPtr<UTexture2D> MagicFrameTexture;

	/** Border_Root 의 M_BGCardFrame 에서 테두리 그림을 받는 파라미터. 텍스처가 물려 있으면 안 쓰인다. */
	UPROPERTY(EditAnywhere, Category = "BG|Card|Frame")
	FName FrameTexParamName = TEXT("FrameTex");

	// ── 등급 연출 (글로우 머테리얼) ──────────────────────────────────
	//
	//	리테이너의 Effect Material 로 만든 이 카드 전용 DMI(GetCardFXMID)를 디졸브와 함께 쓴다 —
	//	둘 다 합성된 카드 한 장을 대상으로 하는 화면 효과라 마스터 하나에 모으고 값만 다르게 준다.

	/**
	 * C++ 글로우로 등급을 표현할지. **기본은 꺼짐** — 등급 연출은 WBP 의 UMG 애니메이션이 맡는다.
	 * 되돌릴 수 있도록 코드를 지우지 않고 스위치로 뒀다(§13.1 비활성 정책). 꺼져 있으면 DMI 도 안 만든다.
	 */
	UPROPERTY(EditAnywhere, Category = "BG|Card|TierFX")
	bool bUseTierGlow = false;

	//	파라미터 이름을 값으로 두는 이유: 어떤 머테리얼이 물릴지 코드가 모른다.
	//	이름이 틀리면 DMI 가 조용히 무시하므로 안 먹으면 여기부터 볼 것.
	UPROPERTY(EditAnywhere, Category = "BG|Card|TierFX")
	FName ColorParamName = TEXT("GlowColor");

	UPROPERTY(EditAnywhere, Category = "BG|Card|TierFX")
	FName IntensityParamName = TEXT("GlowIntensity");

	UPROPERTY(EditAnywhere, Category = "BG|Card|TierFX")
	FName RingParamName = TEXT("GlowRingStrength");

	/** 리테이너의 픽셀 크기. 팽창 반경을 UV 로 바꾸는 데 필요하다 — 안 먹이면 외곽선이 눌린 타원이 된다. */
	UPROPERTY(EditAnywhere, Category = "BG|Card|TierFX")
	FName WidgetSizeParamName = TEXT("WidgetSize");

	// ── 호버 외곽선 글로우 — 코드가 손대지 않는다 ────────────────────
	//
	//	Border_Root 의 M_BGCardFrame 이 그리고 세기(HoverAmount)는 WBP 의 UMG 애니메이션이 몬다.
	//	⚠️ C++ 에서 HoverAmount 를 도로 밀지 말 것 — 같은 이름을 둘이 매 프레임 쓰면 마지막이 이긴다.
	//	⚠️ 리테이너로 되돌리지 말 것 — 평상시엔 꺼 두는데(SetRetainRendering(false)) 꺼져 있으면
	//	   Effect Material 자체가 적용되지 않아 상시 연출인 호버 글로우는 애초에 안 보인다.
	//	자세한 것은 BG/Docs/M_BGCardFrame-매뉴얼.md 를 볼 것.

	/** 잦아드는 곡선(0~1 → 1~0). 비우면 선형. 애셋으로 빼 둬야 아티스트가 빌드 없이 고친다. */
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

	//	"확률 보정중" 문구. 호버 중에만 띄운다 — 항상 떠 있으면 확률 숫자를 읽는 데 방해된다.
	//	문구와 서식은 WBP 가 정하고, 여기서는 보이고 숨기는 것만 한다.
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
	//	보간 속도는 "초당 남은 거리의 비율"(FInterpTo 규약). 클수록 빠르고 딱딱하다. 10~20 이 무난.

	UPROPERTY(EditAnywhere, Category = "BG|Card|Motion", meta = (ClampMin = "0.1"))
	float MoveInterpSpeed = 14.f;

	UPROPERTY(EditAnywhere, Category = "BG|Card|Motion", meta = (ClampMin = "0.1"))
	float AngleInterpSpeed = 14.f;

	UPROPERTY(EditAnywhere, Category = "BG|Card|Motion", meta = (ClampMin = "0.1"))
	float ScaleInterpSpeed = 12.f;

	// ── 덱에서 뽑히는 비행 (Slay the Spire 류의 과장된 등장) ──────────
	//
	//	세 가지가 겹쳐야 "뽑았다" 가 된다: 휘는 궤적(ArcHeight) + 펴지는 회전(SpinDegrees)
	//	+ 작게 출발해 지나쳤다 안착(StartScale·ScaleOvershoot).
	//	⚠️ 이 단계만 고정 시간이다 — 진행도(0~1)를 알아야 곡선·회전·오버슈트를 계산할 수 있다.

	/** 덱에서 손패까지 날아오는 시간(초). 0 이면 비행을 건너뛰고 곧장 보간으로 간다. */
	UPROPERTY(EditAnywhere, Category = "BG|Card|Motion|Deal", meta = (ClampMin = "0"))
	float DealFlightDuration = 0.2f;

	/** 궤적이 휘는 정도(픽셀). 진행 방향의 수직으로 항상 위로 볼록하게. 0 이면 직선. */
	UPROPERTY(EditAnywhere, Category = "BG|Card|Motion|Deal", meta = (ClampMin = "0"))
	float DealArcHeight = 170.f;

	/** 출발 기울기(도). 날아오며 제자리 각도로 펴진다. ±360 이면 한 바퀴 도는 등장. */
	UPROPERTY(EditAnywhere, Category = "BG|Card|Motion|Deal")
	float DealSpinDegrees = -160.f;

	/** 안착 직전에 제 크기를 지나치는 정도. 1.7 이 고전값, 3 을 넘기면 눈에 띄게 튀어오른다. */
	UPROPERTY(EditAnywhere, Category = "BG|Card|Motion|Deal", meta = (ClampMin = "0"))
	float DealScaleOvershoot = 3.f;

	/** 위치 곡선의 지수. 클수록 양끝이 더 부드럽고 중간이 빠르다. */
	UPROPERTY(EditAnywhere, Category = "BG|Card|Motion|Deal", meta = (ClampMin = "1"))
	float DealEaseExponent = 2.5f;

	/**
	 * true 면 가속→감속(미끄러지듯), false 면 감속만(덱에서 튕겨 나온 듯).
	 * ⚠️ false 는 지수 2.5 기준 시간의 20% 만에 거리의 44% 를 가버린다 — 출발이 순간이동처럼
	 *    보이는 원인이 대개 이것이고, 비행 시간을 늘려도 앞부분만 더 급해질 뿐이다.
	 */
	UPROPERTY(EditAnywhere, Category = "BG|Card|Motion|Deal")
	bool bDealEaseInOut = true;

	/** 비행 시작에 서서히 나타나는 시간(초). 짧게라도 넣어야 "덱에서 뽑혀 나온다" 로 읽힌다. */
	UPROPERTY(EditAnywhere, Category = "BG|Card|Motion|Deal", meta = (ClampMin = "0"))
	float DealFadeInDuration = 0.12f;

	/**
	 * 날아가던 카드가 바뀐 목표를 다시 겨누는 속도. 낮으면(4~6) 넓게 휘어 돌고, 높으면(15~) 즉시 꺾인다.
	 * ⚠️ 0 이면 겨냥이 안 따라가 엉뚱한 자리에 착지한다(남은 오차는 MoveInterpSpeed 가 흡수).
	 */
	UPROPERTY(EditAnywhere, Category = "BG|Card|Motion|Deal", meta = (ClampMin = "0.1"))
	float DealAimInterpSpeed = 7.f;

	/** 덱에서 출발할 때의 크기. 1 보다 작으면 "멀리서 다가오는" 느낌이 난다. */
	UPROPERTY(EditAnywhere, Category = "BG|Card|Motion", meta = (ClampMin = "0.01"))
	float DealStartScale = 0.35f;

	// ── 관성 기울기 (secondary motion) ───────────────────────────────
	//	이동 방향으로 살짝 눕는다 — 키를 찍지 않고 "무게가 있는 물체" 를 만드는 가장 싼 방법.

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
	//	⚠️ 루트의 렌더 트랜스폼을 쓴다. WBP 의 호버 애니메이션은 자식 컨테이너에 걸 것(서로 덮어쓴다).

	/** 마우스가 올라갔을 때의 크기 배율. */
	UPROPERTY(EditAnywhere, Category = "BG|Card|Motion|Hover", meta = (ClampMin = "0.01"))
	float HoverScale = 1.18f;

	/** 마우스가 올라갔을 때 떠오르는 높이(픽셀). 음수가 위쪽이다. */
	UPROPERTY(EditAnywhere, Category = "BG|Card|Motion|Hover")
	float HoverLift = -34.f;

	/** 커서가 카드 좌우 **끝**에 있을 때의 기울기(도). 커서를 따라 기운다. */
	UPROPERTY(EditAnywhere, Category = "BG|Card|Motion|Hover")
	float HoverTiltMax = 9.f;

	/**
	 * 기울기가 붙는 곡선(최대치가 아니라 반응 모양). 1 은 선형, 2 는 중앙이 평평, 0.5 는 예민하다.
	 * 양끝(±1)에서는 곡선과 무관하게 항상 HoverTiltMax 다 — 최대치와 반응을 따로 맞추기 위해서.
	 * ⚠️ 비틀림(HoverShearMax)도 같은 곡선을 쓴다. 다르면 카드가 접히는 것처럼 보인다.
	 */
	UPROPERTY(EditAnywhere, Category = "BG|Card|Motion|Hover", meta = (ClampMin = "0.05"))
	float HoverResponseExponent = 1.f;

	/**
	 * 커서 위치를 따라가는 속도. 12~20 이 "따라오되 떨지 않는" 구간이다.
	 * ⚠️ 0 으로 두지 말 것 — 커서를 빠르게 흔들면 기울기가 프레임마다 튀어 카드가 덜덜 떤다.
	 */
	UPROPERTY(EditAnywhere, Category = "BG|Card|Motion|Hover", meta = (ClampMin = "0"))
	float HoverFollowSpeed = 16.f;

	/** 커서 위치에 따른 비틀림(shear). 2D 위젯에 입체감을 준다. 0 이면 끈다. */
	UPROPERTY(EditAnywhere, Category = "BG|Card|Motion|Hover")
	float HoverShearMax = 5.f;

	/** 호버 연출이 붙고 풀리는 속도. */
	UPROPERTY(EditAnywhere, Category = "BG|Card|Motion|Hover", meta = (ClampMin = "0.1"))
	float HoverInterpSpeed = 15.f;

	/**
	 * 호버가 **풀리는** 기준을 **가로만** 커지기 전 카드 폭으로 되돌린다.
	 * 세로는 커진 카드 높이 그대로다.
	 *
	 * 끄면(슬레이트 기본 동작) 들어갈 때는 원래 크기, 나올 때는 HoverScale 만큼 커진
	 * 영역이 기준이라 경계가 어긋난다. 부채꼴에서 카드가 겹쳐 있으면 커진 카드가 옆 카드를
	 * 덮어서, 커서를 옆으로 옮겨도 호버가 넘어가지 않고 끈적하게 붙어 있는다.
	 *
	 * ⚠️ 세로까지 좁히지 않는 이유: 카드가 겹치는 축은 가로뿐이라 세로를 좁혀도 얻는 것이
	 *    없고, 눈에는 카드 위에 커서가 있는데 호버가 풀리는 띠만 생긴다.
	 *    판정은 UpdateHoverBounds 에 있다.
	 *
	 * ⚠️ 슬레이트는 렌더 트랜스폼을 히트테스트에도 반영한다 — 그래서 코드가 직접 재야 한다.
	 *    들어오는 판정은 건드리지 않는다(그쪽은 원래도 원래 크기 기준이다).
	 */
	UPROPERTY(EditAnywhere, Category = "BG|Card|Motion|Hover")
	bool bUnhoverAtOriginalBounds = true;

	/**
	 * 경계에서 호버가 떠는 것을 막는 여유(0~1). 나갈 때는 경계에서, 다시 들어올 때는
	 * 그보다 이만큼 안쪽에서 붙는다.
	 *
	 * ⚠️ 0 으로 두지 말 것. 커서를 경계에 걸치면 OnHoverChanged 가 프레임마다 오가고,
	 *    거기 물린 WBP 애니메이션이 계속 처음으로 되감긴다.
	 */
	UPROPERTY(EditAnywhere, Category = "BG|Card|Motion|Hover", meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float HoverExitDeadband = 0.06f;

	/** 퇴장할 때 줄어드는 최종 크기. */
	UPROPERTY(EditAnywhere, Category = "BG|Card|Motion", meta = (ClampMin = "0.01"))
	float ExitEndScale = 0.65f;

	/** 퇴장에 걸리는 시간(초). 이 시간이 지나면 위젯이 파괴된다. */
	UPROPERTY(EditAnywhere, Category = "BG|Card|Motion", meta = (ClampMin = "0.01"))
	float ExitDuration = 0.35f;

	// ── 퇴장 디졸브 (불타 사라짐 · 잘려 사라짐) ──────────────────────
	//
	//	카드는 테두리+그림+글씨의 위젯 트리다. Img_Art 에만 머테리얼을 물리면 그림만 타고 글씨는 남는다 —
	//	트리 전체를 렌더 타겟에 그린 뒤 머테리얼을 먹이는 RetainerBox 가 UMG 의 유일한 방법이다.
	//	⚠️ 리테이너는 인스턴스마다 RT 를 하나씩 잡는다(손패 7장이면 7개). 평상시엔 꺼 두고 퇴장에만 켠다 —
	//	   WBP 에서 켜 둔 채로 두지 말 것.

	/** 디졸브로 퇴장할지. 꺼져 있거나 리테이너·머테리얼이 없으면 불투명도만 낮추며 사라진다. */
	UPROPERTY(EditAnywhere, Category = "BG|Card|Dissolve")
	bool bUseDissolveExit = true;

	/** 카드 트리 전체를 감싼 리테이너(Effect Material 에 디졸브를 물린다). 없으면 디졸브만 조용히 꺼진다. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<URetainerBox> Retainer_Card;

	/** 사용한 카드만 디졸브할지. 켜 두기를 권한다 — 버려지는 카드까지 타면 무엇이 발동했는지 묻힌다. */
	UPROPERTY(EditAnywhere, Category = "BG|Card|Dissolve")
	bool bDissolveOnlyOnPlay = true;

	/** 타들어가는 진행 곡선(0~1 → 0~1). 비우면 선형. 뒤쪽을 가파르게 하면 "확 무너진다" 가 된다. */
	UPROPERTY(EditAnywhere, Category = "BG|Card|Dissolve")
	TObjectPtr<UCurveFloat> DissolveCurve;

	/**
	 * 원형별 사라지는 방식. 카드마다 적지 않고 효과 원형에서 유도한다 —
	 * DT 에 연출 컬럼을 두면 효과를 바꿨을 때 연출만 옛것으로 남는다(DeriveFXArchetype 과 같은 규약).
	 */
	UPROPERTY(EditAnywhere, Category = "BG|Card|Dissolve")
	TMap<EBGCardFXArchetype, FBGCardDissolveFX> DissolveByArchetype;

	/** 원형이 없거나(None) 위 맵에 없는 카드가 쓰는 값. */
	UPROPERTY(EditAnywhere, Category = "BG|Card|Dissolve")
	FBGCardDissolveFX DefaultDissolve;

	//	파라미터 이름을 값으로 두는 이유는 TierFX 와 같다 — 틀리면 DMI 가 조용히 무시한다.

	/** 타들어간 정도 0~1. **이것만 있으면 최소한 사라지기는 한다.** */
	UPROPERTY(EditAnywhere, Category = "BG|Card|Dissolve|Param")
	FName DissolveAmountParamName = TEXT("DissolveAmount");

	UPROPERTY(EditAnywhere, Category = "BG|Card|Dissolve|Param")
	FName DissolveMaskParamName = TEXT("DissolveMask");

	UPROPERTY(EditAnywhere, Category = "BG|Card|Dissolve|Param")
	FName DissolveEdgeColorParamName = TEXT("EdgeColor");

	UPROPERTY(EditAnywhere, Category = "BG|Card|Dissolve|Param")
	FName DissolveEdgeWidthParamName = TEXT("EdgeWidth");

	UPROPERTY(EditAnywhere, Category = "BG|Card|Dissolve|Param")
	FName DissolveEdgeIntensityParamName = TEXT("EdgeIntensity");

	UPROPERTY(EditAnywhere, Category = "BG|Card|Dissolve|Param")
	FName DissolveSeparateParamName = TEXT("SeparateAmount");

	UPROPERTY(EditAnywhere, Category = "BG|Card|Dissolve|Param")
	FName DissolveCutAngleParamName = TEXT("CutAngle");

	UPROPERTY(EditAnywhere, Category = "BG|Card|Dissolve|Param")
	FName DissolveEdgeSoftnessParamName = TEXT("EdgeSoftness");

	UPROPERTY(EditAnywhere, Category = "BG|Card|Dissolve|Param")
	FName DissolveSurfaceGlowParamName = TEXT("SurfaceGlow");

	UPROPERTY(EditAnywhere, Category = "BG|Card|Dissolve|Param")
	FName DissolveRiseParamName = TEXT("RiseAmount");

	UPROPERTY(EditAnywhere, Category = "BG|Card|Dissolve|Param")
	FName DissolveEmberMaskParamName = TEXT("EmberMask");

	UPROPERTY(EditAnywhere, Category = "BG|Card|Dissolve|Param")
	FName DissolveEmberIntensityParamName = TEXT("EmberIntensity");

	UPROPERTY(EditAnywhere, Category = "BG|Card|Dissolve|Param")
	FName DissolveHaloWidthParamName = TEXT("HaloWidth");


	// ── 사용한 카드의 중앙 연출 ──────────────────────────────────────

	/** 손패에서 중앙까지 올라가는 시간(초). */
	UPROPERTY(EditAnywhere, Category = "BG|Card|Motion|Play", meta = (ClampMin = "0.01"))
	float PresentDuration = 0.25f;

	/** 중앙에 있을 때의 크기. 1 보다 크면 앞으로 튀어나온 느낌이 난다. */
	UPROPERTY(EditAnywhere, Category = "BG|Card|Motion|Play", meta = (ClampMin = "0.01"))
	float PresentScale = 1.25f;

	/**
	 * 효과 발동 뒤(ReleaseHold) 중앙에 더 머무는 시간(초). 판정 위젯의 표시 시간과 맞출 것.
	 * ⚠️ 대상 선택 대기는 여기 포함되지 않는다(그쪽은 ReleaseHold 까지 무한 대기).
	 * ⚠️ bWaitForDwellAnimation 을 켜면 이 값은 **안전 상한**으로만 쓰인다.
	 */
	UPROPERTY(EditAnywhere, Category = "BG|Card|Motion|Play", meta = (ClampMin = "0"))
	float DwellDuration = 2.2f;

	/**
	 * 중앙 대기의 길이를 UMG 애니메이션이 정하게 한다(BP 가 FinishDwell 을 부를 때까지 대기).
	 * ⚠️ 그래도 DwellDuration 이 상한으로 돈다 — BP 가 FinishDwell 을 빠뜨리면 카드가 중앙에
	 *    영원히 박혀 손패가 잠기는데, 그 상태에서 아무 에러도 안 나기 때문이다.
	 */
	UPROPERTY(EditAnywhere, Category = "BG|Card|Motion|Play")
	bool bWaitForDwellAnimation = false;

	/**
	 * 끄기 신호(OnPlayFXFinished)의 시점을 바깥이 정하게 한다.
	 * 끄면 카드가 중앙을 뜰 때 꺼지고, 켜면 NotifyPlayFXFinished 를 부를 때까지 기다린다 —
	 * 월드의 공격·피격 몽타주가 카드보다 오래 갈 때 이쪽이다.
	 * ⚠️ 상한은 카드의 파괴다. 신호가 안 오면 그때까지 중앙에 서 있다.
	 */
	UPROPERTY(EditAnywhere, Category = "BG|Card|Motion|Play")
	bool bWaitForExternalPlayFXFinish = false;

private:
	void RefreshVisual();
	void NotifyDealStarted();
	void PlayConfirmAudioIfReady();

	/** 시각 Hover와 독립된 루프 사운드 세션. 드래그 중에는 시각 Hover만 꺼지고 사운드는 유지한다. */
	void StartHoverAudio();
	void StopHoverAudio(bool bPlayEndSound = true);

	/** 켜기 신호를 이미 쐈는가. 끄기가 한 번만 먹게 하는 짝 표시다. */
	bool bPlayFXStarted = false;

	/** 현재 보간값을 실제 슬롯/렌더 트랜스폼에 적용한다. */
	void ApplyMotionTransform();

	/** 지금 위치에서 사라지기 시작한다. 곧장 퇴장과 중앙 대기 후 퇴장이 여기서 만난다. */
	void EnterExitPhase(const FVector2D& ToPosition);

	/** 판정 연출을 터뜨린다. 중앙에 서 있는 상태에서만 불린다(ReleaseHold / 도착 시점). */
	void PlayResolvedFX(const FBGCardPlayContext& Context);


	/** 중앙에 도착한 순간, 날아오는 사이에 밀린 신호를 순서대로 소비한다. */
	void ConsumePendingPlaySignals();

	/**
	 * 손패를 떠나기 시작할 때의 공통 처리(가시성 갈무리·입력 차단). 이미 떠나는 중이면 false.
	 * BeginExit 와 BeginPlayExit 이 여기까지 같고, 갈라지는 것은 Center.bHolding · bPlayExit 이다.
	 */
	bool BeginLeaveHand();

	// ── 연출 단계별 한 프레임 ────────────────────────────────────────
	//	TickMotion 이 상태에 맞는 것을 고른다. 한 함수에 넣으면 200 줄짜리 switch 가 된다.

	/** false 면 아직 stagger 대기 중이다 — 이번 프레임은 트랜스폼도 건드리지 않는다. */
	bool TickPending(float DeltaTime);

	void TickDealing(float DeltaTime);
	void TickMoving(float DeltaTime);
	void TickPresenting(float DeltaTime);
	void TickDwelling(float DeltaTime);
	void TickExiting(float DeltaTime);

	/** 주 움직임에 얹히는 부수 효과(관성 기울기·호버). 정지 상태에서도 굴러야 기울기가 0 으로 돌아온다. */
	void TickSecondaryMotion(float DeltaTime, const FVector2D& PositionBeforeTick);

	/**
	 * 커서가 카드 밖으로 나갔는지 보고 bHovered 를 갱신한다.
	 * 기준이 축마다 다르다 — 가로는 **커지기 전** 폭, 세로는 **커진** 높이다
	 * (이유는 bUnhoverAtOriginalBounds 주석 참고).
	 *
	 * 인자는 클램프하지 않은 로컬 좌표(-1~+1 이 커진 카드 전체)다 — 밖으로 나간 것을
	 * 알아야 하므로 클램프된 HoverLocalNormalized 로는 판정할 수 없다.
	 */
	void UpdateHoverBounds(const FVector2D& RawLocal);

	/** 등급별 확률 숫자를 채운다. 판정하지 않는 카드면 숨긴다. */
	void RefreshChanceText();

	/** "확률 보정중" 문구를 보이고 숨긴다. 보정이 걸렸고 호버 중일 때만 보인다. */
	void RefreshChanceModifiedBanner();

	/** 테두리 안쪽 그림을 채운다. 뷰에 그림이 없으면 폴백으로, 그것도 없으면 칸을 접는다. */
	void RefreshArt();

	/**
	 * 대분류에 맞는 테두리를 고른다. 브러시의 FrameTexParamName 파라미터만 갈아 끼운다 —
	 * ⚠️ SetBrushFromTexture 로 덮으면 MI 가 날아가고, 이 함수는 디자인 타임에도 불리므로
	 *    그 상태로 WBP 가 저장되면 애셋에 텍스처가 박힌다.
	 */
	void RefreshFrame();

	// ── 등급 연출 ────────────────────────────────────────────────────

	/** 리테이너 크기를 머테리얼에 먹인다. 크기를 쓰는 연출이 모두 이걸 부른다. */
	void PushWidgetSize(UMaterialInstanceDynamic* MID);

	/** 등급별 연출값을 고른다. 등급이 없으면(장비 등) nullptr. */
	const FBGCardTierFX* FindTierFX(EBGResolutionTier Tier) const;

	/** 잦아드는 진행도(1=방금 터짐, 0=평상시)를 반영한다. 평상시 값은 DMI 생성 시 붙잡아 둔 것으로 되돌린다. */
	void ApplyTierFX(float Shape);

	/** 감쇠를 굴린다. TickMotion 이 매 프레임 부른다. */
	void TickTierFX(float DeltaTime);

	//	호버 외곽선 글로우에 대응하는 함수는 없다. 위의 "코드가 손대지 않는다" 참고.

	// ── 퇴장 디졸브 ──────────────────────────────────────────────────

	/** 리테이너의 DMI(처음 부를 때 생성). 리테이너나 Effect Material 이 없으면 nullptr — 조용히 꺼진다. */
	UMaterialInstanceDynamic* GetCardFXMID();

	/** 이 카드의 원형에 맞는 사라지는 방식. 맵에 없으면 DefaultDissolve. */
	const FBGCardDissolveFX& FindDissolveFX() const;

	/** 디졸브 시작. 성공하면 true — 그때는 불투명도를 건드리지 않는다(경계의 불꽃이 안 보인다). */
	bool BeginDissolveExit();

	/** 타들어간 정도(0 = 멀쩡, 1 = 다 사라짐)를 머테리얼에 반영한다. */
	void ApplyDissolve(float Amount);

	// ── 퇴장 디졸브 런타임 상태 ──────────────────────────────────────

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> CardFXMID;

	FBGCardDissolveFX ActiveDissolve;

	//	디졸브가 실제로 걸렸는가. 실패했으면(리테이너 없음 등) 불투명도로 사라진다.
	bool bDissolveActive = false;

	//	사용해서 나가는 중인가(중앙 경유). bDissolveOnlyOnPlay 판단에 쓴다 —
	//	두 퇴장 경로가 EnterExitPhase 에서 만나므로 거기서는 더 이상 구분할 수 없다.
	bool bPlayExit = false;

	// ── 등급 연출 런타임 상태 ────────────────────────────────────────
	//	글로우도 리테이너의 CardFXMID 를 함께 쓴다 — 디졸브와 같은 성격의 연출이라 DMI 를 따로 두지 않는다.

	//	DMI 생성 시점의 값 — 연출이 끝나면 여기로 돌아간다. 0 으로 되돌리면 평상시 글로우까지 죽는다.
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

	//	직전 프레임의 호버 상태. 바뀌는 순간에만 BP 훅을 쏜다 — 매 프레임이면 애니메이션이 계속 되감긴다.
	bool bWasHoverActive = false;

	//	Hover.Begin 루프를 이 카드가 시작했는가. 드래그 중 MouseLeave와 실제 종료를 구분한다.
	bool bHoverAudioActive = false;

	//	Border_Root 브러시에 머테리얼이 없다고 이미 알렸는가. RefreshVisual 은 자주 불리므로 한 번만.
	bool bWarnedFrameBrushNotMaterial = false;

	// ── 연출 내부 상태 ──
	EBGCardMotionState MotionState = EBGCardMotionState::Idle;

	FVector2D CurrentPosition = FVector2D::ZeroVector;
	FVector2D TargetPosition  = FVector2D::ZeroVector;
	float CurrentAngle = 0.f;
	float TargetAngle  = 0.f;
	float CurrentScale = 1.f;

	//	덱 → 손패 비행, 그리고 손패 → 중앙 대기. 값의 뜻은 각 구조체 주석에 있다.
	//	⚠️ 여기 있는 것은 런타임 상태뿐이다. 튜닝 값(위쪽 EditAnywhere)을 옮기면
	//	   WBP 에 저장된 디자이너 값이 전부 기본값으로 돌아간다.
	FBGCardDealRuntime   Deal;
	FBGCardCenterRuntime Center;

	//	퇴장은 고정 시간 보간이라 시작점을 기억해야 한다.
	FVector2D ExitStartPosition = FVector2D::ZeroVector;
	float ExitStartScale = 1.f;
	float ExitElapsed = 0.f;

	//	떠나기 전의 가시성. 손패로 되돌릴 때 복원한다.
	ESlateVisibility PreLeaveVisibility = ESlateVisibility::Visible;

	//	드래그로 실루엣이 되기 전의 가시성·불투명도. ⚠️ Visible / 1.0 으로 하드코딩하면
	//	WBP 가 정한 히트테스트 설정을 덮어쓴다(Deal.PreVisibility 와 같은 이유).
	ESlateVisibility PreDragVisibility = ESlateVisibility::Visible;
	float PreDragOpacity = 1.f;

	// ── 부수 효과 내부 상태 ──
	//	속도는 위치 차분으로 구한다. 별도로 들고 다니면 스냅·순간이동 때 어긋난다.
	float VelocityTilt = 0.f;

	bool bHovered = false;

	//	커서의 카드 안 위치. (-1,-1) 좌상단 ~ (1,1) 우하단. 마우스 이벤트가 그대로 넣는다.
	FVector2D HoverLocalNormalized = FVector2D::ZeroVector;

	//	위를 보간해 실제 연출에 먹이는 값. 호버가 풀리는 동안은 0 으로 되돌아간다.
	FVector2D HoverLocalSmoothed = FVector2D::ZeroVector;

	//	호버 연출의 강도 0~1. 마우스가 들고 날 때 이 값이 보간된다.
	float HoverBlend = 0.f;
};
