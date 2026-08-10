#include "Session/TRReconnectComponent.h"

#include "Core/TRGameInstance.h"

#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

UTRReconnectComponent::UTRReconnectComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

FString UTRReconnectComponent::ParsePlayerKey(const FString& Options)
{
	return UGameplayStatics::ParseOption(Options, UTRGameInstance::PlayerKeyOption);
}

FString UTRReconnectComponent::MakeNetIdKey(const FUniqueNetIdRepl& UniqueId)
{
	// FUniqueNetIdRepl::ToString 은 OnlineSubsystemUtils 심볼이라 게임 모듈에서 링크되지 않는다.
	const FUniqueNetIdPtr Raw = UniqueId.GetUniqueNetId();
	return Raw.IsValid() ? Raw->ToString() : FString();
}

void UTRReconnectComponent::PruneExpired()
{
	if (RejoinWindowSeconds <= 0.f)
	{
		return;
	}

	const double Now = FPlatformTime::Seconds();

	AbsentPlayers.RemoveAll([this, Now](const FTRAbsentPlayer& Absent)
	{
		return (Now - Absent.LeftAt) > RejoinWindowSeconds;
	});
}

int32 UTRReconnectComponent::FindAbsentIndex(const FString& PlayerKey, const FString& NetId) const
{
	// 열쇠가 1순위다. NULL 서브시스템은 접속마다 NetId 가 바뀌어 믿을 수 없다.
	if (!PlayerKey.IsEmpty())
	{
		const int32 ByKey = AbsentPlayers.IndexOfByPredicate(
			[&PlayerKey](const FTRAbsentPlayer& A) { return A.PlayerKey == PlayerKey; });

		if (ByKey != INDEX_NONE)
		{
			return ByKey;
		}
	}

	if (!NetId.IsEmpty())
	{
		return AbsentPlayers.IndexOfByPredicate(
			[&NetId](const FTRAbsentPlayer& A) { return !A.NetId.IsEmpty() && A.NetId == NetId; });
	}

	return INDEX_NONE;
}

void UTRReconnectComponent::RememberPlayer(const FString& PlayerKey, const FUniqueNetIdRepl& UniqueId)
{
	// 서버 전용이다. 클라에서 불렸다면 호출부가 잘못됐다.
	if (!ensureMsgf(GetOwner() && GetOwner()->HasAuthority(),
			TEXT("RememberPlayer 는 서버에서만 불러야 한다")))
	{
		return;
	}

	// 열쇠도 NetId 도 없으면 돌아와도 알아볼 방법이 없다. 설정에 따라 생길 수 있어 어서트하지 않는다.
	const FString NetId = MakeNetIdKey(UniqueId);
	if (PlayerKey.IsEmpty() && NetId.IsEmpty())
	{
		return;
	}

	ForgetPlayer(PlayerKey, UniqueId);

	FTRAbsentPlayer& Absent = AbsentPlayers.AddDefaulted_GetRef();
	Absent.PlayerKey = PlayerKey;
	Absent.NetId     = NetId;
	Absent.LeftAt    = FPlatformTime::Seconds();
}

bool UTRReconnectComponent::IsKnownPlayer(const FString& PlayerKey, const FUniqueNetIdRepl& UniqueId) const
{
	const_cast<UTRReconnectComponent*>(this)->PruneExpired();

	return FindAbsentIndex(PlayerKey, MakeNetIdKey(UniqueId)) != INDEX_NONE;
}

bool UTRReconnectComponent::NotifyPlayerLogin(APlayerController* Joining,
	const FString& PlayerKey, const FUniqueNetIdRepl& UniqueId)
{
	if (!ensureMsgf(IsValid(Joining) && GetOwner() && GetOwner()->HasAuthority(),
			TEXT("NotifyPlayerLogin 은 유효한 컨트롤러로 서버에서만 불러야 한다")))
	{
		return false;
	}

	PruneExpired();

	const int32 Index = FindAbsentIndex(PlayerKey, MakeNetIdKey(UniqueId));
	if (Index == INDEX_NONE)
	{
		return false;
	}

	AbsentPlayers.RemoveAt(Index);

	// 무엇을 되돌릴지는 게임 규칙의 몫이다. 여기서는 "돌아왔다" 만 알린다.
	OnPlayerRejoined.Broadcast(Joining);
	return true;
}

void UTRReconnectComponent::ForgetPlayer(const FString& PlayerKey, const FUniqueNetIdRepl& UniqueId)
{
	const int32 Index = FindAbsentIndex(PlayerKey, MakeNetIdKey(UniqueId));
	if (Index != INDEX_NONE)
	{
		AbsentPlayers.RemoveAt(Index);
	}
}

void UTRReconnectComponent::ForgetAll()
{
	AbsentPlayers.Reset();
}
