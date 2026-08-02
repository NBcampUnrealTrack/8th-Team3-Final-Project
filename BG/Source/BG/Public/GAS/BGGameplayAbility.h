// BANG! 어빌리티 베이스 — 턴제라 예측을 쓰지 않는다 (md §1.2).
//   NetExecutionPolicy = ServerOnly, InstancingPolicy = InstancedPerActor.
// 카드 메커니즘 GA(GA_Bang/GA_Heal/...)는 전부 이걸 상속한다.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "BGGameplayAbility.generated.h"

UCLASS(Abstract)
class BG_API UBGGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UBGGameplayAbility();

	/**
	 * 액터(폰)에서 BG PlayerState 를 얻는다.
	 *
	 * 여러 GA 가 같이 쓰므로 베이스에 둔다. 각 .cpp 의 익명 네임스페이스에 같은 이름으로
	 * 두면 유니티 빌드가 파일을 한 TU 로 합칠 때 중복 정의가 된다.
	 */
	static class ABGPlayerState* GetBGPlayerState(AActor* Actor);
};
