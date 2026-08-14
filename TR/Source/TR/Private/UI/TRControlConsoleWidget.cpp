// TRControlConsoleWidget.cpp

#include "UI/TRControlConsoleWidget.h"
#include "UI/TRConsoleButton.h"
#include "Components/TextBlock.h"
#include "Core/TRGameState.h"
#include "Debug/TRGameplayLog.h"
#include "Interaction/TRActionComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Player/TRPlayerCharacter.h"
#include "TRTypes.h"
#include "Train/TRArmor.h"
#include "Train/TRTrain.h"
#include "Train/TRTurret.h"
#include "GameFramework/PlayerController.h"

void UTRControlConsoleWidget::NativeConstruct()
{
	Super::NativeConstruct();

	SlotButtons.Reset();
	CollectSlotRow(StaticEnum<ETRArmorSlot>(), TEXT("Btn_Armor_"), true);
	CollectSlotRow(StaticEnum<ETRTurretSlot>(), TEXT("Btn_Turret_"), false);

	// 위젯이 재사용되어 NativeConstruct 가 다시 돌아도 중복으로 쌓이지 않게 내 것만 먼저 떼어 낸다.
	if (IsValid(Btn_UpgradeArmor))
	{
		Btn_UpgradeArmor->OnClicked().RemoveAll(this);
		Btn_UpgradeArmor->OnClicked().AddUObject(this, &UTRControlConsoleWidget::HandleUpgradeClicked, true);
	}
	if (IsValid(Btn_UpgradeTurret))
	{
		Btn_UpgradeTurret->OnClicked().RemoveAll(this);
		Btn_UpgradeTurret->OnClicked().AddUObject(this, &UTRControlConsoleWidget::HandleUpgradeClicked, false);
	}
	if (IsValid(Btn_Close))
	{
		Btn_Close->OnClicked().RemoveAll(this);
		Btn_Close->OnClicked().AddUObject(this, &UTRControlConsoleWidget::RequestClose);
	}

	ApplyPopupInputMode();
	Refresh();
}

void UTRControlConsoleWidget::NativeDestruct()
{
	RestoreGameInputMode();

	Super::NativeDestruct();
}

void UTRControlConsoleWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// 표시하는 값이 모두 복제 값이라 언제든 바뀐다. 모달 UI라 매 프레임 갱신해도 부담이 없다.
	Refresh();
}

void UTRControlConsoleWidget::ApplyPopupInputMode()
{
	APlayerController* PC = GetOwningPlayer();
	if (!IsValid(PC))
	{
		return;
	}

	bPrevShowMouseCursor = PC->bShowMouseCursor;

	//	GameAndUI로 둔다. UIOnly 면 컨트롤러가 키를 못 받아 E 로 다시 닫을 수 없다.
	FInputModeGameAndUI Mode;
	Mode.SetWidgetToFocus(TakeWidget());
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);

	PC->SetInputMode(Mode);
	PC->SetShowMouseCursor(true);

	//	시점과 이동을 막아야 관제석 조준이 유지된다 - 서버가 재설치를 조준 대상으로 검증한다.
	PC->SetIgnoreLookInput(true);
	PC->SetIgnoreMoveInput(true);
	bInputSuppressed = true;
}

void UTRControlConsoleWidget::RestoreGameInputMode()
{
	APlayerController* PC = GetOwningPlayer();
	if (!IsValid(PC))
	{
		return;
	}

	if (bInputSuppressed)
	{
		PC->SetIgnoreLookInput(false);
		PC->SetIgnoreMoveInput(false);
		bInputSuppressed = false;
	}

	// 기본 캡처 모드로 맞춘다. 이걸 빼면 닫은 뒤 첫 클릭이 씹힌다.
	FInputModeGameOnly Mode;
	Mode.SetConsumeCaptureMouseDown(true);

	PC->SetInputMode(Mode);
	PC->SetShowMouseCursor(bPrevShowMouseCursor);
}

void UTRControlConsoleWidget::CollectSlotRow(const UEnum* SlotEnum, const TCHAR* NamePrefix, bool bArmor)
{
	if (!IsValid(SlotEnum))
	{
		return;
	}

	TArray<FString> Missing;

	// UHT가 마지막에 _MAX 를 자동으로 붙이므로 하나 뺀다.
	const int32 SlotCount = SlotEnum->NumEnums() - 1;
	for (int32 EnumIndex = 0; EnumIndex < SlotCount; ++EnumIndex)
	{
		const FString SlotName = SlotEnum->GetNameStringByIndex(EnumIndex);

		UTRConsoleButton* Button = Cast<UTRConsoleButton>(GetWidgetFromName(FName(*(NamePrefix + SlotName))));
		if (!IsValid(Button))
		{
			Missing.Add(SlotName);
			continue;
		}

		const int32 SlotIndex = SlotButtons.Add({ Button, bArmor, static_cast<uint8>(SlotEnum->GetValueByIndex(EnumIndex)) });

		Button->OnClicked().RemoveAll(this);
		Button->OnClicked().AddUObject(this, &UTRControlConsoleWidget::BuildSlot, SlotIndex);
	}

	if (!Missing.IsEmpty())
	{
		UE_LOG(LogTRGameplay, Warning, TEXT("%s: console buttons not found: %s%s"), *GetName(), NamePrefix, *FString::Join(Missing, TEXT(", ")));
	}
}

void UTRControlConsoleWidget::Refresh()
{
	RefreshResourceAndLevels();
	RefreshSlots();
}

void UTRControlConsoleWidget::RefreshResourceAndLevels()
{
	const ATRGameState* GameState = GetWorld()->GetGameState<ATRGameState>();
	if (!IsValid(GameState))
	{
		return;
	}

	const int32 MonsterResource = GameState->GetMonsterResource();
	const int32 ArmorLevel = GameState->GetArmorLevel();
	const int32 TurretLevel = GameState->GetTurretLevel();

	// SetText 은 값이 같아도 Slate 를 무효화하므로 바뀐 것만 넘긴다.
	if (IsValid(Txt_MonsterResource) && MonsterResource != LastMonsterResource)
	{
		Txt_MonsterResource->SetText(FText::FromString(
			FString::Printf(TEXT("MonsterResource: %d"), MonsterResource)));
	}
	if (IsValid(Txt_ArmorLevel) && ArmorLevel != LastArmorLevel)
	{
		Txt_ArmorLevel->SetText(FText::FromString(
			FString::Printf(TEXT("장갑  LV. %d/%d"), ArmorLevel, ATRGameState::MaxArmorLevel)));
	}
	if (IsValid(Txt_TurretLevel) && TurretLevel != LastTurretLevel)
	{
		Txt_TurretLevel->SetText(FText::FromString(
			FString::Printf(TEXT("포탑  LV. %d/%d"), TurretLevel, ATRGameState::MaxTurretLevel)));
	}

	LastMonsterResource = MonsterResource;
	LastArmorLevel = ArmorLevel;
	LastTurretLevel = TurretLevel;

	// 최대 레벨에 닿으면 서버가 거부하므로 미리 잠근다.
	if (IsValid(Btn_UpgradeArmor))
	{
		Btn_UpgradeArmor->SetIsEnabled(ArmorLevel < ATRGameState::MaxArmorLevel);
	}
	if (IsValid(Btn_UpgradeTurret))
	{
		Btn_UpgradeTurret->SetIsEnabled(TurretLevel < ATRGameState::MaxTurretLevel);
	}
}

void UTRControlConsoleWidget::RefreshSlots()
{
	// 진행 상태가 모두에게 복제되므로 같은 진행률이 보인다.
	const UTRActionComponent* BuildingComp = UTRActionComponent::FindBuildingComponent(GetWorld());
	const bool bBuilding = BuildingComp != nullptr;

	// 파츠 HP 가 복제되므로 파괴 여부는 클라이언트에서 그대로 판단할 수 있다.
	const ATRTrain* Train = GetTrain();

	const ATRGameState* GameState = GetWorld()->GetGameState<ATRGameState>();
	const int32 MonsterResource = IsValid(GameState) ? GameState->GetMonsterResource() : 0;

	const float BuildProgress01 = bBuilding ? BuildingComp->GetActionProgress01() : -1.f;
	const AActor* BuildTarget = bBuilding ? BuildingComp->GetCurrentActionTarget() : nullptr;

	for (const FConsoleSlot& ConsoleSlot : SlotButtons)
	{
		UTRConsoleButton* Button = ConsoleSlot.Button.Get();
		if (!IsValid(Button))
		{
			continue;
		}

		const ATRInstallablePart* Part = FindSlotPart(Train, ConsoleSlot);
		const bool bDestroyed = IsValid(Part) && Part->IsPartDestroyed();

		Button->SetIsEnabled(!bBuilding && bDestroyed && MonsterResource >= Part->GetInstallCost());

		if (Part == BuildTarget)
		{
			Button->SetProgress(BuildProgress01);
			Button->SetStateText(FText::FromString(
				FString::Printf(TEXT("%d%%"), FMath::FloorToInt(BuildProgress01 * 100.f))));
			continue;
		}

		Button->SetProgress(-1.f);
		Button->SetStateText(FText::FromString(
			!IsValid(Part) ? TEXT("비어 있음") : (bDestroyed ? TEXT("재설치") : TEXT("정상"))));
	}
}

ATRInstallablePart* UTRControlConsoleWidget::FindSlotPart(const ATRTrain* Train, const FConsoleSlot& ConsoleSlot) const
{
	if (!IsValid(Train))
	{
		return nullptr;
	}

	return ConsoleSlot.bArmor
		? StaticCast<ATRInstallablePart*>(Train->GetArmorAt(static_cast<ETRArmorSlot>(ConsoleSlot.SlotValue)))
		: StaticCast<ATRInstallablePart*>(Train->GetTurretAt(static_cast<ETRTurretSlot>(ConsoleSlot.SlotValue)));
}

void UTRControlConsoleWidget::BuildSlot(int32 SlotIndex)
{
	ATRTrain* Train = GetTrain();
	UTRActionComponent* ActionComp = GetActionComponent();
	if (!IsValid(Train) || !IsValid(ActionComp) || !SlotButtons.IsValidIndex(SlotIndex))
	{
		return;
	}

	ATRInstallablePart* Part = FindSlotPart(Train, SlotButtons[SlotIndex]);
	if (!IsValid(Part))
	{
		return;
	}

	// 자원 확인과 실제 설치는 서버가 다시 한다.
	ActionComp->ServerStartAction(ETRActionType::Build, Part);
}

void UTRControlConsoleWidget::HandleUpgradeClicked(bool bArmor)
{
	ATRPlayerCharacter* Character = GetOwnerCharacter();
	if (!IsValid(Character))
	{
		return;
	}

	if (bArmor)
	{
		Character->ServerUpgradeArmor();
	}
	else
	{
		Character->ServerUpgradeTurret();
	}
}

void UTRControlConsoleWidget::RequestClose()
{
	if (ATRPlayerCharacter* Character = GetOwnerCharacter())
	{
		Character->ServerReleaseControlConsole();
	}
}

ATRPlayerCharacter* UTRControlConsoleWidget::GetOwnerCharacter() const
{
	const APlayerController* PC = GetOwningPlayer();
	return IsValid(PC) ? Cast<ATRPlayerCharacter>(PC->GetPawn()) : nullptr;
}

UTRActionComponent* UTRControlConsoleWidget::GetActionComponent() const
{
	ATRPlayerCharacter* Character = GetOwnerCharacter();
	return IsValid(Character) ? Character->GetActionComponent() : nullptr;
}

ATRTrain* UTRControlConsoleWidget::GetTrain()
{
	if (!CachedTrain.IsValid())
	{
		CachedTrain = Cast<ATRTrain>(UGameplayStatics::GetActorOfClass(GetWorld(), ATRTrain::StaticClass()));
	}
	return CachedTrain.Get();
}
