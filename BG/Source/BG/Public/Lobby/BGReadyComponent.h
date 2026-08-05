// BANG! 준비 상태 — PlayerState 에 붙는다. 상태만 들고 있고 시작 판정은 MatchStart 가 한다.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BGReadyComponent.generated.h"

class APlayerState;

/** 누군가의 준비 상태가 바뀌었다. 로비 UI 가 폴링 대신 이걸 구독한다. */
DECLARE_MULTICAST_DELEGATE(FOnBGReadyChanged);

UCLASS(ClassGroup = (BG), meta = (BlueprintSpawnableComponent))
class BG_API UBGReadyComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBGReadyComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 주어진 PlayerState 의 준비 컴포넌트. 없으면 nullptr.
	static UBGReadyComponent* Find(const APlayerState* PlayerState);

	// 월드의 접속자 수와 준비 인원을 센다. PlayerState 는 전원에게 복제되므로 클라에서도 유효하다.
	static void CountLobby(const UWorld* World, int32& OutTotal, int32& OutReady);

	// 준비 상태나 로비 인원이 바뀌었다. 컴포넌트마다 따로 구독할 수 없으니
	// (누가 들어올지 모른다) 정적 이벤트 하나로 모은다.
	static FOnBGReadyChanged OnAnyReadyChanged;

	// 서버·클라 공통. 인원이 들고 날 때 호출한다(준비 변경은 이 컴포넌트가 스스로 알린다).
	static void NotifyLobbyChanged();

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

	UFUNCTION()
	void OnRep_Ready();

	UPROPERTY(ReplicatedUsing = OnRep_Ready)
	bool bReady = false;
};
