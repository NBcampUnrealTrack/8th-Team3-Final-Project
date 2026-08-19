// TRTargetDirectorSubsystem.h

#pragma once

#include "CoreMinimal.h"
#include "Mass/ExternalSubsystemTraits.h"
#include "Mass/Core/TRMassTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "TRTargetDirectorSubsystem.generated.h"

/** 목표를 이루는 구역 하나. 기차의 장갑 한 칸에 해당한다. */
USTRUCT()
struct FTRTargetZoneState
{
	GENERATED_BODY()

	/** 구역의 형상을 정하는 프리미티브. TRMassTargetVolume 태그가 붙은 것. */
	UPROPERTY()
	TWeakObjectPtr<UPrimitiveComponent> Volume;

	/** 구역이 속한 액터. 파괴 여부 판정에 쓴다. */
	UPROPERTY()
	TWeakObjectPtr<AActor> Owner;

	/** 이 구역을 몇 번째로 노릴지. */
	UPROPERTY()
	ETRTargetZoneTier Tier = ETRTargetZoneTier::Armor;
};

/** 채널 하나의 현재 목표 상태. */
USTRUCT()
struct FTRTargetChannelState
{
	GENERATED_BODY()

	/** 액터 목표. 유효하면 매 틱 이 액터의 현재 위치를 쓴다. */
	UPROPERTY()
	TWeakObjectPtr<AActor> TargetActor;

	/** 액터가 없을 때 사용하는 고정 위치 목표. */
	UPROPERTY()
	FVector TargetLocation = FVector::ZeroVector;

	UPROPERTY()
	bool bHasTarget = false;

	/**
	 * 목표 액터의 로컬 공간 바운드. 등록 시 한 번 계산한다.
	 *
	 * 기차는 길쭉해서 중심 한 점을 목표로 주면 전원이 그 한 점으로 수렴한다.
	 * 실제로 원하는 것은 "표면 최근접점"이므로 형상이 필요하다. 액터가 움직여도
	 * 로컬 바운드는 변하지 않으므로 매 틱 다시 계산하지 않고, 현재 트랜스폼만 읽어서 쓴다.
	 *
	 * 값의 출처는 두 가지다 — TRMassTargetVolume 태그가 붙은 컴포넌트가 있으면
	 * 그 컴포넌트, 없으면 메시 전체 바운드. 전자가 에디터에서 직접 편집하는 경로다.
	 */
	UPROPERTY()
	FBox LocalBounds = FBox(ForceInit);

	UPROPERTY()
	bool bHasBounds = false;

	/** 목표를 나눈 구역들. 적이 노리는 대상은 이 구역뿐이다. */
	UPROPERTY()
	TArray<FTRTargetZoneState> Zones;
};

/** 프로세서가 한 프레임에 한 번 받아가는 목표 해석 결과. */
struct FTRResolvedTarget
{
	/** 스냅샷이 담을 수 있는 구역 수. 기차는 장갑 4 + 포탑 5를 쓴다. */
	static constexpr int32 MaxZones = 16;

	/** 목표 중심(액터 위치 또는 고정 위치). */
	FVector Center = FVector::ZeroVector;

	/** 목표 액터의 현재 트랜스폼. 바운드가 있을 때만 의미가 있다. */
	FTransform Transform = FTransform::Identity;

	FBox LocalBounds = FBox(ForceInit);

	bool bHasBounds = false;

	bool bValid = false;

	/** 구역별 월드 공간 바운드. 앞에서부터 ZoneCount 개만 유효하다. */
	FBox ZoneBounds[MaxZones];

	/** 구역 소유자가 살아 있는지. 표적 선택에는 쓰지 않고 진단용으로만 남긴다. */
	bool ZoneAlive[MaxZones] = {};

	/** 구역별 등급. 표적 선택에는 쓰지 않고 진단용으로만 남긴다. */
	ETRTargetZoneTier ZoneTier[MaxZones] = {};

	/** 등록된 구역 수. 0이면 노릴 곳이 없다. */
	int32 ZoneCount = 0;

	/**
	 * 노릴 지점 — 가장 가까운 구역(장갑·포탑)의 표면 최근접점.
	 *
	 * 구역만 표적이다. 차체 전체를 표적으로 두면 최근접점이 객실 표면이 되어
	 * 적이 장갑을 지나쳐 안으로 들어간다. 노릴 구역이 없으면 false.
	 */
	bool TryGetClosestZonePoint(const FVector& From, FVector& OutPoint) const
	{
		const int32 Nearest = FindNearestZoneAnyTier(From);
		if (Nearest == INDEX_NONE)
		{
			return false;
		}

		OutPoint = ZoneBounds[Nearest].GetClosestPointTo(From);
		return true;
	}

	/** 등급과 파괴 여부를 따지지 않고 가장 가까운 구역. 없으면 INDEX_NONE. */
	int32 FindNearestZoneAnyTier(const FVector& From) const
	{
		int32 Nearest = INDEX_NONE;
		double NearestDistanceSq = 0.0;

		for (int32 Index = 0; Index < ZoneCount; ++Index)
		{
			// 바운드가 비면 최근접점이 월드 원점이 된다. 그 자리를 노리게 두면 안 된다.
			if (!ZoneBounds[Index].IsValid)
			{
				continue;
			}

			const double DistanceSq = FVector::DistSquared(ZoneBounds[Index].GetClosestPointTo(From), From);
			if (Nearest == INDEX_NONE || DistanceSq < NearestDistanceSq)
			{
				Nearest = Index;
				NearestDistanceSq = DistanceSq;
			}
		}

		return Nearest;
	}

};

/**
 * Mass 추적 엔티티들의 목표를 관리하는 단일 진실원.
 *
 * 엔티티는 채널만 알고 목표는 모른다. 여기서 채널의 목표를 바꾸면
 * 이미 소환된 엔티티까지 전부 다음 틱에 새 목표를 따라간다.
 *
 * 액터에서 읽은 위치·형상은 게임 스레드에서 스냅샷으로 떠 둔다. 프로세서는 워커
 * 스레드에서 돌 수 있으므로 액터를 직접 만지면 안 되고, 그 스냅샷만 읽는다.
 *
 * 주의: 목표 변경은 서버에서만 호출할 것. Mass 시뮬레이션은 서버 권위로 돌리고
 * 클라이언트는 표현(액터)만 복제받는 구조를 전제로 한다.
 */
UCLASS(BlueprintType)
class TR_API UTRTargetDirectorSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	/** 액터를 목표로 지정. 액터가 움직이면 추적 대상 위치도 따라 움직인다. */
	UFUNCTION(BlueprintCallable, Category = "TR|Mass")
	void SetTargetActor(ETRTargetChannel Channel, AActor* NewTarget);

	/** 고정 위치를 목표로 지정. */
	UFUNCTION(BlueprintCallable, Category = "TR|Mass")
	void SetTargetLocation(ETRTargetChannel Channel, FVector NewLocation);

	/** 목표 해제. 해당 채널 엔티티들은 정지한다. */
	UFUNCTION(BlueprintCallable, Category = "TR|Mass")
	void ClearTarget(ETRTargetChannel Channel);

	UFUNCTION(BlueprintPure, Category = "TR|Mass")
	bool HasTarget(ETRTargetChannel Channel) const;

	/**
	 * 채널의 목표 액터. 고정 위치 목표이거나 액터가 파괴됐으면 nullptr.
	 *
	 * **게임 스레드에서만 부를 것.** 액터 포인터를 돌려주므로 프로세서(워커 스레드)에서
	 * 쓰면 안 된다. 프로세서는 위치·형상만 필요하므로 GetResolvedTarget을 쓴다.
	 */
	UFUNCTION(BlueprintPure, Category = "TR|Mass")
	AActor* GetTargetActor(ETRTargetChannel Channel) const;

	/**
	 * 목표 조회. 게임 스레드가 떠 둔 스냅샷을 그대로 돌려준다 — 액터를 만지지 않으므로
	 * 워커 스레드에서 불러도 안전하다.
	 *
	 * 청크당 한 번만 호출하면 되므로 엔티티 수와 무관하게 저렴하다.
	 * 표면 최근접점은 결과 구조체의 TryGetClosestZonePoint로 개체마다 구한다.
	 */
	bool GetResolvedTarget(ETRTargetChannel Channel, FTRResolvedTarget& OutTarget) const;

	/**
	 * 목표 액터의 영역을 다시 잰다.
	 * 기차에 차량을 붙이거나 떼는 등 형상이 바뀌었을 때 부른다.
	 */
	UFUNCTION(BlueprintCallable, Category = "TR|Mass")
	void RefreshTargetBounds(ETRTargetChannel Channel);

	/** 추적 구역을 직접 등록한다. 목표 액터에 붙어 있지 않은 장갑·포탑이 스스로 부를 때 쓴다. */
	UFUNCTION(BlueprintCallable, Category = "TR|Mass")
	void RegisterTargetZone(ETRTargetChannel Channel, UPrimitiveComponent* Volume,
		ETRTargetZoneTier Tier = ETRTargetZoneTier::Armor);

	/** 구역을 표적에서 뺀다. 장갑이 파괴됐을 때 부른다. */
	UFUNCTION(BlueprintCallable, Category = "TR|Mass")
	void UnregisterTargetZone(ETRTargetChannel Channel, UPrimitiveComponent* Volume);

	// UTickableWorldSubsystem — 매 틱 스냅샷을 갱신한다.

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

protected:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

private:
	static constexpr int32 ChannelCount = static_cast<int32>(ETRTargetChannel::MAX);

	static bool IsValidChannel(ETRTargetChannel Channel)
	{
		return static_cast<int32>(Channel) >= 0 && static_cast<int32>(Channel) < ChannelCount;
	}

	/**
	 * 액터에서 현재 위치·트랜스폼을 읽어 스냅샷에 옮긴다. 게임 스레드 전용.
	 * 목표를 바꾼 직후에도 부른다 — 첫 틱 전에 조회하는 쪽이 있다.
	 */
	void RefreshSnapshot();

	/** 채널 enum을 인덱스로 쓰는 고정 배열 — 해시 조회 없이 O(1). */
	UPROPERTY()
	TArray<FTRTargetChannelState> Channels;

	/**
	 * 프로세서가 읽는 값. 액터 포인터가 들어 있지 않은 순수 데이터라 스레드를 가리지 않는다.
	 * Channels와 인덱스가 1:1로 대응한다.
	 */
	TArray<FTRResolvedTarget> Snapshot;
};

/**
 * 프로세서 쿼리에서 이 서브시스템을 요구하려면 필요한 특성 선언.
 * 읽기 전용 조회만 하므로 게임 스레드에 묶이지 않는다.
 */
template<>
struct TMassExternalSubsystemTraits<UTRTargetDirectorSubsystem> final
{
	enum
	{
		GameThreadOnly = false,
		ThreadSafeWrite = false
	};
};
