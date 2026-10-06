#include "Core/BGGameInstance.h"
#include "Session/BGOssPolicy.h"
#include "Session/BGSessionFlow.h"
#include "UI/Session/BGSessionMenuController.h"
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
		OnCreateSessionComplete.Broadcast(StartListen());
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
		OnCreateSessionComplete.Broadcast(StartListen());
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

	// 게임 맵이면 제자리 리슨, 메인메뉴면 게임 맵으로 한 번 이동한다.
	OnCreateSessionComplete.Broadcast(StartListen());
}

bool UBGGameInstance::StartListen()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogBGNet, Warning, TEXT("StartListen: World 없음"));
		return false;
	}

	const FString CurrentMap = World->GetOutermost()->GetName();
	const FString TargetMap = PickGameMap(CurrentMap);
	if (TargetMap.IsEmpty())
	{
		//	BP 에서 맵을 안 골랐다. 조용히 넘어가면 호스트 버튼이 아무 반응도 없는 것처럼 보인다.
		UE_LOG(LogBGNet, Warning,
			TEXT("StartListen: GameMaps/GameMap 이 비어 있다 — BP_BGGameInstance 의 BG|맵 에서 게임 맵을 고를 것."));
		return false;
	}

	// 메인메뉴에서 호스트하면 게임 맵으로 한 번 이동해야 한다. 이미 게임 맵이면 그대로 연다.
	if (CurrentMap == TargetMap)
	{
		return StartListenInPlace();
	}

	UE_LOG(LogBGNet, Log, TEXT("StartListen: 게임 맵으로 이동 (%s → %s)"), *CurrentMap, *TargetMap);
	World->ServerTravel(TargetMap + TEXT("?listen"), /*bAbsolute*/ true);
	return true;
}

FString UBGGameInstance::PickGameMap(const FString& CurrentMap) const
{
	TArray<FString> Paths;
	for (const FSoftObjectPath& Map : GameMaps)
	{
		const FString Path = ToTravelPath(Map);
		if (Path.IsEmpty())
		{
			continue;
		}
		if (Path == CurrentMap)
		{
			return Path;
		}
		Paths.Add(Path);
	}
	return Paths.IsEmpty() ? ToTravelPath(GameMap) : Paths[FMath::RandRange(0, Paths.Num() - 1)];
}

FString UBGGameInstance::ToTravelPath(const FSoftObjectPath& Map)
{
	//	"/Game/GameSystem/TestMap.TestMap" → "/Game/GameSystem/TestMap".
	//	비어 있으면 빈 문자열이 나온다 — 부르는 쪽이 그것으로 "안 골랐다" 를 가른다.
	return Map.IsValid() ? Map.GetLongPackageName() : FString();
}

void UBGGameInstance::ReturnToMainMenu()
{
	//	연출 2배속 선택은 메인메뉴로 나가면 끈다. 로비로 돌아갈 때는 유지한다.
	bPresentationDoubleSpeed = false;

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogBGNet, Warning, TEXT("ReturnToMainMenu: World 없음"));
		return;
	}

	const FString TargetMap = ToTravelPath(MainMenuMap);
	if (TargetMap.IsEmpty())
	{
		UE_LOG(LogBGNet, Warning,
			TEXT("ReturnToMainMenu: MainMenuMap 이 비어 있다 — BP_BGGameInstance 의 BG|맵 에서 고를 것."));
		return;
	}

	const FString CurrentMap = World->GetOutermost()->GetName();
	if (CurrentMap == TargetMap)
	{
		return;
	}

	APlayerController* PC = GetFirstLocalPlayerController();
	if (!PC)
	{
		UE_LOG(LogBGNet, Warning, TEXT("ReturnToMainMenu: 로컬 PlayerController 없음"));
		return;
	}

	// 호스트도 ClientTravel 로 자기 자신만 빠진다 — 남은 접속자는 넷드라이버가 끊어 준다.
	UE_LOG(LogBGNet, Log, TEXT("ReturnToMainMenu: 메인메뉴로 이동 (%s → %s)"), *CurrentMap, *TargetMap);
	PC->ClientTravel(TargetMap, ETravelType::TRAVEL_Absolute);
}

const TCHAR* UBGGameInstance::PlayerKeyOption = TEXT("BGKey");

FString UBGGameInstance::GetPlayerKey() const
{
	//	검증용 오버라이드가 최우선 — 한 PC 의 여러 클라이언트는 설정 파일을 공유해
	//	열쇠가 같아지고, 그러면 서로의 자리를 가져간다.
	FString Override;
	if (FParse::Value(FCommandLine::Get(), TEXT("BGKey="), Override) && !Override.IsEmpty())
	{
		return Override;
	}

	if (PlayerKey.IsEmpty())
	{
		PlayerKey = FGuid::NewGuid().ToString(EGuidFormats::Digits);
		const_cast<UBGGameInstance*>(this)->SaveConfig();
		UE_LOG(LogBGNet, Log, TEXT("PlayerKey 생성: %s"), *PlayerKey);
	}
	return PlayerKey;
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

	// 판이 진행 중이어도 접속은 시도한다. 자리를 비워 둔 본인일 수 있는데, 그 여부는
	// 예약을 들고 있는 서버만 안다 — 여기서 막으면 복귀 자체가 불가능해진다.
	// 난입 차단은 ABGGameMode::PreLogin 이 한다.
	const FOnlineSessionSearchResult& Result = SessionSearch->SearchResults[SearchResultIndex];
	if (!Result.Session.SessionSettings.bAllowJoinInProgress)
	{
		UE_LOG(LogBGNet, Log, TEXT("JoinFoundSession: 판 진행 중 — 복귀 시도(수락 여부는 서버가 정한다)"));
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

	//	열쇠를 URL 에 실어 보낸다. 서버는 이 값으로 판 중에 나갔던 사람을 알아보고 자리를 돌려준다.
	const FString TravelURL = FString::Printf(TEXT("%s?%s=%s"),
		*ConnectString, PlayerKeyOption, *GetPlayerKey());

	if (APlayerController* PC = GetFirstLocalPlayerController())
	{
		UE_LOG(LogBGNet, Log, TEXT("JoinSession 접속: %s"), *TravelURL);
		PC->ClientTravel(TravelURL, ETravelType::TRAVEL_Absolute);
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
	const bool bIsServer = NetDriver && NetDriver->ServerConnection == nullptr;
	if (bIsServer)
	{
		return;
	}

	UE_LOG(LogBGNet, Warning, TEXT("접속 끊김(%s: %s) — 메인메뉴로 복귀"),
		ENetworkFailure::ToString(FailureType), *ErrorString);

	// 조인으로 만들어진 로컬 세션이 남으면 이후 방 생성이 계속 거부된다.
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (Sessions.IsValid() && Sessions->GetSessionState(NAME_GameSession) != EOnlineSessionState::NoSession)
	{
		DestroyCurrentSession();
	}

	// 맵 복귀는 엔진이 GameDefaultMap(메인메뉴)으로 브라우즈해 준다. 다만 세션 메뉴는
	// 열린 상태로 살아남아 메인메뉴 위에 다시 뜨므로 여기서 닫는다.
	if (UBGSessionMenuController* Menu = GetSubsystem<UBGSessionMenuController>())
	{
		Menu->Close();
	}
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
// PvE (단독 플레이)
// ─────────────────────────────────────────────────────────────

bool UBGGameInstance::StartPvE()
{
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		UE_LOG(LogBGNet, Error, TEXT("StartPvE: World 가 없습니다."));
		return false;
	}

	const FString TargetMap = PickGameMap(World->GetOutermost()->GetName());
	if (TargetMap.IsEmpty())
	{
		UE_LOG(LogBGNet, Warning,
			TEXT("StartPvE: GameMaps/GameMap 이 비어 있다 — BP_BGGameInstance 의 BG|맵 에서 게임 맵을 고를 것."));
		return false;
	}

	//	세션을 만들지 않는다 - 단독 플레이라 광고할 방이 없다.
	//	?listen 도 붙이지 않는다. 넷드라이버를 열면 난입 경로가 생긴다.
	UE_LOG(LogBGNet, Log, TEXT("StartPvE: PvE 로 게임 맵 이동 (%s)"), *TargetMap);

	World->ServerTravel(TargetMap + TEXT("?PlayMode=PvE"), /*bAbsolute*/ true);
	return true;
}

void UBGGameInstance::AdvancePvEEncounter()
{
	++PvEEncounterIndex;
	UE_LOG(LogBGNet, Log, TEXT("[PvE] 조우 승리. 진행도 %d"), PvEEncounterIndex);
}

void UBGGameInstance::ResetPvERun()
{
	PvEEncounterIndex = 1;
	UE_LOG(LogBGNet, Log, TEXT("[PvE] 런 종료. 진행도 초기화"));
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
