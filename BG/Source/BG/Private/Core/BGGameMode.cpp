#include "Core/BGGameMode.h"
#include "BGGameplayTags.h"
#include "Core/BGCampfire.h"
#include "Lobby/BGMatchStartComponent.h"
#include "Lobby/BGSeatComponent.h"
#include "Tutorial/BGTutorialComponent.h"
#include "Card/BGDataSubsystem.h"
#include "Core/BGPlayerState.h"
#include "Debug/BGNetLog.h"
#include "Kismet/GameplayStatics.h"
#include "Core/BGCharacter.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Core/BGGameState.h"
#include "AI/BGAIPlayerController.h"
#include "Lobby/BGReadyComponent.h"
#include "Core/BGGameInstance.h"
#include "Core/BGAINameRow.h"
#include "Engine/DataTable.h"

namespace
{
	// Fisher-Yates 셔플 (뒤에서부터 랜덤 교환). 역할 풀과 직업 풀이 같은 방식을 쓴다.
	void ShuffleTags(TArray<FGameplayTag>& Tags)
	{
		for (int32 i = Tags.Num() - 1; i > 0; --i)
		{
			const int32 j = FMath::RandRange(0, i);
			Tags.Swap(i, j);
		}
	}

	// 같은 알고리즘을 AI 이름 풀(역할별 후보)에도 쓴다.
	void ShuffleNames(TArray<FString>& Names)
	{
		for (int32 i = Names.Num() - 1; i > 0; --i)
		{
			const int32 j = FMath::RandRange(0, i);
			Names.Swap(i, j);
		}
	}
}

ABGGameMode::ABGGameMode()
{
	// 좌석/손패 등 플레이어별 데이터의 소유자(md §1.2).
	PlayerStateClass = ABGPlayerState::StaticClass();
	GameStateClass = ABGGameState::StaticClass();
	
	// 판 시작 진행은 컴포넌트가 맡는다.
	MatchStart = CreateDefaultSubobject<UBGMatchStartComponent>(TEXT("MatchStart"));

	// 좌석 배정도 마찬가지 — 게임모드는 훅만 넘긴다.
	Seats = CreateDefaultSubobject<UBGSeatComponent>(TEXT("Seats"));

	// 튜토리얼 단계 진행. 튜토리얼 판이 아니면 BeginPlay 에서 스스로 물러난다.
	Tutorial = CreateDefaultSubobject<UBGTutorialComponent>(TEXT("Tutorial"));
}

UBGTutorialComponent* ABGGameMode::GetTutorial() const
{
	return Tutorial;
}

EBGPlayMode ABGGameMode::GetPlayMode(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const ABGGameMode* GM = World ? World->GetAuthGameMode<ABGGameMode>() : nullptr;
	return GM ? GM->GetPlayMode() : EBGPlayMode::PvP;
}

EBGPlayMode ABGGameMode::ResolvePlayMode(const FString& MapName, const FString& Options) const
{
	// URL 옵션이 최우선 — 같은 맵을 다른 모드로 열 수 있어야 한다(?PlayMode=PvE).
	const FString Requested = UGameplayStatics::ParseOption(Options, TEXT("PlayMode"));
	if (!Requested.IsEmpty())
	{
		const UEnum* Enum = StaticEnum<EBGPlayMode>();
		const int64 Value = Enum ? Enum->GetValueByNameString(Requested) : INDEX_NONE;
		if (Value != INDEX_NONE)
		{
			return static_cast<EBGPlayMode>(Value);
		}
		UE_LOG(LogBGNet, Warning, TEXT("[GameMode] 알 수 없는 PlayMode 옵션 '%s' — 맵 기본값을 쓴다."), *Requested);
	}

	for (const FBGMapPlayMode& Entry : MapPlayModes)
	{
		if (Entry.MapName == MapName)
		{
			return Entry.Mode;
		}
	}
	return EBGPlayMode::PvP;
}

void ABGGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	PlayMode = ResolvePlayMode(MapName, Options);
	bTutorial = PlayMode == EBGPlayMode::PvE && UGameplayStatics::HasOption(Options, TEXT("Tutorial"));
	Super::InitGame(MapName, Options, ErrorMessage);

	UE_LOG(LogBGNet, Log, TEXT("[GameMode] 맵 %s — 모드 %s%s"),
		*MapName, *StaticEnum<EBGPlayMode>()->GetNameStringByValue(static_cast<int64>(PlayMode)),
		bTutorial ? TEXT(" (튜토리얼)") : TEXT(""));
}

void ABGGameMode::PreLogin(const FString& Options, const FString& Address,
	const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
	Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
	if (!ErrorMessage.IsEmpty())
	{
		return;
	}

	// 자리를 비워 둔 본인은 판 중에도 돌아올 수 있다 — 난입이 아니라 복귀다.
	if (Seats && Seats->HasReservedSeat(UBGSeatComponent::ParsePlayerKey(Options), UniqueId))
	{
		UE_LOG(LogBGNet, Log, TEXT("[Seat] 복귀 접속 허용 — %s"), *Address);
		return;
	}

	// 입장 가부 판정은 진행 컴포넌트가 갖는다 — 여기는 엔진 훅을 넘겨주기만 한다.
	if (MatchStart && !MatchStart->CanAcceptPlayer(ErrorMessage))
	{
		UE_LOG(LogBGNet, Log, TEXT("[MatchStart] 접속 거절(%s) — %s"), *Address, *ErrorMessage);
	}
}

FString ABGGameMode::InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId,
	const FString& Options, const FString& Portal)
{
	// Super 안에서 ChoosePlayerStart 가 불려 결과가 캐시되므로 좌석은 그 전에 줘야 한다.
	// 복귀 처리가 좌석 배정보다 먼저다 — 순서를 바꾸면 돌아온 사람이 빈 자리를 새로 받는다.
	const FString PlayerKey = UBGSeatComponent::ParsePlayerKey(Options);

	if (ABGPlayerState* PS = NewPlayerController ? NewPlayerController->GetPlayerState<ABGPlayerState>() : nullptr)
	{
		PS->SetPlayerKey(PlayerKey);
	}

	if (Seats)
	{
		if (!Seats->RestoreSeatOnRejoin(NewPlayerController, PlayerKey, UniqueId))
		{
			Seats->AssignSeatOnJoin(NewPlayerController);
		}
	}

	return Super::InitNewPlayer(NewPlayerController, UniqueId, Options, Portal);
}

void ABGGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	//	두고 간 폰을 다시 문다 — 새로 스폰하면 만신창이로 나갔던 사람이 만피로 돌아온다.
	if (Seats && Seats->RepossessAbandonedPawn(NewPlayer))
	{
		return;
	}

	Super::HandleStartingNewPlayer_Implementation(NewPlayer);

	//	새로 스폰됐다면 역할과 직업을 되돌려 준다 — 안 하면 돌아온 사람이 역할 없는 무직이 되고,
	//	GetSeatRole 이 폰을 먼저 보므로 승패 판정까지 틀어진다.
	ABGPlayerState* PS = NewPlayer ? NewPlayer->GetPlayerState<ABGPlayerState>() : nullptr;
	ABGCharacter*   Char = NewPlayer ? Cast<ABGCharacter>(NewPlayer->GetPawn()) : nullptr;
	if (!PS || !Char || PS->GetCachedRole().IsValid() == false)
	{
		return;
	}

	//	직업이 먼저다 — 스탯 GE 를 붙이는 쪽이라 역할보다 앞서야 순서가 자연스럽다.
	if (PS->GetCachedJob().IsValid())
	{
		Char->SetJobTag(PS->GetCachedJob());
	}
	Char->SetRoleTag(PS->GetCachedRole());

	UE_LOG(LogBGNet, Log, TEXT("[Seat] 좌석 %d 폰 재생성 — 역할·직업 복원(%s / %s)"),
		PS->GetSeatIndex(), *PS->GetCachedRole().ToString(), *PS->GetCachedJob().ToString());

	//	새 폰은 로비용 PlayerStart 에서 태어난다. 판이 도는 중이면 자기 좌석 자리로 옮긴다.
	if (Seats)
	{
		Seats->SnapToSeat(PS);
	}

	//	직업 GE 로 MaxHealth 가 정해진 다음이라야 체력을 제자리에 맞출 수 있다.
	PS->RestoreVitals();
}

ABGCampfire* ABGGameMode::GetCampfire() const
{
	//	레벨 액터라 생성자에서 만들 수 없다. 한 번 찾으면 캐시하고, 없으면 매번 다시 찾는다 —
	//	레벨 스트리밍으로 나중에 올라올 수 있으므로 "없음" 을 캐시하면 영영 못 찾는다.
	if (!Campfire)
	{
		Campfire = ABGCampfire::Find(GetWorld());
	}
	return Campfire;
}

void ABGGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	// 정원이 차면 컴포넌트가 알아서 판을 연다(md §1.4 — 로비에서 대기하다 전환).
	if (MatchStart)
	{
		MatchStart->NotifyPlayerJoined();
	}

	// 로비에서는 각자 PlayerStart 자리에 그대로 선다. 원형 배치는 판 시작 때 한 번만 —
	// 사람이 들고 날 때마다 전원이 움직이면 자리가 계속 흔들린다.
}

AActor* ABGGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	// 자리 선택도 좌석 컴포넌트가 안다 — 여기는 엔진 훅을 넘겨주기만 한다.
	AActor* Start = Seats ? Seats->ChooseStartFor(Player) : nullptr;
	return Start ? Start : Super::ChoosePlayerStart_Implementation(Player);
}

void ABGGameMode::Logout(AController* Exiting)
{
	//	판이 도는 중이면 좌석을 반납하지 않는다 — 자리를 비워 두고 밧줄이 복귀를 기다린다.
	//	로비에서 나갔거나 이미 죽은 사람이면 예약이 걸리지 않으므로 평소대로 반납한다.
	if (Seats && !Seats->ReserveSeatOnLeave(Exiting))
	{
		Seats->ReleaseSeat(Exiting);
	}

	Super::Logout(Exiting);

	//	판 중 퇴장에는 재배치하지 않는다 — 남은 사람이 다 움직이면 대상 지정을 하던 중에
	//	자리가 바뀐다. 로비에서는 각자 PlayerStart 에 서 있으므로 재배치할 것도 없다.
}

TArray<FGameplayTag> ABGGameMode::BuildRolePool(int32 NumPlayers)
{
	using namespace BG::Role;

	TArray<FGameplayTag> Pool;

	// 공통: 보안관 1, 배신자 1.
	Pool.Add(Sheriff.GetTag());
	Pool.Add(Renegade.GetTag());

	// 무법자: 4~5인 2명 / 6~7인 3명.
	const int32 NumOutlaws = (NumPlayers <= 5) ? 2 : 3;
	for (int32 i = 0; i < NumOutlaws; ++i)
	{
		Pool.Add(Outlaw.GetTag());
	}

	// 부관: 남는 자리를 채운다 → 4인 0 / 5·6인 1 / 7인 2.
	const int32 NumDeputies = NumPlayers - Pool.Num();
	for (int32 i = 0; i < NumDeputies; ++i)
	{
		Pool.Add(Deputy.GetTag());
	}

	return Pool;
}

TArray<FGameplayTag> ABGGameMode::BuildBotRolePoolPvE(int32 NumBots) const
{
	using namespace BG::Role;

	TArray<FGameplayTag> Pool;
	Pool.Reserve(NumBots);

	//	4좌석 미만은 PvP 풀을 못 쓴다(BuildRolePool 이 4~7 전제) - 전원 무법자.
	const int32 NumSeats = NumBots + 1;   // 사람 1명 포함
	if (NumSeats < MinPlayers)
	{
		for (int32 i = 0; i < NumBots; ++i)
		{
			Pool.Add(Outlaw.GetTag());
		}
		return Pool;
	}

	//	4좌석 이상은 PvP 표를 그대로 쓴다 - 부관 비율을 두 벌 관리하지 않는다.
	//	5좌석부터 부관이 섞여 나오는데, 그 전에 둘이 되어 있어야 한다 -
	//	보안관의 부관 사살 벌칙을 PvE 에서 끄고, AI 가 아군을 대상에서 빼야 한다.
	Pool = BuildRolePool(NumSeats);

	//	보안관은 사람 몫이라 뺀다. 남은 개수가 정확히 봇 수가 된다.
	Pool.RemoveSingle(Sheriff.GetTag());

	//	배신자는 PvE 에서 쓰지 않는다 - 무법자로 바꾼다. 튜토리얼은 역할군을 전부 보여 주려고 남긴다.
	if (!IsTutorial())
	{
		for (FGameplayTag& Tag : Pool)
		{
			if (Tag == Renegade.GetTag())
			{
				Tag = Outlaw.GetTag();
			}
		}
	}

	return Pool;
}

TArray<FGameplayTag> ABGGameMode::BuildJobPool()
{
	using namespace BG::Character::Job;

	// 12종 전부를 후보로 넣는다. 최대 인원이 7 이라 셔플 후 앞에서 잘라 쓰면 중복이 없다.
	//
	// ⚠️ 여기가 직업 태그를 열거하는 **유일한** 곳이다. 직업을 추가하고 이 목록에 넣지 않으면
	//    DT 에 Row 가 있어도 아무도 그 직업으로 배정되지 않는다 — 조용히 빠진다.
	return {
		Knight.GetTag(),
		Mercenary.GetTag(),
		Hunter.GetTag(),
		Scout.GetTag(),
		Priest.GetTag(),
		Healer.GetTag(),
		Mage.GetTag(),
		Diviner.GetTag(),
		Monk.GetTag(),
		Brawler.GetTag(),
		Thief.GetTag(),
		Assassin.GetTag()
	};
}

TArray<FGameplayTag> ABGGameMode::BuildAllowedEnemyJobPool(int32 EncounterIndex) const
{
	using namespace BG::Character::Job;

	// 순서나 경계는 플레이테스트로 바뀔 수 있다.
	static const TArray<TArray<FGameplayTag>> TierUnlocks =
	{
		{ Knight.GetTag(),     Scout.GetTag()    },   // 조우1
		{ Priest.GetTag(),     Healer.GetTag()   },   // 조우2
		{ Diviner.GetTag(),    Thief.GetTag()    },   // 조우3
		{ Hunter.GetTag(),     Mage.GetTag(),
		  Monk.GetTag(),       Brawler.GetTag()  },   // 조우4
		{ Mercenary.GetTag(),  Assassin.GetTag() },   // 조우5(보스)
	};

	TArray<FGameplayTag> Pool;
	//	EncounterIndex 는 1-base(조우1부터)지만 TierUnlocks 는 배열이라 0-base다 - 여기서만 보정한다.
	const int32 Tier = FMath::Clamp(EncounterIndex - 1, 0, TierUnlocks.Num() - 1);
	for (int32 i = 0; i <= Tier; ++i)
	{
		Pool.Append(TierUnlocks[i]);
	}
	return Pool;
}

void ABGGameMode::AssignJobs()
{
	if (!HasAuthority())
	{
		return;
	}

	TArray<APlayerController*> Players;
	CollectPlayers(Players);

	const int32 Num = Players.Num();
	if (Num < MinPlayers || Num > MaxPlayers)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[BG] AssignJobs: 접속 인원 %d 명은 지원 범위(%d~%d) 밖 — 배정 취소."),
			Num, MinPlayers, MaxPlayers);
		return;
	}

	TArray<FGameplayTag> Pool = BuildJobPool();
	if (Pool.Num() < Num)
	{
		// 직업 종류보다 사람이 많으면 중복 없이 나눠줄 수 없다 — 잘못 배정하느니 멈춘다.
		UE_LOG(LogTemp, Error, TEXT("[BG] AssignJobs: 직업 %d종 < 인원 %d명 — 중복 없는 배정 불가."),
			Pool.Num(), Num);
		return;
	}

	ShuffleTags(Pool);

	// 셔플된 앞쪽 Num 개를 한 명씩 가져간다 → 같은 직업이 두 명 나올 수 없다.
	for (int32 i = 0; i < Num; ++i)
	{
		APlayerController* PC = Players[i];

		ABGCharacter* Char = PC ? Cast<ABGCharacter>(PC->GetPawn()) : nullptr;
		if (!Char)
		{
			UE_LOG(LogTemp, Warning, TEXT("[BG] AssignJobs: %s 의 캐릭터 없음 — 배정 스킵."),
				PC ? *PC->GetName() : TEXT("null"));
			continue;
		}

		const FGameplayTag JobTag = Pool[i];

		// 태그 부여 + DT Row 의 스켈레탈 메쉬 교체까지 캐릭터가 한 번에 처리한다.
		Char->SetJobTag(JobTag);

		// 재접속으로 폰을 새로 스폰하게 되면 이 값으로 직업을 다시 세운다.
		if (ABGPlayerState* PS = PC->GetPlayerState<ABGPlayerState>())
		{
			PS->SetCachedJob(JobTag);
		}

		UE_LOG(LogTemp, Log, TEXT("[BG] AssignJobs: %s → %s"),
			*PC->GetName(), *JobTag.ToString());
	}
}

void ABGGameMode::CollectPlayers(TArray<APlayerController*>& OutPlayers) const
{
	OutPlayers.Reset();

	if (const UWorld* World = GetWorld())
	{
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			if (APlayerController* PC = It->Get())
			{
				OutPlayers.Add(PC);
			}
		}
	}
}

int32 ABGGameMode::SpawnAIPlayers(int32 Count)
{
	if (!HasAuthority() || Count <= 0)
	{
		return 0;
	}

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		UE_LOG(LogBGNet, Error, TEXT("[PvE] SpawnAIPlayers: World 가 없습니다."));
		return 0;
	}

	if (!AIControllerClass)
	{
		UE_LOG(LogBGNet, Error, TEXT("[PvE] SpawnAIPlayers: AIControllerClass 가 비었습니다 — "
								"BP_BGGameMode 의 BG|PvE 항목에 봇 컨트롤러 BP 를 꽂으세요."));
		return 0;
	}

	int32 Spawned = 0;
	for (int32 i = 0; i < Count; ++i)
	{
		FActorSpawnParameters Params;
		Params.ObjectFlags |= RF_Transient;   // 판마다 새로 만든다

		ABGAIPlayerController* Bot = World->SpawnActor<ABGAIPlayerController>(
			AIControllerClass, FVector::ZeroVector, FRotator::ZeroRotator, Params);
		if (!IsValid(Bot))
		{
			UE_LOG(LogBGNet, Error, TEXT("[PvE] 봇 스폰 실패 (%d/%d)."), i + 1, Count);
			break;
		}

		//	PlayerState 없는 봇을 남기면 좌석·역할 배정이 인원수를 잘못 센다.
		ABGPlayerState* PS = Bot->GetPlayerState<ABGPlayerState>();
		if (!IsValid(PS))
		{
			UE_LOG(LogBGNet, Error, TEXT("[PvE] 봇 %s 에 PlayerState 가 없습니다 — 스폰을 되돌립니다."),*Bot->GetName());
			Bot->Destroy();
			break;
		}

		//	번호 이름이 기본이다. 이름 표가 있으면 역할이 정해진 뒤 AssignAINames 가 덮어쓴다.
		PS->SetPlayerName(FString::Printf(TEXT("AI %d"), Spawned + 1));
		PS->SetIsABot(true);

		//	켜지 않으면 CanStartMatch 가 "준비 대기" 로 판을 열지 않는다.
		if (UBGReadyComponent* Ready = UBGReadyComponent::Find(PS))
		{
			Ready->SetReadyAuthoritative(true);
		}

		//	좌석이 먼저다 - RestartPlayer 안의 ChoosePlayerStart 가 그 좌석으로 자리를 고른다.
		if (Seats)
		{
			Seats->AssignSeatOnJoin(Bot);
		}

		RestartPlayer(Bot);

		++Spawned;
		UE_LOG(LogBGNet, Log, TEXT("[PvE] 봇 스폰 — %s → 좌석 %d"), *PS->GetPlayerName(), PS->GetSeatIndex());
	}

	return Spawned;
}

void ABGGameMode::AssignRoles()
{
	if (!HasAuthority())
	{
		return;
	}

	TArray<APlayerController*> Players;
	CollectPlayers(Players);

	const int32 Num = Players.Num();
	if (Num < MinPlayers || Num > MaxPlayers)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[BG] AssignRoles: 접속 인원 %d 명은 지원 범위(%d~%d) 밖 — 배정 취소."),
			Num, MinPlayers, MaxPlayers);
		return;
	}

	TArray<FGameplayTag> Pool = BuildRolePool(Num);
	if (Pool.Num() != Num)
	{
		// 인원수와 풀 크기가 어긋나면(로직 오류) 잘못된 배정을 막는다.
		UE_LOG(LogTemp, Error, TEXT("[BG] AssignRoles: 역할 풀(%d)과 인원(%d) 불일치."),
			Pool.Num(), Num);
		return;
	}

	// 직업을 먼저 배정한다 — 스탯(Health 등)이 직업에서 나오고, 판이 열릴 때
	// StartMatch 가 그 Health 를 보고 초기 손패를 나눠주기 때문이다.
	// SetRoleTag 가 보안관 고정 보너스를 얹긴 하지만 가산(Additive) GE라 순서 무관하다.
	AssignJobs();

	ShuffleTags(Pool);

	// 셔플된 풀에서 한 명씩 태그를 빼서 배정.
	// SetRoleTag 는 태그를 ASC 루즈 태그로 올리고, 보안관이면 고정 보너스도 함께 적용한다.
	for (int32 i = 0; i < Num; ++i)
	{
		APlayerController* PC = Players[i];

		// 좌석은 UBGSeatComponent 가 소유한다 — 여기서 건드리면 랜덤 배정을 덮어쓴다.

		ABGCharacter* Char = PC ? Cast<ABGCharacter>(PC->GetPawn()) : nullptr;
		if (!Char)
		{
			// 아직 폰이 스폰되지 않은 플레이어 — 스킵 (로그로 남겨 추적).
			UE_LOG(LogTemp, Warning, TEXT("[BG] AssignRoles: %s 의 캐릭터 없음 — 배정 스킵."),
				PC ? *PC->GetName() : TEXT("null"));
			continue;
		}

		const FGameplayTag RoleTag = Pool[i];
		Char->SetRoleTag(RoleTag);

		// 보안관만 공개 역할이다. 나머지는 사망하거나 판이 끝날 때까지 감춘다.
		// (RoleTag 자체는 COND_OwnerOnly 라 본인 외에는 복제되지 않는다)
		if (ABGPlayerState* PS = PC->GetPlayerState<ABGPlayerState>())
		{
			// 접속이 끊겨 폰이 사라져도 역할은 남아야 한다 — 승패 판정이 이 값을 본다.
			// (서버 전용 사본이라 은닉 정보가 새지 않는다)
			PS->SetCachedRole(RoleTag);

			if (RoleTag == BG::Role::Sheriff.GetTag())
			{
				PS->SetPublicRole(RoleTag);
			}
		}

		UE_LOG(LogTemp, Log, TEXT("[BG] AssignRoles: %s → %s"),
			*PC->GetName(), *RoleTag.ToString());
	}
}

void ABGGameMode::AssignRolesPvE()
{
	if (!HasAuthority())
	{
		return;
	}

	TArray<APlayerController*> Players;
	CollectPlayers(Players);

	int32 NumBots = 0;
	for (const APlayerController* PC : Players)
	{
		if (PC && PC->IsA<ABGAIPlayerController>())
		{
			++NumBots;
		}
	}

	TArray<FGameplayTag> BotPool = BuildBotRolePoolPvE(NumBots);
	ShuffleTags(BotPool);
	int32 NextBot = 0;

	// 역할을 직업보다 먼저 적용한다 - 뒤에서 그 결과를 읽는다(AssignJobsPvE 참고).
	for (APlayerController* PC : Players)
	{
		ABGCharacter* Char = PC ? Cast<ABGCharacter>(PC->GetPawn()) : nullptr;
		if (!Char)
		{
			UE_LOG(LogBGNet, Warning, TEXT("[PvE] 역할 배정: %s 의 캐릭터 없음 - 스킵."),
				PC ? *PC->GetName() : TEXT("null"));
			continue;
		}

		FGameplayTag RoleTag = BG::Role::Sheriff.GetTag();
		if (PC->IsA<ABGAIPlayerController>())
		{
			//	BotPool 은 봇 수(NumBots)만큼 만들어졌어야 한다. 모자라다는 건
			//	BuildBotRolePoolPvE 가 인원수를 잘못 셌다는 뜻이다.
			if (!ensureMsgf(BotPool.IsValidIndex(NextBot),
				TEXT("[PvE] 봇 역할 풀 부족. %s 배정 실패."), *PC->GetName()))
			{
				continue;
			}
			RoleTag = BotPool[NextBot++];
		}

		Char->SetRoleTag(RoleTag);

		if (ABGPlayerState* PS = PC->GetPlayerState<ABGPlayerState>())
		{
			//	접속이 끊겨 폰이 사라져도 역할은 남아야 한다 - 승패 판정이 이 값을 본다.
			PS->SetCachedRole(RoleTag);

			//	PvE 는 전원 공개다. 역할 은닉은 사람이 상대일 때만 재미가 된다.
			PS->SetPublicRole(RoleTag);
		}

		UE_LOG(LogBGNet, Log, TEXT("[PvE] 역할 배정: %s → %s"),
			*PC->GetName(), *RoleTag.ToString());
	}

	//	이름은 역할이 정해진 뒤에야 붙일 수 있다 - 역할별 이름 표에서 뽑는다.
	AssignAINames(Players);

	AssignJobsPvE(Players);
}

void ABGGameMode::AssignAINames(const TArray<APlayerController*>& Players)
{
	if (!HasAuthority() || !IsValid(AINameTable))
	{
		return;
	}

	//	역할별 이름 후보를 모은다. 이름이나 역할이 비어 있는 행은 건너뛴다.
	TMap<FGameplayTag, TArray<FString>> NamesByRole;
	AINameTable->ForeachRow<FBGAINameRow>(TEXT("AssignAINames"),
		[&NamesByRole](const FName& /*Key*/, const FBGAINameRow& Row)
		{
			if (Row.Role.IsValid() && !Row.DisplayName.IsEmpty())
			{
				NamesByRole.FindOrAdd(Row.Role).Add(Row.DisplayName.ToString());
			}
		});

	//	역할별로 후보를 섞은 뒤 앞에서부터 하나씩 쓴다 - 판마다 무작위 순서이면서도
	//	같은 판에서는 같은 이름이 두 번 나오지 않는다(소비 후 재사용 안 함).
	for (TPair<FGameplayTag, TArray<FString>>& Pair : NamesByRole)
	{
		ShuffleNames(Pair.Value);
	}

	//	부관(Role.Deputy)만 런 전체에서 이름을 고정한다 - 조우가 바뀔 때마다 ServerTravel 로
	//	PlayerState 가 통째로 새로 생기지만, GameInstance 는 살아있어 거기 적어둔 이름을
	//	그대로 재사용할 수 있다. 무법자 등 나머지 역할은 위에서 섞은 대로 조우마다 새로 뽑는다.
	UBGGameInstance* GI = Cast<UBGGameInstance>(GetGameInstance());
	const FGameplayTag DeputyRole = BG::Role::Deputy.GetTag();
	int32 DeputyOrdinal = 0;   // 한 판에 부관이 둘일 수 있다(7인 풀) - 뽑힌 순서로 각자 이름을 고정한다.

	TMap<FGameplayTag, int32> NextIndexByRole;
	for (const APlayerController* PC : Players)
	{
		if (!IsValid(PC) || !PC->IsA<ABGAIPlayerController>())
		{
			continue;
		}

		ABGPlayerState* PS = PC->GetPlayerState<ABGPlayerState>();
		if (!IsValid(PS))
		{
			continue;
		}

		const FGameplayTag BotRole = PS->GetCachedRole();

		//	이 부관 순번의 이름이 이미 고정돼 있으면 표를 다시 뽑지 않고 그대로 재사용한다.
		if (BotRole == DeputyRole && GI)
		{
			const int32 ThisDeputy = DeputyOrdinal++;
			const FString Persisted = GI->GetPersistedDeputyName(ThisDeputy);
			if (!Persisted.IsEmpty())
			{
				PS->SetPlayerName(Persisted);
				continue;
			}

			//	처음 등장한 부관 - 표에서 하나 뽑아 이번 런 동안 쓸 이름으로 고정해둔다.
			const TArray<FString>* Names = NamesByRole.Find(BotRole);
			int32& NextIndex = NextIndexByRole.FindOrAdd(BotRole);
			if (Names && Names->IsValidIndex(NextIndex))
			{
				const FString& NewName = (*Names)[NextIndex++];
				PS->SetPlayerName(NewName);
				GI->SetPersistedDeputyName(ThisDeputy, NewName);
			}
			continue;
		}

		//	이 역할의 이름이 없거나 다 썼으면 SpawnAIPlayers 가 붙인 번호 이름이 그대로 남는다.
		const TArray<FString>* Names = NamesByRole.Find(BotRole);
		int32& NextIndex = NextIndexByRole.FindOrAdd(BotRole);
		if (Names && Names->IsValidIndex(NextIndex))
		{
			PS->SetPlayerName((*Names)[NextIndex]);
			++NextIndex;
		}
	}
}

void ABGGameMode::AssignJobsFromPool(const TArray<APlayerController*>& Players,
	const TArray<FGameplayTag>& Pool, TArray<FGameplayTag>& OutUsedTags)
{
	for (int32 i = 0; i < Players.Num(); ++i)
	{
		APlayerController* PC = Players[i];
		ABGCharacter* Char = PC ? Cast<ABGCharacter>(PC->GetPawn()) : nullptr;
		if (!Char)
		{
			UE_LOG(LogBGNet, Warning, TEXT("[PvE] 직업 배정: %s 의 캐릭터 없음 - 스킵."),
				PC ? *PC->GetName() : TEXT("null"));
			continue;
		}

		const FGameplayTag JobTag = Pool[i];
		OutUsedTags.Add(JobTag);

		Char->SetJobTag(JobTag);

		//	재접속으로 폰을 새로 스폰하면 이 값으로 직업을 다시 세운다.
		if (ABGPlayerState* PS = PC->GetPlayerState<ABGPlayerState>())
		{
			PS->SetCachedJob(JobTag);
		}

		UE_LOG(LogBGNet, Log, TEXT("[PvE] 직업 배정: %s → %s"), *PC->GetName(), *JobTag.ToString());
	}
}

void ABGGameMode::AssignJobsPvE(const TArray<APlayerController*>& Players)
{
	// 화이트리스트는 진영으로 구분. 부관 봇(용사 진영)은 아군이라 대상이 아니다.
	// 역할이 이미 적용된 뒤라 PS 의 캐시된 역할로 진영을 본다.
	TArray<APlayerController*> SheriffSide;
	TArray<APlayerController*> OutlawSide;

	for (APlayerController* PC : Players)
	{
		if (!PC)
		{
			continue;
		}

		const ABGPlayerState* PS = PC->GetPlayerState<ABGPlayerState>();
		const bool bIsOutlaw = PS && PS->GetCachedRole() == BG::Role::Outlaw.GetTag();
		(bIsOutlaw ? OutlawSide : SheriffSide).Add(PC);
	}

	TArray<FGameplayTag> UsedTags;

	// ── 용사 진영: 전체 풀에서 제한 없이 뽑는다 ──
	TArray<FGameplayTag> SheriffPool = BuildJobPool();
	ShuffleTags(SheriffPool);
	AssignJobsFromPool(SheriffSide, SheriffPool, UsedTags);

	if (OutlawSide.Num() == 0)
	{
		return;
	}

	// ── 무법자 진영(적): 조우별 화이트리스트에서만 뽑는다 ──
	int32 EncounterIndex = 1;
	if (const UBGGameInstance* GI = Cast<UBGGameInstance>(GetGameInstance()))
	{
		EncounterIndex = GI->GetPvEEncounterIndex();
	}

	//	튜토리얼의 무법자는 둘이다. 조우1 풀(2종)로는 보안관 쪽이 뽑은 직업과 겹쳐 모자랄 수 있다.
	if (IsTutorial())
	{
		EncounterIndex = TutorialEnemyJobEncounter;
	}

	TArray<FGameplayTag> OutlawPool = BuildAllowedEnemyJobPool(EncounterIndex);
	OutlawPool.RemoveAll([&UsedTags](const FGameplayTag& Tag) { return UsedTags.Contains(Tag); });

	if (OutlawPool.Num() < OutlawSide.Num())
	{
		UE_LOG(LogBGNet, Error,
			TEXT("[PvE] 조우 %d 허용 직업 %d종 < 적 인원 %d명 - 중복 없는 배정 불가."),
			EncounterIndex, OutlawPool.Num(), OutlawSide.Num());
		return;
	}
	ShuffleTags(OutlawPool);
	AssignJobsFromPool(OutlawSide, OutlawPool, UsedTags);

	// 보스 조우면 무법자 중 한 명을 보스로 지정한다.
	if (EncounterIndex == BossEncounterIndex && OutlawSide.Num() > 0)
	{
		APlayerController* BossPC = OutlawSide[FMath::RandRange(0, OutlawSide.Num() - 1)];
		ABGCharacter* BossChar = BossPC ? Cast<ABGCharacter>(BossPC->GetPawn()) : nullptr;
		if (BossChar)
		{
			BossChar->MarkAsBoss(BossStatBoostGE);
			UE_LOG(LogBGNet, Warning, TEXT("[PvE] 조우 %d 보스 지정 - %s"),
				EncounterIndex, *BossPC->GetName());
		}
	}
}

// ======================================================================================
//  [현재 미사용] 확률 기반 카드 분배
//
//  실제 분배 경로는 ABGGameState::DrawFromDeck + ABGPlayerState::AddCardToHand 다.
//  아래는 복원추출(가중치) 방식이라 InstanceId 가 없고 같은 카드가 무한히 나올 수 있어
//  덱 기반 시스템과 섞이면 안 된다. 참고용으로만 남긴다 — 호출처 없음.
// ======================================================================================
FGameplayTag ABGGameMode::DrawWeightedCard() const
{
	const UGameInstance* GI = GetGameInstance();
	const UBGDataSubsystem* DS = GI ? GI->GetSubsystem<UBGDataSubsystem>() : nullptr;
	if (!DS)
	{
		return FGameplayTag();
	}

	TArray<FBGCardProbabilityRow> Rows;
	DS->GetAllCardProbabilities(Rows);
	if (Rows.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BG] DrawWeightedCard: 카드 확률 데이터 없음(DT 비었거나 미지정)."));
		return FGameplayTag();
	}

	// 가중치 합 (음수 방지).
	float Total = 0.f;
	for (const FBGCardProbabilityRow& Row : Rows)
	{
		Total += FMath::Max(0.f, Row.Weight);
	}
	if (Total <= 0.f)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BG] DrawWeightedCard: 가중치 합이 0 — 뽑을 카드 없음."));
		return FGameplayTag();
	}

	// [0, Total) 난수를 누적 가중치 구간에 대응시켜 선택.
	const float Pick = FMath::FRandRange(0.f, Total);
	float Cumulative = 0.f;
	for (const FBGCardProbabilityRow& Row : Rows)
	{
		Cumulative += FMath::Max(0.f, Row.Weight);
		if (Pick < Cumulative)
		{
			return Row.CardId;
		}
	}

	return Rows.Last().CardId; // 부동소수 오차 보정 — 마지막 카드로.
}

void ABGGameMode::DealCards(int32 CardsPerPlayer)
{
	if (!HasAuthority())
	{
		return;
	}

	TArray<APlayerController*> Players;
	CollectPlayers(Players);

	for (APlayerController* PC : Players)
	{
		TArray<FString> Dealt;
		Dealt.Reserve(CardsPerPlayer);

		for (int32 i = 0; i < CardsPerPlayer; ++i)
		{
			const FGameplayTag Card = DrawWeightedCard();
			Dealt.Add(Card.IsValid() ? Card.ToString() : TEXT("(없음)"));

			// TODO(손패 시스템): 여기서 뽑은 Card 를 이 플레이어의 PlayerState.Hand 에 추가.
		}

		UE_LOG(LogTemp, Log, TEXT("[BG] 카드 분배 — %s: %s"),
			PC ? *PC->GetName() : TEXT("null"), *FString::Join(Dealt, TEXT(", ")));
	}
}
