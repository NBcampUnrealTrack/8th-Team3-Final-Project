// TR 재접속 — 판에 있던 사람이 다시 들어왔는지만 판정한다.
//
// 무엇을 되돌릴지는 게임 규칙이 정해져야 한다. 이 컴포넌트는 신원만 알아보고
// 복원은 OnPlayerRejoined 를 구독한 쪽이 한다.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameFramework/OnlineReplStructs.h"
#include "TRReconnectComponent.generated.h"

class APlayerController;

/** 전에 있던 사람이 다시 들어왔다. 무엇을 되돌릴지는 구독자가 정한다. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTRPlayerRejoined, APlayerController*, Rejoining);

/**
 * GameMode 에 붙는 서버 전용 컴포넌트.
 *
 * 흐름: Logout 에서 RememberPlayer → 같은 열쇠로 다시 들어오면 NotifyPlayerLogin 이 참을 준다.
 * 열쇠는 접속 URL 의 ?TRKey= 이고 UTRGameInstance 가 실어 보낸다.
 */
UCLASS(ClassGroup = (TR), meta = (BlueprintSpawnableComponent))
class TR_API UTRReconnectComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTRReconnectComponent();

	/**
	 * 접속 URL 에서 재접속 열쇠를 꺼낸다. PreLogin/Login 의 Options 를 그대로 넘기면 된다.
	 * 없으면 빈 문자열 — 그때는 UniqueNetId 로 대신 찾는다.
	 */
	UFUNCTION(BlueprintPure, Category = "TR|Reconnect")
	static FString ParsePlayerKey(const FString& Options);

	/**
	 * 나간 사람을 기억해 둔다. Logout 에서 부른다.
	 * 복귀를 받지 않을 상황이면 부르지 않으면 된다.
	 */
	UFUNCTION(BlueprintCallable, Category = "TR|Reconnect")
	void RememberPlayer(const FString& PlayerKey, const FUniqueNetIdRepl& UniqueId);

	/**
	 * 전에 있던 사람인가. PreLogin 에서 난입을 막을 때 예외로 통과시킨다.
	 * 판이 진행 중이어도 복귀자는 받아야 하기 때문이다.
	 */
	UFUNCTION(BlueprintPure, Category = "TR|Reconnect")
	bool IsKnownPlayer(const FString& PlayerKey, const FUniqueNetIdRepl& UniqueId) const;

	/**
	 * 들어온 사람이 복귀자면 참을 주고 OnPlayerRejoined 를 발사한다. PostLogin 에서 부른다.
	 * 기억은 여기서 지워진다 — 한 번 돌아온 사람은 더 이상 복귀 대상이 아니다.
	 */
	UFUNCTION(BlueprintCallable, Category = "TR|Reconnect")
	bool NotifyPlayerLogin(APlayerController* Joining, const FString& PlayerKey, const FUniqueNetIdRepl& UniqueId);

	/** 기억을 버린다. 복귀를 포기시키거나 판이 끝날 때. */
	UFUNCTION(BlueprintCallable, Category = "TR|Reconnect")
	void ForgetPlayer(const FString& PlayerKey, const FUniqueNetIdRepl& UniqueId);

	UFUNCTION(BlueprintCallable, Category = "TR|Reconnect")
	void ForgetAll();

	UPROPERTY(BlueprintAssignable, Category = "TR|Reconnect")
	FOnTRPlayerRejoined OnPlayerRejoined;

protected:
	/**
	 * 이만큼 지나면 기억을 스스로 버린다(초). 0 이하면 판이 끝날 때까지 기다린다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "TR|Reconnect")
	float RejoinWindowSeconds = 0.f;

private:
	struct FTRAbsentPlayer
	{
		FString PlayerKey;        // 접속 URL 의 ?TRKey= (1순위)
		FString NetId;            // UniqueNetId 문자열 (2순위, 스팀에서만 안정적)
		double  LeftAt = 0.0;     // 만료 판정용
	};

	TArray<FTRAbsentPlayer> AbsentPlayers;

	// NULL 서브시스템은 접속마다 새 UniqueNetId 를 만든다 — 그때는 열쇠만 믿어야 한다.
	static FString MakeNetIdKey(const FUniqueNetIdRepl& UniqueId);

	int32 FindAbsentIndex(const FString& PlayerKey, const FString& NetId) const;

	// 만료된 기억을 걷는다. 조회 전에 한 번 돌린다.
	void PruneExpired();
};
