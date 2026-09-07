// BANG! 조준 하이라이트 — 캐릭터 메시에 스텐실 값을 찍는다.
// 실제 외곽선은 그 값을 읽는 포스트프로세스 머티리얼이 그린다. 코드는 값만 정한다.

#pragma once

#include "CoreMinimal.h"

class ABGCharacter;

namespace BGTargetHighlight
{
	/** 스텐실 값. 포스트프로세스 머티리얼이 이 숫자로 색을 가른다. */
	enum class EState : uint8
	{
		None       = 0,   // 하이라이트 없음
		Selectable = 1,   // 고를 수 있는 대상
		Aimed      = 2,   // 지금 커서가 올라간 대상
		CurrentTurn = 3   // 지금 차례인 캐릭터. 내 차례일 때는 찍지 않는다 —
		                  // 조준(Selectable/Aimed)은 항상 내 차례에만 켜지므로 겹칠 일이 없다.
	};

	/** 이 캐릭터의 메시에 상태를 적용한다. None 이면 CustomDepth 자체를 끈다. */
	BG_API void Apply(ABGCharacter* Character, EState State);
}
