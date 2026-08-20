// TRRadarSubsystem.h

#pragma once

#include "CoreMinimal.h"
#include "Engine/World.h"
#include "Mass/ExternalSubsystemTraits.h"
#include "Mass/Core/TRMassTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "TRRadarSubsystem.generated.h"

/**
 * 레이더에 찍히는 점 하나. 위치와 종류뿐이다.
 *
 * 여기에 체력·상태 같은 걸 더 담고 싶어지겠지만 참을 것. 이 배열은 매 프레임
 * 통째로 다시 채워지므로, 필드 하나가 늘 때마다 120번의 복사가 함께 는다.
 * 레이더가 실제로 그리는 데 필요한 것만 둔다.
 */
USTRUCT(BlueprintType)
struct FTRRadarBlip
{
	GENERATED_BODY()

	/** 월드 좌표. 레이더 중심을 무엇으로 잡을지는 UI가 정하므로 여기서는 변환하지 않는다. */
	UPROPERTY(BlueprintReadOnly, Category = "TR|Radar")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "TR|Radar")
	ETREnemyType Type = ETREnemyType::Melee;
};

/**
 * 적 위치를 UI로 넘기는 창구. 수집 프로세서가 채우고 레이더 위젯이 읽는다.
 *
 * **왜 SceneCapture 가 아닌가**
 *
 * Mass 표현 시스템은 LOD 밖 엔티티에 액터도 ISM 인스턴스도 만들지 않는다.
 * 카메라로 찍으면 "이미 보이는 적"만 다시 그리게 되는데, 레이더는 원래
 * 안 보이는 적을 보여주는 물건이라 목적과 정면으로 충돌한다. 게다가 캡처는
 * 렌더 패스가 한 벌 더 도는 비용이고, 종류별 색이나 화면 밖 방향 표시처럼
 * 레이더에 필요한 표현은 카메라로 만들 수 없다.
 *
 * 반면 엔티티의 트랜스폼은 LOD와 무관하게 항상 정확하다. 그래서 위치만 긁어
 * 온다. 120마리 × 20바이트면 프레임당 2.4KB 복사고, 이건 측정에 잡히지 않는다.
 *
 * **왜 서버/클라 분기가 없는가**
 *
 * 클라이언트에도 ATRMassNetRelay 가 스냅샷으로 만든 로컬 엔티티가 있고,
 * 그 위치는 스냅샷 사이를 보간해 매 프레임 갱신된다. 즉 양쪽 다 "지금 프레임의
 * 엔티티 트랜스폼"이 정답이라, 같은 프로세서가 그대로 돌면 된다.
 * 레이더를 위한 복제 코드는 한 줄도 필요 없다.
 */
UCLASS()
class TR_API UTRRadarSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/**
	 * 이번 프레임 수집을 시작한다. 이전 프레임의 내용을 버린다.
	 *
	 * 프로세서가 프레임당 한 번만 부른다. 엔티티가 0마리여도 불려야 한다 —
	 * 안 그러면 적이 전멸한 프레임에 직전 점들이 화면에 남는다.
	 */
	void BeginGather()
	{
		Blips.Reset();
		LastGatherFrame = GFrameCounter;
	}

	void AddBlip(const FVector& Location, ETREnemyType Type)
	{
		// 깨진 트랜스폼은 여기서 버린다. NaN 이 화면까지 가면 Slate 가 좌표를
		// 정수로 바꾸는 지점에서 쓰레기 사각형이 되고, 원인이 레이더처럼 보인다.
		// 실제 원인은 늘 그 엔티티를 움직인 쪽이므로, 조용히 버리되 흔적을 남긴다.
		if (Location.ContainsNaN())
		{
			UE_LOG(LogTemp, Verbose, TEXT("TRRadar: NaN 위치를 가진 엔티티를 건너뛴다."));
			return;
		}

		FTRRadarBlip& Blip = Blips.AddDefaulted_GetRef();
		Blip.Location = Location;
		Blip.Type = Type;
	}

	/** 위젯이 읽는다. 게임 스레드 전용 — 수집도 게임 스레드에서만 일어난다. */
	const TArray<FTRRadarBlip>& GetBlips() const { return Blips; }

	/**
	 * 최근 프레임에 수집이 돌았는가.
	 *
	 * 프로세서가 멈추면(월드 전환 직전, Mass 시뮬레이션 중단) 배열은 마지막
	 * 내용을 그대로 들고 있다. 그 점들은 이제 아무 뜻도 없으므로 위젯이 안 그려야
	 * 하는데, 배열만 봐서는 "적이 없다"와 "갱신이 멈췄다"를 구별할 수 없다.
	 *
	 * 여유를 몇 프레임 주는 이유는 그리기가 수집보다 먼저 도는 프레임이 있을 수
	 * 있어서다. 한 프레임만 어긋나도 레이더가 깜빡이면 그게 더 큰 문제다.
	 */
	bool IsDataFresh(uint64 ToleranceFrames = 3) const
	{
		// **일시정지는 예외다.**
		//
		// 멈춘 동안 Mass 프로세싱은 서지만(틱커블은 기본적으로 일시정지 중에
		// 돌지 않는다) GFrameCounter 는 계속 오른다. 그대로 두면 몇 프레임 만에
		// "끊겼다"로 판정되어 적이 화면에서 통째로 사라진다.
		//
		// 멈춘 것과 끊긴 것은 다르다. 멈춰 있는 동안 마지막 위치는 여전히 맞다.
		if (const UWorld* World = GetWorld())
		{
			if (World->IsPaused())
			{
				return true;
			}
		}

		// LastGatherFrame 이 0(한 번도 수집 안 됨)이면 뺄셈이 거대한 값이 되어
		// 자연스럽게 false 가 된다. 별도 분기가 필요 없다.
		return (GFrameCounter - LastGatherFrame) <= ToleranceFrames;
	}

private:
	/**
	 * 매 프레임 Reset 후 다시 채운다. Reset 은 할당을 유지하므로 워밍업 이후
	 * 힙을 건드리지 않는다(Empty 였다면 매 프레임 재할당이 났을 것이다).
	 */
	TArray<FTRRadarBlip> Blips;

	/** 마지막으로 BeginGather 가 불린 프레임. 0이면 아직 한 번도 안 돌았다. */
	uint64 LastGatherFrame = 0;
};

template<>
struct TMassExternalSubsystemTraits<UTRRadarSubsystem> final
{
	enum
	{
		// 배열을 통째로 쓰므로 워커 스레드에서 동시에 건드리면 안 된다.
		// 수집 프로세서가 bRequiresGameThreadExecution 으로 이걸 보장한다.
		GameThreadOnly = true,
		ThreadSafeWrite = false
	};
};
