#include "Session/TRSessionFlow.h"

#include "Core/TRGameInstance.h"
#include "Core/TRGameState.h"
#include "Core/TRPlayerState.h"
#include "Player/TRPlayerController.h"
#include "Session/TRNetLog.h"

#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"

void UTRSessionFlow::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	BindGameInstanceEvents();
}

void UTRSessionFlow::Deinitialize()
{
	UnbindGameInstanceEvents();

	Super::Deinitialize();
}

UTRGameInstance* UTRSessionFlow::GetTRGameInstance() const
{
	return Cast<UTRGameInstance>(GetGameInstance());
}

void UTRSessionFlow::BindGameInstanceEvents()
{
	UTRGameInstance* GI = GetTRGameInstance();
	if (!ensureMsgf(GI, TEXT("GameInstance 가 UTRGameInstance 가 아니다 — 프로젝트 설정을 확인하세요")))
	{
		return;
	}

	GI->OnCreateSessionComplete.AddUniqueDynamic(this, &UTRSessionFlow::HandleCreateSessionComplete);
	GI->OnFindSessionsComplete.AddUniqueDynamic(this, &UTRSessionFlow::HandleFindSessionsComplete);
	GI->OnJoinSessionComplete.AddUniqueDynamic(this, &UTRSessionFlow::HandleJoinSessionComplete);

	UE_LOG(LogTRNet, Log, TEXT("[SessionFlow] Initialized (정원=%d)"), MaxPlayers);
}

void UTRSessionFlow::UnbindGameInstanceEvents()
{
	if (UTRGameInstance* GI = GetTRGameInstance())
	{
		GI->OnCreateSessionComplete.RemoveDynamic(this, &UTRSessionFlow::HandleCreateSessionComplete);
		GI->OnFindSessionsComplete.RemoveDynamic(this, &UTRSessionFlow::HandleFindSessionsComplete);
		GI->OnJoinSessionComplete.RemoveDynamic(this, &UTRSessionFlow::HandleJoinSessionComplete);
	}
}

void UTRSessionFlow::SetPhase(ETRSessionPhase Phase, const FString& Message)
{
	UE_LOG(LogTRNet, Log, TEXT("[SessionFlow] Phase=%d %s"), static_cast<int32>(Phase), *Message);

	OnSessionPhaseChanged.Broadcast(Phase, Message);
}

// ─────────────────────────────────────────────────────────────
// 의도
// ─────────────────────────────────────────────────────────────

void UTRSessionFlow::HostCreateRoom(const FString& RoomName)
{
	UTRGameInstance* GI = GetTRGameInstance();
	if (!ensure(GI))
	{
		SetPhase(ETRSessionPhase::Failed, TEXT("게임 인스턴스를 찾을 수 없습니다"));
		return;
	}

	SetPhase(ETRSessionPhase::Creating, TEXT("방 생성 중..."));
	GI->HostSession(MaxPlayers, /*bLAN=*/false, RoomName);
}

void UTRSessionFlow::JoinRoomByCode(const FString& RoomCode)
{
	UTRGameInstance* GI = GetTRGameInstance();
	if (!ensure(GI))
	{
		SetPhase(ETRSessionPhase::Failed, TEXT("게임 인스턴스를 찾을 수 없습니다"));
		return;
	}

	// 빈 코드는 사용자 입력 오류다 — 어서트하지 않고 안내만 한다.
	const FString Trimmed = RoomCode.TrimStartAndEnd().ToUpper();
	if (Trimmed.IsEmpty())
	{
		SetPhase(ETRSessionPhase::Failed, TEXT("방 코드를 입력하세요"));
		return;
	}

	// 코드로는 바로 붙을 수 없다 — 검색해서 광고된 코드와 맞춰야 세션 핸들을 얻는다.
	PendingJoinCode = Trimmed;
	bWantsJoinAfterSearch = true;
	bWantsListAfterSearch = false;

	SetPhase(ETRSessionPhase::Searching, TEXT("방 검색 중..."));
	GI->FindSessions(/*bLAN=*/false, /*MaxSearchResults=*/50);
}

void UTRSessionFlow::RefreshSessionList(const FTRSessionSearchFilter& Filter)
{
	UTRGameInstance* GI = GetTRGameInstance();
	if (!ensure(GI))
	{
		OnSessionListReady.Broadcast(TArray<FTRSessionSearchResult>());
		return;
	}

	PendingListFilter = Filter;
	bWantsListAfterSearch = true;
	bWantsJoinAfterSearch = false;

	SetPhase(ETRSessionPhase::Searching, TEXT("방 목록 갱신 중..."));
	GI->FindSessions(/*bLAN=*/false, FMath::Max(1, Filter.MaxResults));
}

void UTRSessionFlow::JoinSessionByIndex(int32 ResultIndex)
{
	UTRGameInstance* GI = GetTRGameInstance();
	if (!ensure(GI))
	{
		SetPhase(ETRSessionPhase::Failed, TEXT("게임 인스턴스를 찾을 수 없습니다"));
		return;
	}

	SetPhase(ETRSessionPhase::Joining, TEXT("방 접속 중..."));
	GI->JoinFoundSession(ResultIndex);
}

void UTRSessionFlow::LeaveSession()
{
	UTRGameInstance* GI = GetTRGameInstance();
	if (!ensure(GI))
	{
		return;
	}

	GI->DestroyCurrentSession();
	GI->ReturnToMainMenu();

	SetPhase(ETRSessionPhase::Idle);
}

void UTRSessionFlow::OpenInviteOverlay()
{
	if (UTRGameInstance* GI = GetTRGameInstance())
	{
		GI->OpenInviteOverlay();
	}
}

// ─────────────────────────────────────────────────────────────
// 조회
// ─────────────────────────────────────────────────────────────

bool UTRSessionFlow::IsHost() const
{
	const UWorld* World = GetWorld();
	if (!ensure(World))
	{
		return false;
	}

	const ENetMode NetMode = World->GetNetMode();
	return NetMode == NM_ListenServer || NetMode == NM_DedicatedServer || NetMode == NM_Standalone;
}

FString UTRSessionFlow::GetRoomCode() const
{
	const UTRGameInstance* GI = GetTRGameInstance();
	return GI ? GI->GetHostRoomCode() : FString();
}

FTRLobbyStatus UTRSessionFlow::GetLobbyStatus() const
{
	FTRLobbyStatus Status;

	// GameState 는 접속 직후 아직 안 와 있을 수 있다 — 정상 상태라 어서트하지 않는다.
	const UWorld* World = GetWorld();
	const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
	if (!GS)
	{
		return Status;
	}

	Status.CurrentPlayers = GS->PlayerArray.Num();

	const ATRGameState* TRGS = Cast<ATRGameState>(GS);
	if (TRGS == nullptr)
	{
		return Status;
	}

	Status.ReadyPlayers = TRGS->GetReadyPlayerCount();
	Status.RequiredPlayers = TRGS->GetRequiredReadyCount();
	Status.bMatchStarted = TRGS->IsMatchStarted();

	const ATRPlayerState* LocalState = GetLocalPlayerState();
	Status.bLocalReady = LocalState != nullptr && LocalState->IsReady();
	Status.bCanStart = LocalState != nullptr && LocalState->IsHost() && TRGS->CanStartMatch();

	return Status;
}

void UTRSessionFlow::SetReady(bool bInReady)
{
	if (ATRPlayerController* PC = GetLocalController())
	{
		PC->SetLobbyReady(bInReady);
	}
}

void UTRSessionFlow::StartMatch()
{
	if (ATRPlayerController* PC = GetLocalController())
	{
		PC->RequestStartMatch();
	}
}

ATRPlayerController* UTRSessionFlow::GetLocalController() const
{
	const UWorld* World = GetWorld();

	return World ? Cast<ATRPlayerController>(World->GetFirstPlayerController()) : nullptr;
}

const ATRPlayerState* UTRSessionFlow::GetLocalPlayerState() const
{
	const ATRPlayerController* PC = GetLocalController();

	return PC ? PC->GetPlayerState<ATRPlayerState>() : nullptr;
}

// ─────────────────────────────────────────────────────────────
// GameInstance 콜백
// ─────────────────────────────────────────────────────────────

void UTRSessionFlow::HandleCreateSessionComplete(bool bSuccess)
{
	if (!bSuccess)
	{
		SetPhase(ETRSessionPhase::Failed, TEXT("방 생성에 실패했습니다"));
		return;
	}

	SetPhase(ETRSessionPhase::Hosting, TEXT("방 생성 완료 — 리슨 시작"));
}

void UTRSessionFlow::HandleFindSessionsComplete(bool bSuccess, int32 NumFound)
{
	UTRGameInstance* GI = GetTRGameInstance();

	// 목록 갱신 의도였으면 필터를 적용해 돌려주고 끝낸다.
	if (bWantsListAfterSearch)
	{
		bWantsListAfterSearch = false;
		BroadcastFilteredSessionList(bSuccess);
		return;
	}

	if (!bWantsJoinAfterSearch || !GI)
	{
		SetPhase(ETRSessionPhase::Idle, FString::Printf(TEXT("방 %d개 검색됨"), NumFound));
		return;
	}

	bWantsJoinAfterSearch = false;

	if (!bSuccess)
	{
		SetPhase(ETRSessionPhase::Failed, TEXT("방 검색에 실패했습니다"));
		return;
	}

	// 광고된 방 코드와 맞는 세션을 찾는다.
	const int32 Count = GI->GetFoundSessionCount();
	for (int32 Index = 0; Index < Count; ++Index)
	{
		if (GI->GetFoundSessionCode(Index).Equals(PendingJoinCode, ESearchCase::IgnoreCase))
		{
			SetPhase(ETRSessionPhase::Joining, TEXT("방 접속 중..."));
			GI->JoinFoundSession(Index);
			return;
		}
	}

	SetPhase(ETRSessionPhase::Failed, FString::Printf(TEXT("코드 %s 인 방을 찾지 못했습니다"), *PendingJoinCode));
}

void UTRSessionFlow::HandleJoinSessionComplete(bool bSuccess)
{
	if (!bSuccess)
	{
		SetPhase(ETRSessionPhase::Failed, TEXT("방 접속에 실패했습니다"));
		return;
	}

	SetPhase(ETRSessionPhase::Joined, TEXT("방 접속 완료"));
}

void UTRSessionFlow::BroadcastFilteredSessionList(bool bSearchSuccess)
{
	UTRGameInstance* GI = GetTRGameInstance();
	if (!bSearchSuccess || !GI)
	{
		OnSessionListReady.Broadcast(TArray<FTRSessionSearchResult>());
		SetPhase(ETRSessionPhase::Idle);
		return;
	}

	TArray<FTRSessionSearchResult> Filtered;
	const FString Query = PendingListFilter.RoomNameQuery.TrimStartAndEnd();

	for (const FTRSessionSearchResult& Result : GI->GetFoundSessionResults())
	{
		if (PendingListFilter.bHideFull && !Result.bCanJoin)
		{
			continue;
		}

		// 부분 문자열 필터는 여기서 적용한다 — OSS 쿼리는 부분검색을 지원하지 않는다.
		if (!Query.IsEmpty() && !Result.RoomName.Contains(Query, ESearchCase::IgnoreCase))
		{
			continue;
		}

		Filtered.Add(Result);

		if (PendingListFilter.MaxResults > 0 && Filtered.Num() >= PendingListFilter.MaxResults)
		{
			break;
		}
	}

	OnSessionListReady.Broadcast(Filtered);
	SetPhase(ETRSessionPhase::Idle, FString::Printf(TEXT("방 %d개"), Filtered.Num()));
}
