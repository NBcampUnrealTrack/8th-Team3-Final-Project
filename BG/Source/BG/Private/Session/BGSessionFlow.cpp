#include "Session/BGSessionFlow.h"
#include "Core/BGGameInstance.h"
#include "Lobby/BGMatchStartComponent.h"
#include "Lobby/BGReadyComponent.h"
#include "Debug/BGNetLog.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

void UBGSessionFlow::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	LoadConfig();
	BindGameInstanceEvents();

	UE_LOG(LogBGNet, Log, TEXT("[SessionFlow] Initialized (정원=%d)"), MaxPlayers);
}

void UBGSessionFlow::Deinitialize()
{
	UnbindGameInstanceEvents();
	Super::Deinitialize();
}

UBGGameInstance* UBGSessionFlow::GetBGGameInstance() const
{
	return Cast<UBGGameInstance>(GetGameInstance());
}

void UBGSessionFlow::BindGameInstanceEvents()
{
	if (UBGGameInstance* GI = GetBGGameInstance())
	{
		GI->OnCreateSessionComplete.AddUniqueDynamic(this, &UBGSessionFlow::HandleCreateSessionComplete);
		GI->OnFindSessionsComplete.AddUniqueDynamic(this, &UBGSessionFlow::HandleFindSessionsComplete);
		GI->OnJoinSessionComplete.AddUniqueDynamic(this, &UBGSessionFlow::HandleJoinSessionComplete);
	}
	else
	{
		// DefaultEngine.ini 의 GameInstanceClass 가 UBGGameInstance 가 아니면 세션 API 자체가 없다.
		UE_LOG(LogBGNet, Warning, TEXT("[SessionFlow] GameInstance 가 UBGGameInstance 가 아님 — 세션 바인딩 불가"));
	}
}

void UBGSessionFlow::UnbindGameInstanceEvents()
{
	if (UBGGameInstance* GI = GetBGGameInstance())
	{
		GI->OnCreateSessionComplete.RemoveDynamic(this, &UBGSessionFlow::HandleCreateSessionComplete);
		GI->OnFindSessionsComplete.RemoveDynamic(this, &UBGSessionFlow::HandleFindSessionsComplete);
		GI->OnJoinSessionComplete.RemoveDynamic(this, &UBGSessionFlow::HandleJoinSessionComplete);
	}
}

void UBGSessionFlow::SetPhase(EBGSessionPhase Phase, const FString& Message)
{
	UE_LOG(LogBGNet, Log, TEXT("[SessionFlow] Phase=%d %s"), static_cast<int32>(Phase), *Message);
	OnSessionPhaseChanged.Broadcast(Phase, Message);
}

bool UBGSessionFlow::IsHost() const
{
	if (const UWorld* World = GetWorld())
	{
		const ENetMode Mode = World->GetNetMode();
		return Mode == NM_ListenServer || Mode == NM_Standalone || Mode == NM_DedicatedServer;
	}
	return false;
}

FString UBGSessionFlow::GetRoomCode() const
{
	// 호스트 프로세스에만 코드가 있다. 참가자에게 보여주려면 GameState 복제가 필요하다(미구현).
	const UBGGameInstance* GI = GetBGGameInstance();
	return GI ? GI->GetHostRoomCode() : FString();
}

// ── 호스트 의도 ──
void UBGSessionFlow::HostCreateRoom(const FString& RoomName)
{
	UBGGameInstance* GI = GetBGGameInstance();
	if (!GI)
	{
		SetPhase(EBGSessionPhase::Failed, TEXT("GameInstance 가 UBGGameInstance 가 아님"));
		return;
	}

	// 이미 클라이언트로 다른 세션에 접속한 상태에서는 방 생성 금지 — 클라 월드에서는 리슨을 열 수 없다.
	if (const UWorld* World = GI->GetWorld(); World && World->GetNetMode() == NM_Client)
	{
		SetPhase(EBGSessionPhase::Failed, TEXT("이미 다른 세션에 접속 중입니다"));
		return;
	}

	SetPhase(EBGSessionPhase::Creating, TEXT("방 생성 중..."));
	// 성공 시 GameInstance 가 현재 월드에서 그대로 리슨을 연다(맵 이동 없음).
	GI->HostSession(MaxPlayers, /*bLAN=*/false, RoomName);
}

// ── 클라이언트 의도 ──
void UBGSessionFlow::JoinRoomByCode(const FString& RoomCode)
{
	UBGGameInstance* GI = GetBGGameInstance();
	if (!GI)
	{
		SetPhase(EBGSessionPhase::Failed, TEXT("GameInstance 가 UBGGameInstance 가 아님"));
		return;
	}

	// 코드 정규화(공백 제거 + 대문자) — 호스트 광고값과 동일 규칙으로 매칭.
	PendingJoinCode = RoomCode.TrimStartAndEnd().ToUpper();
	if (PendingJoinCode.IsEmpty())
	{
		SetPhase(EBGSessionPhase::Failed, TEXT("방 코드를 입력하세요"));
		return;
	}

	bWantsJoinAfterSearch = true;
	bWantsListAfterSearch = false; // 방찾기 검색과 동시 요청 시 마지막 의도만 유지
	SetPhase(EBGSessionPhase::Searching, TEXT("세션 검색 중..."));
	GI->FindSessions(/*bLAN=*/false, /*MaxSearchResults=*/20);
}

// ── 방찾기(세션 브라우저) ──
void UBGSessionFlow::RefreshSessionList(const FBGSessionSearchFilter& Filter)
{
	UBGGameInstance* GI = GetBGGameInstance();
	if (!GI)
	{
		SetPhase(EBGSessionPhase::Failed, TEXT("GameInstance 가 UBGGameInstance 가 아님"));
		return;
	}

	PendingListFilter = Filter;
	bWantsListAfterSearch = true;
	bWantsJoinAfterSearch = false; // 코드 조인 검색과 동시 요청 시 마지막 의도만 유지
	SetPhase(EBGSessionPhase::Searching, TEXT("세션 검색 중..."));
	// 클라 측 필터로 걸러질 것을 감안해 넉넉히 요청(최종 개수는 Filter.MaxResults 로 제한).
	GI->FindSessions(/*bLAN=*/false, /*MaxSearchResults=*/50);
}

void UBGSessionFlow::JoinSessionByIndex(int32 ResultIndex)
{
	UBGGameInstance* GI = GetBGGameInstance();
	if (!GI)
	{
		SetPhase(EBGSessionPhase::Failed, TEXT("GameInstance 가 UBGGameInstance 가 아님"));
		return;
	}

	SetPhase(EBGSessionPhase::Joining, TEXT("방 접속 중..."));
	// 인덱스 검증은 JoinFoundSession 에 위임 — 실패는 OnJoinSessionComplete(false) 로 돌아온다.
	GI->JoinFoundSession(ResultIndex);
}

void UBGSessionFlow::BroadcastFilteredSessionList(bool bSearchSuccess)
{
	UBGGameInstance* GI = GetBGGameInstance();
	if (!bSearchSuccess || !GI)
	{
		SetPhase(EBGSessionPhase::Failed, TEXT("세션 검색 실패"));
		OnSessionListReady.Broadcast(TArray<FBGSessionSearchResult>());
		return;
	}

	const FString NameQuery = PendingListFilter.RoomNameQuery.TrimStartAndEnd();
	TArray<FBGSessionSearchResult> Filtered;
	for (const FBGSessionSearchResult& Item : GI->GetFoundSessionResults())
	{
		if (!NameQuery.IsEmpty() && !Item.RoomName.Contains(NameQuery))
		{
			continue;
		}
		if (PendingListFilter.bHideFull && Item.CurrentPlayers >= Item.MaxPlayers)
		{
			continue;
		}
		Filtered.Add(Item);
		if (PendingListFilter.MaxResults > 0 && Filtered.Num() >= PendingListFilter.MaxResults)
		{
			break;
		}
	}

	// 0개는 실패가 아니라 "빈 목록" — 위젯이 '방 없음' 표시를 담당한다.
	SetPhase(EBGSessionPhase::Idle, FString::Printf(TEXT("방 %d개 검색됨"), Filtered.Num()));
	OnSessionListReady.Broadcast(Filtered);
}

// ── 공용 ──
void UBGSessionFlow::LeaveSession()
{
	if (UBGGameInstance* GI = GetBGGameInstance())
	{
		GI->DestroyCurrentSession();
	}
	// 맵 이동이 없으므로 타이틀 복귀 트래블도 없다 — 파기 후 그대로 Phase.Lobby 로 되돌아간다.
	SetPhase(EBGSessionPhase::Idle, TEXT("세션 종료"));
}

// ── 로비 준비 ──
UBGReadyComponent* UBGSessionFlow::GetLocalReadyComponent() const
{
	const UWorld* World = GetWorld();
	const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	return UBGReadyComponent::Find(PC ? PC->PlayerState : nullptr);
}

void UBGSessionFlow::SetLocalReady(bool bReady)
{
	UBGReadyComponent* Ready = GetLocalReadyComponent();
	if (!Ready)
	{
		SetPhase(EBGSessionPhase::Failed, TEXT("아직 방에 들어가지 않았습니다"));
		return;
	}
	Ready->RequestSetReady(bReady);
}

bool UBGSessionFlow::IsLocalReady() const
{
	const UBGReadyComponent* Ready = GetLocalReadyComponent();
	return Ready && Ready->IsReady();
}

FBGLobbyStatus UBGSessionFlow::GetLobbyStatus() const
{
	FBGLobbyStatus Status;
	UBGReadyComponent::CountLobby(GetWorld(), Status.CurrentPlayers, Status.ReadyPlayers);

	// MatchStart 는 서버에만 있다 — 클라에서는 인원/준비 수만 채워진다.
	if (const UBGMatchStartComponent* Match = UBGMatchStartComponent::Find(GetWorld()))
	{
		Status.RequiredPlayers = Match->GetMinStartPlayers();
		Status.bMatchStarted = Match->IsMatchStarted();
	}
	return Status;
}

bool UBGSessionFlow::CanStartGame(FString& OutReason) const
{
	if (!IsHost())
	{
		OutReason = TEXT("호스트만 시작할 수 있습니다");
		return false;
	}

	const UBGMatchStartComponent* Match = UBGMatchStartComponent::Find(GetWorld());
	if (!Match)
	{
		OutReason = TEXT("게임모드에 MatchStart 컴포넌트가 없습니다");
		return false;
	}
	return Match->CanStartMatch(OutReason);
}

void UBGSessionFlow::StartGame()
{
	FString Reason;
	if (!CanStartGame(Reason))
	{
		SetPhase(EBGSessionPhase::Failed, Reason);
		return;
	}

	// 맵 이동 없이 제자리에서 시작한다. 난입 차단은 StartMatch 안에서 처리된다.
	UBGMatchStartComponent* Match = UBGMatchStartComponent::Find(GetWorld());
	if (!Match || !Match->StartMatch())
	{
		SetPhase(EBGSessionPhase::Failed, TEXT("판 시작 실패"));
		return;
	}

	SetPhase(EBGSessionPhase::Hosting, TEXT("게임 시작"));
}

// ── UBGGameInstance 콜백 ──
void UBGSessionFlow::HandleCreateSessionComplete(bool bSuccess)
{
	SetPhase(bSuccess ? EBGSessionPhase::Hosting : EBGSessionPhase::Failed,
		bSuccess ? TEXT("방 생성 완료 — 리슨 시작") : TEXT("방 생성 실패"));
}

void UBGSessionFlow::HandleFindSessionsComplete(bool bSuccess, int32 NumFound)
{
	// 방찾기 목록 갱신 경로 — 결과에 필터를 적용해 OnSessionListReady 로 통지.
	if (bWantsListAfterSearch)
	{
		bWantsListAfterSearch = false;
		BroadcastFilteredSessionList(bSuccess);
		return;
	}

	if (!bWantsJoinAfterSearch)
	{
		return; // 단순 검색(외부 직접 호출 등)은 무시.
	}
	bWantsJoinAfterSearch = false;

	UBGGameInstance* GI = GetBGGameInstance();
	if (!bSuccess || NumFound <= 0 || !GI)
	{
		SetPhase(EBGSessionPhase::Failed, TEXT("접속 가능한 방이 없습니다"));
		return;
	}

	// 입력 코드와 광고된 방 코드가 일치하는 세션을 찾는다.
	int32 JoinIndex = INDEX_NONE;
	for (int32 i = 0; i < NumFound; ++i)
	{
		const FString FoundCode = GI->GetFoundSessionCode(i).TrimStartAndEnd().ToUpper();
		if (!FoundCode.IsEmpty() && FoundCode == PendingJoinCode)
		{
			JoinIndex = i;
			break;
		}
	}

	if (JoinIndex == INDEX_NONE)
	{
		SetPhase(EBGSessionPhase::Failed, FString::Printf(TEXT("코드 '%s' 와 일치하는 방이 없습니다"), *PendingJoinCode));
		return;
	}

	SetPhase(EBGSessionPhase::Joining, TEXT("방 접속 중..."));
	GI->JoinFoundSession(JoinIndex);
}

void UBGSessionFlow::HandleJoinSessionComplete(bool bSuccess)
{
	SetPhase(bSuccess ? EBGSessionPhase::Joined : EBGSessionPhase::Failed,
		bSuccess ? TEXT("방 접속 완료") : TEXT("방 접속 실패"));
}
