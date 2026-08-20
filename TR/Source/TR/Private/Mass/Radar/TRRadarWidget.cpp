// TRRadarWidget.cpp

#include "Mass/Radar/TRRadarWidget.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Rendering/DrawElements.h"
#include "Mass/Radar/TRRadarSubsystem.h"
#include "Train/TRTrain.h"

namespace
{
	/** 기차 탐색에 실패했을 때 다음 시도까지 기다리는 시간(초). */
	constexpr double TrainSearchCooldownSeconds = 0.5;

	/**
	 * 이만큼 시간이 건너뛰면 파형 일정을 따라잡지 않고 지금으로 리셋한다.
	 *
	 * 레벨 로딩이나 일시정지 해제로 월드 시간이 크게 튀면 따라잡기 루프가
	 * 수천 번 돌 수 있다. 어차피 그 사이의 파형은 아무도 못 봤다.
	 */
	constexpr double PulseCatchUpLimitSeconds = 5.0;

	/**
	 * 파형 시작 간격의 절대 하한.
	 *
	 * 0이면 일정 따라잡기 루프가 시간을 전혀 못 밀어내고 그대로 멈춘다.
	 * 성능 여유가 아니라 무한 루프를 막는 값이다.
	 */
	constexpr double MinPulseIntervalSeconds = 0.05;

	/** 브러시 없는 파형의 링 사이 간격(진행도 기준). */
	constexpr double PulseTrailSpacing = 0.045;

	/**
	 * 반지름에 맞는 분할 수. 작은 레이더에 48각형을 그릴 이유가 없다.
	 *
	 * 픽셀당 한 변씩 주면 과하고, 대략 6px 마다 한 변이면 곡선으로 보인다.
	 */
	int32 CircleSegmentsFor(double Radius)
	{
		return FMath::Clamp(FMath::RoundToInt(Radius / 6.0), 12, 48);
	}

	/** 원 위의 점들. 마지막에 첫 점을 다시 넣어 닫는다. */
	TArray<FVector2f> MakeCirclePoints(const FVector2D& Center, double Radius, int32 Segments)
	{
		TArray<FVector2f> Points;
		Points.Reserve(Segments + 1);

		for (int32 Index = 0; Index <= Segments; ++Index)
		{
			const double Angle = (2.0 * UE_DOUBLE_PI * Index) / Segments;
			Points.Emplace(
				static_cast<float>(Center.X + FMath::Sin(Angle) * Radius),
				static_cast<float>(Center.Y - FMath::Cos(Angle) * Radius));
		}

		return Points;
	}

	/**
	 * 화면 방향 벡터. 각도는 **위(-Y)에서 시계 방향**으로 잰다.
	 *
	 * Slate 는 Y 가 아래로 자라므로 월드의 수학과 부호가 반대다. 이 규칙을
	 * 한 곳에 모아 두지 않으면 스윕과 점의 각도 기준이 조용히 어긋난다.
	 */
	FVector2D ScreenDirectionFromAngle(float AngleRad)
	{
		return FVector2D(FMath::Sin(AngleRad), -FMath::Cos(AngleRad));
	}
}

bool UTRRadarWidget::HasAuthoredArt(const FSlateBrush& Brush)
{
	return Brush.GetResourceObject() != nullptr
		|| Brush.DrawAs == ESlateBrushDrawType::RoundedBox;
}

AActor* UTRRadarWidget::ResolveCenterActor() const
{
	if (AActor* Override = CenterActorOverride.Get())
	{
		return Override;
	}

	if (AActor* Cached = CachedTrain.Get())
	{
		return Cached;
	}

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return nullptr;
	}

	const APlayerController* PC = GetOwningPlayer();
	APawn* PawnFallback = PC ? PC->GetPawn() : nullptr;

	// 실패한 탐색에 쿨다운을 건다. 기차가 없는 맵에서 이걸 매 프레임 돌리면
	// 레벨 전체 훑기가 게임 스레드에 그대로 얹힌다. 그리기는 프레임당 여러 번
	// 불릴 수도 있어서 실제로는 더 나쁘다.
	const double Now = World->GetTimeSeconds();
	if (Now < NextTrainSearchTime)
	{
		return PawnFallback;
	}

	NextTrainSearchTime = Now + TrainSearchCooldownSeconds;

	// 기차를 직접 찾는다. UTRTargetDirectorSubsystem 을 쓰고 싶어지지만
	// 목표 등록이 HasAuthority 로 막혀 있어 **클라이언트에서는 비어 있다.**
	// 레이더는 양쪽에서 똑같이 돌아야 하므로 레벨 액터를 직접 본다.
	for (TActorIterator<ATRTrain> It(World); It; ++It)
	{
		CachedTrain = *It;
		return *It;
	}

	// 기차가 아직 없는 프레임(로딩 중 등)에는 폰으로 버틴다. 캐시하지 않는다 —
	// 다음 프레임에 기차가 생기면 그때 잡아야 한다.
	return PawnFallback;
}

bool UTRRadarWidget::ResolveViewPoint(FVector& OutCenter, float& OutYaw) const
{
	const AActor* Center = ResolveCenterActor();
	if (Center == nullptr)
	{
		return false;
	}

	OutCenter = Center->GetActorLocation();

	switch (Orientation)
	{
	case ETRRadarOrientation::ViewForward:
		{
			// 폰이 아니라 컨트롤 회전을 쓴다. 폰이 몸을 늦게 돌리는 구성에서
			// 폰 Yaw 를 쓰면 레이더가 화면보다 뒤늦게 도는 게 눈에 띈다.
			const APlayerController* PC = GetOwningPlayer();
			OutYaw = PC ? PC->GetControlRotation().Yaw : 0.f;
			break;
		}

	default:
		// 기차 앞을 위로. 기차는 트레드밀에서 회전하지 않으므로 이 값은
		// 사실상 상수이고, 결과적으로 레이더가 돌지 않는다.
		OutYaw = Center->GetActorRotation().Yaw;
		break;
	}

	return true;
}

double UTRRadarWidget::RollPulseInterval(double TravelSeconds) const
{
	// 상한이 하한보다 작게 설정돼도 뒤집지 않고 하한으로 눌러 둔다.
	// 뒤집으면 "왜 내가 넣은 최소값보다 짧게 나오지"가 되어 더 헷갈린다.
	const double Min = FMath::Max(static_cast<double>(PulseIntervalMin), 0.0);
	const double Max = FMath::Max(static_cast<double>(PulseIntervalMax), Min);

	double Interval = PulseStream.FRandRange(Min, Max);

	// 겹침을 끄면 앞 파형이 가장자리에 닿은 뒤에야 다음이 출발한다.
	// 간격 값은 그대로 두고 이 스위치만으로 두 느낌을 비교할 수 있다.
	if (!bAllowOverlappingPulses)
	{
		Interval = FMath::Max(Interval, TravelSeconds);
	}

	// **0 간격은 무한 루프다.** 아래 따라잡기 루프가 시간을 전혀 못 밀어낸다.
	// 블루프린트에서도 값을 넣을 수 있으므로 바닥은 여기서 깐다.
	return FMath::Max(Interval, MinPulseIntervalSeconds);
}

void UTRRadarWidget::PushPulseStart(double StartTime) const
{
	const int32 Keep = FMath::Min(PulseStartCount, TrackedPulseCount - 1);

	for (int32 Index = Keep; Index > 0; --Index)
	{
		PulseStartTimes[Index] = PulseStartTimes[Index - 1];
	}

	PulseStartTimes[0] = StartTime;
	PulseStartCount = Keep + 1;
}

void UTRRadarWidget::AdvancePulseSchedule(double Now) const
{
	const double Travel = FMath::Max(static_cast<double>(PulseTravelSeconds), 0.05);

	if (!bPulseScheduleReady)
	{
		PulseStream.Initialize(FMath::Rand());
		PulseStartCount = 0;

		// 첫 파형은 바로 시작한다. 켜자마자 몇 초간 아무 일도 안 일어나면
		// 레이더가 고장 난 것처럼 보인다.
		PushPulseStart(Now);

		NextPulseStartTime = Now + RollPulseInterval(Travel);
		bPulseScheduleReady = true;

		return;
	}

	// 시간이 크게 튀었으면 따라잡지 않는다. 그 사이의 파형은 아무도 못 봤다.
	if (Now > NextPulseStartTime + PulseCatchUpLimitSeconds)
	{
		PulseStartCount = 0;
		PushPulseStart(Now);
		NextPulseStartTime = Now + RollPulseInterval(Travel);

		return;
	}

	// 예정 시각을 그대로 시작 시각으로 쓴다. Now 를 쓰면 프레임 경계만큼의
	// 오차가 매번 더해져서, 간격이 프레임 레이트에 따라 조금씩 늘어난다.
	while (Now >= NextPulseStartTime)
	{
		PushPulseStart(NextPulseStartTime);
		NextPulseStartTime += RollPulseInterval(Travel);
	}
}

bool UTRRadarWidget::TryGetLastPulseHitTime(double Now, double NormalizedDistance,
	double TravelSeconds, double& OutHitTime) const
{
	// 0번이 가장 최근이므로 각성 시각도 0번이 가장 늦다. 즉 앞에서부터 훑다가
	// 처음으로 "이미 지나갔다"가 되는 항목이 곧 가장 최근의 각성이다.
	for (int32 Index = 0; Index < PulseStartCount; ++Index)
	{
		const double HitTime = PulseStartTimes[Index] + NormalizedDistance * TravelSeconds;
		if (HitTime <= Now)
		{
			OutHitTime = HitTime;
			return true;
		}
	}

	return false;
}

void UTRRadarWidget::PaintGrid(const FGeometry& Geometry, FSlateWindowElementList& Out, int32 Layer,
	const FVector2D& Center, double RadiusPx) const
{
	const FPaintGeometry PaintGeometry = Geometry.ToPaintGeometry();

	// 0 이하 두께는 Slate 에서 정의되지 않은 취급이라 렌더러마다 다르게 나온다.
	// 블루프린트로도 바꿀 수 있으므로 여기서 바닥을 깐다.
	const float LineThickness = FMath::Max(GridThickness, 0.1f);

	// 바깥 테두리를 포함해 균등 간격으로 나눈다. RangeRingCount 가 2면
	// 1/3, 2/3, 그리고 사거리 경계에 하나씩 놓인다. 상한을 두는 이유는
	// 블루프린트가 큰 값을 넣으면 링 하나당 드로우 콜이 그대로 늘기 때문이다.
	const int32 RingCount = FMath::Clamp(RangeRingCount, 0, 16) + 1;
	for (int32 Ring = 1; Ring <= RingCount; ++Ring)
	{
		const double Radius = (RadiusPx * Ring) / RingCount;

		FSlateDrawElement::MakeLines(Out, Layer, PaintGeometry,
			MakeCirclePoints(Center, Radius, CircleSegmentsFor(Radius)), ESlateDrawEffect::None,
			GridTint, /*bAntialias=*/true, LineThickness);
	}

	if (!bDrawCrosshair)
	{
		return;
	}

	const float CenterX = static_cast<float>(Center.X);
	const float CenterY = static_cast<float>(Center.Y);
	const float Extent = static_cast<float>(RadiusPx);

	FSlateDrawElement::MakeLines(Out, Layer, PaintGeometry,
		TArray<FVector2f>{ FVector2f(CenterX - Extent, CenterY), FVector2f(CenterX + Extent, CenterY) },
		ESlateDrawEffect::None, GridTint, /*bAntialias=*/true, LineThickness);

	FSlateDrawElement::MakeLines(Out, Layer, PaintGeometry,
		TArray<FVector2f>{ FVector2f(CenterX, CenterY - Extent), FVector2f(CenterX, CenterY + Extent) },
		ESlateDrawEffect::None, GridTint, /*bAntialias=*/true, LineThickness);
}

void UTRRadarWidget::PaintSweep(const FGeometry& Geometry, FSlateWindowElementList& Out, int32 Layer,
	const FVector2D& Center, double RadiusPx, float SweepAngleRad) const
{
	// 브러시가 있으면 그림 한 장을 돌리는 게 항상 낫다. 아래의 선 부채꼴은
	// 애셋 없이도 형태가 보이게 하려는 대용품이다.
	if (HasAuthoredArt(SweepBrush))
	{
		const FVector2D BrushSize(RadiusPx * 2.0, RadiusPx * 2.0);
		const FPaintGeometry SweepGeometry = Geometry.ToPaintGeometry(
			BrushSize, FSlateLayoutTransform(Center - BrushSize * 0.5));

		FSlateDrawElement::MakeRotatedBox(Out, Layer, SweepGeometry, &SweepBrush,
			ESlateDrawEffect::None, SweepAngleRad, TOptional<FVector2f>(),
			FSlateDrawElement::RelativeToElement, PingTint);

		return;
	}

	const FPaintGeometry PaintGeometry = Geometry.ToPaintGeometry();

	// 세 값 모두 블루프린트에서 바뀔 수 있다. 분할 수는 그대로 드로우 콜이 되고,
	// 잔광 폭이 한 바퀴를 넘으면 자기 자신을 덮어 스윕이 원판처럼 보인다.
	const int32 Segments = FMath::Clamp(SweepTrailSegments, 1, 64);
	const float TrailRad = FMath::DegreesToRadians(FMath::Clamp(SweepTrailDegrees, 1.f, 180.f));
	const float LineThickness = FMath::Max(SweepThickness, 0.1f);

	// 선두에서 뒤로 갈수록 어두워진다. 잔광이 남은 것처럼 읽히는 최소한의 장치다.
	for (int32 Index = 0; Index < Segments; ++Index)
	{
		const float Ratio = static_cast<float>(Index) / static_cast<float>(Segments);
		const float Angle = SweepAngleRad - TrailRad * Ratio;

		FLinearColor LineTint = PingTint;
		LineTint.A *= (1.f - Ratio);

		const FVector2D Edge = Center + ScreenDirectionFromAngle(Angle) * RadiusPx;

		FSlateDrawElement::MakeLines(Out, Layer, PaintGeometry,
			TArray<FVector2f>{
				FVector2f(static_cast<float>(Center.X), static_cast<float>(Center.Y)),
				FVector2f(static_cast<float>(Edge.X), static_cast<float>(Edge.Y)) },
			ESlateDrawEffect::None, LineTint, /*bAntialias=*/true, LineThickness);
	}
}

void UTRRadarWidget::PaintPulses(const FGeometry& Geometry, FSlateWindowElementList& Out, int32 Layer,
	const FVector2D& Center, double RadiusPx, double Now) const
{
	const double Travel = FMath::Max(static_cast<double>(PulseTravelSeconds), 0.05);
	const int32 DrawLimit = FMath::Clamp(MaxConcurrentPulses, 1, TrackedPulseCount);

	int32 Drawn = 0;
	for (int32 Index = 0; Index < PulseStartCount && Drawn < DrawLimit; ++Index)
	{
		const double Progress = (Now - PulseStartTimes[Index]) / Travel;

		// 아직 시작 전이거나 이미 가장자리를 넘은 파형은 건너뛴다. 목록에서
		// 빼지는 않는다 — 그리기는 끝났어도 점을 각성시킨 기록으로는 남는다.
		if (Progress < 0.0 || Progress > 1.0)
		{
			continue;
		}

		PaintPulseRing(Geometry, Out, Layer, Center, RadiusPx, Progress);
		++Drawn;
	}
}

void UTRRadarWidget::PaintPulseRing(const FGeometry& Geometry, FSlateWindowElementList& Out, int32 Layer,
	const FVector2D& Center, double RadiusPx, double Progress) const
{
	// 쉬는 중이면 그릴 것이 없다. 이게 파형의 절반이다 — 아무것도 없는 시간이
	// 있어야 다음 파형이 사건으로 읽힌다.
	if (Progress < 0.0 || Progress > 1.0)
	{
		return;
	}

	// 가장자리에 가까워질수록 옅어진다. 끝에서 뚝 끊기면 벽에 부딪힌 것처럼 보인다.
	const float EdgeFade = static_cast<float>(1.0 - Progress);

	if (HasAuthoredArt(PulseBrush))
	{
		const double Diameter = FMath::Max(RadiusPx * 2.0 * Progress, 1.0);
		const FVector2D BrushSize(Diameter, Diameter);

		const FPaintGeometry PulseGeometry = Geometry.ToPaintGeometry(
			BrushSize, FSlateLayoutTransform(Center - BrushSize * 0.5));

		FLinearColor Tint = PingTint;
		Tint.A *= EdgeFade;

		FSlateDrawElement::MakeBox(Out, Layer, PulseGeometry, &PulseBrush,
			ESlateDrawEffect::None, Tint);

		return;
	}

	const FPaintGeometry PaintGeometry = Geometry.ToPaintGeometry();
	const int32 Rings = FMath::Clamp(PulseTrailRings, 1, 8);
	const float LineThickness = FMath::Max(PulseThickness, 0.1f);

	// 선두 링 뒤로 몇 겹을 더 깔아 띠처럼 보이게 한다. 뒤쪽일수록 옅다.
	for (int32 Ring = 0; Ring < Rings; ++Ring)
	{
		const double RingProgress = Progress - PulseTrailSpacing * Ring;
		if (RingProgress <= 0.0)
		{
			break;
		}

		const double Radius = RadiusPx * RingProgress;

		FLinearColor RingTint = PingTint;
		RingTint.A *= EdgeFade * (1.f - static_cast<float>(Ring) / static_cast<float>(Rings));

		FSlateDrawElement::MakeLines(Out, Layer, PaintGeometry,
			MakeCirclePoints(Center, Radius, CircleSegmentsFor(Radius)), ESlateDrawEffect::None,
			RingTint, /*bAntialias=*/true, LineThickness);
	}
}

int32 UTRRadarWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId,
	const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	LayerId = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId,
		InWidgetStyle, bParentEnabled);

	const UWorld* World = GetWorld();
	const UTRRadarSubsystem* Radar = World ? World->GetSubsystem<UTRRadarSubsystem>() : nullptr;
	if (Radar == nullptr)
	{
		return LayerId;
	}

	FVector ViewCenter = FVector::ZeroVector;
	float ViewYaw = 0.f;
	if (!ResolveViewPoint(ViewCenter, ViewYaw))
	{
		return LayerId;
	}

	const FVector2D LocalSize = AllottedGeometry.GetLocalSize();
	const FVector2D WidgetCenter = LocalSize * 0.5;

	// 원형 레이더라 짧은 변이 반지름을 정한다. 가로로 늘어난 칸에 넣어도
	// 점이 밖으로 새지 않는다.
	const double RadiusPx = FMath::Min(WidgetCenter.X, WidgetCenter.Y);

	// 레이아웃이 아직 안 잡힌 프레임에는 크기가 0이거나, 드물게 유한하지 않은
	// 값이 온다(애니메이션 중인 스케일이 0으로 나눠질 때 등). 그대로 그리면
	// 화면 전체를 덮는 쓰레기 사각형이 나온다.
	if (!FMath::IsFinite(RadiusPx) || RadiusPx <= 0.0 || RadarRangeCm <= 0.f)
	{
		return LayerId;
	}

	const double PixelPerCm = RadiusPx / RadarRangeCm;

	const int32 GridLayer = LayerId + 1;
	const int32 PingLayer = LayerId + 2;
	const int32 BlipLayer = LayerId + 3;
	const int32 CenterLayer = LayerId + 4;

	if (bDrawGrid)
	{
		PaintGrid(AllottedGeometry, OutDrawElements, GridLayer, WidgetCenter, RadiusPx);
	}

	// 게임 시간을 쓴다. 일시정지 중에는 핑도 멈추는 편이 맞다 —
	// 도는 스윕이나 퍼지는 파형은 "정보가 갱신되는 중"이라는 신호인데,
	// 멈춘 게임에서는 거짓말이다.
	const double Now = World->GetTimeSeconds();
	const float TwoPi = 2.f * UE_PI;

	float SweepAngle = 0.f;

	switch (PingMode)
	{
	case ETRRadarPingMode::Sweep:
		if (SweepPeriodSeconds > 0.f)
		{
			SweepAngle = FMath::Fmod(static_cast<float>(Now) / SweepPeriodSeconds, 1.f) * TwoPi;
			PaintSweep(AllottedGeometry, OutDrawElements, PingLayer, WidgetCenter, RadiusPx, SweepAngle);
		}
		break;

	case ETRRadarPingMode::Pulse:
		AdvancePulseSchedule(Now);
		PaintPulses(AllottedGeometry, OutDrawElements, PingLayer, WidgetCenter, RadiusPx, Now);
		break;

	default:
		break;
	}

	const bool bFading = bFadeBlipsWithPing && PingMode != ETRRadarPingMode::None;

	// 수집이 멈춘 뒤에도 배열은 마지막 내용을 들고 있다. 그 점들은 이제 아무
	// 뜻도 없으므로 그리지 않는다. 격자·핑·중심 표식은 계속 그린다 —
	// 레이더 자체가 사라지는 것보다 "적이 안 잡힌다"가 정직한 표현이다.
	const TArray<FTRRadarBlip>& Blips = Radar->GetBlips();
	const int32 BlipCount = Radar->IsDataFresh()
		? FMath::Min(Blips.Num(), FMath::Max(MaxDrawnBlips, 1))
		: 0;

	for (int32 BlipIndex = 0; BlipIndex < BlipCount; ++BlipIndex)
	{
		const FTRRadarBlip& Blip = Blips[BlipIndex];

		// 1) 중심 기준 평면 오프셋. 높이는 버린다 — 탑다운은 평면 정보다.
		const FVector Delta = Blip.Location - ViewCenter;
		FVector2D Planar(Delta.X, Delta.Y);

		// 2) 월드 벡터를 기준 방향으로 되돌린다. Yaw 가 0이면 항등이다.
		if (ViewYaw != 0.f)
		{
			Planar = Planar.GetRotated(-ViewYaw);
		}

		// 3) 월드(X 앞, Y 오른쪽) → 화면(X 오른쪽, Y 아래). Y 부호가 뒤집힌다.
		FVector2D ScreenOffset(Planar.Y * PixelPerCm, -Planar.X * PixelPerCm);

		const double Distance = ScreenOffset.Size();

		// 중심 액터의 트랜스폼이 깨졌으면 여기서 처음 드러난다. 점 하나를
		// 버리는 편이 화면 전체가 깨지는 것보다 낫다.
		if (!FMath::IsFinite(Distance))
		{
			continue;
		}

		const bool bOffRange = (Distance > RadiusPx);

		// 페이드는 원래 거리로 판단한다. 가장자리로 끌어당긴 뒤의 거리를 쓰면
		// 사거리 밖 적이 전부 같은 시점에 깜빡여서 한 덩어리로 보인다.
		const double TrueDistance = Distance;

		if (bOffRange)
		{
			if (!bShowOffRangeBlips)
			{
				continue;
			}

			// 가장자리로 끌어당긴다. 거리는 잃지만 방향은 남는다.
			ScreenOffset *= RadiusPx / Distance;
		}

		const FTRRadarBlipStyle* Style = BlipStyles.Find(Blip.Type);
		const FTRRadarBlipStyle& Resolved = (Style != nullptr) ? *Style : DefaultBlipStyle;

		FLinearColor Tint = Resolved.Tint;

		if (bFading)
		{
			// 지수는 블루프린트에서도 쓸 수 있으므로 여기서 한 번 더 막는다.
			// 0이면 Pow 가 항상 1을 돌려줘 페이드가 조용히 사라지고, 음수면
			// 밑이 0에 가까울 때 무한대가 나온다.
			const float Power = FMath::Max(BlipFadePower, 0.01f);
			float Freshness = 1.f;

			if (PingMode == ETRRadarPingMode::Sweep)
			{
				// 스윕이 이 방위를 지난 뒤 얼마나 돌았는가. 등속 회전이라
				// 각도 차이가 곧 경과 시간이고, 그래서 기록이 필요 없다.
				const float BlipAngle = FMath::Atan2(ScreenOffset.X, -ScreenOffset.Y);

				float Behind = FMath::Fmod(SweepAngle - BlipAngle, TwoPi);
				if (Behind < 0.f)
				{
					Behind += TwoPi;
				}

				Freshness = FMath::Pow(1.f - (Behind / TwoPi), Power);
			}
			else
			{
				// 파형은 거리에서 시각을 얻는다. 링이 이 점을 지난 시각은
				// "파형 시작 + 정규화 거리 × 이동 시간"이다.
				//
				// 파형이 겹칠 수 있으므로 살아 있는 것 중 **가장 최근에 지나간**
				// 링을 찾아야 한다. 최신 하나만 보면, 그 링이 아직 여기까지
				// 오지 않았을 때 이미 지나간 앞 링의 각성을 놓친다.
				const double Travel = FMath::Max(static_cast<double>(PulseTravelSeconds), 0.05);
				const double Normalized = FMath::Clamp(TrueDistance / RadiusPx, 0.0, 1.0);

				double HitTime = 0.0;
				if (TryGetLastPulseHitTime(Now, Normalized, Travel, HitTime))
				{
					const double FadeSeconds = FMath::Max(static_cast<double>(BlipFadeSeconds), 0.05);
					const double Age = FMath::Clamp((Now - HitTime) / FadeSeconds, 0.0, 1.0);

					Freshness = FMath::Pow(1.f - static_cast<float>(Age), Power);
				}
				else
				{
					// 어떤 링도 아직 닿지 않았다. 켠 직후 바깥쪽 점이 여기 해당한다.
					Freshness = 0.f;
				}
			}

			Tint.A *= FMath::Clamp(FMath::Max(Freshness, MinBlipOpacity), 0.f, 1.f);
		}

		// 크기가 0 이하인 스타일은 그릴 것이 없다. Slate 에 넘기면 조용히
		// 무시되지만, 드로우 엘리먼트만 쌓이므로 여기서 끊는다.
		if (Resolved.Size.X <= 0.0 || Resolved.Size.Y <= 0.0)
		{
			continue;
		}

		const FVector2D DrawPos = WidgetCenter + ScreenOffset - Resolved.Size * 0.5;
		const FPaintGeometry BlipGeometry = AllottedGeometry.ToPaintGeometry(
			Resolved.Size, FSlateLayoutTransform(DrawPos));

		const bool bUseArrow = bOffRange && HasAuthoredArt(OffRangeBrush);
		if (bUseArrow)
		{
			// 위(-Y)를 향하는 브러시를 바깥 방향으로 돌린다.
			const FVector2D Direction = ScreenOffset / RadiusPx;
			const float Angle = FMath::Atan2(Direction.X, -Direction.Y);

			FSlateDrawElement::MakeRotatedBox(OutDrawElements, BlipLayer, BlipGeometry,
				&OffRangeBrush, ESlateDrawEffect::None, Angle, TOptional<FVector2f>(),
				FSlateDrawElement::RelativeToElement, Tint);
		}
		else
		{
			FSlateDrawElement::MakeBox(OutDrawElements, BlipLayer, BlipGeometry,
				&Resolved.Brush, ESlateDrawEffect::None, Tint);
		}
	}

	// 기차 표식은 항상 맨 위에. 점에 가려지면 중심을 놓친다.
	if (CenterSize.X > 0.0 && CenterSize.Y > 0.0)
	{
		const FPaintGeometry MarkerGeometry = AllottedGeometry.ToPaintGeometry(
			CenterSize, FSlateLayoutTransform(WidgetCenter - CenterSize * 0.5));

		FSlateDrawElement::MakeBox(OutDrawElements, CenterLayer, MarkerGeometry,
			&CenterBrush, ESlateDrawEffect::None, CenterTint);
	}

	return CenterLayer;
}
