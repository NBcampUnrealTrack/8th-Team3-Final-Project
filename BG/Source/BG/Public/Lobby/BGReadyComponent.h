// BANG! 준비 상태 — PlayerState 에 붙는다. 상태만 들고 있고 시작 판정은 MatchStart 가 한다.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BGReadyComponent.generated.h"

class APlayerState;

UCLASS(ClassGroup = (BG), meta = (BlueprintSpawnableComponent))
class BG_API UBGReadyComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBGReadyComponent();

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 주어진 PlayerState 의 준비 컴포넌트. 없으면 nullptr.
	static UBGReadyComponent* Find(const APlayerState* PlayerState);

	// 월드의 접속자 수와 준비 인원을 센다. PlayerState 는 전원에게 복제되므로 클라에서도 유효하다.
	static void CountLobby(const UWorld* World, int32& OutTotal, int32& OutReady);

	UFUNCTION(BlueprintPure, Category = "BG|Ready")
	bool IsReady() const { return bReady; }

	// 로컬 플레이어가 준비를 토글할 때 부른다. 실제 반영은 서버에서.
	UFUNCTION(BlueprintCallable, Category = "BG|Ready")
	void RequestSetReady(bool bInReady);

	// 서버 전용 직접 설정(호스트 자동 준비 등).
	void SetReadyAuthoritative(bool bInReady);

private:
	UFUNCTION(Server, Reliable)
	void ServerSetReady(bool bInReady);

	UPROPERTY(Replicated)
	bool bReady = false;
};
