#include "Core/TRGameInstance.h"

#include "Session/TRNetLog.h"
#include "Session/TROssPolicy.h"
#include "Session/TRSessionFlow.h"
#include "UI/Session/TRSessionMenuController.h"

#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Interfaces/OnlineExternalUIInterface.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Misc/Base64.h"
#include "Online/OnlineSessionNames.h"   // SETTING_MAPNAME / SEARCH_LOBBIES 등 표준 키
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"

DEFINE_LOG_CATEGORY(LogTRNet);

namespace
{
	// TR 세션 식별 키 — 검색 시 우리 게임 세션만 필터링.
	const FName TR_SESSION_KEY = TEXT("TRGameName");
	const FString TR_SESSION_VALUE = TEXT("TR");

	// 방 코드 광고 키 — 클라이언트가 입력한 코드와 매칭.
	const FName TR_ROOMCODE_KEY = TEXT("TRRoomCode");

	// 방 이름 광고 키 — 방찾기 목록 표시·부분 문자열 검색용.
	const FName TR_ROOMNAME_KEY = TEXT("TRRoomName");

	// 광고 문자열은 Base64(ASCII)로 왕복 — Steam 로비 데이터는 ANSI 로 읽혀 한글이 깨진다.
	const FString TR_B64_PREFIX = TEXT("b64:");

	FString EncodeAdvertisedString(const FString& In)
	{
		const FTCHARToUTF8 Utf8(*In);
		TArray<uint8> Bytes(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
		return TR_B64_PREFIX + FBase64::Encode(Bytes);
	}

	FString DecodeAdvertisedString(const FString& In)
	{
		if (!In.StartsWith(TR_B64_PREFIX))
		{
			return In;
		}

		TArray<uint8> Bytes;
		if (!FBase64::Decode(In.Mid(TR_B64_PREFIX.Len()), Bytes))
		{
			return In;
		}

		Bytes.Add(0);   // 널 종단
		return FString(UTF8_TO_TCHAR(reinterpret_cast<const char*>(Bytes.GetData())));
	}
}

const TCHAR* UTRGameInstance::PlayerKeyOption = TEXT("TRKey");

void UTRGameInstance::Init()
{
	Super::Init();

	// 접속이 끊기면 남은 로컬 세션을 치워야 다음 방 생성이 거부되지 않는다.
	if (GEngine)
	{
		NetworkFailureHandle = GEngine->OnNetworkFailure().AddUObject(
			this, &UTRGameInstance::HandleNetworkFailure);
	}

	// 초대 구독은 월드가 생긴 뒤에 건다 — Init 시점의 GetWorld() 는 신뢰할 수 없고,
	// PIE 는 인스턴스마다 별도 서브시스템을 갖는다.
	PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(
		this, &UTRGameInstance::HandleFirstWorldReady);
}

void UTRGameInstance::HandleFirstWorldReady(UWorld* LoadedWorld)
{
	if (InviteAcceptedHandle.IsValid())
	{
		return;   // 한 번만 구독한다.
	}

	IOnlineSubsystem* OSS = Online::GetSubsystem(LoadedWorld);
	IOnlineSessionPtr Sessions = OSS ? OSS->GetSessionInterface() : nullptr;
	if (!Sessions.IsValid())
	{
		UE_LOG(LogTRNet, Warning, TEXT("초대 수락 구독 실패 — SessionInterface 없음"));
		return;
	}

	// 이 구독이 없으면 오버레이의 "게임 참가" 가 조용히 무시된다.
	InviteAcceptedHandle = Sessions->AddOnSessionUserInviteAcceptedDelegate_Handle(
		FOnSessionUserInviteAcceptedDelegate::CreateUObject(
			this, &UTRGameInstance::HandleSessionUserInviteAccepted));
}

void UTRGameInstance::Shutdown()
{
	if (GEngine && NetworkFailureHandle.IsValid())
	{
		GEngine->OnNetworkFailure().Remove(NetworkFailureHandle);
		NetworkFailureHandle.Reset();
	}

	if (PostLoadMapHandle.IsValid())
	{
		FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
		PostLoadMapHandle.Reset();
	}

	if (IOnlineSessionPtr Sessions = GetSessionInterface())
	{
		if (InviteAcceptedHandle.IsValid())
		{
			Sessions->ClearOnSessionUserInviteAcceptedDelegate_Handle(InviteAcceptedHandle);
			InviteAcceptedHandle.Reset();
		}
	}

	Super::Shutdown();
}

IOnlineSessionPtr UTRGameInstance::GetSessionInterface() const
{
	// Online::GetSubsystem 을 쓴다 — PIE 는 인스턴스마다 별도 서브시스템을 갖는다.
	IOnlineSubsystem* OSS = Online::GetSubsystem(GetWorld());
	return OSS ? OSS->GetSessionInterface() : nullptr;
}

FString UTRGameInstance::GenerateRoomCode()
{
	// 혼동 문자(0/O/1/I)를 뺀 32자 알파벳. 6자면 약 10억 가지다.
	static const TCHAR* Alphabet = TEXT("ABCDEFGHJKLMNPQRSTUVWXYZ23456789");
	static const int32  AlphabetLen = FCString::Strlen(Alphabet);

	FString Code;
	Code.Reserve(6);
	for (int32 i = 0; i < 6; ++i)
	{
		Code.AppendChar(Alphabet[FMath::RandRange(0, AlphabetLen - 1)]);
	}
	return Code;
}

FString UTRGameInstance::GetPlayerKey() const
{
	// 검증용 오버라이드가 최우선 — 한 PC 의 여러 클라이언트는 설정 파일을 공유해
	// 열쇠가 같아지고, 그러면 서로의 자리를 가져간다.
	FString Override;
	if (FParse::Value(FCommandLine::Get(), TEXT("TRKey="), Override) && !Override.IsEmpty())
	{
		return Override;
	}

	if (PlayerKey.IsEmpty())
	{
		PlayerKey = FGuid::NewGuid().ToString(EGuidFormats::Digits);
		const_cast<UTRGameInstance*>(this)->SaveConfig();
		UE_LOG(LogTRNet, Log, TEXT("PlayerKey 생성: %s"), *PlayerKey);
	}
	return PlayerKey;
}

// ─────────────────────────────────────────────────────────────
// 방 생성
// ─────────────────────────────────────────────────────────────

void UTRGameInstance::HostSession(int32 MaxPlayers, bool bLAN, const FString& RoomName)
{
	// 온라인 플러그인이 켜져 있으면 항상 있어야 한다 — 없으면 프로젝트 설정이 잘못된 것이다.
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (!ensureMsgf(Sessions.IsValid(), TEXT("SessionInterface 없음 — 온라인 플러그인 설정을 확인하세요")))
	{
		OnCreateSessionComplete.Broadcast(false);
		return;
	}

	// 남은 세션이 있으면 파기 완료 후 생성 — 즉시 CreateSession 하면 "세션 이미 존재" 로 조용히 실패한다.
	const EOnlineSessionState::Type SessionState = Sessions->GetSessionState(NAME_GameSession);
	if (SessionState != EOnlineSessionState::NoSession)
	{
		UE_LOG(LogTRNet, Log, TEXT("HostSession: 기존 세션(상태=%s) 파기 완료 후 생성 예약"),
			EOnlineSessionState::ToString(SessionState));

		// 생성/조인 예약은 동시에 살아 있으면 안 된다.
		PendingJoinRequest = FPendingJoinRequest();
		PendingHostRequest.RoomName   = RoomName;
		PendingHostRequest.MaxPlayers = MaxPlayers;
		PendingHostRequest.bLAN       = bLAN;
		PendingHostRequest.bValid     = true;

		if (SessionState != EOnlineSessionState::Destroying)
		{
			DestroyCurrentSession();
		}
		return;
	}

	StartHostSession(MaxPlayers, bLAN, RoomName);
}

void UTRGameInstance::StartHostSession(int32 MaxPlayers, bool bLAN, const FString& RoomName)
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (!ensure(Sessions.IsValid()))
	{
		OnCreateSessionComplete.Broadcast(false);
		return;
	}

	HostRoomCode = GenerateRoomCode();

	FString ResolvedRoomName = RoomName;
	if (ResolvedRoomName.IsEmpty())
	{
		IOnlineSubsystem* OSS = Online::GetSubsystem(GetWorld());
		IOnlineIdentityPtr Identity = OSS ? OSS->GetIdentityInterface() : nullptr;
		const FString Nickname = Identity.IsValid() ? Identity->GetPlayerNickname(0) : FString();

		ResolvedRoomName = Nickname.IsEmpty() ? TR_SESSION_VALUE : FString::Printf(TEXT("%s의 방"), *Nickname);
	}

	const int32 Slots = FMath::Max(1, MaxPlayers);

	// Steam 이 안 떠 있으면(NULL 폴백) 호출부가 뭘 넘겼든 LAN 매치여야 서로를 찾는다.
	const FTROssPolicy Policy = FTROssPolicy::Resolve(GetWorld());
	const bool bLanMatch = bLAN || Policy.bLanOnly;

	UE_LOG(LogTRNet, Log, TEXT("CreateSession 요청: 방코드=%s 방이름='%s' 정원=%d LAN=%d [%s]"),
		*HostRoomCode, *ResolvedRoomName, Slots, bLanMatch, *Policy.ToString());

	const UWorld* World = GetWorld();
	const FString MapName = World ? World->GetMapName() : FString();

	FOnlineSessionSettings Settings;
	Settings.bIsLANMatch = bLanMatch;
	Settings.NumPublicConnections = Slots;
	Settings.NumPrivateConnections = 0;
	Settings.bShouldAdvertise = true;          // 검색 목록에 노출
	Settings.bAllowJoinInProgress = true;      // 판이 시작되면 SetAllowJoinInProgress(false)
	Settings.bAllowJoinViaPresence = Policy.bUsePresence && !bLanMatch;
	Settings.bUsesPresence = Policy.bUsePresence && !bLanMatch;
	Settings.bUseLobbiesIfAvailable = Policy.bUseLobbies && !bLanMatch;
	Settings.bAllowInvites = true;             // 친구 초대(오버레이)

	Settings.Set(TR_SESSION_KEY, TR_SESSION_VALUE, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	Settings.Set(SETTING_MAPNAME, MapName, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	Settings.Set(TR_ROOMCODE_KEY, HostRoomCode, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	Settings.Set(TR_ROOMNAME_KEY, EncodeAdvertisedString(ResolvedRoomName), EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);

	CreateSessionCompleteHandle = Sessions->AddOnCreateSessionCompleteDelegate_Handle(
		FOnCreateSessionCompleteDelegate::CreateUObject(this, &UTRGameInstance::HandleCreateSessionComplete));

	if (!Sessions->CreateSession(0, NAME_GameSession, Settings))
	{
		UE_LOG(LogTRNet, Warning, TEXT("CreateSession 호출 실패 (세션상태=%s) — 직전 LogOnlineSession 경고 확인"),
			EOnlineSessionState::ToString(Sessions->GetSessionState(NAME_GameSession)));

		Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteHandle);
		HostRoomCode.Reset();
		OnCreateSessionComplete.Broadcast(false);
	}
}

void UTRGameInstance::HandleCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (IOnlineSessionPtr Sessions = GetSessionInterface())
	{
		Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteHandle);
	}

	UE_LOG(LogTRNet, Log, TEXT("CreateSession 완료: %s (성공=%d)"), *SessionName.ToString(), bWasSuccessful);

	if (!bWasSuccessful)
	{
		HostRoomCode.Reset();
		OnCreateSessionComplete.Broadcast(false);
		return;
	}

	OnCreateSessionComplete.Broadcast(StartListen());
}

// ─────────────────────────────────────────────────────────────
// 리슨 / 맵 이동
// ─────────────────────────────────────────────────────────────

bool UTRGameInstance::StartListen()
{
	UWorld* World = GetWorld();
	if (!ensure(World))
	{
		return false;
	}

	// 메인메뉴에서 호스트하면 게임 맵으로 한 번 이동해야 한다. 이미 게임 맵이면 그대로 연다.
	// PIE 에서는 패키지 이름에 UEDPIE_N_ 이 붙으므로 없애야 같은 맵으로 인식된다.
	const FString CurrentMap = UWorld::RemovePIEPrefix(World->GetOutermost()->GetName());
	if (CurrentMap == GameMapPath)
	{
		return StartListenInPlace();
	}

	UE_LOG(LogTRNet, Log, TEXT("StartListen: 게임 맵으로 이동 (%s → %s)"), *CurrentMap, *GameMapPath);
	World->ServerTravel(GameMapPath + TEXT("?listen"), /*bAbsolute=*/true);
	return true;
}

bool UTRGameInstance::StartListenInPlace()
{
	UWorld* World = GetWorld();
	if (!ensure(World))
	{
		return false;
	}

	const ENetMode NetMode = World->GetNetMode();
	if (NetMode == NM_ListenServer || NetMode == NM_DedicatedServer)
	{
		UE_LOG(LogTRNet, Log, TEXT("StartListenInPlace: 이미 리슨 중 — 유지"));
		return true;
	}
	if (NetMode == NM_Client)
	{
		UE_LOG(LogTRNet, Warning, TEXT("StartListenInPlace: 클라이언트로 접속 중 — 리슨 불가"));
		return false;
	}

	// 맵 재로드 없이 넷드라이버만 연다(엔진의 URL ?listen 처리와 같은 경로).
	FURL ListenURL = World->URL;
	ListenURL.AddOption(TEXT("Listen"));

	if (!World->Listen(ListenURL))
	{
		UE_LOG(LogTRNet, Error, TEXT("StartListenInPlace: Listen 실패 — 직전 LogNet 오류 확인"));
		return false;
	}

	UE_LOG(LogTRNet, Log, TEXT("StartListenInPlace: 리슨 시작 (맵=%s, 이동 없음)"), *World->GetMapName());
	return true;
}

void UTRGameInstance::ReturnToMainMenu()
{
	UWorld* World = GetWorld();
	if (!ensure(World))
	{
		return;
	}

	// 이미 메인메뉴다. 정상 상태이므로 조용히 빠져나간다.
	const FString CurrentMap = World->GetOutermost()->GetName();
	if (CurrentMap == MainMenuMapPath)
	{
		return;
	}

	APlayerController* PC = GetFirstLocalPlayerController();
	if (!ensureMsgf(PC, TEXT("로컬 PlayerController 없이 메인메뉴 복귀를 시도했다")))
	{
		return;
	}

	// 호스트도 ClientTravel 로 자기 자신만 빠진다 — 남은 접속자는 넷드라이버가 끊어 준다.
	UE_LOG(LogTRNet, Log, TEXT("ReturnToMainMenu: 메인메뉴로 이동 (%s → %s)"), *CurrentMap, *MainMenuMapPath);
	PC->ClientTravel(MainMenuMapPath, ETravelType::TRAVEL_Absolute);
}

// ─────────────────────────────────────────────────────────────
// 검색 / 조인
// ─────────────────────────────────────────────────────────────

void UTRGameInstance::FindSessions(bool bLAN, int32 MaxSearchResults)
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (!ensure(Sessions.IsValid()))
	{
		OnFindSessionsComplete.Broadcast(false, 0);
		return;
	}

	const FTROssPolicy Policy = FTROssPolicy::Resolve(GetWorld());
	const bool bLanQuery = bLAN || Policy.bLanOnly;

	SessionSearch = MakeShared<FOnlineSessionSearch>();
	SessionSearch->bIsLanQuery = bLanQuery;
	SessionSearch->MaxSearchResults = FMath::Max(1, MaxSearchResults);

	// Steam 로비 세션 검색(호스트의 bUseLobbiesIfAvailable 과 짝). LAN 검색에선 비적용.
	if (Policy.bUseLobbies && !bLanQuery)
	{
		SessionSearch->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);
	}
	SessionSearch->QuerySettings.Set(TR_SESSION_KEY, TR_SESSION_VALUE, EOnlineComparisonOp::Equals);

	UE_LOG(LogTRNet, Log, TEXT("FindSessions 요청: LAN=%d 최대=%d [%s]"),
		bLanQuery, SessionSearch->MaxSearchResults, *Policy.ToString());

	FindSessionsCompleteHandle = Sessions->AddOnFindSessionsCompleteDelegate_Handle(
		FOnFindSessionsCompleteDelegate::CreateUObject(this, &UTRGameInstance::HandleFindSessionsComplete));

	if (!Sessions->FindSessions(0, SessionSearch.ToSharedRef()))
	{
		UE_LOG(LogTRNet, Warning, TEXT("FindSessions: 호출 실패"));
		Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteHandle);
		OnFindSessionsComplete.Broadcast(false, 0);
	}
}

void UTRGameInstance::HandleFindSessionsComplete(bool bWasSuccessful)
{
	if (IOnlineSessionPtr Sessions = GetSessionInterface())
	{
		Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteHandle);
	}

	const int32 Num = SessionSearch.IsValid() ? SessionSearch->SearchResults.Num() : 0;
	UE_LOG(LogTRNet, Log, TEXT("FindSessions 완료: 성공=%d, %d개"), bWasSuccessful, Num);

	OnFindSessionsComplete.Broadcast(bWasSuccessful, Num);
}

void UTRGameInstance::JoinFoundSession(int32 SearchResultIndex)
{
	// 인덱스는 직전 검색 결과에서 온 것이어야 한다 — 어긋나면 UI 가 낡은 목록을 들고 있는 것이다.
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (!ensureMsgf(Sessions.IsValid() && SessionSearch.IsValid()
			&& SessionSearch->SearchResults.IsValidIndex(SearchResultIndex),
			TEXT("유효하지 않은 검색 인덱스(%d) — 목록을 다시 갱신해야 한다"), SearchResultIndex))
	{
		OnJoinSessionComplete.Broadcast(false);
		return;
	}

	// 난입이 막혀 있어도 접속은 시도한다. 돌아온 본인일 수 있는데 그 여부는 서버만 안다 —
	// 여기서 막으면 복귀 자체가 불가능해진다. 수락 판단은 GameMode 의 몫이다.
	const FOnlineSessionSearchResult& Result = SessionSearch->SearchResults[SearchResultIndex];
	if (!Result.Session.SessionSettings.bAllowJoinInProgress)
	{
		UE_LOG(LogTRNet, Log, TEXT("JoinFoundSession: 난입 차단 상태 — 복귀 시도(수락 여부는 서버가 정한다)"));
	}

	// 생성과 같은 제약 — 로컬에 세션이 남아 있으면 JoinSession 이 실패한다.
	const EOnlineSessionState::Type SessionState = Sessions->GetSessionState(NAME_GameSession);
	if (SessionState != EOnlineSessionState::NoSession)
	{
		UE_LOG(LogTRNet, Log, TEXT("JoinFoundSession: 기존 세션(상태=%s) 파기 완료 후 조인 예약"),
			EOnlineSessionState::ToString(SessionState));

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

void UTRGameInstance::StartJoinSession(const FOnlineSessionSearchResult& Result)
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (!ensure(Sessions.IsValid()))
	{
		OnJoinSessionComplete.Broadcast(false);
		return;
	}

	JoinSessionCompleteHandle = Sessions->AddOnJoinSessionCompleteDelegate_Handle(
		FOnJoinSessionCompleteDelegate::CreateUObject(this, &UTRGameInstance::HandleJoinSessionComplete));

	if (!Sessions->JoinSession(0, NAME_GameSession, Result))
	{
		UE_LOG(LogTRNet, Warning, TEXT("StartJoinSession: JoinSession 호출 실패"));
		Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteHandle);
		OnJoinSessionComplete.Broadcast(false);
	}
}

void UTRGameInstance::HandleJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
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
		UE_LOG(LogTRNet, Warning, TEXT("JoinSession 실패: 결과=%d"), static_cast<int32>(Result));
		return;
	}

	// 세션 → 접속 주소 해석 후 ClientTravel. 참가자가 호스트 월드로 넘어가는 필수 경로다.
	// 조인에 성공했는데 주소가 안 나오면 세션 상태가 깨진 것이다.
	FString ConnectString;
	if (!ensureMsgf(Sessions->GetResolvedConnectString(NAME_GameSession, ConnectString),
			TEXT("조인은 성공했는데 ConnectString 해석에 실패했다")))
	{
		return;
	}

	// 열쇠를 URL 에 실어 보낸다. 서버는 이 값으로 나갔던 사람을 알아본다.
	const FString TravelURL = FString::Printf(TEXT("%s?%s=%s"),
		*ConnectString, PlayerKeyOption, *GetPlayerKey());

	APlayerController* PC = GetFirstLocalPlayerController();
	if (!ensure(PC))
	{
		return;
	}

	UE_LOG(LogTRNet, Log, TEXT("JoinSession 접속: %s"), *TravelURL);
	PC->ClientTravel(TravelURL, ETravelType::TRAVEL_Absolute);
}

// ─────────────────────────────────────────────────────────────
// 친구 초대
// ─────────────────────────────────────────────────────────────

bool UTRGameInstance::OpenInviteOverlay()
{
	IOnlineSubsystem* OSS = Online::GetSubsystem(GetWorld());
	IOnlineExternalUIPtr ExternalUI = OSS ? OSS->GetExternalUIInterface() : nullptr;
	if (!ExternalUI.IsValid())
	{
		UE_LOG(LogTRNet, Log, TEXT("OpenInviteOverlay: 오버레이 미지원 서브시스템 — 무시"));
		return false;
	}

	// 세션이 있어야 초대가 의미를 갖는다. 오버레이는 현재 세션을 대상으로 연다.
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (!Sessions.IsValid() || Sessions->GetSessionState(NAME_GameSession) == EOnlineSessionState::NoSession)
	{
		UE_LOG(LogTRNet, Warning, TEXT("OpenInviteOverlay: 세션이 없어 초대할 수 없다"));
		return false;
	}

	return ExternalUI->ShowInviteUI(0, NAME_GameSession);
}

void UTRGameInstance::HandleSessionUserInviteAccepted(bool bWasSuccessful, int32 ControllerId,
	FUniqueNetIdPtr UserId, const FOnlineSessionSearchResult& InviteResult)
{
	if (!bWasSuccessful || !InviteResult.IsValid())
	{
		UE_LOG(LogTRNet, Warning, TEXT("초대 수락 실패 — 세션 정보가 유효하지 않다"));
		return;
	}

	UE_LOG(LogTRNet, Log, TEXT("초대 수락 — 조인 시작"));

	// 검색을 거치지 않고 초대에 실린 결과로 바로 붙는다. 남은 세션이 있으면 먼저 치운다.
	IOnlineSessionPtr Sessions = GetSessionInterface();
	const EOnlineSessionState::Type SessionState =
		Sessions.IsValid() ? Sessions->GetSessionState(NAME_GameSession) : EOnlineSessionState::NoSession;

	if (SessionState != EOnlineSessionState::NoSession)
	{
		PendingHostRequest = FPendingHostRequest();
		PendingJoinRequest.Result = InviteResult;
		PendingJoinRequest.bValid = true;

		if (SessionState != EOnlineSessionState::Destroying)
		{
			DestroyCurrentSession();
		}
		return;
	}

	StartJoinSession(InviteResult);
}

// ─────────────────────────────────────────────────────────────
// 파기 / 실패 처리
// ─────────────────────────────────────────────────────────────

void UTRGameInstance::DestroyCurrentSession()
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (!Sessions.IsValid())
	{
		return;
	}

	DestroySessionCompleteHandle = Sessions->AddOnDestroySessionCompleteDelegate_Handle(
		FOnDestroySessionCompleteDelegate::CreateUObject(this, &UTRGameInstance::HandleDestroySessionComplete));

	Sessions->DestroySession(NAME_GameSession);
}

void UTRGameInstance::HandleDestroySessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (IOnlineSessionPtr Sessions = GetSessionInterface())
	{
		Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteHandle);
	}

	UE_LOG(LogTRNet, Log, TEXT("DestroySession 완료: %s (성공=%d)"), *SessionName.ToString(), bWasSuccessful);
	HostRoomCode.Reset();

	// 파기를 기다리던 방 생성 요청을 이제 실행한다.
	if (PendingHostRequest.bValid)
	{
		const FPendingHostRequest Request = PendingHostRequest;
		PendingHostRequest = FPendingHostRequest();

		UE_LOG(LogTRNet, Log, TEXT("세션 파기 완료 — 예약된 방 생성 재개"));
		StartHostSession(Request.MaxPlayers, Request.bLAN, Request.RoomName);
		return;
	}

	// 파기를 기다리던 조인 요청(재접속·초대 경로).
	if (PendingJoinRequest.bValid)
	{
		const FOnlineSessionSearchResult Result = PendingJoinRequest.Result;
		PendingJoinRequest = FPendingJoinRequest();

		UE_LOG(LogTRNet, Log, TEXT("세션 파기 완료 — 예약된 조인 재개"));
		StartJoinSession(Result);
	}
}

void UTRGameInstance::HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver,
	ENetworkFailure::Type FailureType, const FString& ErrorString)
{
	// 서버 쪽 실패는 여기서 처리하지 않는다 — 클라가 각자 돌아간다.
	const bool bIsServer = NetDriver && NetDriver->ServerConnection == nullptr;
	if (bIsServer)
	{
		return;
	}

	UE_LOG(LogTRNet, Warning, TEXT("접속 끊김(%s: %s) — 메인메뉴로 복귀"),
		ENetworkFailure::ToString(FailureType), *ErrorString);

	// 조인으로 만들어진 로컬 세션이 남으면 이후 방 생성이 계속 거부된다.
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (Sessions.IsValid() && Sessions->GetSessionState(NAME_GameSession) != EOnlineSessionState::NoSession)
	{
		DestroyCurrentSession();
	}
}

// ─────────────────────────────────────────────────────────────
// 조회
// ─────────────────────────────────────────────────────────────

//	판 시작 규칙이 정해지면 주석을 푼다. 세션 설정만 갱신하면 되므로 이 구현은 그대로 쓸 수 있다.
//
//	void UTRGameInstance::SetAllowJoinInProgress(bool bAllow)
//	{
//		IOnlineSessionPtr Sessions = GetSessionInterface();
//		if (!Sessions.IsValid())
//		{
//			return;
//		}
//
//		FOnlineSessionSettings* Settings = Sessions->GetSessionSettings(NAME_GameSession);
//		if (!Settings || Settings->bAllowJoinInProgress == bAllow)
//		{
//			return;
//		}
//
//		Settings->bAllowJoinInProgress = bAllow;
//		Sessions->UpdateSession(NAME_GameSession, *Settings, /*bShouldRefreshOnlineData=*/true);
//
//		UE_LOG(LogTRNet, Log, TEXT("난입 허용 = %d"), bAllow);
//	}

void UTRGameInstance::TR_SessionUI()
{
	if (UTRSessionMenuController* Controller = GetSubsystem<UTRSessionMenuController>())
	{
		Controller->Toggle();
	}
}

void UTRGameInstance::TR_JoinCode(const FString& RoomCode)
{
	if (UTRSessionFlow* Flow = GetSubsystem<UTRSessionFlow>())
	{
		Flow->JoinRoomByCode(RoomCode);
	}
}

int32 UTRGameInstance::GetFoundSessionCount() const
{
	return SessionSearch.IsValid() ? SessionSearch->SearchResults.Num() : 0;
}

FString UTRGameInstance::GetFoundSessionCode(int32 SearchResultIndex) const
{
	if (!SessionSearch.IsValid() || !SessionSearch->SearchResults.IsValidIndex(SearchResultIndex))
	{
		return FString();
	}

	FString RoomCode;
	SessionSearch->SearchResults[SearchResultIndex].Session.SessionSettings.Get(TR_ROOMCODE_KEY, RoomCode);
	return RoomCode;
}

TArray<FTRSessionSearchResult> UTRGameInstance::GetFoundSessionResults() const
{
	TArray<FTRSessionSearchResult> Results;
	if (!SessionSearch.IsValid())
	{
		return Results;
	}

	Results.Reserve(SessionSearch->SearchResults.Num());

	for (int32 Index = 0; Index < SessionSearch->SearchResults.Num(); ++Index)
	{
		const FOnlineSessionSearchResult& Raw = SessionSearch->SearchResults[Index];
		const FOnlineSessionSettings& Settings = Raw.Session.SessionSettings;

		FTRSessionSearchResult& Out = Results.AddDefaulted_GetRef();
		Out.ResultIndex = Index;
		Out.PingMs = Raw.PingInMs;
		Out.MaxPlayers = Settings.NumPublicConnections;
		Out.CurrentPlayers = Settings.NumPublicConnections - Raw.Session.NumOpenPublicConnections;
		Out.HostDisplayName = Raw.Session.OwningUserName;

		FString Advertised;
		Settings.Get(TR_ROOMNAME_KEY, Advertised);
		Out.RoomName = Advertised.IsEmpty() ? Out.HostDisplayName : DecodeAdvertisedString(Advertised);

		Out.bCanJoin = Settings.bAllowJoinInProgress && Raw.Session.NumOpenPublicConnections > 0;
	}

	return Results;
}
