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
#include "Core/BGGameState.h"
#include "Core/BGPlayerState.h"
#include "GAS/BGAttributeSet.h"

namespace
{
	/** 좌석이 배정된 PlayerState 만 좌석 번호 순으로 모은다. */
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

	/** PlayerState 소유 폰의 ASC. ASC 는 ABGCharacter 가 들고 있다. */
	const UAbilitySystemComponent* GetSeatASC(const ABGPlayerState* PS)
	{
		return PS ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PS->GetPawn()) : nullptr;
	}

	FString RoleDisplayName(const FGameplayTag& Role)
	{
		if (Role == BG::Role::Sheriff.GetTag())  { return TEXT("보안관"); }
		if (Role == BG::Role::Deputy.GetTag())   { return TEXT("부관"); }
		if (Role == BG::Role::Outlaw.GetTag())   { return TEXT("무법자"); }
		if (Role == BG::Role::Renegade.GetTag()) { return TEXT("배신자"); }
		return Role.IsValid() ? Role.ToString() : FString();
	}

	ABGGameState* GetBGGameState(const APlayerController* Viewer)
	{
		const UWorld* World = Viewer ? Viewer->GetWorld() : nullptr;
		return World ? World->GetGameState<ABGGameState>() : nullptr;
	}

	/**
	 * 살아 있는 좌석만 좌석 번호 순으로. 거리 계산의 기준 원이다.
	 *
	 * ⚠️ "살아 있다" 의 판정은 ABGGameState 한곳에만 둔다. 여기서 따로 계산하면
	 *    UI 는 살아 있다고 보는데 턴 진행은 건너뛰는 식으로 조용히 어긋난다.
	 */
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

	//	⚠️ 거리는 "살아 있는 좌석" 의 원에서 센다. 사망자는 원에서 빠지므로
	//	   누군가 죽으면 모든 거리가 변한다. 월드 좌표로 계산하면 반드시 틀린다.
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

		//	파란 카드(장비)는 공개 정보다.
		for (const FBGCardInstance& Equip : PS->GetEquipment())
		{
			const FBGCardRow* Row = Data ? Data->GetCardRow(Equip.CardId) : nullptr;
			View.EquipmentNames.Add(Row ? Row->DisplayName : FText::FromName(Equip.CardId));
		}

		//	⚠️ 반드시 PublicRoleTag 를 본다. GetSeatRole() 은 서버 권위 진짜 역할이라
		//	   리슨서버 호스트 화면에서는 전원의 역할이 그대로 노출된다.
		//	   공개 여부 판정은 서버가 이미 끝냈고(보안관 / 사망자 / 판 종료) UI 는 결과만 쓴다.
		FGameplayTag VisibleRole = PS->GetPublicRole();

		//	본인은 자기 역할을 안다 — RoleTag 가 COND_OwnerOnly 로 자기에게만 온다.
		if (!VisibleRole.IsValid() && View.bIsLocalPlayer && GS)
		{
			VisibleRole = GS->GetSeatRole(View.SeatIndex);
		}

		View.RoleName = VisibleRole.IsValid()
			? FText::FromString(RoleDisplayName(VisibleRole))
			: FText::GetEmpty();

		View.Distance = GetRingDistance(Viewer, ViewerSeat, View.SeatIndex);

		View.bIsCurrentTurn = (CurrentSeat != INDEX_NONE && View.SeatIndex == CurrentSeat);
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
