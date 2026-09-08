#include "UI/View/BGSeatViewLibrary.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "Debug/BGCardLog.h"
#include "Card/BGCardType.h"
#include "Card/BGDataSubsystem.h"
#include "Card/BGGameDataTypes.h"
#include "BGGameplayTags.h"
#include "Core/BGCharacter.h"
#include "Core/BGGameState.h"
#include "Core/BGPlayerState.h"
#include "GAS/BGAttributeSet.h"

namespace
{
	void CollectSeatedPlayerStates(const APlayerController* Viewer, TArray<ABGPlayerState*>& OutStates)
	{
		OutStates.Reset();

		const UWorld* World = Viewer ? Viewer->GetWorld() : nullptr;
		const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
		if (!GS)
		{
			return;
		}

		for (APlayerState* Base : GS->PlayerArray)
		{
			ABGPlayerState* PS = Cast<ABGPlayerState>(Base);
			if (PS && PS->GetSeatIndex() != INDEX_NONE)
			{
				OutStates.Add(PS);
			}
		}

		OutStates.Sort([](const ABGPlayerState& A, const ABGPlayerState& B)
		{
			return A.GetSeatIndex() < B.GetSeatIndex();
		});
	}

	/**
	 * 이 좌석의 폰. 이탈자는 UnPossess 로 폰을 월드에 남겨 두므로 PlayerState 의
	 * 역참조가 끊긴다 — 서버에서만 그렇고 클라에서는 남아 있어, 호스트 화면에서만
	 * 직업·체력이 사라지는 것으로 보인다. 두고 간 폰을 폴백으로 본다.
	 */
	APawn* GetSeatPawn(const ABGPlayerState* PS)
	{
		if (!PS)
		{
			return nullptr;
		}

		APawn* Live = PS->GetPawn();
		return Live ? Live : PS->GetAbandonedPawn();
	}

	const UAbilitySystemComponent* GetSeatASC(const ABGPlayerState* PS)
	{
		return UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetSeatPawn(PS));
	}

	FString RoleDisplayName(const FGameplayTag& Role)
	{
		if (Role == BG::Role::Sheriff.GetTag())  { return TEXT("보안관"); }
		if (Role == BG::Role::Deputy.GetTag())   { return TEXT("부관"); }
		if (Role == BG::Role::Outlaw.GetTag())   { return TEXT("무법자"); }
		if (Role == BG::Role::Renegade.GetTag()) { return TEXT("배신자"); }
		return Role.IsValid() ? Role.ToString() : FString();
	}

	// 직업 이름. DT 의 DisplayName 이 우선이고, 비어 있으면 태그의 마지막 마디를 쓴다.
	// 이름을 코드에 박지 않으려는 것이라 폴백이 보기 나빠도 그대로 둔다 — DT 를 채우라는 신호다.
	FText JobDisplayName(const FGameplayTag& Job, const UBGDataSubsystem* Data)
	{
		if (!Job.IsValid())
		{
			return FText::GetEmpty();
		}

		if (const FBGCharacterRow* Row = Data ? Data->GetCharacterRow(Job) : nullptr)
		{
			if (!Row->DisplayName.IsEmpty())
			{
				return Row->DisplayName;
			}
		}

		FString Leaf = Job.ToString();
		int32 LastDot = INDEX_NONE;
		if (Leaf.FindLastChar(TEXT('.'), LastDot))
		{
			Leaf = Leaf.RightChop(LastDot + 1);
		}
		return FText::FromString(Leaf);
	}

	ABGGameState* GetBGGameState(const APlayerController* Viewer)
	{
		const UWorld* World = Viewer ? Viewer->GetWorld() : nullptr;
		return World ? World->GetGameState<ABGGameState>() : nullptr;
	}

	void CollectAliveSeats(const APlayerController* Viewer, TArray<int32>& OutSeats)
	{
		OutSeats.Reset();

		if (const ABGGameState* GS = GetBGGameState(Viewer))
		{
			OutSeats = GS->GetAliveSeatsInTableOrder();
		}
	}
}

int32 UBGSeatViewLibrary::GetRingDistance(const APlayerController* Viewer, int32 FromSeat, int32 ToSeat)
{
	if (FromSeat == INDEX_NONE || ToSeat == INDEX_NONE || FromSeat == ToSeat)
	{
		return INDEX_NONE;
	}

	//	주의: 거리는 "살아 있는 좌석" 의 원에서 센다. 사망자는 원에서 빠지므로
	// 누군가 죽으면 모든 거리가 변한다. 월드 좌표로 계산하면 반드시 틀린다.
	TArray<int32> Alive;
	CollectAliveSeats(Viewer, Alive);

	const int32 i = Alive.IndexOfByKey(FromSeat);
	const int32 j = Alive.IndexOfByKey(ToSeat);
	if (i == INDEX_NONE || j == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	const int32 N = Alive.Num();
	const int32 Step = FMath::Abs(i - j);

	//	TODO(STEP 7): Scope(보는 쪽 -1) / Mustang(보이는 쪽 +1) 어트리뷰트 보정 후
	//	              FMath::Max(1, ...) 로 하한을 건다.
	return FMath::Min(Step, N - Step);
}

ABGPlayerState* UBGSeatViewLibrary::FindPlayerStateBySeat(const APlayerController* Viewer, int32 SeatIndex)
{
	if (SeatIndex == INDEX_NONE)
	{
		return nullptr;
	}

	TArray<ABGPlayerState*> States;
	CollectSeatedPlayerStates(Viewer, States);

	for (ABGPlayerState* PS : States)
	{
		if (PS->GetSeatIndex() == SeatIndex)
		{
			return PS;
		}
	}
	return nullptr;
}

TArray<FBGSeatView> UBGSeatViewLibrary::MakeSeatViews(const APlayerController* Viewer)
{
	TArray<FBGSeatView> Views;

	TArray<ABGPlayerState*> States;
	CollectSeatedPlayerStates(Viewer, States);
	if (States.IsEmpty())
	{
		return Views;
	}

	const ABGPlayerState* ViewerState = Viewer ? Viewer->GetPlayerState<ABGPlayerState>() : nullptr;
	const int32 ViewerSeat = ViewerState ? ViewerState->GetSeatIndex() : INDEX_NONE;

	const UWorld* World = Viewer ? Viewer->GetWorld() : nullptr;
	const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	const UBGDataSubsystem* Data = GI ? GI->GetSubsystem<UBGDataSubsystem>() : nullptr;

	const ABGGameState* GS = GetBGGameState(Viewer);
	const int32 CurrentSeat = GS ? GS->GetCurrentSeat() : INDEX_NONE;

	Views.Reserve(States.Num());

	for (const ABGPlayerState* PS : States)
	{
		FBGSeatView& View = Views.AddDefaulted_GetRef();
		View.SeatIndex      = PS->GetSeatIndex();
		View.PlayerName     = FText::FromString(PS->GetPlayerName());
		View.HandCount      = PS->GetHandCount();   //	장수만. 내용은 복제되지 않는다.
		View.bIsLocalPlayer = (PS == ViewerState);

		if (const UAbilitySystemComponent* ASC = GetSeatASC(PS))
		{
			View.Health    = FMath::RoundToInt(ASC->GetNumericAttribute(UBGAttributeSet::GetHealthAttribute()));
			View.MaxHealth = FMath::RoundToInt(ASC->GetNumericAttribute(UBGAttributeSet::GetMaxHealthAttribute()));
		}

		//	생사 판정도 GameState 한곳에서만 한다(턴 진행과 어긋나면 안 된다).
		View.bIsAlive = GS ? GS->IsSeatAlive(View.SeatIndex) : true;

		//	직업은 역할과 달리 조건 없이 전원에게 복제된다(GDD §7) — 숨김 처리가 필요 없다.
		if (const ABGCharacter* Character = Cast<ABGCharacter>(GetSeatPawn(PS)))
		{
			const FGameplayTag JobTag = Character->GetJobTag();
			View.JobTag  = JobTag;
			View.JobName = JobDisplayName(JobTag, Data);

			//	설명은 폴백을 만들지 않는다 — 비면 툴팁이 표의 기본 문구를 쓴다.
			if (const FBGCharacterRow* JobRow = Data ? Data->GetCharacterRow(JobTag) : nullptr)
			{
				View.JobDescription = JobRow->Description;
			}
		}

		//	파란 카드(장비)는 공개 정보다.
		for (const FBGCardInstance& Equip : PS->GetEquipment())
		{
			const FBGCardRow* Row = Data ? Data->GetCardRow(Equip.CardId) : nullptr;
			FBGEquipmentView& Item = View.Equipment.AddDefaulted_GetRef();
			Item.Name        = Row ? Row->DisplayName : FText::FromName(Equip.CardId);
			Item.Description = Row ? Row->Description : FText::GetEmpty();
		}

		//	주의: 반드시 PublicRoleTag 를 본다. GetSeatRole() 은 서버 권위 진짜 역할이라
		// 리슨서버 호스트 화면에서는 전원의 역할이 그대로 노출된다.
		// 공개 여부 판정은 서버가 이미 끝냈고(보안관 / 사망자 / 판 종료) UI 는 결과만 쓴다.
		FGameplayTag VisibleRole = PS->GetPublicRole();

		//	본인은 자기 역할을 안다 — RoleTag 가 COND_OwnerOnly 로 자기에게만 온다.
		if (!VisibleRole.IsValid() && View.bIsLocalPlayer && GS)
		{
			VisibleRole = GS->GetSeatRole(View.SeatIndex);
		}

		View.RoleName = VisibleRole.IsValid()
			? FText::FromString(RoleDisplayName(VisibleRole))
			: FText::GetEmpty();

		View.RoleTag = VisibleRole;   //	RoleName 과 같은 조건 — 공개된 것만 온다.

		View.Distance = GetRingDistance(Viewer, ViewerSeat, View.SeatIndex);

		View.bIsCurrentTurn  = (CurrentSeat != INDEX_NONE && View.SeatIndex == CurrentSeat);
		View.bIsDisconnected = PS->IsDisconnected();
		View.bMatchRunning   = (GS != nullptr) && GS->IsMatchRunning();
	}

	return Views;
}

TArray<int32> UBGSeatViewLibrary::GetSelectableSeats(const APlayerController* Viewer, bool bExcludeSelf)
{
	TArray<int32> Seats;

	const ABGPlayerState* ViewerState = Viewer ? Viewer->GetPlayerState<ABGPlayerState>() : nullptr;
	const int32 ViewerSeat = ViewerState ? ViewerState->GetSeatIndex() : INDEX_NONE;

	CollectAliveSeats(Viewer, Seats);

	if (bExcludeSelf && ViewerSeat != INDEX_NONE)
	{
		Seats.Remove(ViewerSeat);
	}

	return Seats;
}
