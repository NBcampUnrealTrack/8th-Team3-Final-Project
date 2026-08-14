// TRControlConsoleWidget.h
//
// 관제석 팝업 — 파괴된 파츠 재설치와 장갑/포탑 업그레이드.
// 여닫는 판단은 ATRPlayerHUD 가 한다. 이 위젯은 떠 있는 동안의 입력 상태만 책임진다.

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "TRControlConsoleWidget.generated.h"

class ATRInstallablePart;
class ATRTrain;
class ATRPlayerCharacter;
class UTRActionComponent;
class UTRConsoleButton;
class UCommonButtonBase;
class UTextBlock;

UCLASS(Abstract)
class TR_API UTRControlConsoleWidget : public UCommonUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_MonsterResource;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_ArmorLevel;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_TurretLevel;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCommonButtonBase> Btn_UpgradeArmor;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCommonButtonBase> Btn_UpgradeTurret;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCommonButtonBase> Btn_Close;

private:
	// 버튼 하나 = 열차 슬롯 하나.
	struct FConsoleSlot
	{
		TObjectPtr<UTRConsoleButton> Button;
		bool bArmor = false;
		uint8 SlotValue = 0;
	};

	void CollectSlotRow(const UEnum* SlotEnum, const TCHAR* NamePrefix, bool bArmor);

	void Refresh();
	void RefreshResourceAndLevels();
	void RefreshSlots();

	// 레벨에서 슬롯 할당이 누락되면 nullptr.
	ATRInstallablePart* FindSlotPart(const ATRTrain* Train, const FConsoleSlot& ConsoleSlot) const;

	// 버튼 클릭 — 어느 자리인지는 페이로드로 받는다.
	void BuildSlot(int32 SlotIndex);
	void HandleUpgradeClicked(bool bArmor);
	void RequestClose();

	void ApplyPopupInputMode();
	void RestoreGameInputMode();

	ATRPlayerCharacter* GetOwnerCharacter() const;
	UTRActionComponent* GetActionComponent() const;
	ATRTrain* GetTrain();

	TArray<FConsoleSlot> SlotButtons;

	// 이 클라이언트가 누른 자리. 진행률 바를 그 자리 그린다.
	TWeakObjectPtr<UTRConsoleButton> ActiveBuildButton;

	// 열차는 레벨에 하나뿐이므로 한 번 찾아 들고 있는다.
	TWeakObjectPtr<ATRTrain> CachedTrain;

	bool bPrevShowMouseCursor = false;

	bool bInputSuppressed = false;
};
