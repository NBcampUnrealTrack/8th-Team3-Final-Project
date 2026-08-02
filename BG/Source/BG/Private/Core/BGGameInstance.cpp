#include "Core/BGGameInstance.h"
#include "Session/BGOssPolicy.h"
#include "Session/BGSessionFlow.h"
#include "Debug/BGNetLog.h"
#include "Core/BGGameMode.h"
#include "AbilitySystemGlobals.h"
#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Online/OnlineSessionNames.h"   // SETTING_MAPNAME / SEARCH_LOBBIES 등 표준 키
#include "Misc/Base64.h"

namespace
{
	// BG 세션 식별 키 — 검색 시 우리 게임 세션만 필터링.
	const FName BG_SESSION_KEY = TEXT("BGGameName");
	const FString BG_SESSION_VALUE = TEXT("BG");

	// 방 코드 광고 키 — 클라이언트가 입력한 코드와 매칭.
	const FName BG_ROOMCODE_KEY = TEXT("BGRoomCode");

	// 방 이름 광고 키 — 방찾기 목록 표시·부분 문자열 검색용.
	const FName BG_ROOMNAME_KEY = TEXT("BGRoomName");

	// 광고 문자열은 Base64(ASCII)로 왕복 — Steam 로비 데이터는 ANSI 로 읽혀 한글이 깨진다.
	const FString BG_B64_PREFIX = TEXT("b64:");

	FString EncodeAdvertisedString(const FString& In)
	{
		const FTCHARToUTF8 Utf8(*In);
		TArray<uint8> Bytes(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
		return BG_B64_PREFIX + FBase64::Encode(Bytes);
	}

	FString DecodeAdvertisedString(const FString& In)
	{
		if (!In.StartsWith(BG_B64_PREFIX))
		{
			return In;
		}
		TArray<uint8> Bytes;
		if (!FBase64::Decode(In.Mid(BG_B64_PREFIX.Len()), Bytes))
		{
			return In;
		}
		Bytes.Add(0);	// 널 종단
		return FString(UTF8_TO_TCHAR(reinterpret_cast<const char*>(Bytes.GetData())));
	}
}

void UBGGameInstance::Init()
{
	Super::Init();

	// GAS TargetData 직렬화가 이 초기화를 전제로 한다 — 누락 시 첫 어빌리티 타깃 전달에서 크래시.
	UAbilitySystemGlobals::Get().InitGlobalData();

	// 접속 끊김 감시 — 클라이언트로 튕겼을 때 남는 로컬 세션을 정리한다(Shutdown 에서 해제).
	if (GEngine)
	{
		NetworkFailureHandle = GEngine->OnNetworkFailure().AddUObject(this, &UBGGameInstance::HandleNetworkFailure);
	}

}

void UBGGameInstance::Shutdown()
{
	if (GEngine)
	{
		GEngine->OnNetworkFailure().Remove(NetworkFailureHandle);
	}
	Super::Shutdown();
}

IOnlineSessionPtr UBGGameInstance::GetSessionInterface() const
{
	IOnlineSubsystem* OSS = Online::GetSubsystem(GetWorld());
	if (!OSS)
	{
		return nullptr;
	}
	return OSS->GetSessionInterface();
}

// 6자 방 코드 — 구두 전달을 전제로 혼동 문자(0/O, 1/I) 제외한 32자 집합.
FString UBGGameInstance::GenerateRoomCode()
{
	static const TCHAR Alphabet[] = TEXT("ABCDEFGHJKLMNPQRSTUVWXYZ23456789");
	const int32 AlphabetLen = UE_ARRAY_COUNT(Alphabet) - 1;	// 널 종단 제외
	FString Code;
	Code.Reserve(6);
	for (int32 i = 0; i < 6; ++i)
	{
		Code.AppendChar(Alphabet[FMath::RandRange(0, AlphabetLen - 1)]);
	}
	return Code;
}

// ─────────────────────────────────────────────────────────────
// 호스트
// ─────────────────────────────────────────────────────────────

void UBGGameInstance::HostSession(int32 MaxPlayers, bool bLAN, const FString& RoomName)
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (!Sessions.IsValid())
	{
		// OSS 미초기화 시 폴백 — 광고 없이 리슨만 열어 직접 IP 접속(open <IP>)은 살린다.
		UE_LOG(LogBGNet, Warning, TEXT("HostSession: SessionInterface 없음 — 세션 광고 없이 리슨만 시작"));
		OnCreateSessionComplete.Broadcast(StartListenInPlace());
		return;
	}

	// 남은 세션이 있으면 파기 완료 후 생성 — 즉시 CreateSession 하면 "세션 이미 존재"로 조용히 실패한다.
	const EOnlineSessionState::Type SessionState = Sessions->GetSessionState(NAME_GameSession);
	if (SessionState != EOnlineSessionState::NoSession)
	{
		UE_LOG(LogBGNet, Log, TEXT("HostSession: 기존 세션(상태=%s) 파기 완료 후 생성 예약"),
			EOnlineSessionState::ToString(SessionState));

		// 생성/조인 예약은 동시에 살아 있으면 안 된다.
		PendingJoinRequest = FPendingJoinRequest();
		PendingHostRequest.RoomName = RoomName;
		PendingHostRequest.MaxPlayers = MaxPlayers;
		PendingHostRequest.bLAN = bLAN;
		PendingHostRequest.bValid = true;

		if (SessionState != EOnlineSessionState::Destroying)
		{
			DestroyCurrentSession();
		}
		return;
	}

	StartHostSession(MaxPlayers, bLAN, RoomName);
}

void UBGGameInstance::StartHostSession(int32 MaxPlayers, bool bLAN, const FString& RoomName)
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (!Sessions.IsValid())
	{
		UE_LOG(LogBGNet, Warning, TEXT("StartHostSession: SessionInterface 없음 — 세션 광고 없이 리슨만 시작"));
		OnCreateSessionComplete.Broadcast(StartListenInPlace());
		return;
	}

	// 이번 세션의 방 코드 생성 — 호스트 UI 표시 + 클라 매칭용.
	HostRoomCode = GenerateRoomCode();

	// 방 이름 결정 — 미지정 시 호스트 표시명 기반 기본값(방찾기 목록 표시용).
	FString ResolvedRoomName = RoomName.TrimStartAndEnd();
	if (ResolvedRoomName.IsEmpty())
	{
		IOnlineSubsystem* OSS = Online::GetSubsystem(GetWorld());
		IOnlineIdentityPtr Identity = OSS ? OSS->GetIdentityInterface() : nullptr;
		const FString Nickname = Identity.IsValid() ? Identity->GetPlayerNickname(0) : FString();
		ResolvedRoomName = Nickname.IsEmpty() ? BG_SESSION_VALUE : FString::Printf(TEXT("%s의 방"), *Nickname);
	}

	const int32 Slots = FMath::Clamp(MaxPlayers, ABGGameMode::MinPlayers, ABGGameMode::MaxPlayers);

	// Steam 이 안 떠 있으면(NULL 폴백) 호출부가 뭘 넘겼든 LAN 매치여야 서로를 찾는다.
	const FBGOssPolicy Policy = FBGOssPolicy::Resolve(GetWorld());
	const bool bLanMatch = bLAN || Policy.bLanOnly;

	UE_LOG(LogBGNet, Log, TEXT("CreateSession 요청: 방코드=%s 방이름='%s' 정원=%d LAN=%d [%s]"),
		*HostRoomCode, *ResolvedRoomName, Slots, bLanMatch, *Policy.ToString());

	// 맵 이동을 하지 않으므로 현재 월드의 맵 이름을 그대로 광고한다.
	const UWorld* World = GetWorld();
	const FString MapName = World ? World->GetMapName() : FString();

	FOnlineSessionSettings Settings;
	Settings.bIsLANMatch = bLanMatch;
	Settings.NumPublicConnections = Slots;
	Settings.NumPrivateConnections = 0;
	Settings.bShouldAdvertise = true;          // 검색 목록에 노출
	Settings.bAllowJoinInProgress = true;      // Phase.Lobby 동안 참여 허용(판 시작 시 SetAllowJoinInProgress(false))
	Settings.bAllowJoinViaPresence = Policy.bUsePresence && !bLanMatch;
	Settings.bUsesPresence = Policy.bUsePresence && !bLanMatch;
	Settings.bUseLobbiesIfAvailable = Policy.bUseLobbies && !bLanMatch;
	Settings.bAllowInvites = true;

	Settings.Set(BG_SESSION_KEY, BG_SESSION_VALUE, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	Settings.Set(SETTING_MAPNAME, MapName, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	Settings.Set(BG_ROOMCODE_KEY, HostRoomCode, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	Settings.Set(BG_ROOMNAME_KEY, EncodeAdvertisedString(ResolvedRoomName), EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);

	CreateSessionCompleteHandle = Sessions->AddOnCreateSessionCompleteDelegate_Handle(
		FOnCreateSessionCompleteDelegate::CreateUObject(this, &UBGGameInstance::HandleCreateSessionComplete));

	if (!Sessions->CreateSession(0, NAME_GameSession, Settings))
	{
		UE_LOG(LogBGNet, Warning, TEXT("CreateSession 호출 실패 (세션상태=%s) — 직전 LogOnlineSession 경고 확인"),
			EOnlineSessionState::ToString(Sessions->GetSessionState(NAME_GameSession)));
		Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteHandle);
		HostRoomCode.Reset();
		OnCreateSessionComplete.Broadcast(false);
	}
}

void UBGGameInstance::HandleCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (IOnlineSessionPtr Sessions = GetSessionInterface())
	{
		Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteHandle);
	}

	UE_LOG(LogBGNet, Log, TEXT("CreateSession 완료: %s (성공=%d)"), *SessionName.ToString(), bWasSuccessful);

	if (!bWasSuccessful)
	{
		HostRoomCode.Reset();
		OnCreateSessionComplete.Broadcast(false);
		return;
	}

	// 단일 맵이라 ServerTravel 대신 제자리 리슨.
	OnCreateSessionComplete.Broadcast(StartListenInPlace());
}

bool UBGGameInstance::StartListenInPlace()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogBGNet, Warning, TEXT("StartListenInPlace: World 없음"));
		return false;
	}

	const ENetMode NetMode = World->GetNetMode();
	if (NetMode == NM_ListenServer || NetMode == NM_DedicatedServer)
	{
		UE_LOG(LogBGNet, Log, TEXT("StartListenInPlace: 이미 리슨 중 — 유지"));
		return true;
	}
	if (NetMode == NM_Client)
	{
		UE_LOG(LogBGNet, Warning, TEXT("StartListenInPlace: 클라이언트로 접속 중 — 리슨 불가"));
		return false;
	}

	// 맵 재로드 없이 넷드라이버만 연다(엔진의 URL ?listen 처리와 같은 경로).
	FURL ListenURL = World->URL;
	ListenURL.AddOption(TEXT("Listen"));

	if (!World->Listen(ListenURL))
	{
		UE_LOG(LogBGNet, Error, TEXT("StartListenInPlace: Listen 실패 — 직전 LogNet 오류 확인"));
		return false;
	}

	UE_LOG(LogBGNet, Log, TEXT("StartListenInPlace: 리슨 시작 (맵=%s, 이동 없음)"), *World->GetMapName());
	return true;
}

// ─────────────────────────────────────────────────────────────
// 검색 / 조인
// ─────────────────────────────────────────────────────────────

void UBGGameInstance::FindSessions(bool bLAN, int32 MaxSearchResults)
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (!Sessions.IsValid())
	{
		UE_LOG(LogBGNet, Warning, TEXT("FindSessions: SessionInterface 없음"));
		OnFindSessionsComplete.Broadcast(false, 0);
		return;
	}

	const FBGOssPolicy Policy = FBGOssPolicy::Resolve(GetWorld());
	const bool bLanQuery = bLAN || Policy.bLanOnly;

	SessionSearch = MakeShared<FOnlineSessionSearch>();
	SessionSearch->bIsLanQuery = bLanQuery;
	SessionSearch->MaxSearchResults = FMath::Max(1, MaxSearchResults);
	// Steam 로비 세션 검색(호스트의 bUseLobbiesIfAvailable 과 짝). LAN 검색에선 비적용.
	if (Policy.bUseLobbies && !bLanQuery)
	{
		SessionSearch->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);
	}
	SessionSearch->QuerySettings.Set(BG_SESSION_KEY, BG_SESSION_VALUE, EOnlineComparisonOp::Equals);

	UE_LOG(LogBGNet, Log, TEXT("FindSessions 요청: LAN=%d 최대=%d [%s]"),
		bLanQuery, SessionSearch->MaxSearchResults, *Policy.ToString());

	FindSessionsCompleteHandle = Sessions->AddOnFindSessionsCompleteDelegate_Handle(
		FOnFindSessionsCompleteDelegate::CreateUObject(this, &UBGGameInstance::HandleFindSessionsComplete));

	if (!Sessions->FindSessions(0, SessionSearch.ToSharedRef()))
	{
		UE_LOG(LogBGNet, Warning, TEXT("FindSessions: 호출 실패"));
		Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteHandle);
		OnFindSessionsComplete.Broadcast(false, 0);
	}
}

void UBGGameInstance::HandleFindSessionsComplete(bool bWasSuccessful)
{
	if (IOnlineSessionPtr Sessions = GetSessionInterface())
	{
		Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteHandle);
	}

	const int32 Num = SessionSearch.IsValid() ? SessionSearch->SearchResults.Num() : 0;
	UE_LOG(LogBGNet, Log, TEXT("FindSessions 완료: 성공=%d, %d개"), bWasSuccessful, Num);
	OnFindSessionsComplete.Broadcast(bWasSuccessful, Num);
}

void UBGGameInstance::JoinFoundSession(int32 SearchResultIndex)
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (!Sessions.IsValid() || !SessionSearch.IsValid() ||
		!SessionSearch->SearchResults.IsValidIndex(SearchResultIndex))
	{
		UE_LOG(LogBGNet, Warning, TEXT("JoinFoundSession: 유효하지 않은 인덱스/검색결과(%d)"), SearchResultIndex);
		OnJoinSessionComplete.Broadcast(false);
		return;
	}

	// 판이 진행 중이면 참여 불가.
	const FOnlineSessionSearchResult& Result = SessionSearch->SearchResults[SearchResultIndex];
	if (!Result.Session.SessionSettings.bAllowJoinInProgress)
	{
		UE_LOG(LogBGNet, Warning, TEXT("JoinFoundSession: 판 진행 중 — 참여 불가"));
		OnJoinSessionComplete.Broadcast(false);
		return;
	}

	// 생성과 같은 제약 — 로컬에 세션이 남아 있으면 JoinSession 이 실패한다.
	// 재접속(강제 종료 후 다시 참여)에서 그대로 걸린다.
	const EOnlineSessionState::Type SessionState = Sessions->GetSessionState(NAME_GameSession);
	if (SessionState != EOnlineSessionState::NoSession)
	{
		UE_LOG(LogBGNet, Log, TEXT("JoinFoundSession: 기존 세션(상태=%s) 파기 완료 후 조인 예약"),
			EOnlineSessionState::ToString(SessionState));

		// 생성/조인 예약은 동시에 살아 있으면 안 된다.
		PendingHostRequest = FPendingHostRequest();
		PendingJoinRequest.Result = Result;
		PendingJoinRequest.bValid = true;

		if (SessionState != EOnlineSessionState::Destroying)
		{
			DestroyCurrentSession();
		}
		return;
	}

	StartJoinSession(Result);
}

void UBGGameInstance::StartJoinSession(const FOnlineSessionSearchResult& Result)
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (!Sessions.IsValid())
	{
		UE_LOG(LogBGNet, Warning, TEXT("StartJoinSession: SessionInterface 없음"));
		OnJoinSessionComplete.Broadcast(false);
		return;
	}

	JoinSessionCompleteHandle = Sessions->AddOnJoinSessionCompleteDelegate_Handle(
		FOnJoinSessionCompleteDelegate::CreateUObject(this, &UBGGameInstance::HandleJoinSessionComplete));

	if (!Sessions->JoinSession(0, NAME_GameSession, Result))
	{
		UE_LOG(LogBGNet, Warning, TEXT("StartJoinSession: JoinSession 호출 실패"));
		Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteHandle);
		OnJoinSessionComplete.Broadcast(false);
	}
}

void UBGGameInstance::HandleJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (Sessions.IsValid())
	{
		Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteHandle);
	}

	const bool bOk = (Result == EOnJoinSessionCompleteResult::Success);
	OnJoinSessionComplete.Broadcast(bOk);

	if (!bOk || !Sessions.IsValid())
	{
		UE_LOG(LogBGNet, Warning, TEXT("JoinSession 실패: 결과=%d"), static_cast<int32>(Result));
		return;
	}

	// 세션 → 접속 주소 해석 후 ClientTravel. 참가자가 호스트 월드로 넘어가는 필수 경로다.
	FString ConnectString;
	if (!Sessions->GetResolvedConnectString(NAME_GameSession, ConnectString))
	{
		UE_LOG(LogBGNet, Warning, TEXT("JoinSession: ConnectString 해석 실패"));
		return;
	}

	if (APlayerController* PC = GetFirstLocalPlayerController())
	{
		UE_LOG(LogBGNet, Log, TEXT("JoinSession 접속: %s"), *ConnectString);
		PC->ClientTravel(ConnectString, ETravelType::TRAVEL_Absolute);
	}
	else
	{
		UE_LOG(LogBGNet, Warning, TEXT("JoinSession: 로컬 PlayerController 없음"));
	}
}

// ─────────────────────────────────────────────────────────────
// 파기 / 실패 처리
// ─────────────────────────────────────────────────────────────

void UBGGameInstance::DestroyCurrentSession()
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (!Sessions.IsValid())
	{
		return;
	}

	DestroySessionCompleteHandle = Sessions->AddOnDestroySessionCompleteDelegate_Handle(
		FOnDestroySessionCompleteDelegate::CreateUObject(this, &UBGGameInstance::HandleDestroySessionComplete));

	Sessions->DestroySession(NAME_GameSession);
}

void UBGGameInstance::HandleDestroySessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (IOnlineSessionPtr Sessions = GetSessionInterface())
	{
		Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteHandle);
	}
	UE_LOG(LogBGNet, Log, TEXT("DestroySession 완료: %s (성공=%d)"), *SessionName.ToString(), bWasSuccessful);
	HostRoomCode.Reset();

	// 파기를 기다리던 방 생성 요청을 이제 실행한다.
	if (PendingHostRequest.bValid)
	{
		const FPendingHostRequest Request = PendingHostRequest;
		PendingHostRequest = FPendingHostRequest();
		UE_LOG(LogBGNet, Log, TEXT("세션 파기 완료 — 예약된 방 생성 재개"));
		StartHostSession(Request.MaxPlayers, Request.bLAN, Request.RoomName);
		return;
	}

	// 파기를 기다리던 조인 요청(재접속 경로).
	if (PendingJoinRequest.bValid)
	{
		const FOnlineSessionSearchResult Result = PendingJoinRequest.Result;
		PendingJoinRequest = FPendingJoinRequest();
		UE_LOG(LogBGNet, Log, TEXT("세션 파기 완료 — 예약된 조인 재개"));
		StartJoinSession(Result);
	}
}

void UBGGameInstance::HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString)
{
	// 클라가 튕기면 조인으로 만들어진 로컬 세션이 남아 이후 방 생성을 계속 거부시킨다.
	const bool bIsServer = NetDriver && NetDriver->ServerConnection == nullptr;
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (bIsServer || !Sessions.IsValid() || Sessions->GetSessionState(NAME_GameSession) == EOnlineSessionState::NoSession)
	{
		return;
	}

	UE_LOG(LogBGNet, Warning, TEXT("접속 끊김(%s: %s) — 남은 로컬 세션 파기"),
		ENetworkFailure::ToString(FailureType), *ErrorString);
	DestroyCurrentSession();
}

void UBGGameInstance::SetAllowJoinInProgress(bool bAllow)
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (!Sessions.IsValid())
	{
		return;
	}

	FOnlineSessionSettings* Settings = Sessions->GetSessionSettings(NAME_GameSession);
	if (!Settings)
	{
		return;
	}

	Settings->bAllowJoinInProgress = bAllow;
	Sessions->UpdateSession(NAME_GameSession, *Settings, true);

	UE_LOG(LogBGNet, Log, TEXT("SetAllowJoinInProgress: %s"), bAllow ? TEXT("true") : TEXT("false"));
}

// ─────────────────────────────────────────────────────────────
// 조회
// ─────────────────────────────────────────────────────────────

int32 UBGGameInstance::GetFoundSessionCount() const
{
	return SessionSearch.IsValid() ? SessionSearch->SearchResults.Num() : 0;
}

FString UBGGameInstance::GetFoundSessionCode(int32 SearchResultIndex) const
{
	if (!SessionSearch.IsValid() || !SessionSearch->SearchResults.IsValidIndex(SearchResultIndex))
	{
		return FString();
	}

	FString RoomCode;
	SessionSearch->SearchResults[SearchResultIndex].Session.SessionSettings.Get(BG_ROOMCODE_KEY, RoomCode);
	return RoomCode;
}

TArray<FBGSessionSearchResult> UBGGameInstance::GetFoundSessionResults() const
{
	TArray<FBGSessionSearchResult> Results;
	if (!SessionSearch.IsValid())
	{
		return Results;
	}

	Results.Reserve(SessionSearch->SearchResults.Num());
	for (int32 i = 0; i < SessionSearch->SearchResults.Num(); ++i)
	{
		const FOnlineSessionSearchResult& Raw = SessionSearch->SearchResults[i];
		const FOnlineSessionSettings& Settings = Raw.Session.SessionSettings;

		FBGSessionSearchResult Item;
		// 광고된 방 이름이 없으면 호스트 표시명으로 폴백.
		if (!Settings.Get(BG_ROOMNAME_KEY, Item.RoomName) || Item.RoomName.IsEmpty())
		{
			Item.RoomName = Raw.Session.OwningUserName;
		}
		else
		{
			Item.RoomName = DecodeAdvertisedString(Item.RoomName);
		}
		Item.HostDisplayName = Raw.Session.OwningUserName;
		Item.MaxPlayers = Settings.NumPublicConnections;
		Item.CurrentPlayers = FMath::Clamp(
			Settings.NumPublicConnections - Raw.Session.NumOpenPublicConnections, 0, Settings.NumPublicConnections);
		Item.PingMs = Raw.PingInMs;
		Item.ResultIndex = i;
		// 꽉 찼거나 판이 진행 중(JoinFoundSession 의 거부 규칙과 동일)이면 참가 불가 표시.
		Item.bCanJoin = Raw.Session.NumOpenPublicConnections > 0 && Settings.bAllowJoinInProgress;

		Results.Add(Item);
	}
	return Results;
}
