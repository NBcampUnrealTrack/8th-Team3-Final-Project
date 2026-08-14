// TRMassTargetZoneComponent.h

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Mass/Core/TRMassTypes.h"
#include "TRMassTargetZoneComponent.generated.h"

class UPrimitiveComponent;

/**
 * 이 액터를 적의 표적 구역으로 등록한다. 기차의 장갑 한 칸에 붙인다.
 *
 * 액터가 목표(기차)에 어태치돼 있지 않아도 동작한다.
 */
UCLASS(ClassGroup = (TR), meta = (BlueprintSpawnableComponent, DisplayName = "TR Mass Target Zone"),
	HideCategories = (Tags, ComponentTick, ComponentReplication, Activation, Cooking, AssetUserData))
class TR_API UTRMassTargetZoneComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTRMassTargetZoneComponent();

	/** 지금 표적으로 잡히는 상태인지. */
	UFUNCTION(BlueprintPure, Category = "TR|Mass")
	bool IsRegistered() const { return bRegistered; }

	/** 구역을 표적에서 뺀다. 차량을 떼어낼 때만 쓴다. 파괴는 자동 처리된다. */
	UFUNCTION(BlueprintCallable, Category = "TR|Mass")
	void UnregisterZone();

	/** 구역을 표적에 올린다. 차량을 붙일 때만 쓴다. */
	UFUNCTION(BlueprintCallable, Category = "TR|Mass")
	void RegisterZone();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 이 구역을 노릴 적의 채널. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TR|Mass")
	ETRTargetChannel Channel = ETRTargetChannel::Train;

	/** 적이 노리는 순서. 포탑을 먼저 비우고 장갑으로 넘어간다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TR|Mass")
	ETRTargetZoneTier Tier = ETRTargetZoneTier::Armor;

	/**
	 * 구역 형상으로 쓸 컴포넌트 이름. 기본값은 장갑의 외부 피격 판정 영역이다.
	 * 못 찾으면 소유 액터의 첫 프리미티브로 물러난다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TR|Mass")
	FName VolumeComponentName = TEXT("DamageVolume");

private:
	/** 이름으로 볼륨을 찾는다. 이름이 비면 첫 프리미티브. */
	UPrimitiveComponent* ResolveVolume() const;

	UPROPERTY(Transient)
	TObjectPtr<UPrimitiveComponent> Volume;

	bool bRegistered = false;
};
