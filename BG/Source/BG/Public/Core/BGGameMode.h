// BANG! 게임모드 — 서버 권위. 접속 인원수(4~7)에 맞춰 역할·직업 태그를 셔플 배정한다.
// 배정은 캐릭터의 SetRoleTag / SetJobTag 로 전달되고, 캐릭터가 직업 DT(FBGCharacterRow)를
// 읽어 스탯/능력/외형을 스스로 부여한다. (Team4Project 의 GODGameMode::AssignRoles 대응)
//
// 축이 둘이다 — 역할(Role.*, 숨김)과 직업(Character.Job.*, 공개). 한 판에서 한 명이 둘 다 받는다.
// 역할은 인원수에 맞춰 짠 풀에서, 직업은 8종 중 중복 없이 뽑는다.
//   역할 = 승리 조건 판정에만 쓴다. DT Row 도, 스탯도 없다.
//   직업 = 스탯/능력/외형의 유일한 출처. DT_BGCharacterRow 의 Row 를 읽는다.
//
// 판 시작 진행(트래블 도착 판정 등)은 UBGMatchStartComponent 가 맡는다 —
// 이 클래스는 "누가 무슨 역할인가"만 안다.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameplayTagContainer.h"
#include "Card/BGGameDataTypes.h"   // FBGCardProbabilityRow
#include "BGGameMode.generated.h"

class ABGCampfire;
class UBGMatchStartComponent;
class UBGSeatComponent;

// 판의 진행 모드. PvP/PvE 는 규칙이 같고 빈자리를 누가 채우느냐만 다르므로
// 게임모드 클래스를 나누지 않고 이 값으로 가른다. (메인메뉴는 ABGMainMenuGameMode 담당)
UENUM(BlueprintType)
enum class EBGPlayMode : uint8
{
	// 사람끼리 붙는 기본 모드.
	PvP,
	// AI 상대. 빈자리를 AI 가 채운다(AI 자체는 미구현).
	PvE
};

// 맵 → 모드 기본값. DefaultGame.ini 에 한 줄 넣으면 레벨이 늘어도 클래스는 그대로다.
//   +MapPlayModes=(MapName="L_PvELobby",Mode=PvE)
USTRUCT()
struct FBGMapPlayMode
{
	GENERATED_BODY()

	// 맵 이름(경로 없이). 로드된 맵 이름과 정확히 비교한다.
	UPROPERTY(Config, EditAnywhere, Category = "BG")
	FString MapName;

	UPROPERTY(Config, EditAnywhere, Category = "BG")
	EBGPlayMode Mode = EBGPlayMode::PvP;
};

UCLASS(Config = Game)
class BG_API ABGGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ABGGameMode();

	// 이번 판의 모드. InitGame 에서 한 번 정해지고 그 뒤로 바뀌지 않는다.
	UFUNCTION(BlueprintPure, Category = "BG")
	EBGPlayMode GetPlayMode() const { return PlayMode; }

	// 빈자리를 AI 가 채우는 모드인가. 규칙 분기는 전부 이 한 줄을 통한다.
	UFUNCTION(BlueprintPure, Category = "BG")
	bool IsPvE() const { return PlayMode == EBGPlayMode::PvE; }

	// 서버 게임모드를 찾아 모드를 읽는다. 게임모드가 없는 클라에서는 PvP 로 본다.
	static EBGPlayMode GetPlayMode(const UObject* WorldContext);

	// 지원 인원 범위 (BANG! 기본판).
	static constexpr int32 MinPlayers = 4;
	static constexpr int32 MaxPlayers = 7;

	// 서버: 접속 인원수에 맞춰 역할 태그를 셔플해 한 명씩 배정한다.
	// 인원이 4~7 밖이면 경고 로그만 남기고 아무것도 하지 않는다.
	// (호출 시점은 UBGMatchStartComponent 가 정한다)
	UFUNCTION(BlueprintCallable, Category = "BG")
	void AssignRoles();

	// 인원수에 맞는 역할 태그 풀을 구성 (셔플 전). 4:S/O/O/R, 5:+D, 6:+O, 7:+D.
	static TArray<FGameplayTag> BuildRolePool(int32 NumPlayers);

	/**
	 * 서버: 접속 인원에게 판타지 직업(Character.Job.*)을 **중복 없이** 하나씩 배정한다.
	 * 직업 Row 가 스탯 GE/패시브/어빌리티를 부여하고, SkeletalMesh 가 있으면 외형도 바뀐다.
	 *
	 * ⚠️ AssignRoles 가 시작할 때 자기가 먼저 호출한다 — 보통은 따로 부를 일이 없다.
	 *    직업이 이 게임의 유일한 스탯 출처라, 판이 열리기 전에 반드시 끝나 있어야 한다.
	 */
	UFUNCTION(BlueprintCallable, Category = "BG")
	void AssignJobs();

	// 직업 태그 8종 전부 (셔플 전). 최대 인원 7 < 8 이라 중복 배정이 나올 수 없다.
	static TArray<FGameplayTag> BuildJobPool();

	// 접속한 PlayerController 를 모은다.
	void CollectPlayers(TArray<APlayerController*>& OutPlayers) const;

	// ── 주사위 판정 ──────────────────────────────────────────────
	//  ⚠️ 이사 갔다 → UBGDiceComponent (ABGGameState 에 부착).
	//     GameMode 는 클라에 없어 판정 설정을 클라 UI 가 읽을 수 없고, 시드 난수의 소유자도
	//     GameState 라 그쪽으로 옮겼다. 호출은 UBGDiceComponent::Get(this) 로 한다.

	// ── [현재 미사용] 확률 기반 카드 분배 ────────────────────────
	//
	// ⚠️ 이 두 함수는 현재 게임 흐름에서 호출되지 않는다. 실제 카드 분배는
	//    ABGGameState::DrawFromDeck + ABGPlayerState::AddCardToHand 경로를 쓴다.
	//
	//    두 방식은 근본적으로 다르다:
	//      DrawWeightedCard : DT_BGCardProbability 가중치 기반 **복원추출**.
	//                         같은 카드가 무한히 나올 수 있고 InstanceId 개념이 없다.
	//      DrawFromDeck     : 80 장 유한 덱의 **비복원추출**. 카드마다 고유 InstanceId 가 있어
	//                         손패에서 특정 카드를 지목해 제거할 수 있다.
	//
	//    섞어 쓰면 "덱에 없는 카드가 손에 들어오거나 InstanceId 가 중복되는" 버그가 난다.
	//    참고용으로 남겨두었을 뿐이므로 새 코드에서 호출하지 말 것.
	UFUNCTION(BlueprintCallable, Category = "BG|Deck|Deprecated", meta = (DeprecatedFunction,
		DeprecationMessage = "미사용. 덱 기반 분배는 ABGGameState::DrawFromDeck 을 사용하세요."))
	FGameplayTag DrawWeightedCard() const;

	UFUNCTION(BlueprintCallable, Category = "BG|Deck|Deprecated", meta = (DeprecatedFunction,
		DeprecationMessage = "미사용. 손패 분배는 BG_DealHand / DrawFromDeck + AddCardToHand 를 사용하세요."))
	void DealCards(int32 CardsPerPlayer = 5);

	// ── 콘솔 테스트 트리거 (구현: BGCardExec.cpp) ──────────────
	UFUNCTION(Exec)
	void BG_DumpDeck();

	UFUNCTION(Exec)
	void BG_PrintDeck();

	// 접속한 전원에게 덱에서 Count 장씩 나눠준다 (서버 권위). 생략하면 5 장.
	UFUNCTION(Exec)
	void BG_DealHand(int32 Count);

	// 좌석에 피해를 준다(사망/승패 테스트용). Amount 생략 시 즉사시킨다.
	UFUNCTION(Exec)
	void BG_Damage(int32 Seat, int32 Amount);

	/** 레벨의 캠프파이어(좌석 "위치" 의 기준점). 없는 레벨이면 nullptr. */
	UFUNCTION(BlueprintPure, Category = "BG|Seating")
	ABGCampfire* GetCampfire() const;

	/**
	 * 캠프파이어 둘레에 접속한 폰을 다시 세운다 (구현: BGSeatExec.cpp).
	 *
	 * 평소에는 PostLogin/Logout 이 알아서 부르지만, 테스트 중에는 접속을 흉내내지 않고
	 * 지금 있는 폰만 재배치하고 싶을 때가 있다 — 반지름을 바꿔 보는 경우가 대표적이다.
	 *
	 * Radius 를 주면 캠프파이어의 SeatRadius 를 그 값으로 바꾼 뒤 배치한다.
	 * 생략하면(0) 현재 반지름을 그대로 쓴다 — exec 는 생략된 인자를 0 으로 넘긴다.
	 */
	UFUNCTION(Exec)
	void BG_ArrangeSeats(float Radius);

protected:
	// 모드는 여기서 확정한다 — 첫 로그인보다 먼저라 좌석·역할 배정이 모드를 볼 수 있다.
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;

	// 판이 시작된 뒤의 접속은 여기서 막는다. 클라의 검색 결과는 캐시라
	// 시작 직전에 검색한 클라이언트는 bAllowJoinInProgress 검사를 통과해버린다.
	virtual void PreLogin(const FString& Options, const FString& Address,
		const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;

	// 좌석은 여기서 준다 — 엔진이 이 안에서 시작 지점을 골라 컨트롤러에 캐시한다.
	virtual FString InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId,
		const FString& Options, const FString& Portal = FString()) override;

	virtual void PostLogin(APlayerController* NewPlayer) override;

	// 퇴장한 플레이어의 좌석을 반납한다.
	virtual void Logout(AController* Exiting) override;

	// 좌석 번호에 대응하는 PlayerStart 로 스폰시킨다. 없으면 엔진 기본 동작.
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

private:
	// URL 옵션(?PlayMode=PvE) > MapPlayModes > PvP 순으로 정한다.
	EBGPlayMode ResolvePlayMode(const FString& MapName, const FString& Options) const;

	EBGPlayMode PlayMode = EBGPlayMode::PvP;

	UPROPERTY(Config)
	TArray<FBGMapPlayMode> MapPlayModes;

	UPROPERTY(VisibleAnywhere, Category = "BG")
	TObjectPtr<UBGMatchStartComponent> MatchStart;

	// 좌석 배정 전담. 입장 시 빈 자리, 판 시작 시 랜덤 재배정.
	UPROPERTY(VisibleAnywhere, Category = "BG")
	TObjectPtr<UBGSeatComponent> Seats;

	/**
	 * 레벨의 캠프파이어. 처음 찾을 때 캐시한다.
	 *
	 * ⚠️ 컴포넌트가 아니라 **레벨에 배치된 액터**다. 위 둘과 달리 생성자에서 만들지 않는다 —
	 *    없는 레벨도 있고(PlayerStart 방식 레벨), 레벨 스트리밍으로 나중에 올 수도 있어
	 *    BeginPlay 에 캐시하지 않고 요청 시점에 찾는다. 그래서 mutable 이다.
	 */
	UPROPERTY(Transient)
	mutable TObjectPtr<class ABGCampfire> Campfire;
};
