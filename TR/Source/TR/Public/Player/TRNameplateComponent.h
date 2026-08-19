// TRNameplateComponent.h

#pragma once

#include "CoreMinimal.h"
#include "Components/WidgetComponent.h"
#include "TRNameplateComponent.generated.h"

/**
 * 소유 폰 머리 위에 닉네임을 띄운다. 화면 쪽을 보도록 매 틱 돌리고,
 * PlayerState 가 늦게 도착하는 것을 대비해 이름은 계속 확인한다.
 */
UCLASS(ClassGroup = (TR), meta = (BlueprintSpawnableComponent))
class TR_API UTRNameplateComponent : public UWidgetComponent
{
	GENERATED_BODY()

public:
	UTRNameplateComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	/** 내 캐릭터 것은 감춘다. 1인칭이라 볼 일이 없다. */
	UPROPERTY(EditDefaultsOnly, Category = "TR|Nameplate")
	bool bHideForLocalOwner = true;

private:
	/** 카메라를 향하도록 Yaw 만 맞춘다. 글자가 눕지 않게 Pitch·Roll 은 0 으로 둔다. */
	void FaceLocalCamera();

	/** PlayerState 의 이름을 위젯에 넣는다. 바뀐 것이 없으면 건드리지 않는다. */
	void RefreshName();

	FString CachedName;
};
