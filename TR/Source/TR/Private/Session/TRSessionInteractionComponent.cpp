#include "Session/TRSessionInteractionComponent.h"

#include "Interaction/TRInteractionComponent.h"
#include "Session/TRNetLog.h"
#include "Session/TRSessionTerminalComponent.h"
#include "UI/Session/TRSessionMenuController.h"

#include "Components/InputComponent.h"
#include "EnhancedInputComponent.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

UTRSessionInteractionComponent::UTRSessionInteractionComponent()
{
	// 조준 대상은 통지가 없어 폴링을 피할 수 없다. 다만 매 프레임은 아니다.
	PrimaryComponentTick.bCanEverTick = true;
}

APlayerController* UTRSessionInteractionComponent::GetOwningController() const
{
	AActor* Owner = GetOwner();
	if (APlayerController* PC = Cast<APlayerController>(Owner))
	{
		return PC;
	}

	const APawn* Pawn = Cast<APawn>(Owner);
	return Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
}

void UTRSessionInteractionComponent::BeginPlay()
{
	Super::BeginPlay();

	// 보는 사람 화면에만 있는 것이다. 서버가 남의 폰에 대해 돌릴 이유가 없다.
	const APlayerController* PC = GetOwningController();
	if (!PC || !PC->IsLocalController())
	{
		SetComponentTickEnabled(false);
		return;
	}

	TryBindInput();
}

void UTRSessionInteractionComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// 컨트롤러가 폰보다 늦게 붙을 수 있다. 걸릴 때까지 다시 시도한다.
	if (!bInputBound)
	{
		TryBindInput();
	}

	TimeSinceFocusCheck += DeltaTime;
	if (FocusCheckInterval > 0.f && TimeSinceFocusCheck < FocusCheckInterval)
	{
		return;
	}
	TimeSinceFocusCheck = 0.f;

	UpdateFocus();
}

bool UTRSessionInteractionComponent::TryBindInput()
{
	APlayerController* PC = GetOwningController();
	if (!PC || !PC->InputComponent)
	{
		return false;
	}

	// 조작 키는 Enhanced Input 이라 인풋 컴포넌트 종류가 맞아야 건다.
	if (IA_UseTerminal)
	{
		if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PC->InputComponent))
		{
			EIC->BindAction(IA_UseTerminal, ETriggerEvent::Started, this,
				&UTRSessionInteractionComponent::TryOpenFromTerminal);
		}
		else
		{
			UE_LOG(LogTRNet, Warning,
				TEXT("[SessionTerminal] EnhancedInputComponent 가 아니라 IA_UseTerminal 을 걸지 못했다"));
		}
	}

	bInputBound = true;
	return true;
}

void UTRSessionInteractionComponent::UpdateFocus()
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn)
	{
		const APlayerController* PC = GetOwningController();
		Pawn = PC ? PC->GetPawn() : nullptr;
	}

	// 조준 판정은 기존 컴포넌트 것을 그대로 쓴다. 트레이스를 또 쏘지 않는다.
	const UTRInteractionComponent* Interaction =
		Pawn ? Pawn->FindComponentByClass<UTRInteractionComponent>() : nullptr;

	AActor* Target = Interaction ? Interaction->GetCurrentTarget() : nullptr;

	UTRSessionTerminalComponent* Terminal =
		Target ? Target->FindComponentByClass<UTRSessionTerminalComponent>() : nullptr;

	// 꺼 둔 단말은 없는 것으로 친다.
	if (Terminal && !Terminal->CanUse())
	{
		Terminal = nullptr;
	}

	if (FocusedTerminal.Get() == Terminal)
	{
		return;
	}

	FocusedTerminal = Terminal;
	OnTerminalFocused.Broadcast(Terminal);
}

void UTRSessionInteractionComponent::TryOpenFromTerminal()
{
	// 단말을 보고 있지 않으면 조작 키는 기존 상호작용의 몫이다. 여기서는 넘긴다.
	UTRSessionTerminalComponent* Terminal = FocusedTerminal.Get();
	if (!Terminal || !Terminal->CanUse())
	{
		return;
	}

	UGameInstance* GI = GetOwner() ? GetOwner()->GetGameInstance() : nullptr;
	UTRSessionMenuController* Controller = GI ? GI->GetSubsystem<UTRSessionMenuController>() : nullptr;
	if (!ensure(Controller))
	{
		return;
	}

	UE_LOG(LogTRNet, Log, TEXT("[SessionTerminal] %s 조작 — 메뉴 열기"), *GetOwner()->GetName());
	Controller->Open();
}
