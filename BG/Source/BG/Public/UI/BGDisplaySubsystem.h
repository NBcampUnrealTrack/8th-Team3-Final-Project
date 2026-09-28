// 게임을 켤 때마다 창이 올라가 있는 모니터 해상도의 전체 창 모드로 시작한다.
// 에디터 Standalone 이 붙이는 -windowed 도 여기서 덮는다.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "BGDisplaySubsystem.generated.h"

UCLASS()
class BG_API UBGDisplaySubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	// 게임 창과 뷰포트가 다 만들어진 뒤에 적용해야 해서 첫 틱으로 미룬다.
	bool HandleFirstTick(float DeltaTime);

	FTSTicker::FDelegateHandle TickHandle;
};
