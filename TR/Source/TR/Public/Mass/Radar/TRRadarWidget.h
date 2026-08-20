// TRRadarWidget.h

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "Math/RandomStream.h"
#include "Styling/SlateBrush.h"
#include "Mass/Core/TRMassTypes.h"
#include "TRRadarWidget.generated.h"

/** 레이더의 위쪽이 무엇을 가리키는가. */
UENUM(BlueprintType)
enum class ETRRadarOrientation : uint8
{
	/**
	 * 기차 앞이 항상 위. 탑다운 레이더의 기본값이다.
	 *
	 * 기차가 월드에서 고정이므로 이 모드에서 레이더는 **절대 돌지 않는다.**
	 * 그게 핵심이다 — 화면 왼쪽에 찍힌 점은 언제나 기차의 왼쪽이라,
	 * 플레이어가 방향을 매번 다시 읽지 않고 근육으로 기억할 수 있다.
	 */
	TrainForward,

	/**
	 * 보고 있는 방향이 위. 레이더가 시야를 따라 돈다.
	 *
	 * 조준선과 1:1로 맞출 수 있지만, 고개를 돌릴 때마다 점이 통째로 회전해서
	 * "어디서 오는 중이었는지"의 기억이 매번 깨진다. 기본값으로는 권하지 않는다.
	 */
	ViewForward,
};

/** 적이 드러나는 방식. */
UENUM(BlueprintType)
enum class ETRRadarPingMode : uint8
{
	/** 아무 연출 없이 항상 또렷하게. 디버깅에 편하다. */
	None,

	/**
	 * 도는 스윕. 등속 회전이라 스케줄을 기억할 필요가 없다 —
	 * 점의 방위각과 현재 스윕 각의 차이가 곧 경과 시간이다.
	 */
	Sweep,

	/**
	 * 중심에서 퍼지는 파형. 한 번 퍼지고 나면 불규칙한 시간만큼 쉬었다가 다시 퍼진다.
	 *
	 * 쉬는 시간이 매번 달라지므로 "언제 시작했나"를 들고 있어야 한다. 스윕과 달리
	 * 시간에서 위상을 역산할 수 없기 때문이다. 대신 기억하는 건 파형 하나의 일정뿐이고,
	 * 점마다 기록을 두는 일은 여전히 없다.
	 */
	Pulse,
};

/** 적 종류 하나를 어떻게 찍을지. 브러시를 비워 두면 색만 있는 사각형이 나온다. */
USTRUCT(BlueprintType)
struct FTRRadarBlipStyle
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TR|Radar")
	FSlateBrush Brush;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TR|Radar")
	FLinearColor Tint = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TR|Radar")
	FVector2D Size = FVector2D(10.f, 10.f);
};

/**
 * 기차를 중심으로 한 탑다운 레이더. 점을 위젯으로 만들지 않고 Slate 에 직접 그린다.
 *
 * **왜 UImage 를 풀링하지 않는가**
 *
 * 점 하나당 위젯 하나면 120개의 SWidget 이 생기고, 그만큼의 레이아웃 계산과
 * 무효화 전파가 매 프레임 붙는다. 점은 클릭도 툴팁도 필요 없는 순수 그림이라
 * 위젯일 이유가 없다. MakeBox 는 드로우 엘리먼트 하나를 큐에 넣을 뿐이라
 * 수백 개까지 선형으로 싸게 늘어난다.
 *
 * **점마다 상태를 두지 않는다**
 *
 * "이 적을 언제 훑었나"를 점마다 기록하고 싶어지지만, 필요한 건 언제나
 * "핑이 이 지점을 지난 시각" 하나이고 그건 계산으로 나온다. 스윕은 각도에서,
 * 파형은 중심으로부터의 거리에서. Mass 는 매 프레임 목록이 통째로 갈리므로
 * 점 단위 기록은 관리 비용만 남는다.
 *
 * **배경은 여기서 그리지 않는다**
 *
 * 지형·철로는 UMG 에서 이 위젯 뒤에 Image 를 깔면 된다. 트레드밀 구조라
 * 지형 스크롤 오프셋은 UTRGroundMotionSubsystem 의 속도를 적분한 누적 주행거리로
 * 만든다(기차의 월드 위치로 만들면 안 된다 — 그건 변하지 않는다).
 * 격자·사거리 링·중심 표식은 값이 레이더 설정과 묶여 있어서 여기서 그린다.
 *
 * 주의 — 이 위젯을 Invalidation Box 안에 넣으면 다시 그리지 않는다.
 * 매 프레임 값이 바뀌므로 Volatile 로 두거나 Invalidation Box 밖에 둘 것.
 */
UCLASS(Abstract)
class TR_API UTRRadarWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * 레이더 중심으로 쓸 액터를 직접 지정한다. 비워 두면 월드의 ATRTrain 을 찾는다.
	 *
	 * 기차가 여러 량이거나 중심을 특정 차량에 두고 싶을 때만 쓰면 된다.
	 * 평소에는 부를 필요가 없다.
	 */
	UFUNCTION(BlueprintCallable, Category = "TR|Radar")
	void SetCenterActor(AActor* InCenterActor) { CenterActorOverride = InCenterActor; }

protected:
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId,
		const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	/** 레이더 중심이 될 액터. 지정 → 캐시된 기차 → 새로 찾은 기차 순. */
	AActor* ResolveCenterActor() const;

	/** 중심 위치와 그리기에 쓸 Yaw. 못 구하면 false. */
	bool ResolveViewPoint(FVector& OutCenter, float& OutYaw) const;

	/** 십자선과 사거리 링. 점보다 아래 레이어에 깔린다. */
	void PaintGrid(const FGeometry& Geometry, FSlateWindowElementList& Out, int32 Layer,
		const FVector2D& Center, double RadiusPx) const;

	/** 도는 스윕. 브러시가 있으면 회전시키고, 없으면 선을 부채꼴로 겹쳐 잔광을 흉내낸다. */
	void PaintSweep(const FGeometry& Geometry, FSlateWindowElementList& Out, int32 Layer,
		const FVector2D& Center, double RadiusPx, float SweepAngleRad) const;

	/** 살아 있는 파형들을 그린다. 겹쳐 있으면 겹친 만큼 링이 나온다. */
	void PaintPulses(const FGeometry& Geometry, FSlateWindowElementList& Out, int32 Layer,
		const FVector2D& Center, double RadiusPx, double Now) const;

	/** 링 하나. Progress 는 0(중심)에서 1(가장자리). 범위 밖이면 아무것도 안 그린다. */
	void PaintPulseRing(const FGeometry& Geometry, FSlateWindowElementList& Out, int32 Layer,
		const FVector2D& Center, double RadiusPx, double Progress) const;

	/**
	 * 파형 일정을 현재 시각까지 진행시킨다.
	 *
	 * 시간만 보고 갱신하므로 한 프레임에 여러 번 불러도 결과가 같다.
	 * 그리기가 두 번 일어나는 구성에서도 파형이 두 칸씩 뛰지 않는다.
	 */
	void AdvancePulseSchedule(double Now) const;

	/** 파형 시작 간격을 하나 뽑는다. 겹침을 끄면 이동 시간 이상으로 밀어낸다. */
	double RollPulseInterval(double TravelSeconds) const;

	/** 시작 시각을 목록 맨 앞에 넣는다. 가장 오래된 것이 밀려난다. */
	void PushPulseStart(double StartTime) const;

	/**
	 * 이 거리의 점을 마지막으로 각성시킨 시각.
	 *
	 * 파형이 여러 개 살아 있을 수 있으므로 "가장 최근에 지나간 링"을 찾아야 한다.
	 * 아직 어떤 링도 닿지 않았으면 false — 그 점은 완전히 잦아든 상태다.
	 */
	bool TryGetLastPulseHitTime(double Now, double NormalizedDistance, double TravelSeconds,
		double& OutHitTime) const;

	/**
	 * 이 브러시를 "사용자가 채웠다"로 볼지.
	 *
	 * 리소스 유무만 보면 Draw As = Rounded Box 로 만든 브러시가 조용히 무시된다.
	 * 그런 브러시는 텍스처 없이도 멀쩡히 그려지므로, 설정한 사람 입장에서는
	 * 아무 반응이 없는 것으로만 보인다. 판정을 한 곳에 모아 그걸 막는다.
	 */
	static bool HasAuthoredArt(const FSlateBrush& Brush);

protected:
	/** 이 거리(cm)가 레이더 반지름에 대응한다. 8000이면 80m. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TR|Radar", meta = (ClampMin = "100.0", ForceUnits = "cm"))
	float RadarRangeCm = 8000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TR|Radar")
	ETRRadarOrientation Orientation = ETRRadarOrientation::TrainForward;

	/**
	 * 사거리 밖 적을 가장자리에 붙여 표시할지.
	 *
	 * 끄면 그냥 사라진다. 켜 두는 쪽을 권한다 — "어느 방향에서 오고 있다"는
	 * 정보가 레이더의 본질이고, 그건 거리보다 먼저 알아야 하는 값이다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TR|Radar")
	bool bShowOffRangeBlips = true;

	/** 종류별 표시. 없는 종류는 DefaultBlipStyle 로 그린다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TR|Radar")
	TMap<ETREnemyType, FTRRadarBlipStyle> BlipStyles;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TR|Radar")
	FTRRadarBlipStyle DefaultBlipStyle;

	/**
	 * 사거리 밖 표시에 쓸 브러시. 위(-Y)를 향하는 모양으로 만들 것 —
	 * 그 방향을 기준으로 회전시킨다.
	 *
	 * 텍스처를 넣거나 Draw As 를 Rounded Box 로 두면 "채웠다"로 본다.
	 * 둘 다 아니면 종류별 브러시를 그대로 가장자리에 붙인다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TR|Radar")
	FSlateBrush OffRangeBrush;

	/**
	 * 한 프레임에 그릴 점의 상한. 넘는 만큼은 버린다.
	 *
	 * 동시 상한이 120마리라 정상 범위에서는 절대 걸리지 않는다. 스폰 규칙이
	 * 바뀌거나 어딘가 버그로 목록이 폭발했을 때, 프레임이 통째로 멈추는 대신
	 * 레이더가 조금 부정확해지는 쪽으로 끝내기 위한 상한이다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TR|Radar", meta = (ClampMin = "1"))
	int32 MaxDrawnBlips = 256;

	//----------------------------------------------------------------------//
	//  핑 — 공통
	//----------------------------------------------------------------------//

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TR|Radar|Ping")
	ETRRadarPingMode PingMode = ETRRadarPingMode::Pulse;

	/** 스윕과 파형이 함께 쓰는 색. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TR|Radar|Ping")
	FLinearColor PingTint = FLinearColor(0.25f, 1.f, 0.45f, 0.55f);

	/** 핑이 지나간 뒤 점이 서서히 사라질지. 끄면 항상 또렷하다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TR|Radar|Ping")
	bool bFadeBlipsWithPing = true;

	/**
	 * 사라지는 속도. 1이면 고르게 흐려지고, 크면 핑 직후에 빠르게 어두워진다.
	 * 2~3 이 레이더처럼 읽힌다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TR|Radar|Ping", meta = (ClampMin = "0.1", ClampMax = "10.0"))
	float BlipFadePower = 2.5f;

	/**
	 * 아무리 흐려져도 남는 최소 불투명도.
	 *
	 * 0이면 완전히 사라져서 연출은 그럴듯하지만, 다음 핑까지 적을 아예 못 본다.
	 * 파형은 쉬는 시간이 최대 몇 초까지 갈 수 있어서 스윕보다 이 값이 더 중요하다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TR|Radar|Ping", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MinBlipOpacity = 0.12f;

	//----------------------------------------------------------------------//
	//  핑 — 파형 (PingMode == Pulse)
	//----------------------------------------------------------------------//

	/** 중심에서 가장자리까지 퍼지는 데 걸리는 시간. 짧을수록 날카롭게 튄다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TR|Radar|Ping|Pulse", meta = (ClampMin = "0.05", ForceUnits = "s"))
	float PulseTravelSeconds = 1.1f;

	/**
	 * 파형 **시작에서 다음 파형 시작까지**의 간격, 그 하한.
	 *
	 * 끝난 시점이 아니라 시작 시점을 기준으로 잰다는 점이 중요하다. 이 값이
	 * PulseTravelSeconds 보다 짧으면 앞 파형이 아직 퍼지는 중에 다음 파형이
	 * 출발한다 — 링이 여러 겹으로 동시에 나아가는 그림이 된다.
	 *
	 * 상한과 같게 두면 규칙적인 파형이 된다. 둘의 간격이 곧 불규칙함의 크기다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TR|Radar|Ping|Pulse", meta = (ClampMin = "0.05", ForceUnits = "s"))
	float PulseIntervalMin = 0.45f;

	/**
	 * 시작 간격의 상한.
	 *
	 * 하한보다 작게 두면 하한으로 맞춰진다. 뒤집지 않는 이유는, 뒤집으면
	 * "내가 넣은 최소값보다 짧게 나온다"가 되어 더 헷갈리기 때문이다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TR|Radar|Ping|Pulse", meta = (ClampMin = "0.05", ForceUnits = "s"))
	float PulseIntervalMax = 1.5f;

	/**
	 * 앞 파형이 퍼지는 중에 다음 파형이 출발해도 되는지.
	 *
	 * 끄면 간격이 아무리 짧게 뽑혀도 PulseTravelSeconds 이상으로 밀어낸다 —
	 * 즉 한 번에 하나씩만 퍼진다. 간격 값을 그대로 두고 이것만 껐다 켜면서
	 * 둘을 비교해 볼 수 있다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TR|Radar|Ping|Pulse")
	bool bAllowOverlappingPulses = true;

	/**
	 * 동시에 그릴 파형의 최대 개수.
	 *
	 * 간격을 아주 짧게 잡으면 링이 화면을 가득 채워 배경이 안 보인다. 겹침을
	 * 허용하되 몇 겹까지만 보이게 하는 상한이다. 여기서 잘린 파형도 점을
	 * 각성시키는 일은 그대로 한다 — 안 보일 뿐 정보는 유지된다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TR|Radar|Ping|Pulse", meta = (ClampMin = "1", ClampMax = "8"))
	int32 MaxConcurrentPulses = 4;

	/**
	 * 파형이 지나간 뒤 점이 완전히 흐려지기까지의 시간.
	 *
	 * 쉬는 시간이 불규칙하므로 페이드 길이를 주기에서 유도할 수 없다. 그래서
	 * 따로 둔다. 이 값이 쉬는 시간보다 짧으면 다음 파형 전에 점이 잦아든다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TR|Radar|Ping|Pulse", meta = (ClampMin = "0.05", ForceUnits = "s"))
	float BlipFadeSeconds = 2.2f;

	/**
	 * 파형 그림. 넣으면 원형으로 그린 텍스처가 반지름에 맞춰 커진다.
	 *
	 * 텍스처를 넣거나 Draw As 를 Rounded Box 로 두면 "채웠다"로 본다.
	 * 둘 다 아니면 아래의 링으로 대신한다 — 애셋 없이도 바로 돌아간다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TR|Radar|Ping|Pulse")
	FSlateBrush PulseBrush;

	/** 브러시가 없을 때 겹쳐 그릴 링 개수. 여러 겹이면 띠처럼 보인다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TR|Radar|Ping|Pulse", meta = (ClampMin = "1", ClampMax = "8"))
	int32 PulseTrailRings = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TR|Radar|Ping|Pulse", meta = (ClampMin = "0.5"))
	float PulseThickness = 2.5f;

	//----------------------------------------------------------------------//
	//  핑 — 스윕 (PingMode == Sweep)
	//----------------------------------------------------------------------//

	/** 한 바퀴 도는 데 걸리는 시간. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TR|Radar|Ping|Sweep", meta = (ClampMin = "0.1", ForceUnits = "s"))
	float SweepPeriodSeconds = 2.5f;

	/**
	 * 스윕 그림. **위(-Y)를 향하도록** 그려 두면 그 방향을 기준으로 회전한다.
	 * 비우면 아래의 부채꼴 선으로 대신한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TR|Radar|Ping|Sweep")
	FSlateBrush SweepBrush;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TR|Radar|Ping|Sweep", meta = (ClampMin = "1.0", ClampMax = "180.0"))
	float SweepTrailDegrees = 70.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TR|Radar|Ping|Sweep", meta = (ClampMin = "1", ClampMax = "64"))
	int32 SweepTrailSegments = 14;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TR|Radar|Ping|Sweep", meta = (ClampMin = "0.5"))
	float SweepThickness = 2.f;

	//----------------------------------------------------------------------//
	//  중심 표식과 격자
	//----------------------------------------------------------------------//

	/**
	 * 중심에 그릴 기차 표식. 비워 두면 흰 사각형이 나오는데, 세로로 긴 기본
	 * 크기 덕에 그 자체로 위를 향한 기차 실루엣처럼 읽힌다. 텍스처는 나중에.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TR|Radar|Center")
	FSlateBrush CenterBrush;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TR|Radar|Center")
	FVector2D CenterSize = FVector2D(14.f, 40.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TR|Radar|Center")
	FLinearColor CenterTint = FLinearColor(0.9f, 0.95f, 1.f, 0.9f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TR|Radar|Grid")
	bool bDrawGrid = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TR|Radar|Grid")
	FLinearColor GridTint = FLinearColor(0.35f, 0.9f, 0.5f, 0.25f);

	/** 바깥 테두리를 뺀 안쪽 링 개수. 2면 사거리의 1/3, 2/3 지점에 그린다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TR|Radar|Grid", meta = (ClampMin = "0", ClampMax = "8"))
	int32 RangeRingCount = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TR|Radar|Grid")
	bool bDrawCrosshair = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TR|Radar|Grid", meta = (ClampMin = "0.5"))
	float GridThickness = 1.f;

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> CenterActorOverride;

private:
	/**
	 * 찾아 둔 기차. NativePaint 가 const 라 mutable 이다.
	 *
	 * 매 프레임 TActorIterator 를 돌리지 않기 위한 캐시다. 약참조라
	 * GC 를 막지 않고, 레벨이 바뀌면 자연히 무효가 되어 다시 찾는다.
	 */
	mutable TWeakObjectPtr<AActor> CachedTrain;

	/**
	 * 기차 탐색을 다시 시도해도 되는 시각(월드 시간, 초).
	 *
	 * 캐시가 비어 있으면 TActorIterator 로 레벨 전체를 훑는데, 기차가 없는
	 * 맵에서는 그 상태가 영원히 계속된다. 그리기는 프레임당 여러 번 불릴 수도
	 * 있어서, 액터가 수천 개인 레벨이면 게임 스레드에 그대로 얹힌다.
	 *
	 * 그래서 실패한 탐색에는 쿨다운을 건다. 성공하면 캐시가 잡히므로 다시
	 * 훑을 일이 없고, 실패해도 비용이 초당 두 번으로 묶인다.
	 */
	mutable double NextTrainSearchTime = 0.0;

	/** 기억해 두는 파형의 개수. 겹침 상한(8)과 같아야 잘린 파형도 페이드에 반영된다. */
	static constexpr int32 TrackedPulseCount = 8;

	/**
	 * 최근 파형들의 시작 시각. **0번이 가장 최근**이고 뒤로 갈수록 오래됐다.
	 *
	 * 하나만 들고 있으면 안 되는 이유는 바깥쪽 점 때문이다. 새 파형이 막
	 * 시작한 순간, 가장자리 근처의 점은 아직 그 링에 닿지 않았다. 그 점의
	 * 마지막 각성 시각은 앞선 파형에 있으므로 뒤로 몇 개는 기억해야 한다.
	 * 파형이 겹치기 시작하면 그 "몇 개"가 둘 이상이 된다.
	 */
	mutable double PulseStartTimes[TrackedPulseCount] = {};

	/** PulseStartTimes 에 실제로 들어 있는 개수. */
	mutable int32 PulseStartCount = 0;

	/** 다음 파형이 시작할 시각. 매번 새로 뽑은 간격이 반영되어 있다. */
	mutable double NextPulseStartTime = 0.0;

	/**
	 * 쉬는 시간을 뽑는 난수원.
	 *
	 * 전역 FMath::FRand 를 쓰지 않는 이유는, 순전히 보기 위한 연출이 게임 로직이
	 * 쓰는 난수 흐름을 건드리게 되기 때문이다. 재현이 필요한 버그를 쫓을 때
	 * 그런 오염이 제일 성가시다.
	 */
	mutable FRandomStream PulseStream;

	/** 첫 그리기에서 일정을 초기화했는지. */
	mutable bool bPulseScheduleReady = false;
};
