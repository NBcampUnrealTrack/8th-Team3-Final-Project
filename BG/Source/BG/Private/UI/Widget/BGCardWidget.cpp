// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Widget/BGCardWidget.h"
#include "UI/Widget/BGCardDragDropOperation.h"
#include "UI/Widget/BGHandWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Border.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/RetainerBox.h"
#include "Components/RichTextBlock.h"
#include "Components/TextBlock.h"
#include "Curves/CurveFloat.h"
#include "Debug/BGCardLog.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
	/**
	 * 목표를 살짝 지나쳤다 되돌아오는 감속(ease-out-back).
	 *
	 * 등장에 "탄력" 을 주는 고전적인 곡선이다. Overshoot 이 0 이면 평범한 감속이 된다.
	 */
	float EaseOutBack(float Alpha, float Overshoot)
	{
		const float A = Alpha - 1.f;
		return 1.f + (Overshoot + 1.f) * A * A * A + Overshoot * A * A;
	}

	/**
	 * 시작→목표 직선의 수직 방향. 궤적을 활처럼 휘게 하는 데 쓴다.
	 * 항상 "위로 볼록" 하도록 부호를 맞춘다 — 아래로 처지면 카드가 떨어지는 것처럼 보인다.
	 */
	FVector2D UpwardPerpendicular(const FVector2D& Delta)
	{
		FVector2D Perpendicular(-Delta.Y, Delta.X);
		if (Perpendicular.Y > 0.f)
		{
			Perpendicular = -Perpendicular;   // 화면 좌표는 아래로 갈수록 Y 가 크다
		}

		//	출발점과 목표가 거의 같으면 방향을 정할 수 없다. 그냥 위로 둔다.
		return Perpendicular.Normalize() ? Perpendicular : FVector2D(0.f, -1.f);
	}

	/**
	 * 고정 시간 연출의 진행도 0~1.
	 *
	 * 0 으로 나누는 것을 막는 자리가 연출 단계마다 필요하다 — 튜닝값을 0 으로 두는 것은
	 * "그 단계를 건너뛴다" 는 뜻이라 실제로 일어난다(DealFlightDuration = 0 등).
	 */
	float Progress(float Elapsed, float Duration)
	{
		return FMath::Clamp(Elapsed / FMath::Max(Duration, KINDA_SMALL_NUMBER), 0.f, 1.f);
	}

	/**
	 * 커브가 있으면 그것이 모양을 정하고, 없으면 Fallback 을 쓴다.
	 *
	 * 커브를 안 넣어도 동작해야 튜닝을 나중으로 미룰 수 있다 — 애셋이 없으면
	 * 연출이 통째로 멈추는 구조면 아티스트가 붙기 전까지 확인을 못 한다.
	 */
	float CurveOr(const UCurveFloat* Curve, float Alpha, float Fallback)
	{
		return Curve ? Curve->GetFloatValue(Alpha) : Fallback;
	}

	/**
	 * -1~+1 입력에 응답 곡선을 먹인다. 부호(방향)는 그대로 두고 **크기만** 굽힌다.
	 *
	 *   1 보다 크면 : 중앙 근처가 둔해지고 가장자리에서 급해진다
	 *   1 보다 작으면 : 조금만 벗어나도 확 기운다
	 *
	 * 양끝(±1)에서는 항상 ±1 이라 최대치가 곡선에 따라 변하지 않는다 —
	 * 그래야 Max 값과 곡선을 따로 만질 수 있다.
	 */
	float ShapeResponse(float Value, float Exponent)
	{
		if (FMath::IsNearlyEqual(Exponent, 1.f))
		{
			return Value;   // 선형. Pow 를 태울 이유가 없다.
		}

		return FMath::Sign(Value)
			* FMath::Pow(FMath::Abs(Value), FMath::Max(Exponent, KINDA_SMALL_NUMBER));
	}
}

void UBGCardWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	RefreshVisual();
}

void UBGCardWidget::SetOwningHand(UBGHandWidget* InHand)
{
	OwningHand = InHand;
}

void UBGCardWidget::SetCardView(const FBGCardView& InCardView)
{
	CardView = InCardView;
	RefreshVisual();
}

void UBGCardWidget::SetBeingDragged(bool bInBeingDragged)
{
	if (bBeingDragged == bInBeingDragged)
	{
		return;
	}
	bBeingDragged = bInBeingDragged;
	RefreshVisual();
}

void UBGCardWidget::SetDragLocked(bool bInLocked)
{
	if (bDragLocked == bInLocked)
	{
		return;
	}
	bDragLocked = bInLocked;
	RefreshVisual();
}

void UBGCardWidget::RefreshChanceText()
{
	if (!Text_ChanceCriticalFailure && !Text_ChanceFailure && !Text_ChanceSuccess && !Text_ChanceCriticalSuccess)
	{
		return;   // WBP 가 확률 칸을 두지 않았다. 전부 선택 사항이다.
	}

	//	판정하지 않는 카드(장비)에 확률을 띄우면 "장비도 실패할 수 있다" 는 오해를 준다.
	//	Collapsed 로 자리까지 회수한다 — 장비 카드에 빈 줄이 남지 않도록.
	const ESlateVisibility ChanceVisibility =
		CardView.bHasChances ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;

	//	보정이 걸렸으면 보정 후 값을 보여준다. GDD §8 이 요구하는 것은 "최종 확률" 이다.
	const FBGCardChanceView& Shown = CardView.FinalChance;

	//	반올림 때문에 합이 100 이 안 될 수 있다. 등급별 값이 정확한 쪽을 택했다.
	//	태그 이름은 DT_BGChanceTextStyle 의 행 이름과 정확히 같아야 한다.
	const auto ApplyTierChance = [bHasChances = CardView.bHasChances, ChanceVisibility](URichTextBlock* Block, const FText& Format, float Value)
	{
		if (!Block)
		{
			return;   // 이 등급 칸을 두지 않은 WBP.
		}

		Block->SetVisibility(ChanceVisibility);
		if (bHasChances)
		{
			Block->SetText(FText::Format(Format, FText::AsNumber(FMath::RoundToInt(Value))));
		}
	};

	ApplyTierChance(Text_ChanceCriticalFailure, NSLOCTEXT("BG", "CardChanceCriticalFailure", "<CriticalFailure>{0}</>"), Shown.CriticalFailure);
	ApplyTierChance(Text_ChanceFailure, NSLOCTEXT("BG", "CardChanceFailure", "<Failure>{0}</>"), Shown.Failure);
	ApplyTierChance(Text_ChanceSuccess, NSLOCTEXT("BG", "CardChanceSuccess", "<Success>{0}</>"), Shown.Success);
	ApplyTierChance(Text_ChanceCriticalSuccess, NSLOCTEXT("BG", "CardChanceCriticalSuccess", "<CriticalSuccess>{0}</>"), Shown.CriticalSuccess);
}

void UBGCardWidget::RefreshChanceModifiedBanner()
{
	if (!Text_ChanceModified)
	{
		return;
	}

	//	호버 중일 때만 띄운다.
	const bool bShow = CardView.bHasChances && CardView.bChanceModified && IsHoverActive();

	Text_ChanceModified->SetVisibility(bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (bShow)
	{
		Text_ChanceModified->SetColorAndOpacity(FSlateColor(ModifiedChanceColor));
	}
}

void UBGCardWidget::RefreshVisual()
{
	if (Text_CardName)
	{
		Text_CardName->SetText(CardView.DisplayName);
	}
	if (Text_CardDescription)
	{
		Text_CardDescription->SetText(CardView.Description);
	}

	RefreshChanceText();
	RefreshChanceModifiedBanner();
	RefreshArt();
	RefreshFrame();

	if (Border_Root)
	{
		//	잠금이 가장 우선 — 잠긴 동안에는 사용 가능 여부와 무관하게 손댈 수 없다.
		const FLinearColor Tint =
			bDragLocked          ? LockedTint :
			bBeingDragged        ? DraggedTint :
			!CardView.bPlayable  ? UnplayableTint :
			                       NormalTint;
		Border_Root->SetBrushColor(Tint);
	}
}

void UBGCardWidget::RefreshArt()
{
	if (!Img_Art)
	{
		return;   // WBP 가 그림 칸을 두지 않았다. 선택 사항이므로 조용히 넘어간다.
	}

	//	뷰의 그림이 없으면 폴백. 폴백도 없으면 브러시를 건드리지 않고 칸을 접는다 —
	//	null 브러시를 넣으면 카드 한복판이 투명해져서 "카드가 사라졌다" 로 보인다.
	UTexture2D* Art = CardView.ArtTexture ? CardView.ArtTexture.Get() : FallbackArtTexture.Get();

	if (!Art)
	{
		Img_Art->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	Img_Art->SetVisibility(ESlateVisibility::HitTestInvisible);
	Img_Art->SetBrushFromTexture(Art, /*bMatchSize=*/false);
}

void UBGCardWidget::RefreshFrame()
{
	if (!Border_Root)
	{
		return;
	}

	UTexture2D* Frame = nullptr;
	switch (CardView.Category)
	{
		case EBGCardCategory::Equipment:
			Frame = EquipmentFrameTexture; 
			break;
		case EBGCardCategory::Attack:
			Frame = AttackFrameTexture;    
			break;
		case EBGCardCategory::Magic:
			Frame = MagicFrameTexture;     
			break;
	}

	if (!Frame)
	{
		return;   // null 로 덮으면 카드가 통째로 투명해진다(RefreshArt 와 같은 이유).
	}

	Border_Root->SetBrushFromTexture(Frame);
}

// ══════════════════════════════════════════════════════════════════════════════════════
//  등급 연출 (판정 4단계를 글로우로)
//
//  ⚠️ 시간축을 여기서 굴리는 것은 **카드 한 장짜리라 성립하는 단순화**다.
//     "대성공 → 피해 → 상대 사망 → 승리 연출" 처럼 여러 연출이 연쇄될 때는 누가 누구를
//     기다리는지 중재할 곳이 필요하다. 나중에 연출 큐를 만들면 TickTierFX 의 진행도 계산만
//     그쪽으로 옮기면 되도록, 재생(PlayTierFX) · 진행(TickTierFX) · 적용(ApplyTierFX)을
//     세 함수로 갈라 두었다.
// ══════════════════════════════════════════════════════════════════════════════════════

void UBGCardWidget::PushWidgetSize(UMaterialInstanceDynamic* MID)
{
	//	팽창 반경을 UV 로 바꾸려면 픽셀 크기가 필요하다. 안 먹이면 카드가 세로로 긴
	//	비율이라 외곽선이 가로로 눌린 타원으로 번진다.
	//
	//	매번 재는 이유는 손패 재배치 · 호버 스케일로 크기가 계속 변하기 때문이다.
	if (!MID || WidgetSizeParamName.IsNone() || !Retainer_Card)
	{
		return;
	}

	const FVector2D Size = Retainer_Card->GetCachedGeometry().GetLocalSize();
	if (Size.IsNearlyZero())
	{
		return;   // 레이아웃이 아직 안 잡혔다. 다음 프레임에 다시 잰다.
	}

	MID->SetVectorParameterValue(WidgetSizeParamName,
		FLinearColor(static_cast<float>(Size.X), static_cast<float>(Size.Y), 0.f, 0.f));
}

void UBGCardWidget::TickHoverGlow()
{
	if (!bUseHoverOutlineGlow)
	{
		return;
	}

	UMaterialInstanceDynamic* MID = GetCardFXMID();
	if (!MID)
	{
		return;   // 리테이너가 없거나 Effect Material 이 비었다. 조용히 꺼진다.
	}

	PushWidgetSize(MID);

	//	켜지고 꺼지는 보간은 HoverBlend 가 이미 하고 있다. 여기서 또 굴리면
	//	두 개의 감쇠가 겹쳐 반응이 느려진다.
	//
	//	⚠️ 파라미터만 바꾸는 것은 리테이너의 렌더 타겟을 다시 그리지 않는다.
	//	   RT 는 자식이 바뀔 때만 갱신되고, 파라미터는 그 RT 를 화면에 합성할 때
	//	   쓰인다. 그래서 매 프레임 먹여도 비싸지 않다.
	if (!HoverParamName.IsNone())
	{
		MID->SetScalarParameterValue(HoverParamName, HoverBlend);
	}
}

const FBGCardTierFX* UBGCardWidget::FindTierFX(EBGResolutionTier Tier) const
{
	switch (Tier)
	{
	case EBGResolutionTier::CriticalFailure: return &CriticalFailureFX;
	case EBGResolutionTier::Failure:         return &FailureFX;
	case EBGResolutionTier::Success:         return &SuccessFX;
	case EBGResolutionTier::CriticalSuccess: return &CriticalSuccessFX;

	//	None = 판정하지 않은 카드(장비 등). 연출이 없는 것이 정답이다 —
	//	기본값으로 떨어뜨리면 장비를 낼 때마다 대실패처럼 보인다.
	default: return nullptr;
	}
}

void UBGCardWidget::PlayTierFX(EBGResolutionTier Tier)
{
	if (!bUseTierGlow)
	{
		return;   // 등급 연출은 WBP 애니메이션이 맡는다. DMI 도 만들지 않는다.
	}

	const FBGCardTierFX* Params = FindTierFX(Tier);
	if (!Params || !GetCardFXMID())
	{
		return;
	}

	ActiveTierFX  = *Params;
	TierFXElapsed = 0.f;
	bTierFXActive = true;

	ApplyTierFX(1.f);   // 터지는 순간을 한 프레임 기다리지 않고 바로 반영한다
}

void UBGCardWidget::ApplyTierFX(float Shape)
{
	UMaterialInstanceDynamic* MID = GetCardFXMID();
	if (!MID)
	{
		return;
	}

	PushWidgetSize(MID);

	const float Clamped = FMath::Clamp(Shape, 0.f, 1.f);

	MID->SetVectorParameterValue(ColorParamName,
		FMath::Lerp(RestColor, ActiveTierFX.Color, Clamped));

	MID->SetScalarParameterValue(IntensityParamName,
		FMath::Lerp(RestIntensity, ActiveTierFX.Intensity, Clamped));

	MID->SetScalarParameterValue(RingParamName,
		FMath::Lerp(RestRing, ActiveTierFX.RingStrength, Clamped));
}

void UBGCardWidget::TickTierFX(float DeltaTime)
{
	if (!bTierFXActive)
	{
		return;
	}

	TierFXElapsed += DeltaTime;

	const float Alpha = Progress(TierFXElapsed, ActiveTierFX.Seconds);

	//	커브가 있으면 그것이 모양을 정한다(가로 0~1 → 세로 1~0).
	//	없으면 선형으로 잦아든다.
	ApplyTierFX(CurveOr(TierFXCurve, Alpha, 1.f - Alpha));

	if (Alpha >= 1.f)
	{
		bTierFXActive = false;
		ApplyTierFX(0.f);   // 평상시 값으로 정확히 맞춰 놓고 끝낸다
	}
}

// ══════════════════════════════════════════════════════════════════════════════════════
//  퇴장 디졸브
//
//  카드 트리 전체(테두리 · 그림 · 글씨)를 한 장의 그림으로 만들어 태우거나 자른다.
//  그 "한 장으로 만드는" 일을 RetainerBox 가 한다 — 자식을 렌더 타겟에 그린 뒤
//  결과에 머테리얼을 먹인다. Img_Art 에 물리면 그림만 타고 글씨가 남는다.
//
//  진행도는 퇴장 보간(ExitElapsed/ExitDuration)을 그대로 쓴다. 별도 시계를 두면
//  카드가 다 사라졌는데 아직 타고 있거나 그 반대가 된다.
// ══════════════════════════════════════════════════════════════════════════════════════

UMaterialInstanceDynamic* UBGCardWidget::GetCardFXMID()
{
	if (CardFXMID)
	{
		return CardFXMID;
	}

	if (!Retainer_Card)
	{
		return nullptr;   // WBP 가 리테이너를 두지 않았다. 선택 사항이다.
	}

	//	리테이너의 Effect Material 로 만들어진 이 위젯 전용 DMI.
	//	카드 전체를 대상으로 하는 연출(디졸브 · 외곽선 글로우 · 등급 글로우)이 이것 하나를
	//	나눠 쓴다. 머테리얼을 안 물렸으면 null 이 돌아온다 — 그때는 전부 조용히 꺼진다.
	//
	//	⚠️ DMI 없이 MI 를 그대로 쓰면 손패 전원이 같은 인스턴스를 공유한다 —
	//	   한 장이 대성공 나면 손패가 통째로 번쩍인다.
	CardFXMID = Retainer_Card->GetEffectMaterial();
	if (!CardFXMID)
	{
		//	리테이너는 뒀는데 머테리얼이 없다. 이건 설정을 하다 만 것이므로 원인을 남긴다 —
		//	리테이너가 아예 없는 경우(선택 사항)와 달리 의도한 상태가 아니다.
		UE_LOG(LogBGCard, Warning,
			TEXT("[Card] Retainer_Card 는 있는데 Effect Material 이 비었다 — 디졸브 · 글로우가 꺼진다. "
			     "WBP 의 리테이너 Effect 항목에 카드 FX 머테리얼을 물릴 것."));
		return nullptr;
	}

	//	평상시 값을 붙잡아 둔다. 연출이 끝나면 0 이 아니라 여기로 돌아가야
	//	아티스트가 잡아 둔 기본 글로우가 살아남는다.
	if (!bRestCaptured)
	{
		CardFXMID->GetVectorParameterValue(FMaterialParameterInfo(ColorParamName), RestColor);
		CardFXMID->GetScalarParameterValue(FMaterialParameterInfo(IntensityParamName), RestIntensity);
		CardFXMID->GetScalarParameterValue(FMaterialParameterInfo(RingParamName), RestRing);
		bRestCaptured = true;
	}

	return CardFXMID;
}

const FBGCardDissolveFX& UBGCardWidget::FindDissolveFX() const
{
	//	카드마다 적지 않고 원형에서 유도한다. 효과를 바꾸면 사라지는 모습도 따라 바뀐다.
	if (const FBGCardDissolveFX* Found = DissolveByArchetype.Find(CardView.FXArchetype))
	{
		return *Found;
	}

	return DefaultDissolve;
}

bool UBGCardWidget::BeginDissolveExit()
{
	if (!bUseDissolveExit)
	{
		return false;
	}

	//	버려지는 카드까지 화려하게 타면 무엇이 발동했는지 묻힌다.
	if (bDissolveOnlyOnPlay && !bPlayExit)
	{
		return false;
	}

	if (!Retainer_Card)
	{
		//	WBP 가 리테이너를 두지 않았다. 선택 사항이므로 불투명도 페이드로 떨어질 뿐이다 —
		//	다만 이름을 틀리게 지었거나 'Is Variable' 을 안 켠 경우와 구분이 안 되므로
		//	찾을 실마리는 남긴다. 평소에는 안 보이도록 Verbose 다.
		UE_LOG(LogBGCard, Verbose,
			TEXT("[Dissolve] Retainer_Card 바인딩이 비어 디졸브를 건너뛴다 — WBP 의 RetainerBox "
			     "이름이 정확히 'Retainer_Card' 인지, 'Is Variable' 이 켜져 있는지 확인할 것."));
		return false;
	}

	UMaterialInstanceDynamic* MID = GetCardFXMID();
	if (!MID)
	{
		return false;   // 원인은 GetCardFXMID 가 남긴다. 불투명도 페이드로 떨어진다.
	}

	ActiveDissolve = FindDissolveFX();

	//	원형마다 바뀌지 않는 값은 여기서 한 번만 먹인다. 매 프레임 바뀌는 것은
	//	타들어간 정도뿐이다 — 나머지를 틱에서 계속 세팅하면 헛일이다.
	if (!DissolveMaskParamName.IsNone() && ActiveDissolve.MaskTexture)
	{
		MID->SetTextureParameterValue(DissolveMaskParamName, ActiveDissolve.MaskTexture);
	}

	//	재 · 잔불은 공격만 쓴다. 회복은 EmberIntensity 가 0 이라 텍스처를 안 넣어도 된다.
	if (!DissolveEmberMaskParamName.IsNone() && ActiveDissolve.EmberMask)
	{
		MID->SetTextureParameterValue(DissolveEmberMaskParamName, ActiveDissolve.EmberMask);
	}

	MID->SetVectorParameterValue(DissolveEdgeColorParamName, ActiveDissolve.EdgeColor);
	MID->SetScalarParameterValue(DissolveEdgeWidthParamName, ActiveDissolve.EdgeWidth);
	MID->SetScalarParameterValue(DissolveEdgeIntensityParamName, ActiveDissolve.EdgeIntensity);
	MID->SetScalarParameterValue(DissolveSeparateParamName, ActiveDissolve.SeparateAmount);
	MID->SetScalarParameterValue(DissolveCutAngleParamName, ActiveDissolve.CutAngleDegrees);

	//	여기부터가 공격과 회복을 가르는 값들이다. 머테리얼은 하나뿐이고 이 숫자들만
	//	달라진다 — 원형마다 머테리얼을 두면 DMI 와 셰이더 퍼뮤테이션이 그만큼 늘어난다.
	MID->SetScalarParameterValue(DissolveEdgeSoftnessParamName, ActiveDissolve.EdgeSoftness);
	MID->SetScalarParameterValue(DissolveSurfaceGlowParamName, ActiveDissolve.SurfaceGlow);
	MID->SetScalarParameterValue(DissolveRiseParamName, ActiveDissolve.RiseAmount);
	MID->SetScalarParameterValue(DissolveEmberIntensityParamName, ActiveDissolve.EmberIntensity);
	MID->SetScalarParameterValue(DissolveHaloWidthParamName, ActiveDissolve.HaloWidth);

	//	여기서야 렌더 타겟을 잡는다. 손패에 있는 동안 켜 두면 장수만큼 RT 가 생긴다.
	Retainer_Card->SetRetainRendering(true);

	bDissolveActive = true;
	ApplyDissolve(0.f);   // 첫 프레임을 멀쩡한 카드로 맞춰 놓고 시작한다

	return true;
}

void UBGCardWidget::ApplyDissolve(float Amount)
{
	if (!CardFXMID)
	{
		return;
	}

	CardFXMID->SetScalarParameterValue(
		DissolveAmountParamName, FMath::Clamp(Amount, 0.f, 1.f));
}

// ══════════════════════════════════════════════════════════════════════════════════════
//  연출 (Presentation)
//
//  게임 상태와 화면 상태는 시간축이 다르다 — 손패에서 이미 빠진 카드도 Exiting 동안
//  화면에 남는다. 여기 있는 값은 전부 클라 로컬이고 서버는 알지 못한다.
// ══════════════════════════════════════════════════════════════════════════════════════

bool UBGCardWidget::IsLeavingHand() const
{
	return MotionState == EBGCardMotionState::Presenting
		|| MotionState == EBGCardMotionState::Dwelling
		|| MotionState == EBGCardMotionState::Exiting;
}

void UBGCardWidget::SetLayoutTarget(const FVector2D& InPosition, float InAngleDegrees)
{
	//	떠나는 중인 카드는 손패 자리로 되돌리면 안 된다 — 이미 손패 소속이 아니다.
	if (IsLeavingHand())
	{
		return;
	}

	TargetPosition = InPosition;
	TargetAngle    = InAngleDegrees;

	//	Pending 은 그대로 둔다. 대기가 끝나면 Moving 으로 넘어가면서 이 목표를 향한다.
	if (MotionState == EBGCardMotionState::Idle)
	{
		MotionState = EBGCardMotionState::Moving;
	}
}

void UBGCardWidget::SnapToLayoutTarget()
{
	//	⚠️ 등장/퇴장 중에는 스냅하지 않는다.
	//	   손패는 패널 크기를 처음 알게 된 프레임에 스냅을 건다. 그런데 판 첫 손패는
	//	   바로 그 프레임에 배분되므로, 여기서 걸러내지 않으면 "게임 첫 드로우만
	//	   연출 없이 툭 나타나는" 상태가 된다.
	if (bDealInProgress || IsLeavingHand())
	{
		return;
	}

	CurrentPosition = TargetPosition;
	CurrentAngle    = TargetAngle;
	CurrentScale    = 1.f;
	MotionState     = EBGCardMotionState::Idle;

	ApplyMotionTransform();
}

void UBGCardWidget::SetMotionPosition(const FVector2D& InPosition)
{
	CurrentPosition = InPosition;
	ApplyMotionTransform();
}

void UBGCardWidget::BeginDeal(const FVector2D& FromPosition, float Delay, const FBGCardDealContext& Context)
{
	CurrentPosition   = FromPosition;
	DealStartPosition = FromPosition;
	CurrentAngle      = TargetAngle + DealSpinDegrees;   // 기울어진 채로 출발해 날아오며 펴진다
	CurrentScale      = DealStartScale;
	DealElapsed       = 0.f;

	//	겨냥은 첫 비행 프레임에 잡는다. 지금 TargetPosition 을 읽으면 손패가 아직
	//	자리를 다시 잡기 전이라 옛 자리를 겨눈다(SetHand 는 BeginDeal 을 먼저 부른다).
	bDealAimInitialized = false;

	DealDelayRemaining = FMath::Max(0.f, Delay);
	DealContext        = Context;
	DealContext.FromPosition = FromPosition;   // 손패가 안 채웠어도 여기서는 확실히 안다
	bDealInProgress    = true;   // 자리에 도착할 때까지 스냅에서 보호한다

	if (DealFadeInDuration > 0.f)
	{
		SetRenderOpacity(0.f);
	}

	ApplyMotionTransform();

	if (DealDelayRemaining <= 0.f)
	{
		//	대기가 없으면 숨겼다 켜는 한 프레임 깜빡임이 생기므로 바로 출발한다.
		MotionState = (DealFlightDuration > 0.f)
			? EBGCardMotionState::Dealing
			: EBGCardMotionState::Moving;
		OnDealt(DealContext);
		return;
	}

	//	⚠️ 원래 가시성을 기억해 둔다. 되돌릴 때 Visible 로 하드코딩하면
	//	   WBP 가 정한 히트테스트 설정(SelfHitTestInvisible 등)을 덮어쓴다.
	PreDealVisibility = GetVisibility();
	SetVisibility(ESlateVisibility::Hidden);
	MotionState = EBGCardMotionState::Pending;
}

bool UBGCardWidget::BeginLeaveHand()
{
	if (IsLeavingHand())
	{
		return false;   // 이미 나가는 중이다. 다시 부르면 연출이 처음부터 되감긴다.
	}

	//	대기 중에 빠진 카드는 나타난 적이 없다. 숨은 채로 퇴장시키면
	//	"보이지도 않았는데 날아가는" 그림이 되므로 가시성을 되돌려 둔다.
	if (MotionState == EBGCardMotionState::Pending)
	{
		SetVisibility(PreDealVisibility);
	}

	//	떠나는 중에는 입력을 받지 않는다 — 날아가는 카드를 다시 집을 수 있으면 안 된다.
	PreLeaveVisibility = GetVisibility();
	SetVisibility(ESlateVisibility::HitTestInvisible);
	bDealInProgress = false;

	return true;
}

void UBGCardWidget::BeginExit(const FVector2D& ToPosition)
{
	if (!BeginLeaveHand())
	{
		return;
	}

	bHoldingAtCenter = false;   // 곧장 퇴장은 잡아 두지 않는다
	bPlayExit        = false;   // 버려지는 카드다 — 사용한 카드와 사라지는 방식이 다르다

	EnterExitPhase(ToPosition);
}

void UBGCardWidget::BeginPlayExit(const FVector2D& CenterPosition, const FVector2D& ToPosition)
{
	if (!BeginLeaveHand())
	{
		return;
	}

	//	중앙에 도착하면 놓아 줄 때까지 기다린다. 대상을 고르는 동안 카드가
	//	중앙에 서 있어야 무엇을 내고 있는지 보인다.
	bHoldingAtCenter = true;
	bPlayExit        = true;   // 사용한 카드다. 대기가 끝난 뒤 디졸브로 사라진다

	PresentStartPosition  = CurrentPosition;
	PresentStartScale     = CurrentScale;
	PresentStartAngle     = CurrentAngle;
	PresentTargetPosition = CenterPosition;
	PresentElapsed        = 0.f;

	//	대기가 끝난 뒤에 쓸 목적지. 지금 잡아 두는 이유는 그 시점에 손패가
	//	이미 다른 상태일 수 있어서다(앵커가 바뀌었을 수도 있다).
	PendingExitPosition = ToPosition;

	MotionState = EBGCardMotionState::Presenting;
}

void UBGCardWidget::FinishDwell()
{
	//	대기 중이 아니면 조용히 넘어간다 — 애니메이션 Finished 는 중복으로 올 수 있고,
	//	이미 퇴장에 들어간 카드를 다시 퇴장시키면 궤적이 처음부터 다시 그려진다.
	if (MotionState != EBGCardMotionState::Dwelling || bHoldingAtCenter)
	{
		return;
	}

	DwellRemaining = 0.f;
	EnterExitPhase(PendingExitPosition);
}

FBGCardPlayContext UBGCardWidget::ReleaseHold(const FBGCardPlayContext& Context)
{
	//	부르는 쪽은 판정만 알고 카드 데이터는 모른다. 빈 칸은 여기서 메운다 —
	//	두 해제 경로(판정 통지 / 손패 갱신 폴백)가 같은 결과를 갖게 하는 지점이다.
	FBGCardPlayContext Filled = Context;
	if (Filled.Archetype == EBGCardFXArchetype::None)
	{
		Filled.Archetype = CardView.FXArchetype;
	}

	if (!bHoldingAtCenter)
	{
		return Filled;   // 이미 풀렸다. 맥락만 돌려준다.
	}

	bHoldingAtCenter = false;

	//	대기 시간은 "풀린 시점" 부터 센다. 중앙에 도착한 시점부터 세면
	//	대상을 오래 고른 사람은 효과를 볼 시간이 없다.
	DwellRemaining = DwellDuration;

	//	아직 중앙으로 가는 중이면 도착 후 자연스럽게 대기로 넘어간다.
	if (MotionState == EBGCardMotionState::Dwelling || MotionState == EBGCardMotionState::Presenting)
	{
		//	등급 연출을 BP 훅보다 먼저 터뜨린다. BP 가 이 위에 무엇을 얹든
		//	글로우는 이미 올라와 있어야 "판정이 확정된 순간" 으로 읽힌다.
		PlayTierFX(Filled.Tier);

		OnPlayResolved(Filled);
	}

	return Filled;
}

bool UBGCardWidget::ReturnToHand()
{
	//	이미 사라지는 중이면 되돌리지 않는다. 반투명해진 카드가 손패에 다시
	//	나타나면 더 이상해 보인다 — 그냥 사라지게 두고 손패가 새로 만든다.
	if (!IsLeavingHand() || MotionState == EBGCardMotionState::Exiting)
	{
		return false;
	}

	bHoldingAtCenter = false;
	bPlayExit        = false;   // 더 이상 사용 중인 카드가 아니다
	DwellRemaining   = 0.f;

	SetRenderOpacity(1.f);
	SetVisibility(PreLeaveVisibility);

	//	손패가 곧 SetLayoutTarget 으로 자리를 알려 준다. 그때까지는 현재 위치에서 기다린다.
	MotionState = EBGCardMotionState::Moving;

	OnReturnedToHand();
	return true;
}

void UBGCardWidget::EnterExitPhase(const FVector2D& ToPosition)
{
	ExitStartPosition = CurrentPosition;
	ExitStartScale    = CurrentScale;
	ExitElapsed       = 0.f;
	TargetPosition    = ToPosition;
	MotionState       = EBGCardMotionState::Exiting;

	//	⚠️ BP 훅보다 먼저 건다. BP 가 여기에 무엇을 얹든 카드는 이미 타기 시작해야
	//	   "사라지는 중" 으로 읽힌다(PlayTierFX 를 OnPlayResolved 앞에 두는 것과 같은 이유).
	BeginDissolveExit();

	OnExitStarted();
}

bool UBGCardWidget::IsExitFinished() const
{
	//	Presenting / Dwelling 은 아직 끝이 아니다 — 여기서 true 를 돌려주면
	//	중앙에 올라가자마자 파괴된다.
	return MotionState == EBGCardMotionState::Exiting && ExitElapsed >= ExitDuration;
}

bool UBGCardWidget::IsHoverActive() const
{
	//	⚠️ 보간값(HoverBlend)이 아니라 실제 마우스 상태를 본다.
	//	   연출이 풀리는 동안에도 위층에 남아 있으면, 커서를 옆 카드로 옮긴 직후
	//	   방금 떠난 카드가 새로 고른 카드를 잠깐 가린다.
	//
	//	⚠️ 호버 연출이 억제되는 상태(잠금·드래그·등장/퇴장 중)에서는 false 여야 한다.
	//	   예전에는 여기서 bHovered 만 봤는데, 그러면 손패가 잠긴 동안 카드가
	//	   **떠오르지는 않으면서 Z 순서만** 올라갔다. 부채꼴에서 카드가 서로 겹쳐
	//	   있으므로 앞뒤가 바뀌는 순간 커서 아래 카드가 뒤바뀌고, 그게 다시 호버를
	//	   옮겨 Z 순서를 또 바꾼다 — 손패 전체가 깜빡이는 되먹임이 된다.
	//	   (내 턴에는 카드가 실제로 떠올라 커서를 확실히 덮으므로 잘 드러나지 않는다.)
	return bHovered
		&& !bIsDragVisual
		&& !bBeingDragged
		&& !bDragLocked
		&& !IsLeavingHand()
		&& !bDealInProgress;
}

void UBGCardWidget::TickMotion(float DeltaTime)
{
	//	등급 연출은 이동 상태와 무관하게 굴러야 한다 — 카드가 Idle 로 멈춰도 잦아드는 중일 수 있다.
	TickTierFX(DeltaTime);

	const FVector2D PositionBeforeTick = CurrentPosition;

	switch (MotionState)
	{
	case EBGCardMotionState::Pending:
		if (!TickPending(DeltaTime))
		{
			return;   // 아직 숨어 있다. 트랜스폼도 건드릴 것이 없다.
		}
		break;

	case EBGCardMotionState::Dealing:    TickDealing(DeltaTime);    break;
	case EBGCardMotionState::Moving:     TickMoving(DeltaTime);     break;
	case EBGCardMotionState::Presenting: TickPresenting(DeltaTime); break;
	case EBGCardMotionState::Dwelling:   TickDwelling(DeltaTime);   break;
	case EBGCardMotionState::Exiting:    TickExiting(DeltaTime);    break;

	default:
		//	Idle — 주 움직임은 없지만 부수 효과는 계속 굴러야 한다.
		//	기울기가 0 으로 풀리고 호버가 붙고 떨어지는 것이 전부 여기서 일어난다.
		break;
	}

	TickSecondaryMotion(DeltaTime, PositionBeforeTick);

	//	완전히 정지했고 얹힐 효과도 없으면 트랜스폼을 건드리지 않는다 —
	//	매 프레임 SetPosition 을 부르면 손패 전체가 계속 슬레이트를 무효화한다.
	const bool bFullyAtRest = MotionState == EBGCardMotionState::Idle
		&& FMath::IsNearlyZero(VelocityTilt, 0.01f)
		&& FMath::IsNearlyZero(HoverBlend, 0.001f);

	if (!bFullyAtRest)
	{
		ApplyMotionTransform();
	}
}

bool UBGCardWidget::TickPending(float DeltaTime)
{
	DealDelayRemaining -= DeltaTime;
	if (DealDelayRemaining > 0.f)
	{
		return false;
	}

	SetVisibility(PreDealVisibility);
	MotionState = (DealFlightDuration > 0.f)
		? EBGCardMotionState::Dealing
		: EBGCardMotionState::Moving;
	OnDealt(DealContext);

	return true;
}

void UBGCardWidget::TickDealing(float DeltaTime)
{
	DealElapsed += DeltaTime;

	const float Alpha = Progress(DealElapsed, DealFlightDuration);

	if (!bDealAimInitialized)
	{
		//	첫 비행 프레임. 여기서야 목표가 유효하다(BeginDeal 은 손패가 자리를
		//	다시 잡기 전에 불린다). 호가 휠 방향도 이때 한 번만 정하고 고정한다.
		DealAimPosition      = TargetPosition;
		DealArcDirection     = UpwardPerpendicular(TargetPosition - DealStartPosition);
		bDealAimInitialized  = true;
	}
	else
	{
		//	손패가 벌어지며 자리가 밀린다. 그 순간이동을 그대로 따라가지 않고
		//	겨냥이 천천히 쫓아가게 해서 궤적을 하나의 곡선으로 유지한다.
		DealAimPosition = FMath::Vector2DInterpTo(
			DealAimPosition, TargetPosition, DeltaTime, DealAimInterpSpeed);
	}

	//	가속 → 감속. 출발이 부드러워야 "미끄러져 나온다" 로 읽힌다.
	//	감속만 하면 첫 몇 프레임에 거리의 절반 가까이를 가버려서 순간이동처럼 보인다.
	const float PositionEased = bDealEaseInOut
		? FMath::InterpEaseInOut(0.f, 1.f, Alpha, DealEaseExponent)
		: FMath::InterpEaseOut(0.f, 1.f, Alpha, DealEaseExponent);

	//	활 모양 궤적. sin 은 양끝이 0 이라 출발점·도착점은 정확히 유지된다.
	const float ArcOffset = DealArcHeight * FMath::Sin(PI * Alpha);

	CurrentPosition = FMath::Lerp(DealStartPosition, DealAimPosition, PositionEased)
		+ DealArcDirection * ArcOffset;

	//	기울기를 편다. 위치보다 조금 빨리 정렬돼야 도착 순간에 반듯해 보인다.
	CurrentAngle = FMath::Lerp(TargetAngle + DealSpinDegrees, TargetAngle, PositionEased);

	//	제 크기를 살짝 지나쳤다 돌아온다 — "툭 놓이는" 탄력.
	CurrentScale = FMath::Lerp(DealStartScale, 1.f, EaseOutBack(Alpha, DealScaleOvershoot));

	if (DealFadeInDuration > 0.f)
	{
		SetRenderOpacity(Progress(DealElapsed, DealFadeInDuration));
	}

	if (Alpha >= 1.f)
	{
		//	투명도를 확실히 되돌린다. 반올림으로 0.99 가 남으면 손패 카드 하나만
		//	미묘하게 흐린 채로 굳는다.
		SetRenderOpacity(1.f);

		//	보간 단계로 넘긴다. 여기서 Idle 로 바로 가지 않는 이유는 비행 중에
		//	목표가 바뀌었을 수 있어서다 — 남은 오차를 보간이 부드럽게 흡수한다.
		MotionState = EBGCardMotionState::Moving;
	}
}

void UBGCardWidget::TickMoving(float DeltaTime)
{
	CurrentPosition = FMath::Vector2DInterpTo(CurrentPosition, TargetPosition, DeltaTime, MoveInterpSpeed);
	CurrentAngle    = FMath::FInterpTo(CurrentAngle, TargetAngle, DeltaTime, AngleInterpSpeed);
	CurrentScale    = FMath::FInterpTo(CurrentScale, 1.f, DeltaTime, ScaleInterpSpeed);

	//	InterpTo 는 목표에 정확히 닿지 않는다. 충분히 가까우면 붙이고 틱을 멈춘다 —
	//	안 그러면 손패의 모든 카드가 영원히 미세하게 움직이며 틱을 먹는다.
	const bool bArrived =
		CurrentPosition.Equals(TargetPosition, 0.5f)
		&& FMath::IsNearlyEqual(CurrentAngle, TargetAngle, 0.1f)
		&& FMath::IsNearlyEqual(CurrentScale, 1.f, 0.005f);

	if (bArrived)
	{
		CurrentPosition = TargetPosition;
		CurrentAngle    = TargetAngle;
		CurrentScale    = 1.f;
		MotionState     = EBGCardMotionState::Idle;
		bDealInProgress = false;
	}
}

void UBGCardWidget::TickPresenting(float DeltaTime)
{
	PresentElapsed += DeltaTime;

	const float Alpha = Progress(PresentElapsed, PresentDuration);
	//	중앙으로는 빠르게 나갔다가 부드럽게 멈춘다 — "탁 놓는" 느낌.
	const float Eased = FMath::InterpEaseOut(0.f, 1.f, Alpha, 2.f);

	CurrentPosition = FMath::Lerp(PresentStartPosition, PresentTargetPosition, Eased);
	CurrentScale    = FMath::Lerp(PresentStartScale, PresentScale, Eased);
	//	부채꼴로 기울어 있던 각도를 편다. 중앙에서는 똑바로 서야 읽기 좋다.
	CurrentAngle    = FMath::Lerp(PresentStartAngle, 0.f, Eased);

	if (Alpha >= 1.f)
	{
		MotionState = EBGCardMotionState::Dwelling;
		OnPlayPresented();
	}
}

void UBGCardWidget::TickDwelling(float DeltaTime)
{
	//	중앙에 가만히 서 있는다.
	//	  잡혀 있는 동안(대상 선택 대기)   → 시간이 흐르지 않는다
	//	  풀린 뒤(효과 발동됨)            → DwellDuration 만큼 더 보여 주고 사라진다
	if (bHoldingAtCenter)
	{
		return;   // 부수 효과는 계속 굴러야 기울기가 0 으로 되돌아온다
	}

	//	애니메이션이 길이를 쥐는 모드에서도 시계는 계속 돈다 — 다만 그때 DwellDuration 은
	//	연출 길이가 아니라 **안전 상한**이다. BP 가 FinishDwell 을 빠뜨려도 카드가
	//	중앙에 영원히 박히지 않게 하는 장치다(그 상태는 에러도 안 나고 손패가 잠긴다).
	DwellRemaining -= DeltaTime;

	if (DwellRemaining <= 0.f)
	{
		if (bWaitForDwellAnimation)
		{
			//	상한까지 왔는데 아직 신호가 없다. 연출이 잘린 것이므로 원인을 남긴다.
			UE_LOG(LogBGCard, Warning,
				TEXT("[Card] 대기 상한(%.2f초) 초과 — FinishDwell 이 안 왔다. "
				     "WBP 애니메이션 Finished 에 FinishDwell 을 물렸는지, "
				     "애니메이션이 DwellDuration 보다 길지 않은지 확인할 것."),
				DwellDuration);
		}

		EnterExitPhase(PendingExitPosition);
	}
}

void UBGCardWidget::TickExiting(float DeltaTime)
{
	ExitElapsed += DeltaTime;

	const float Alpha = Progress(ExitElapsed, ExitDuration);

	CurrentPosition = FMath::Lerp(ExitStartPosition, TargetPosition, FMath::InterpEaseIn(0.f, 1.f, Alpha, 2.f));
	CurrentScale    = FMath::Lerp(ExitStartScale, ExitEndScale, Alpha);

	if (bDissolveActive)
	{
		//	⚠️ 디졸브 중에는 불투명도를 건드리지 않는다. 타면서 동시에 흐려지면
		//	   경계의 불꽃이 묻혀서 그냥 페이드아웃처럼 보인다.
		//	   투명도가 필요하면 머테리얼 안에서 낼 것.
		//	원형이 자기 곡선을 들고 있으면 그것이 이긴다. 공격은 뒤가 가파르고
		//	회복은 고르게 — 곡선 하나로는 둘 다 만족시킬 수 없다.
		const UCurveFloat* ExitCurve = ActiveDissolve.Curve ? ActiveDissolve.Curve.Get() : DissolveCurve.Get();
		ApplyDissolve(CurveOr(ExitCurve, Alpha, Alpha));
	}
	else
	{
		SetRenderOpacity(1.f - Alpha);
	}
}

void UBGCardWidget::TickSecondaryMotion(float DeltaTime, const FVector2D& PositionBeforeTick)
{
	if (DeltaTime <= 0.f)
	{
		return;
	}

	// ── 관성 기울기 ──
	//	속도는 위치 차분으로 구한다. 별도 변수로 적분하면 스냅이나 순간이동
	//	(드롭 지점으로 옮기는 등) 때 실제 화면 움직임과 어긋난다.
	const float HorizontalSpeed = (CurrentPosition.X - PositionBeforeTick.X) / DeltaTime;

	//	이동 방향으로 눕는다. 부호가 반대면 진행 방향을 거슬러 서는 것처럼 보인다.
	const float DesiredTilt = FMath::Clamp(
		HorizontalSpeed * VelocityTiltPerSpeed, -MaxVelocityTilt, MaxVelocityTilt);

	VelocityTilt = FMath::FInterpTo(VelocityTilt, DesiredTilt, DeltaTime, VelocityTiltInterpSpeed);

	// ── 호버 ──
	//	손패에 얌전히 있는 카드만 반응한다. 날아오는 중이거나 떠나는 중인 카드가
	//	커서에 반응하면 연출이 두 개 겹쳐 흔들린다.
	//
	//	⚠️ 조건은 IsHoverActive 하나만 쓴다. 예전에는 여기(연출)와 손패의 Z 순서가
	//	   각자 판단해서, "안 떠오르는데 앞으로 나오는" 상태가 생겼다.
	const bool bHoverAllowed = IsHoverActive();
	HoverBlend = FMath::FInterpTo(HoverBlend, bHoverAllowed ? 1.f : 0.f, DeltaTime, HoverInterpSpeed);

	//	커서 위치도 보간해서 쓴다. 마우스 이벤트 값을 그대로 먹이면 커서를 빠르게
	//	움직일 때 기울기가 프레임마다 튀어 카드가 덜덜 떤다.
	//
	//	커서가 카드를 벗어나면(호버 해제) 마지막 위치가 그대로 남는다. 그러면 다음에
	//	반대쪽에서 들어올 때 그 각도에서 출발해 홱 돈다 — 풀리는 동안 0 으로 되돌린다.
	const FVector2D DesiredHoverLocal = bHoverAllowed ? HoverLocalNormalized : FVector2D::ZeroVector;
	HoverLocalSmoothed = (HoverFollowSpeed > 0.f)
		? FMath::Vector2DInterpTo(HoverLocalSmoothed, DesiredHoverLocal, DeltaTime, HoverFollowSpeed)
		: DesiredHoverLocal;

	//	상태가 바뀌는 순간에만 BP 로 알린다. 매 프레임 쏘면 BP 쪽 애니메이션이
	//	계속 0 프레임으로 되감겨 아무것도 재생되지 않는다.
	if (bHoverAllowed != bWasHoverActive)
	{
		bWasHoverActive = bHoverAllowed;
		RefreshChanceModifiedBanner();   // 문구는 호버가 켜지고 꺼질 때만 바뀐다
		OnHoverChanged(bHoverAllowed);
	}

	//	보간이 끝난 HoverBlend 를 머테리얼로 흘려보낸다. 상태가 바뀌는 순간이 아니라
	//	매 프레임인 이유는, 잦아드는 동안에도 글로우가 같이 옅어져야 하기 때문이다.
	TickHoverGlow();
}

void UBGCardWidget::ApplyMotionTransform()
{
	//	배치가 정한 값 위에 부수 효과를 더한다. 순서가 아니라 합이다 —
	//	배치는 배치대로, 호버는 호버대로 독립적으로 계산돼야 서로를 지우지 않는다.
	const FVector2D HoverOffset(0.f, HoverLift * HoverBlend);

	//	커서가 카드 중심에서 얼마나 벗어났는가(-1~+1)에 응답 곡선을 먹인 값.
	//	기울기와 비틀림이 같은 입력을 쓰므로 둘이 따로 놀지 않는다.
	const FVector2D HoverResponse(
		ShapeResponse(HoverLocalSmoothed.X, HoverResponseExponent),
		ShapeResponse(HoverLocalSmoothed.Y, HoverResponseExponent));

	const float HoverTilt = HoverResponse.X * HoverTiltMax * HoverBlend;
	const float FinalScale = CurrentScale * FMath::Lerp(1.f, HoverScale, HoverBlend);

	//	'Slot' 은 UWidget 의 멤버명이라 지역 변수 이름을 달리한다 (C4458 은 에러로 승격됨).
	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Slot))
	{
		CanvasSlot->SetPosition(CurrentPosition + HoverOffset);
	}

	//	캔버스가 아니면(구형 HorizontalBox 손패) 위치는 패널이 정한다 — 회전/크기만 반영된다.
	SetRenderTransformAngle(CurrentAngle + VelocityTilt + HoverTilt);
	SetRenderScale(FVector2D(FinalScale, FinalScale));

	//	비틀림으로 입체감을 낸다. 커서가 위쪽이면 카드 윗변이 뒤로 눕는 것처럼 보인다.
	//	UMG 는 2D 라 진짜 3D 회전은 없지만, shear 로 그 인상을 만들 수 있다.
	if (HoverShearMax != 0.f)
	{
		SetRenderShear(FVector2D(
			-HoverResponse.Y * HoverShearMax * HoverBlend,
			 HoverResponse.X * HoverShearMax * HoverBlend));
	}
}

// ── 호버 입력 ────────────────────────────────────────────────────────────────

void UBGCardWidget::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);

	if (bIsDragVisual)
	{
		return;   // 커서를 따라다니는 복제본은 반응하지 않는다
	}

	bHovered = true;
}

void UBGCardWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);

	bHovered = false;

	//	커서 위치는 남겨 둔다. 0 으로 리셋하면 빠져나가는 순간 기울기가 튕겨 돌아간다 —
	//	HoverBlend 가 0 으로 줄면서 자연스럽게 풀리게 두는 편이 낫다.
}

FReply UBGCardWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bIsDragVisual)
	{
		return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
	}

	const FVector2D LocalSize = InGeometry.GetLocalSize();
	if (LocalSize.X > KINDA_SMALL_NUMBER && LocalSize.Y > KINDA_SMALL_NUMBER)
	{
		//	커서의 카드 안 위치를 -1 ~ +1 로. 카드 크기가 달라져도 같은 감도로 반응한다.
		const FVector2D Local = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
		HoverLocalNormalized = FVector2D(
			FMath::Clamp((Local.X / LocalSize.X - 0.5f) * 2.f, -1.f, 1.f),
			FMath::Clamp((Local.Y / LocalSize.Y - 0.5f) * 2.f, -1.f, 1.f));
	}

	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

FReply UBGCardWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// 드래그 비주얼 복제본은 입력을 받지 않는다.
	// 잠금 중(대상 선택 대기 등)에도 드래그를 시작시키지 않는다.
	if (bIsDragVisual || bDragLocked)
	{
		return FReply::Unhandled();
	}

	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		// 즉시 드래그하지 않고 임계 거리를 넘어야 드래그로 승격된다.
		return UWidgetBlueprintLibrary::DetectDragIfPressed(InMouseEvent, this, EKeys::LeftMouseButton).NativeReply;
	}

	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UBGCardWidget::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
	Super::NativeOnDragDetected(InGeometry, InMouseEvent, OutOperation);

	if (bIsDragVisual || !CardView.IsValid())
	{
		return;
	}

	UBGCardDragDropOperation* Operation = Cast<UBGCardDragDropOperation>(
		UWidgetBlueprintLibrary::CreateDragDropOperation(UBGCardDragDropOperation::StaticClass()));
	if (!Operation)
	{
		return;
	}

	Operation->CardView         = CardView;
	Operation->SourceCardWidget = this;
	Operation->SourceHandWidget = OwningHand;
	Operation->Pivot            = EDragPivot::MouseDown;

	// 커서를 따라다닐 복제 위젯. DragVisualClass 가 없으면 자기 클래스를 쓴다.
	UClass* VisualClass = DragVisualClass ? DragVisualClass.Get() : GetClass();
	if (UBGCardWidget* Visual = CreateWidget<UBGCardWidget>(this, VisualClass))
	{
		Visual->bIsDragVisual = true;
		Visual->SetCardView(CardView);
		Operation->DefaultDragVisual = Visual;
	}

	SetBeingDragged(true);
	OutOperation = Operation;
}
