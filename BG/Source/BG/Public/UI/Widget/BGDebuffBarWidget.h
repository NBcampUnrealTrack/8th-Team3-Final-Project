// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Core/BGPlayerState.h"

#include "BGDebuffBarWidget.generated.h"

/**
 * 
 */
UCLASS()
class BG_API UBGDebuffBarWidget : public UUserWidget
{
	GENERATED_BODY()
	
protected:
	virtual void NativeConstruct() override
	{
		Super::NativeConstruct();
		Handle = ABGPlayerState::OnAnySeatPublicInfoChanged.AddUObject(
			this, &UBGDebuffBarWidget::HandlePublicInfoChanged);
		Refresh();   // 위젯이 늦게 생기면 통지를 놓치므로 한 번 맞춰 둔다
	}
	virtual void NativeDestruct() override
	{
		ABGPlayerState::OnAnySeatPublicInfoChanged.Remove(Handle);
		Super::NativeDestruct();
	}

	void HandlePublicInfoChanged(int32 Seat)
	{
		const ABGPlayerState* PS = GetOwningPlayerState<ABGPlayerState>();
		if (PS && PS->GetSeatIndex() == Seat) Refresh();
	}

	void Refresh()
	{
		if (const ABGPlayerState* PS = GetOwningPlayerState<ABGPlayerState>())
			OnStatusesUpdated(PS->GetPendingStatuses());
	}

	// BP 에서 HorizontalBox 비우고 아이콘 다시 채우기
	UFUNCTION(BlueprintImplementableEvent)
	void OnStatusesUpdated(const TArray<FBGPendingStatus>& Statuses);

private:
	FDelegateHandle Handle;
};
