// UBGCardWidget — 머테리얼 연출 (등급 글로우 · 퇴장 디졸브).
//
// 이 파일이 UBGCardWidget 의 DMI 를 다루는 유일한 곳이다. 나머지 파일은 여기가 만든
// CardFXMID 를 쓸 뿐 직접 만들지 않는다 — 손패 전원이 인스턴스를 공유하는 사고
// (GetCardFXMID 주석 참고)를 한 지점에서만 막을 수 있게 하려는 것이다.

#include "UI/Widget/BGCardWidget.h"
#include "UI/Widget/BGCardWidgetMotionMath.h"

#include "Components/Border.h"
#include "Components/RetainerBox.h"
#include "Curves/CurveFloat.h"
#include "Debug/BGCardLog.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"


// ══════════════════════════════════════════════════════════════════════════════════════
//  등급 연출 (판정 4단계를 글로우로)
//
//  ⚠️ 시간축을 여기서 굴리는 것은 카드 한 장짜리라 성립하는 단순화다. 연출이 연쇄될 때는
//     중재할 곳이 필요하므로, 나중에 연출 큐로 옮기기 쉽도록 재생(PlayTierFX) ·
//     진행(TickTierFX) · 적용(ApplyTierFX)을 세 함수로 갈라 두었다.
// ══════════════════════════════════════════════════════════════════════════════════════

void UBGCardWidget::PushWidgetSize(UMaterialInstanceDynamic* MID)
{
	//	팽창 반경을 UV 로 바꾸려면 픽셀 크기가 필요하다 — 안 먹이면 외곽선이 눌린 타원이 된다.
	//	매번 재는 이유는 손패 재배치·호버 스케일로 크기가 계속 변하기 때문이다.
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

//	호버 외곽선 글로우를 먹이던 TickHoverGlow 는 없앴다 — 세기(HoverAmount)는 이제 WBP 의
//	UMG 애니메이션이 몬다(코드가 같이 밀면 마지막에 쓴 쪽이 이긴다). 원래는 리테이너의
//	Effect Material 에 값을 밀었는데, 리테이너는 평상시 꺼 두므로 애초에 화면에 나온 적이 없다.

const FBGCardTierFX* UBGCardWidget::FindTierFX(EBGResolutionTier Tier) const
{
	switch (Tier)
	{
	case EBGResolutionTier::CriticalFailure: return &CriticalFailureFX;
	case EBGResolutionTier::Failure:         return &FailureFX;
	case EBGResolutionTier::Success:         return &SuccessFX;
	case EBGResolutionTier::CriticalSuccess: return &CriticalSuccessFX;

	//	None = 판정하지 않은 카드(장비 등). 기본값으로 떨어뜨리면 장비가 대실패처럼 보인다.
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

	const float Alpha = BGCardMotion::Progress(TierFXElapsed, ActiveTierFX.Seconds);

	//	커브가 모양을 정한다(0~1 → 1~0). 없으면 선형으로 잦아든다.
	ApplyTierFX(BGCardMotion::CurveOr(TierFXCurve, Alpha, 1.f - Alpha));

	if (Alpha >= 1.f)
	{
		bTierFXActive = false;
		ApplyTierFX(0.f);   // 평상시 값으로 정확히 맞춰 놓고 끝낸다
	}
}

// ══════════════════════════════════════════════════════════════════════════════════════
//  퇴장 디졸브
//
//  카드 트리 전체(테두리·그림·글씨)를 RetainerBox 로 한 장의 그림으로 만든 뒤 태우거나 자른다.
//  Img_Art 에 물리면 그림만 타고 글씨가 남는다.
//  진행도는 퇴장 보간(ExitElapsed/ExitDuration)을 그대로 쓴다 — 별도 시계를 두면
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

	//	카드 전체를 대상으로 하는 연출(디졸브·글로우)이 이 DMI 하나를 나눠 쓴다.
	//	⚠️ DMI 없이 MI 를 그대로 쓰면 손패 전원이 같은 인스턴스를 공유한다 —
	//	   한 장이 대성공 나면 손패가 통째로 번쩍인다.
	CardFXMID = Retainer_Card->GetEffectMaterial();
	if (!CardFXMID)
	{
		//	리테이너는 뒀는데 머테리얼이 없다 — 설정을 하다 만 것이므로 원인을 남긴다
		//	(리테이너가 아예 없는 경우와 달리 의도한 상태가 아니다).
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
		//	선택 사항이라 불투명도 페이드로 떨어질 뿐이지만, 이름을 틀렸거나 'Is Variable' 을
		//	안 켠 경우와 구분이 안 되므로 실마리는 남긴다(평소에는 안 보이도록 Verbose).
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

	//	매 프레임 바뀌는 것은 타들어간 정도뿐이다 — 나머지는 여기서 한 번만 먹인다.
	if (!DissolveMaskParamName.IsNone() && ActiveDissolve.MaskTexture)
	{
		MID->SetTextureParameterValue(DissolveMaskParamName, ActiveDissolve.MaskTexture);
	}

	//	재·잔불은 공격만 쓴다. 회복은 EmberIntensity 가 0 이라 텍스처를 안 넣어도 된다.
	if (!DissolveEmberMaskParamName.IsNone() && ActiveDissolve.EmberMask)
	{
		MID->SetTextureParameterValue(DissolveEmberMaskParamName, ActiveDissolve.EmberMask);
	}

	MID->SetVectorParameterValue(DissolveEdgeColorParamName, ActiveDissolve.EdgeColor);
	MID->SetScalarParameterValue(DissolveEdgeWidthParamName, ActiveDissolve.EdgeWidth);
	MID->SetScalarParameterValue(DissolveEdgeIntensityParamName, ActiveDissolve.EdgeIntensity);
	MID->SetScalarParameterValue(DissolveSeparateParamName, ActiveDissolve.SeparateAmount);
	MID->SetScalarParameterValue(DissolveCutAngleParamName, ActiveDissolve.CutAngleDegrees);

	//	여기부터가 공격과 회복을 가르는 값들이다. 머테리얼은 하나고 이 숫자만 달라진다 —
	//	원형마다 머테리얼을 두면 DMI 와 셰이더 퍼뮤테이션이 그만큼 늘어난다.
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
