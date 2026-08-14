// TRControlConsole.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TRControlConsole.generated.h"

class APawn;

UCLASS()
class TR_API ATRControlConsole : public AActor
{
	GENERATED_BODY()

public:
	ATRControlConsole();

	// 관제석은 한 번에 한 명만 쓴다. 점유는 서버가 정하고 모두에게 복제된다.
	UFUNCTION(BlueprintPure, Category="ControlConsole")
	bool IsOccupied() const { return IsValid(Occupant); }

	UFUNCTION(BlueprintPure, Category="ControlConsole")
	bool IsOccupiedBy(const APawn* Pawn) const { return IsValid(Occupant) && Occupant == Pawn; }

	// 비어 있을 때만 넘겨준다. 서버 전용.
	bool TryOccupy(APawn* NewOccupant);

	// 점유자 본인만 놓을 수 있다. 서버 전용.
	void Release(const APawn* Pawn);

private:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(VisibleAnywhere, Category="ControlConsole")
	TObjectPtr<UStaticMeshComponent> ConsoleMeshComp;

	// 점유자가 사라지면 IsValid 가 거짓이 되어 자동으로 풀린다.
	UPROPERTY(Replicated)
	TObjectPtr<APawn> Occupant;
};
