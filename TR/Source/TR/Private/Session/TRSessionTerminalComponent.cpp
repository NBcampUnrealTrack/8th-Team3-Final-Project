#include "Session/TRSessionTerminalComponent.h"

UTRSessionTerminalComponent::UTRSessionTerminalComponent()
{
	// 표식일 뿐이라 틱이 필요 없다. 조준 판정은 플레이어 쪽 컴포넌트가 한다.
	PrimaryComponentTick.bCanEverTick = false;
}
