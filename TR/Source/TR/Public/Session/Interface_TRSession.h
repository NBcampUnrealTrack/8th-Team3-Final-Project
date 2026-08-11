// TR 세션 규약 — UI·게임플레이는 세션 구현을 모르고 이 인터페이스에만 의존한다.
// 획득: Cast<IInterface_TRSession>(GetGameInstance()->GetSubsystem<UTRSessionFlow>())

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Session/TRSessionTypes.h"
#include "Interface_TRSession.generated.h"

UINTERFACE(MinimalAPI)
class UInterface_TRSession : public UInterface
{
	GENERATED_BODY()
};

class TR_API IInterface_TRSession
{
	GENERATED_BODY()

public:
	// ── 의도(intent) ──

	// 방 생성. RoomName 이 비면 호스트 닉네임 기반 기본값을 광고한다.
	virtual void HostCreateRoom(const FString& RoomName) = 0;

	// 방 코드 6자로 접속(검색 → 코드 매칭 → 조인).
	virtual void JoinRoomByCode(const FString& RoomCode) = 0;

	// 방 목록 갱신. 완료는 GetSessionListReadyEvent() 로 통지된다.
	virtual void RefreshSessionList(const FTRSessionSearchFilter& Filter) = 0;

	// 직전 검색 결과의 ResultIndex 세션에 조인.
	virtual void JoinSessionByIndex(int32 ResultIndex) = 0;

	// 세션 파기(호스트/클라 공통).
	virtual void LeaveSession() = 0;

	// 스팀 오버레이의 친구 초대 창을 연다. 스팀이 아니면 아무 일도 없다.
	virtual void OpenInviteOverlay() = 0;

	// ── 조회 ──

	virtual bool IsHost() const = 0;

	virtual FTRLobbyStatus GetLobbyStatus() const = 0;

	// 현재 방 코드. 호스트가 아니면 빈 문자열.
	virtual FString GetRoomCode() const = 0;

	// ── 통지 ──

	virtual FOnTRSessionPhaseChanged& GetSessionPhaseChangedEvent() = 0;
	virtual FOnTRSessionListReady& GetSessionListReadyEvent() = 0;
};
