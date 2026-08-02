// BANG! 세션 규약 — UI·게임플레이는 세션 구현을 모르고 이 인터페이스에만 의존한다.
// 획득: Cast<IBGSessionInterface>(GetGameInstance()->GetSubsystem<UBGSessionFlow>())

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Session/BGSessionTypes.h"
#include "BGSessionInterface.generated.h"

UINTERFACE(MinimalAPI)
class UBGSessionInterface : public UInterface
{
	GENERATED_BODY()
};

class BG_API IBGSessionInterface
{
	GENERATED_BODY()

public:
	// ── 의도(intent) ──
	// 방 생성. RoomName 이 비면 호스트 닉네임 기반 기본값을 광고한다.
	virtual void HostCreateRoom(const FString& RoomName) = 0;

	// 방 코드 6자로 접속(검색 → 코드 매칭 → 조인).
	virtual void JoinRoomByCode(const FString& RoomCode) = 0;

	// 방 목록 갱신. 완료는 GetSessionListReadyEvent() 로 통지된다.
	virtual void RefreshSessionList(const FBGSessionSearchFilter& Filter) = 0;

	// 직전 검색 결과의 ResultIndex 세션에 조인.
	virtual void JoinSessionByIndex(int32 ResultIndex) = 0;

	// 세션 파기(호스트/클라 공통).
	virtual void LeaveSession() = 0;

	// 로컬 플레이어의 준비 상태를 바꾼다. 호스트는 준비 없이 시작할 수 있다.
	virtual void SetLocalReady(bool bReady) = 0;

	// 호스트 전용 수동 시작. 맵 이동 없이 제자리에서 판을 연다.
	// 최소 인원이 모이고 전원이 준비해야 통과한다.
	virtual void StartGame() = 0;

	// ── 조회 ──
	// 이 인스턴스가 서버 권위인가.
	virtual bool IsHost() const = 0;

	// 로컬 플레이어가 준비 상태인가.
	virtual bool IsLocalReady() const = 0;

	// 지금 호스트가 시작할 수 있는가. 사유는 OutReason 으로 돌려준다.
	virtual bool CanStartGame(FString& OutReason) const = 0;

	// 로비 인원/준비 현황.
	virtual FBGLobbyStatus GetLobbyStatus() const = 0;

	// 현재 방 코드. 호스트가 아니면 빈 문자열.
	virtual FString GetRoomCode() const = 0;

	// ── 통지 ──
	// 단계 변화(Creating/Hosting/Searching/Joining/Joined/Failed). UI 가 여기에 바인딩한다.
	virtual FOnBGSessionPhaseChanged& GetSessionPhaseChangedEvent() = 0;

	// 방 목록 갱신 완료(필터 적용 결과, 실패 시 빈 배열).
	virtual FOnBGSessionListReady& GetSessionListReadyEvent() = 0;
};
