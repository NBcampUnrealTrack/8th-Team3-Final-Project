#include "Core/BGCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GAS/BGAbilitySystemComponent.h"
#include "GAS/BGAttributeSet.h"
#include "Debug/BGCardLog.h"
#include "Card/BGDataSubsystem.h"
#include "Card/BGGameDataTypes.h"
#include "Card/BGCardType.h"
#include "Card/BGDiceComponent.h"
#include "GAS/Effect/GE_BGDamage.h"
#include "GAS/Effect/GE_BGSheriffBonus.h"
#include "AbilitySystemGlobals.h"
#include "BGGameplayTags.h"
#include "Core/BGGameState.h"
#include "Core/BGPlayerState.h"
#include "Engine/World.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Net/UnrealNetwork.h"
#include "FX/BGStatusFXComponent.h"
#include "Card/BGDataSettings.h"

ABGCharacter::ABGCharacter()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	AbilitySystemComponent = CreateDefaultSubobject<UBGAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	// 7인 턴제라 대역폭이 무의미하고, 모든 클라가 동일한 GE 상태를 봐 디버깅이 쉽다 (md §M3).
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Full);

	AttributeSet = CreateDefaultSubobject<UBGAttributeSet>(TEXT("AttributeSet"));

	//	장비/버프의 복제 상태에서 지속 Niagara 집합을 유도한다.
	StatusFX = CreateDefaultSubobject<UBGStatusFXComponent>(TEXT("StatusFX"));

	//	정체 공개 연출 전까지는 메시도 없고 숨겨진 채다 — RefreshRoleRevealVisual 이 채운다.
	RevealMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RevealMeshComponent"));
	RevealMeshComponent->SetupAttachment(GetCapsuleComponent());
	RevealMeshComponent->SetVisibility(false);
	RevealMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

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
			//
			//	⚠️ 여기서 사망을 보지 않는 것이 설계다. 죽는 일격이어도 평소와 같은 피격 연출을
			//	   재생하고, 쓰러지는 것은 이 카드 연출이 다 끝난 뒤 DeathPrimary 로 온다.
			if (GS->GetCinematicSubject() == this)
			{
				return EBGCharacterAnimState::React;
			}
			break;

		case EBGCinematicStage::DeathPrimary:
			if (GS->GetCinematicSubject() == this)
			{
				return EBGCharacterAnimState::Dying;
			}
			break;

		case EBGCinematicStage::DeathSecondary:
			if (GS->GetCinematicSubject() == this)
			{
				return EBGCharacterAnimState::RoleReveal;
			}
			break;

		default:
			break;
		}
	}

	//	주인공이 아니다. 이미 쓰러진 뒤면 계속 쓰러져 있는다 —
	//	남의 연출이 돈다고 시체가 일어나 구경하면 안 된다.
	//
	//	⚠️ IsDead() 가 아니라 IsDeathShown() 이다. 규칙상 사망은 체력이 0 이 되는 즉시 켜지는데,
	//	   그걸 보면 죽는 일격의 피격 연출이 시작되기도 전에 먼저 쓰러진다(bDeathShown 주석).
	if (const ABGPlayerState* PS = GetPlayerState<ABGPlayerState>())
	{
		if (PS->IsDeathShown())
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

	//	라이브로 지켜보는 클라는 몽타주에 심어 둔 UBGAnimNotify_RoleReveal 이 타이밍을 맞춰 부른다.
	//	여기서는 Dead 진입 때만 안전망으로 부른다 — 그 몽타주를 아예 못 본(늦게 접속한) 클라 대비.
	if (AnimState == EBGCharacterAnimState::Dead)
	{
		RefreshRoleRevealVisual();
	}

	UE_LOG(LogBGCard, Verbose, TEXT("[Anim] %s → 상태 %d (카드 %s, 단계 %d)"),
		*GetName(), static_cast<int32>(AnimState),
		CinematicCardId.IsValid() ? *CinematicCardId.ToString() : TEXT("없음"),
		static_cast<int32>(CinematicStage));

	OnAnimStateChanged.Broadcast(AnimState);
}

void ABGCharacter::RefreshRoleRevealVisual()
{
	//	이미 스왑됐으면 그 사실 자체(메시가 이미 꽂혀 있음)로 판단한다 — 별도 bool 을 손으로
	//	맞춰야 하는 상태로 두면 나중에 메시만 바꾸고 플래그를 깜빡하는 식으로 어긋날 수 있다.
	if (!RevealMeshComponent || RevealMeshComponent->GetStaticMesh() != nullptr)
	{
		return;
	}

	//	RoleReveal 이후(Dead 포함)에만 정체가 드러난다. Dead 도 허용하는 이유는 늦게 접속한
	//	클라가 곧바로 Dead 상태로 시작해 RoleReveal 전이(=몽타주 재생)를 못 보고 지나치기 때문이다.
	const bool bShouldReveal = (AnimState == EBGCharacterAnimState::RoleReveal
		|| AnimState == EBGCharacterAnimState::Dead);
	if (!bShouldReveal)
	{
		return;
	}

	//	진짜 역할(RoleTag)이 아니라 공개된 역할을 봐야 한다 — RoleTag 는 COND_OwnerOnly 라
	//	남의 화면에서는 항상 비어 있다.
	const ABGPlayerState* PS = GetPlayerState<ABGPlayerState>();
	const FGameplayTag PublicRole = PS ? PS->GetPublicRole() : FGameplayTag();
	if (!PublicRole.IsValid())
	{
		return;   // RevealDeadRole 이 아직 안 왔다 — ABGPlayerState::OnRep_PublicRole 이 도착 즉시 다시 부른다.
	}

	const UBGDataSettings* Settings = GetDefault<UBGDataSettings>();
	UStaticMesh* RevealMesh = Settings ? Settings->GetRevealMeshForRole(PublicRole).LoadSynchronous() : nullptr;
	if (!RevealMesh)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BG] 정체 공개 메시 없음 — %s (BGDataSettings 의 Reveal 메시를 채울 것)"),
			*PublicRole.ToString());
		return;
	}

	RevealMeshComponent->SetStaticMesh(RevealMesh);
	RevealMeshComponent->SetVisibility(true);
	GetMesh()->SetVisibility(false, true);   // true = 붙어 있던 무기 등 자식까지 함께 숨긴다.

	//	AnimState==Dead 에서 처리됐다면 몽타주 노티파이보다 안전망이 먼저(또는 대신) 처리한 것이다 —
	//	늦은 접속자라면 정상이지만, 라이브로 보고 있었다면 노티파이 배치를 잊었다는 신호다.
	if (AnimState == EBGCharacterAnimState::Dead)
	{
		UE_LOG(LogBGCard, Log, TEXT("[Anim] %s — 안전망이 정체 공개 스왑 처리 (몽타주 노티파이 미확인)"), *GetName());
	}
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

	// 역할은 원칙적으로 GAS 를 건드리지 않는다. 루즈 태그만 맞추면 승패 판정과
	// 어빌리티의 역할 조건 검사가 이 태그를 본다. 보안관 고정 보너스만 예외로
	// 여기서 같이 적용한다(ApplySheriffBonusIfNeeded 참고).
	RefreshLooseTags();
	ApplySheriffBonusIfNeeded();
}

void ABGCharacter::ApplySheriffBonusIfNeeded()
{
	if (!HasAuthority() || !AbilitySystemComponent || RoleTag != BG::Role::Sheriff.GetTag())
	{
		return;
	}

	FGameplayEffectContextHandle Context = AbilitySystemComponent->MakeEffectContext();
	Context.AddSourceObject(this);

	const FGameplayEffectSpecHandle Spec =
		AbilitySystemComponent->MakeOutgoingSpec(UGE_BGSheriffBonus::StaticClass(), 1.f, Context);
	if (!Spec.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("[BG] 보안관 보너스 GE 적용 실패. %s (Spec 무효)"), *GetName());
		return;
	}

	AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
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

void ABGCharacter::MarkAsBoss(TSubclassOf<UGameplayEffect> StatBoostGE)
{
	if (!HasAuthority() || !AbilitySystemComponent)
	{
		return;
	}

	// State.Dead 등과 같은 방식. 룰 상태는 루즈 게임플레이 태그로 얹는다.
	AbilitySystemComponent->AddLooseGameplayTag(BG::State::Boss.GetTag());

	if (!StatBoostGE)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BG] 보스 강화 GE 없음. %s 는 태그만 붙고 스탯은 그대로."), *GetName());
		return;
	}

	FGameplayEffectContextHandle Context = AbilitySystemComponent->MakeEffectContext();
	Context.AddSourceObject(this);

	const FGameplayEffectSpecHandle Spec = AbilitySystemComponent->MakeOutgoingSpec(StatBoostGE, 1.f, Context);
	if (!Spec.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("[BG] 보스 강화 GE 적용 실패. %s (Spec 무효)"), *GetName());
		return;
	}

	AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	UE_LOG(LogTemp, Log, TEXT("[BG] 보스 지정: %s"), *GetName());
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

namespace
{
	/**
	 * 반사 피해를 적용하는 중인가.
	 *
	 * 수도사 둘이 마주 보면 A→B 반사가 B→A 반사를 부르고 그것이 다시 A→B 를 불러 스택이 터진다.
	 * 반사 피해는 반사를 유발하지 않는다는 규칙을 이 걸쇠로 강제한다.
	 *
	 * 전역이어도 되는 이유: 서버에서 동기적으로만 오간다(피해 GE 적용은 호출 스택 안에서 끝난다).
	 * PIE 로 월드가 여럿이어도 스택이 겹치지 않으므로 서로를 막지 않는다.
	 */
	bool GBGApplyingReflect = false;
}

void ABGCharacter::NotifyDamaged(AActor* DamageInstigator, int32 DamageAmount)
{
	if (!HasAuthority() || DamageAmount <= 0 || !AbilitySystemComponent)
	{
		return;
	}

	//	죽은 뒤에는 되받지도 훔치지도 않는다.
	//	bDead 는 NotifyHealthDepleted 가 즉시 세우지만 사망 규칙 처리(HandleSeatDeath)는 한 틱 뒤다.
	//	그 사이(또는 그 뒤)에 들어온 피해 GE 도 Delta < 0 이라 여기까지 오므로, 이 검사가 없으면
	//	**죽은 수도사가 반사하고 죽은 도둑이 카드를 훔친다.**
	if (bDead)
	{
		return;
	}

	//	가해자가 없거나(다이너마이트 등) 자해면 되받을 대상이 없다.
	APawn* AttackerPawn = Cast<APawn>(DamageInstigator);
	if (!AttackerPawn || AttackerPawn == this)
	{
		return;
	}

	ABGPlayerState* SelfPS     = GetPlayerState<ABGPlayerState>();
	ABGPlayerState* AttackerPS = AttackerPawn->GetPlayerState<ABGPlayerState>();
	if (!SelfPS || !AttackerPS || AttackerPS->IsDead())
	{
		return;
	}

	// ── 수도사 (DamageReflect) ────────────────────────────────────
	const int32 Reflect = FMath::RoundToInt(
		AbilitySystemComponent->GetNumericAttribute(UBGAttributeSet::GetDamageReflectAttribute()));

	if (Reflect > 0 && !GBGApplyingReflect)
	{
		if (UAbilitySystemComponent* AttackerASC =
				UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(AttackerPawn))
		{
			//	반사 피해가 다시 반사를 부르지 않게 잠근다. 아래 ApplyGameplayEffectSpecToTarget 이
			//	가해자의 PostGameplayEffectExecute 를 **이 스택 안에서** 부르기 때문이다.
			TGuardValue<bool> ReflectGuard(GBGApplyingReflect, true);

			FGameplayEffectContextHandle Context = AbilitySystemComponent->MakeEffectContext();
			Context.AddInstigator(this, this);

			const FGameplayEffectSpecHandle Spec = AbilitySystemComponent->MakeOutgoingSpec(
				UGE_BGDamage::StaticClass(), 1.f, Context);

			if (Spec.IsValid())
			{
				Spec.Data->SetSetByCallerMagnitude(
					BG::SetByCaller::Damage.GetTag(), -static_cast<float>(Reflect));
				AbilitySystemComponent->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), AttackerASC);

				UE_LOG(LogBGCard, Log, TEXT("[반사] 좌석 %d → 좌석 %d 에 피해 %d"),
					SelfPS->GetSeatIndex(), AttackerPS->GetSeatIndex(), Reflect);
			}
		}
	}

	// ── 도둑 (DamageStealCount) ───────────────────────────────────
	const int32 StealCount = FMath::RoundToInt(
		AbilitySystemComponent->GetNumericAttribute(UBGAttributeSet::GetDamageStealCountAttribute()));

	if (StealCount > 0)
	{
		UBGDiceComponent* Dice = UBGDiceComponent::Get(this);
		int32 Stolen = 0;

		//	가해자가 **지금 내고 있는 카드**는 훔칠 수 없다. 그 카드는 GA 가 도는 동안
		//	아직 손패에 남아 있을 뿐이고, 곧 ConsumeCard 로 버린 패에 간다.
		//	가져가면 카드가 복제되고 ConsumeCard 가 실패해 덱 총량이 어긋난다.
		//	평소에는 INDEX_NONE 이고, 손패의 어떤 카드도 그 값을 갖지 않는다
		//	(덱은 1 부터, 1회성 카드는 -2 부터).
		const int32 InFlightId = AttackerPS->CardInFlightInstanceId;

		TArray<int32> Candidates;
		for (int32 i = 0; i < StealCount; ++i)
		{
			//	손패는 매번 다시 본다 — 한 장 가져올 때마다 크기가 줄어든다.
			Candidates.Reset();
			for (const FBGCardInstance& Card : AttackerPS->GetHand())
			{
				if (Card.InstanceId != InFlightId)
				{
					Candidates.Add(Card.InstanceId);
				}
			}

			if (Candidates.IsEmpty())
			{
				break;   // 가해자가 빈손(또는 내고 있는 카드 한 장뿐)이면 아무 일도 일어나지 않는다
			}

			const int32 Pick = Dice ? Dice->RandomIndex(Candidates.Num()) : 0;
			if (!Candidates.IsValidIndex(Pick))
			{
				break;
			}

			FBGCardInstance Taken;
			if (!AttackerPS->RemoveCardFromHand(Candidates[Pick], Taken))
			{
				break;
			}
			SelfPS->AddCardToHand(Taken);
			++Stolen;
		}

		if (Stolen > 0)
		{
			UE_LOG(LogBGCard, Log, TEXT("[강탈] 좌석 %d 가 좌석 %d 에게서 %d 장 가져옴"),
				SelfPS->GetSeatIndex(), AttackerPS->GetSeatIndex(), Stolen);
		}
	}
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
	if (!Row)
	{
		//	Row 자체가 없는 경우는 GrantJobAbilities 가 이미 경고한다(스탯도 같이 비는 상태다).
		return;
	}

	//	⚠️ 메쉬와 ABP 는 **각자 판단한다.** 하나로 묶으면 둘 다 조용히 깨진다.
	//	  · 메쉬만 비어 있어도 ABP 까지 건너뛴다 — 지금 12종 메쉬가 전부 미정이라
	//	    ABP 를 채워도 아무 일이 일어나지 않는다.
	//	  · 두 직업이 같은 플레이스홀더 메쉬를 쓰면 "메쉬가 이미 같다" 로 조기 반환해
	//	    나중 직업의 ABP 가 통째로 무시된다.
	//	둘 다 "애니메이션이 안 바뀌는데 ABP 어디를 봐도 원인이 없는" 모양으로 나타난다.

	// ── 메쉬 ──
	bool bMeshChanged = false;
	if (!Row->SkeletalMesh.IsNull())
	{
		if (USkeletalMesh* NewMesh = Row->SkeletalMesh.LoadSynchronous())
		{
			//	같은 메쉬를 다시 꽂으면 AnimInstance 만 헛되이 재초기화된다.
			if (MeshComp->GetSkeletalMeshAsset() != NewMesh)
			{
				MeshComp->SetSkeletalMeshAsset(NewMesh);
				bMeshChanged = true;
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[BG] 직업 메쉬 로드 실패 — %s (%s)"),
				*JobTag.ToString(), *Row->SkeletalMesh.ToString());
		}
	}

	// ── ABP ──
	//	직업 전용 ABP 가 있을 때만 갈아끼운다. 비워 두면 BP_BGCharacter 의 ABP 를 그대로 쓴다
	//	(메쉬를 바꿨다면 위에서 이미 그 ABP 로 재초기화됐다).
	//
	//	메쉬를 **먼저** 꽂는 순서를 지킬 것 — ABP 는 새 메쉬의 스켈레톤 위에서 초기화돼야 한다.
	//	메쉬와 ABP 를 둘 다 바꾸면 초기화가 두 번 도는데, 직업 배정 때 한 번뿐이라 문제되지 않는다.
	bool bAnimChanged = false;
	if (Row->AnimClass && MeshComp->GetAnimClass() != Row->AnimClass.Get())
	{
		MeshComp->SetAnimInstanceClass(Row->AnimClass);
		bAnimChanged = true;
	}

	if (bMeshChanged || bAnimChanged)
	{
		const USkeletalMesh* AppliedMesh = MeshComp->GetSkeletalMeshAsset();
		UE_LOG(LogTemp, Log, TEXT("[BG] 직업 외형 적용 — %s (%s) 메쉬=%s / ABP=%s"),
			*GetName(), *JobTag.ToString(),
			bMeshChanged && AppliedMesh ? *AppliedMesh->GetName() : TEXT("그대로"),
			bAnimChanged ? *Row->AnimClass->GetName() : TEXT("그대로"));
	}
}
