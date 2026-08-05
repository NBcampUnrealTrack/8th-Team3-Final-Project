// BANG! 좌석 배정 — 서버 권위. 세션 참여 시 빈 자리를 주고, 판 시작 시 다시 랜덤으로 섞는다.
// 좌석은 거리 계산·대상 지정의 기준이라 소유자를 한 곳으로 모은다(게임모드에 로직을 얹지 않는다).

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BGSeatComponent.generated.h"

class ABGGameMode;
class ABGPlayerState;

UCLASS(ClassGroup = (BG), meta = (BlueprintSpawnableComponent))
class BG_API UBGSeatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBGSeatComponent();

	// 현재 월드의 게임모드에 붙은 컴포넌트. 서버가 아니면 nullptr.
	static UBGSeatComponent* Find(const UWorld* World);

	// 서버 전용. 빈 좌석 중 가장 작은 번호를 준다. 이미 배정돼 있으면 그대로 둔다.
	// 자리가 없으면 INDEX_NONE (입장 거절은 UBGMatchStartComponent 소관이다).
	int32 AssignSeatOnJoin(APlayerController* NewPlayer);

	// 서버 전용. 퇴장한 플레이어의 좌석을 반납한다. 빈 번호는 메우지 않는다 —
	// 판 시작 시 RandomizeSeats 가 0..N-1 로 다시 채운다.
	void ReleaseSeat(AController* Exiting);

	// ── 이탈 좌석 예약 (판 진행 중 전용) ──────────────────────────
	//
	// 판 중 이탈은 좌석을 반납하지 않는다. 자리가 사라지면 승패 판정이 그 진영을 전멸로 세고
	// (보안관이 튕기면 즉시 무법자 승), 돌아올 자리도 없어지기 때문이다.

	// 서버 전용. 판 중 이탈이면 좌석을 비움 상태로 예약하고 true.
	// 로비이거나 이미 죽은 사람이면 false — 그때는 평소대로 반납한다.
	bool ReserveSeatOnLeave(AController* Exiting);

	// 자리를 비워 둔 채 나간 사람인가(재접속 허용 판정). ?BGKey= 가 1순위, UniqueNetId 가 2순위 —
	// NULL 서브시스템(LAN)은 접속마다 새 id 를 만들어 id 만으로는 알아볼 수 없다.
	bool HasReservedSeat(const FString& PlayerKey, const FUniqueNetIdRepl& UniqueId) const;

	// 서버 전용. 두고 간 좌석·손패·역할을 돌아온 컨트롤러의 PlayerState 로 옮긴다.
	// 예약이 없으면 false — 호출자는 평소대로 새 좌석을 배정하면 된다.
	bool RestoreSeatOnRejoin(APlayerController* Rejoining, const FString& PlayerKey,
		const FUniqueNetIdRepl& UniqueId);

	// 접속 URL 옵션(?BGKey=)에서 재접속 열쇠를 뽑는다. 없으면 빈 문자열.
	static FString ParsePlayerKey(const FString& Options);

	// 서버 전용. 두고 간 폰이 남아 있으면 다시 물린다. true 면 새 폰을 스폰하지 말 것 —
	// 체력과 장비 GE 가 옛 폰에 들어 있다.
	bool RepossessAbandonedPawn(APlayerController* Rejoining);

	// 서버 전용. 폰을 자기 좌석 자리로 옮긴다. 판 중 복귀처럼 로비 자리에서 태어났을 때 쓴다.
	void SnapToSeat(ABGPlayerState* PS) const { SnapPawnToSeat(PS); }

	// 서버 전용. 예약을 지운다(사망 확정 등으로 더 기다릴 필요가 없어졌을 때).
	void ClearReservation(int32 Seat);

	// 서버 전용. 인원수만큼의 자리를 테이블에 고르게 잡고 그 안에서 사람을 섞는다(판 시작).
	// 배정 결과를 로그로 남긴다 — 재현·검증은 이 줄을 본다.
	void RandomizeSeats();

	// 이 좌석을 이미 누가 쓰고 있는가.
	UFUNCTION(BlueprintPure, Category = "BG|Seat")
	bool IsSeatTaken(int32 Seat) const;

	// 이 컨트롤러가 앉을 PlayerStart. 좌석이 없거나 레벨에 자리 액터가 없으면 nullptr
	// (그 경우 경고를 남긴다 — 호출자는 엔진 기본 지점으로 폴백하면 된다).
	AActor* ChooseStartFor(const AController* Player) const;

	// PlayerStartTag 접두사. 레벨 배치와 반드시 맞춰야 한다.
	static const TCHAR* SeatStartTagPrefix;

private:
	ABGGameMode* GetGameMode() const;

	// 자리를 비워 둔 사람 하나. 판이 끝나거나 그 좌석이 죽으면 사라진다.
	struct FBGSeatReservation
	{
		FString PlayerKey;                             // 접속 URL 의 ?BGKey= (1순위)
		FString NetId;                                 // UniqueNetId 문자열 (2순위)
		int32   Seat = INDEX_NONE;
		TWeakObjectPtr<ABGPlayerState> State;          // 살려 둔 PlayerState
		TWeakObjectPtr<APawn>          Pawn;           // 두고 간 폰(있으면 재빙의)
	};

	TArray<FBGSeatReservation> Reservations;

	// UniqueNetId 문자열. 없는 접속이면 빈 문자열이라 매칭되지 않는다.
	static FString MakeNetIdKey(const FUniqueNetIdRepl& UniqueId);

	const FBGSeatReservation* FindReservation(const FString& PlayerKey, const FString& NetId) const;

	// 접속 중인 BG 플레이어 상태를 모은다.
	void CollectPlayerStates(TArray<ABGPlayerState*>& OutStates) const;

	// 자리를 고르는 순서 — 짝수 자리를 먼저 채우고 홀수로 넘어간다(0,2,4,6,1,3,5).
	// 입장 배정과 판 시작 재배정이 같은 규칙을 쓴다.
	static void BuildSeatOrder(TArray<int32>& OutSeats);

	// 좌석 번호에 대응하는 PlayerStart. PlayerStartTag 가 "Seat0".."Seat6" 인 것을 찾는다.
	AActor* FindSeatStart(int32 Seat) const;

	// 폰을 자기 좌석 자리로 옮긴다(좌석이 바뀐 뒤 호출).
	void SnapPawnToSeat(ABGPlayerState* PS) const;
};
