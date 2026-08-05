// BAAM 진단 로그 전용 exec 모음. 상태를 바꾸지 않고 "지금 어떤가" 만 찍는다.
// 동작을 바꾸는 exec(BG_ArrangeSeats / BG_Damage 등)와 섞지 않으려고 파일을 나눠 둔다.
// 관례는 다른 exec 파일과 같다 — 선언은 헤더에 항상, 본문만 Shipping 에서 비운다.

#include "Core/BGGameMode.h"

#include "Core/BGCharacter.h"
#include "Core/BGGameState.h"
#include "Core/BGPlayerState.h"
#include "Debug/BGCardLog.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerStart.h"
#include "Lobby/BGRopeComponent.h"

void ABGGameMode::BG_DumpSeats()
{
#if !UE_BUILD_SHIPPING
	const ABGGameState* GS = GetGameState<ABGGameState>();
	if (!GS)
	{
		UE_LOG(LogBGCard, Warning, TEXT("[Exec] GameState 없음"));
		return;
	}

	//	레벨에 좌석 PlayerStart 가 없으면 스폰이 엔진 기본 지점으로 폴백해 한 점에 겹친다.
	//	겹침을 추적할 때 여기부터 본다.
	TArray<FName> StartTags;
	for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
	{
		StartTags.Add(It->PlayerStartTag);
	}

	//	월드에 실제로 서 있는 캐릭터 수. obj list 는 GC 전 객체까지 세어 실제 액터 수와 다르다.
	int32 LiveChars = 0;
	int32 Ownerless = 0;
	for (TActorIterator<ABGCharacter> It(GetWorld()); It; ++It)
	{
		++LiveChars;
		if (It->GetController() == nullptr)
		{
			++Ownerless;
		}
	}

	UE_LOG(LogBGCard, Warning, TEXT("===== BG_DumpSeats ====="));
	UE_LOG(LogBGCard, Warning, TEXT("  월드 캐릭터 %d개 (컨트롤러 없음 %d개)"), LiveChars, Ownerless);
	UE_LOG(LogBGCard, Warning, TEXT("  레벨 PlayerStart %d개"), StartTags.Num());
	for (const FName& Tag : StartTags)
	{
		UE_LOG(LogBGCard, Warning, TEXT("    태그 : %s"), *Tag.ToString());
	}

	if (const UBGRopeComponent* Rope = GS->GetRope())
	{
		UE_LOG(LogBGCard, Warning, TEXT("  밧줄 : 좌석 %d, 남은 %.1f/%.0f초"),
			Rope->GetRopeSeat(), Rope->GetSecondsRemaining(), Rope->GetRopeLengthSeconds());
	}

	//	같은 좌표에 둘 이상이면 그 자리에 겹쳐 서 있는 것이다. 100cm 안쪽을 겹침으로 본다.
	TArray<FVector> Placed;
	for (APlayerState* Base : GS->PlayerArray)
	{
		const ABGPlayerState* PS = Cast<ABGPlayerState>(Base);
		if (!PS)
		{
			continue;
		}

		const APawn*  Pawn = PS->GetPawn();
		const FVector Loc  = Pawn ? Pawn->GetActorLocation() : FVector::ZeroVector;

		bool bOverlap = false;
		if (Pawn)
		{
			bOverlap = Placed.ContainsByPredicate(
				[&Loc](const FVector& Other) { return FVector::DistSquared2D(Other, Loc) < 100.f * 100.f; });
			Placed.Add(Loc);
		}

		UE_LOG(LogBGCard, Warning,
			TEXT("  좌석 %2d | %-24s | 폰 %-22s | (%.0f, %.0f, %.0f) | 밧줄 %d회%s%s"),
			PS->GetSeatIndex(), *PS->GetPlayerName(), *GetNameSafe(Pawn),
			Loc.X, Loc.Y, Loc.Z, PS->GetRopeStrikes(),
			PS->IsDisconnected() ? TEXT(" [이탈]") : TEXT(""),
			bOverlap ? TEXT(" <<< 겹침") : TEXT(""));
	}
#endif // !UE_BUILD_SHIPPING
}
