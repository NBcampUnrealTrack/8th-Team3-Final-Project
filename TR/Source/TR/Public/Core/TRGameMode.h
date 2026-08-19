// TRGameMode.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "TRGameMode.generated.h"

UCLASS()
class TR_API ATRGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	/**
	 * 웨이브를 시작한다. 호스트가 로비에서 시작을 누를 때 부른다.
	 * 준비가 덜 됐거나 이미 시작했으면 아무것도 하지 않고 false.
	 */
	UFUNCTION(BlueprintCallable, Category = "TR|Lobby")
	bool StartMatch();

	/**
	 * 판을 끝낸다. 선체가 부서지면 ATRTrain 이 부른다.
	 *
	 * 웨이브 중단·잔존 적 정리·상태 복제를 정해진 순서로 한 번에 처리한다.
	 * 기차가 이걸 직접 하지 않는 이유는, 끝내는 경로가 나중에 늘어나도(제한 시간, 항복 등)
	 * 정리 순서를 한 곳에서만 고치면 되게 하기 위해서다.
	 *
	 * 이미 끝난 판이면 아무것도 하지 않고 false.
	 */
	UFUNCTION(BlueprintCallable, Category = "TR|Match")
	bool EndMatch();

protected:
	virtual void PostLogin(APlayerController* NewPlayer) override;
};
