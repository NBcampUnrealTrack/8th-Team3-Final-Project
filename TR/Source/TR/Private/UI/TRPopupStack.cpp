// TRPopupStack.cpp

#include "UI/TRPopupStack.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"

UTRPopupStack* UTRPopupStack::Get(const APlayerController* PC)
{
	const ULocalPlayer* LocalPlayer = IsValid(PC) ? PC->GetLocalPlayer() : nullptr;
	return IsValid(LocalPlayer) ? LocalPlayer->GetSubsystem<UTRPopupStack>() : nullptr;
}
