// TRTargetDirectorSubsystem.h

#pragma once

#include "CoreMinimal.h"
#include "Mass/ExternalSubsystemTraits.h"
#include "Mass/Core/TRMassTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "TRTargetDirectorSubsystem.generated.h"

/** 채널 하나의 현재 목표 상태. */
USTRUCT()
struct FTRTargetChannelState
{
	GENERATED_BODY()

	/** 액터 목표. 유효하면 매 틱 이 액터의 현재 위치를 쓴다(움직이는 열차 대응). */
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
};

/** 프로세서가 한 프레임에 한 번 받아가는 목표 해석 결과. */
struct FTRResolvedTarget
{
	/** 목표 중심(액터 위치 또는 고정 위치). */
	FVector Center = FVector::ZeroVector;

	/** 목표 액터의 현재 트랜스폼. 바운드가 있을 때만 의미가 있다. */
	FTransform Transform = FTransform::Identity;

	FBox LocalBounds = FBox(ForceInit);

	bool bHasBounds = false;

	bool bValid = false;

	/**
	 * 표면 최근접점을 구한다. 바운드가 없으면 중심을 그대로 돌려준다.
	 *
	 * 박스에 대한 클램프 한 번이라 개체당 비용이 거의 없다. 정밀한 콜리전 질의는
	 * 개체 수가 수백이 되면 감당할 수 없고, 접근 목표로는 박스면 충분하다.
	 */
	FVector GetClosestSurfacePoint(const FVector& From) const
	{
		if (!bHasBounds)
		{
			return Center;
		}

		const FVector LocalFrom = Transform.InverseTransformPosition(From);
		const FVector LocalClosest = LocalBounds.GetClosestPointTo(LocalFrom);

		return Transform.TransformPosition(LocalClosest);
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
	 * 표면 최근접점은 결과 구조체의 GetClosestSurfacePoint로 개체마다 구한다.
	 */
	bool GetResolvedTarget(ETRTargetChannel Channel, FTRResolvedTarget& OutTarget) const;

	/**
	 * 목표 액터의 영역을 다시 잰다.
	 * 기차에 차량을 붙이거나 떼는 등 형상이 바뀌었을 때 부른다.
	 */
	UFUNCTION(BlueprintCallable, Category = "TR|Mass")
	void RefreshTargetBounds(ETRTargetChannel Channel);

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
