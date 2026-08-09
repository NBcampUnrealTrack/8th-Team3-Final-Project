#include "Core/BGCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GAS/BGAbilitySystemComponent.h"
#include "GAS/BGAttributeSet.h"
#include "Debug/BGCardLog.h"
#include "Card/BGDataSubsystem.h"
#include "Card/BGGameDataTypes.h"
#include "BGGameplayTags.h"
#include "Core/BGGameState.h"
#include "Core/BGPlayerState.h"
#include "Engine/World.h"
#include "Engine/SkeletalMesh.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Net/UnrealNetwork.h"

ABGCharacter::ABGCharacter()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	AbilitySystemComponent = CreateDefaultSubobject<UBGAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	// 7인 턴제라 대역폭이 무의미하고, 모든 클라가 동일한 GE 상태를 봐 디버깅이 쉽다 (md §M3).
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Full);

	AttributeSet = CreateDefaultSubobject<UBGAttributeSet>(TEXT("AttributeSet"));

	//	── 카메라 (구 ABGPlayerBase 에서 통합) ──
	//	ACharacter 는 CapsuleComponent 를 루트로 하드코딩한다. 별도의 씬 루트를 둘 수 없으므로
	//	두 카메라는 캡슐 밑에 붙는다 — 캡슐이 돌면 함께 돈다.
	PrimaryCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("PrimaryCamera"));
	PrimaryCamera->SetupAttachment(GetCapsuleComponent());

	SecondaryCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("SecondaryCamera"));
	SecondaryCamera->SetupAttachment(GetCapsuleComponent());

	//	활성 카메라는 항상 하나여야 한다 — 이유는 헤더의 SetActiveCamera 주석 참고.
	//	B 는 연출 샷의 위치 마커일 뿐이라 끝까지 비활성으로 남는다.
	PrimaryCamera->SetAutoActivate(true);
	SecondaryCamera->SetAutoActivate(false);

	//	── 몸 회전을 컨트롤러에서 떼어낸다 ──
	//	기본값(true)이면 APawn::FaceRotation 이 **매 프레임** 컨트롤 회전으로 액터 요를 덮어쓴다.
	//	그러면 좌석 배치가 몸을 캠프파이어 쪽으로 돌려놔도 다음 프레임에 원위치하고,
	//	플레이어가 마우스를 돌릴 때마다 캐릭터가 함께 돌아 원형 대형이 무너진다.
	//	이 게임에서 몸의 방향은 "어느 좌석에 앉았는가" 로 정해진다 — 시선과는 별개다.
	bUseControllerRotationYaw = false;

	//	대신 시점은 컨트롤 회전을 따르게 한다. 위에서 몸을 고정했으므로 이걸 켜지 않으면
	//	카메라가 캡슐에 묶여 마우스로 둘러볼 수 없게 된다.
	PrimaryCamera->bUsePawnControlRotation = true;
}

void ABGCharacter::SetBodyFacing(FRotator NewRotation)
{
	if (!HasAuthority())
	{
		return;
	}

	//	서버 자신과, 이 캐릭터를 시뮬레이트하는 다른 클라에는 액터 회전 복제로 전달된다.
	SetActorRotation(NewRotation);

	//	자기 캐릭터를 조종하는 클라는 이동 권위를 자기가 갖고 있어 위 회전이 닿지 않는다.
	//	리슨서버 호스트 자신은 이미 위에서 처리됐으므로 원격 클라에만 보낸다.
	if (!IsLocallyControlled())
	{
		Client_SetBodyFacing(NewRotation);
	}
}

void ABGCharacter::Client_SetBodyFacing_Implementation(FRotator NewRotation)
{
	SetActorRotation(NewRotation);

	//	시선도 같은 방향에서 시작하게 한다. bUseControllerRotationYaw 가 꺼져 있어
	//	이 값이 몸을 돌리지는 않는다 — 카메라(bUsePawnControlRotation)만 따라간다.
	if (AController* C = GetController())
	{
		C->SetControlRotation(NewRotation);
	}
}

void ABGCharacter::SetActiveCamera(bool bUsePrimary)
{
	if (!PrimaryCamera || !SecondaryCamera)
	{
		return;
	}

	//	끄는 쪽을 먼저 처리한다. 켜는 쪽이 먼저면 두 카메라가 동시에 활성인 순간이 생기고,
	//	그 프레임에 CalcCamera 가 돌면 엉뚱한 시점이 한 프레임 섞여 보인다.
	UCameraComponent* Next     = bUsePrimary ? PrimaryCamera : SecondaryCamera;
	UCameraComponent* Previous = bUsePrimary ? SecondaryCamera : PrimaryCamera;

	Previous->Deactivate();
	Next->Activate();
}

bool ABGCharacter::IsPrimaryCameraActive() const
{
	return PrimaryCamera && PrimaryCamera->IsActive();
}

UAbilitySystemComponent* ABGCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void ABGCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// ⚠️ 역할은 뱅의 핵심 은닉 정보다. 조건 없이 복제하면 모든 클라가 모든 역할을 알게 되어
	//    게임이 성립하지 않는다. 본인에게만 보낸다(폰의 Owner 는 소유 PlayerController).
	//    공개되어야 하는 역할(보안관 / 사망자 / 판 종료)은 ABGPlayerState::PublicRoleTag 로
	//    따로 올린다 — UI 는 반드시 그쪽을 봐야 한다.
	DOREPLIFETIME_CONDITION(ABGCharacter, RoleTag, COND_OwnerOnly);

	// 직업은 역할과 반대로 공개 정보다(GDD §7). 조건 없이 전원에게 보낸다 —
	// 다른 클라도 이 태그를 받아야 그 캐릭터의 메쉬를 직업 메쉬로 바꿀 수 있다.
	DOREPLIFETIME(ABGCharacter, JobTag);
}

void ABGCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// 서버: ActorInfo 초기화 후 GAS 최초 부여.
	InitializeAbilityActorInfo();
	ServerInitGAS();
}

void ABGCharacter::OnRep_Controller()
{
	Super::OnRep_Controller();

	// 소유 클라: ActorInfo 초기화 (없으면 UI 가 어트리뷰트/태그를 못 읽는다).
	InitializeAbilityActorInfo();
}

void ABGCharacter::BeginPlay()
{
	Super::BeginPlay();

	// 늦게 관련성이 생긴 클라(또는 리슨서버 호스트)는 JobTag 가 이미 채워진 채로 시작한다 —
	// 그 경우 OnRep 이 오지 않으므로 여기서 한 번 외형을 맞춘다.
	ApplyJobVisual();

	BindCinematicDelegate();
}

void ABGCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ABGGameState* GS = BoundCinematicGameState.Get())
	{
		GS->OnCinematicStageChanged.RemoveAll(this);
	}
	BoundCinematicGameState.Reset();

	if (UWorld* World = GetWorld())
	{
		World->GameStateSetEvent.RemoveAll(this);
	}

	Super::EndPlay(EndPlayReason);
}

// ══════════════════════════════════════════════════════════════════════════════════════
//  연출 애니메이션 상태
//
//  캐릭터는 아무것도 복제하지 않는다. ABGGameState 의 복제된 연출 상태를 읽어
//  "그래서 나는 지금 뭘 해야 하나" 하나만 유도한다.
// ══════════════════════════════════════════════════════════════════════════════════════

void ABGCharacter::BindCinematicDelegate()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	ABGGameState* GS = World->GetGameState<ABGGameState>();
	if (!GS)
	{
		//	폰이 GameState 보다 먼저 도착했다(클라에서 흔하다). 도착하면 다시 부른다 —
		//	타이머로 폴링하지 않는 이유는 이 이벤트가 정확히 그 순간을 알려주기 때문이다.
		World->GameStateSetEvent.AddWeakLambda(this,
			[this](AGameStateBase*) { BindCinematicDelegate(); });
		return;
	}

	if (BoundCinematicGameState.Get() == GS)
	{
		return;   // 이미 구독 중 — GameStateSetEvent 와 BeginPlay 가 겹쳐 불릴 수 있다.
	}

	if (ABGGameState* Previous = BoundCinematicGameState.Get())
	{
		Previous->OnCinematicStageChanged.RemoveAll(this);
	}

	GS->OnCinematicStageChanged.AddUObject(this, &ABGCharacter::HandleCinematicStageChanged);
	BoundCinematicGameState = GS;

	//	구독보다 연출이 먼저 시작됐을 수 있다(늦게 접속·리스폰). 현재 상태를 즉시 한 번 반영한다 —
	//	연출을 "이벤트가 아니라 상태" 로 둔 것의 값어치가 여기서 나온다.
	RefreshAnimState();
}

void ABGCharacter::HandleCinematicStageChanged(EBGCinematicStage /*Stage*/, ABGCharacter* /*Subject*/)
{
	//	인자를 쓰지 않고 다시 읽는다. 사망(PlayerState 복제)처럼 이 통지 없이 바뀌는 축이 있어
	//	"상태를 만드는 곳" 을 RefreshAnimState 하나로 유지하는 편이 어긋날 여지가 없다.
	RefreshAnimState();
}

EBGCharacterAnimState ABGCharacter::ComputeAnimState(const ABGGameState* GS) const
{
	// ── 내가 이번 연출의 주인공인가 ──
	//	주인공이면 사망 여부보다 **우선한다.** 체력이 0 이 된 순간 PlayerState::bIsDead 는
	//	곧바로 켜지지만(NotifyHealthDepleted), 실제 사망 처리는 연출이 끝난 뒤로 미뤄진다
	//	(FlushPendingDeaths). 여기서 사망을 먼저 보면 나를 죽인 그 연출에서 맞는 동작을
	//	건너뛰고 바로 쓰러져 버린다 — "연출 → 사망" 순서가 그림에서 깨진다.
	if (GS)
	{
		switch (GS->GetCinematicStage())
		{
		case EBGCinematicStage::Primary:
			if (GS->GetCinematicInstigator() == this)
			{
				return EBGCharacterAnimState::Cast;
			}
			break;

		case EBGCinematicStage::Secondary:
			//	Subject 로 비교한다 — 대상이 없는 카드(회복 등)는 사용자가 그대로 대상이 되므로
			//	CinematicTarget 으로 비교하면 자기 회복 연출에서 아무도 반응하지 않는다.
			if (GS->GetCinematicSubject() == this)
			{
				return EBGCharacterAnimState::React;
			}
			break;

		default:
			break;
		}
	}

	//	주인공이 아니다. 이미 죽어 있으면 계속 쓰러져 있는다 —
	//	남의 연출이 돈다고 시체가 일어나 구경하면 안 된다.
	if (const ABGPlayerState* PS = GetPlayerState<ABGPlayerState>())
	{
		if (PS->IsDead())
		{
			return EBGCharacterAnimState::Dead;
		}
	}

	return (GS && GS->IsCinematicPlaying())
		? EBGCharacterAnimState::Watch
		: EBGCharacterAnimState::Idle;
}

void ABGCharacter::RefreshAnimState()
{
	const ABGGameState* GS = GetWorld() ? GetWorld()->GetGameState<ABGGameState>() : nullptr;

	const FBGCardResolution EmptyResolution;
	const FBGCardResolution& Resolution = GS ? GS->GetCinematicResolution() : EmptyResolution;

	CinematicStage    = GS ? GS->GetCinematicStage() : EBGCinematicStage::None;
	CinematicCardId   = Resolution.CardId;
	CinematicOutcome  = Resolution.Outcome;
	//	Roll 이 비어 있으면 판정을 굴리지 않은 카드다 — Outcome 의 기본값(성공)을 믿으면 안 된다.
	bHasCinematicOutcome = (Resolution.Roll != INDEX_NONE);
	bCinematicSubject = GS && (GS->GetCinematicSubject() == this);

	const EBGCharacterAnimState NewState = ComputeAnimState(GS);
	if (NewState == AnimState)
	{
		return;   // 몽타주가 매번 처음부터 다시 시작하지 않도록 진짜 바뀔 때만 알린다.
	}

	AnimState = NewState;

	UE_LOG(LogBGCard, Verbose, TEXT("[Anim] %s → 상태 %d (카드 %s, 단계 %d)"),
		*GetName(), static_cast<int32>(AnimState),
		CinematicCardId.IsValid() ? *CinematicCardId.ToString() : TEXT("없음"),
		static_cast<int32>(CinematicStage));

	OnAnimStateChanged.Broadcast(AnimState);
}

void ABGCharacter::InitializeAbilityActorInfo()
{
	if (AbilitySystemComponent)
	{
		// OwnerActor·AvatarActor 모두 this (Character 소유 ASC).
		AbilitySystemComponent->InitAbilityActorInfo(this, this);
	}
}

void ABGCharacter::ServerInitGAS()
{
	if (!HasAuthority() || !AbilitySystemComponent || bGASInitialized)
	{
		return;
	}
	bGASInitialized = true;

	// 스탯/능력은 직업 축이 전담한다. 역할(RoleTag)은 DT 를 조회하지 않는다 — 승패 규칙 전용.
	ApplyJobDataRow();

	RefreshLooseTags();
	ApplyJobVisual();
}

void ABGCharacter::SetRoleTag(const FGameplayTag& NewTag)
{
	RoleTag = NewTag;

	// 역할은 GAS 를 건드리지 않는다. 루즈 태그만 맞춰주면 된다 —
	// 승패 판정과 어빌리티의 역할 조건 검사가 그 태그를 본다.
	RefreshLooseTags();
}

void ABGCharacter::SetJobTag(const FGameplayTag& NewTag)
{
	JobTag = NewTag;

	// 이미 초기화됐다면 즉시 재적용, 아니면 ServerInitGAS 가 최초 적용한다.
	if (HasAuthority() && bGASInitialized)
	{
		ApplyJobDataRow();
		RefreshLooseTags();
	}

	// 외형은 GAS 초기화 여부와 무관하다 — 서버에서 바로 바꾸고, 클라는 OnRep_JobTag 가 맡는다.
	// (서버 자신은 복제를 받지 않으므로 리슨서버 호스트는 이 경로가 유일하다)
	if (HasAuthority())
	{
		ApplyJobVisual();
	}
}

void ABGCharacter::OnRep_RoleTag()
{
	// 소유 클라: 역할 태그를 로컬 ASC 루즈 태그에 반영 (HUD/능력 조건 검사용).
	RefreshLooseTags();
}

void ABGCharacter::OnRep_JobTag()
{
	// 전 클라: 직업 태그를 로컬 ASC 루즈 태그에 반영하고 외형을 교체한다.
	RefreshLooseTags();
	ApplyJobVisual();
}

void ABGCharacter::NotifyHealthDepleted(AActor* Killer)
{
	if (!HasAuthority() || bDead)
	{
		return;
	}
	bDead = true;

	// GA 의 ActivationBlockedTags(State.Dead) 를 걸기 위한 서버 측 태그.
	// 루즈 태그는 복제되지 않으므로, 클라가 보는 "죽었다" 는 PlayerState::bIsDead 가 담당한다.
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->AddLooseGameplayTag(BG::State::Dead.GetTag());
	}

	ABGPlayerState* PS = GetPlayerState<ABGPlayerState>();
	if (!PS)
	{
		return;
	}
	PS->SetDead(true);

	// 죽인 사람의 좌석 (보상/벌칙 판정용). 자살이거나 출처를 모르면 INDEX_NONE.
	int32 KillerSeat = INDEX_NONE;
	if (const APawn* KillerPawn = Cast<APawn>(Killer))
	{
		if (const ABGPlayerState* KillerPS = KillerPawn->GetPlayerState<ABGPlayerState>())
		{
			KillerSeat = KillerPS->GetSeatIndex();
		}
	}

	UE_LOG(LogBGCard, Log, TEXT("[Death] 좌석 %d 사망 (가해자 좌석 %d)"), PS->GetSeatIndex(), KillerSeat);

	// 이후 규칙 처리(카드 정리·보상/벌칙·승패·턴)는 룰 계층이 맡는다.
	if (ABGGameState* GS = GetWorld() ? GetWorld()->GetGameState<ABGGameState>() : nullptr)
	{
		GS->HandleSeatDeath(PS->GetSeatIndex(), KillerSeat);
	}
}

void ABGCharacter::ApplyJobDataRow()
{
	if (!HasAuthority() || !AbilitySystemComponent || !JobTag.IsValid())
	{
		return;
	}

	const UGameInstance* GI = GetGameInstance();
	UBGDataSubsystem* DataSubsystem = GI ? GI->GetSubsystem<UBGDataSubsystem>() : nullptr;
	if (!DataSubsystem)
	{
		return;
	}

	const FBGCharacterRow* Row = DataSubsystem->GetCharacterRow(JobTag);
	if (!Row)
	{
		// 직업 Row 가 없다 = 이 캐릭터의 스탯이 통째로 비는 상태. 조용히 넘기면 안 된다.
		// (Row 이름은 태그 문자열과 정확히 같아야 한다 — "Character.Job.Mage")
		UE_LOG(LogTemp, Warning, TEXT("[BG] 직업 DT Row 없음 — %s (DT_BGCharacterRow 에 같은 이름의 Row 를 추가할 것)"),
			*JobTag.ToString());
		return;
	}

	// 유효한 Row 를 찾았을 때만 이전 부여분을 정리한다
	// (잘못된 태그로 능력이 전부 날아가는 것 방지).
	ClearJobGrants();

	UE_LOG(LogTemp, Log, TEXT("[BG] 능력 부여 시작 — %s (직업=%s, GE %d개 / 어빌리티 %d개)"),
		*GetName(), *JobTag.ToString(),
		(Row->DefaultAttributeGE ? 1 : 0) + Row->PassiveEffects.Num(),
		Row->GrantedAbilities.Num());

	// ── 스탯 GE + 패시브 GE 적용 ──
	TArray<TSubclassOf<UGameplayEffect>> EffectsToApply;
	if (Row->DefaultAttributeGE)
	{
		EffectsToApply.Add(Row->DefaultAttributeGE);
	}
	EffectsToApply.Append(Row->PassiveEffects);

	for (const TSubclassOf<UGameplayEffect>& EffectClass : EffectsToApply)
	{
		if (!EffectClass)
		{
			continue;
		}

		FGameplayEffectContextHandle Context = AbilitySystemComponent->MakeEffectContext();
		Context.AddSourceObject(this);

		const FGameplayEffectSpecHandle Spec = AbilitySystemComponent->MakeOutgoingSpec(EffectClass, 1.f, Context);
		if (!Spec.IsValid())
		{
			UE_LOG(LogTemp, Error, TEXT("[BG]   이펙트 적용 실패 — %s (Spec 무효)"), *EffectClass->GetName());
			continue;
		}

		// Infinite/Duration GE 만 유효 핸들을 돌려준다(Instant 는 무효) → 재적용 시 제거용으로 추적.
		const FActiveGameplayEffectHandle Handle =
			AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
		if (Handle.IsValid())
		{
			JobEffectHandles.Add(Handle);
		}
		UE_LOG(LogTemp, Log, TEXT("[BG]   이펙트 적용 — %s (%s)"),
			*EffectClass->GetName(), Handle.IsValid() ? TEXT("지속, 추적됨") : TEXT("즉시"));
	}

	// ── 능동 어빌리티 부여 ──
	for (const TSubclassOf<UGameplayAbility>& AbilityClass : Row->GrantedAbilities)
	{
		if (!AbilityClass)
		{
			UE_LOG(LogTemp, Warning, TEXT("[BG]   능력 부여 스킵 — %s 의 GrantedAbilities 에 빈 항목."),
				*JobTag.ToString());
			continue;
		}

		const FGameplayAbilitySpecHandle Handle =
			AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(AbilityClass, 1, INDEX_NONE, this));

		if (Handle.IsValid())
		{
			JobAbilityHandles.Add(Handle);
			UE_LOG(LogTemp, Log, TEXT("[BG]   능력 부여 성공 — %s"), *AbilityClass->GetName());
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[BG]   능력 부여 실패 — %s (핸들 무효)"), *AbilityClass->GetName());
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[BG] 능력 부여 완료 — %s / %s (어빌리티 %d개 부여됨)"),
		*GetName(), *JobTag.ToString(), JobAbilityHandles.Num());
}

void ABGCharacter::ClearJobGrants()
{
	if (!AbilitySystemComponent)
	{
		return;
	}

	for (const FActiveGameplayEffectHandle& Handle : JobEffectHandles)
	{
		if (Handle.IsValid())
		{
			AbilitySystemComponent->RemoveActiveGameplayEffect(Handle);
		}
	}
	JobEffectHandles.Reset();

	for (const FGameplayAbilitySpecHandle& Handle : JobAbilityHandles)
	{
		if (Handle.IsValid())
		{
			AbilitySystemComponent->ClearAbility(Handle);
		}
	}
	JobAbilityHandles.Reset();
}

void ABGCharacter::SyncLooseTag(const FGameplayTag& DesiredTag, FGameplayTag& AppliedSlot)
{
	if (!AbilitySystemComponent || AppliedSlot == DesiredTag)
	{
		return;
	}

	if (AppliedSlot.IsValid())
	{
		AbilitySystemComponent->RemoveLooseGameplayTag(AppliedSlot);
	}
	if (DesiredTag.IsValid())
	{
		AbilitySystemComponent->AddLooseGameplayTag(DesiredTag);
	}
	AppliedSlot = DesiredTag;
}

void ABGCharacter::RefreshLooseTags()
{
	// 역할과 직업은 각자 자기 슬롯을 갖는다. 한 슬롯으로 돌리면 나중 태그가 앞 태그를 떼어내
	// "직업을 배정했더니 역할 태그가 사라지는" 상태가 된다.
	SyncLooseTag(RoleTag, AppliedRoleLooseTag);
	SyncLooseTag(JobTag, AppliedJobLooseTag);
}

void ABGCharacter::ApplyJobVisual()
{
	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!MeshComp || !JobTag.IsValid())
	{
		return;
	}

	const UGameInstance* GI = GetGameInstance();
	const UBGDataSubsystem* DataSubsystem = GI ? GI->GetSubsystem<UBGDataSubsystem>() : nullptr;
	if (!DataSubsystem)
	{
		return;
	}

	const FBGCharacterRow* Row = DataSubsystem->GetCharacterRow(JobTag);
	if (!Row || Row->SkeletalMesh.IsNull())
	{
		// 아직 직업 메쉬를 안 꽂았을 뿐이다 — BP 기본 메쉬로 계속 간다.
		return;
	}

	USkeletalMesh* NewMesh = Row->SkeletalMesh.LoadSynchronous();
	if (!NewMesh)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BG] 직업 메쉬 로드 실패 — %s (%s)"),
			*JobTag.ToString(), *Row->SkeletalMesh.ToString());
		return;
	}

	if (MeshComp->GetSkeletalMeshAsset() == NewMesh)
	{
		return; // 이미 같은 메쉬 — 다시 꽂으면 AnimInstance 만 헛되이 재초기화된다.
	}

	MeshComp->SetSkeletalMeshAsset(NewMesh);

	// 직업 전용 ABP 가 있을 때만 갈아끼운다. 비워 두면 BP_BGCharacter 의 ABP 가 그대로
	// 재초기화된다 — 단, 새 메쉬의 스켈레톤이 그 ABP 와 호환될 때만 정상 동작한다.
	if (Row->AnimClass)
	{
		MeshComp->SetAnimInstanceClass(Row->AnimClass);
	}

	UE_LOG(LogTemp, Log, TEXT("[BG] 직업 외형 적용 — %s → %s (%s)"),
		*GetName(), *NewMesh->GetName(), *JobTag.ToString());
}
