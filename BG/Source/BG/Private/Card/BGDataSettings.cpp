// Fill out your copyright notice in the Description page of Project Settings.


#include "Card/BGDataSettings.h"

#include "BGGameplayTags.h"

UBGDataSettings::UBGDataSettings()
{
	//	기본값을 코드에서 준다. 비워 두면 암살자가 대성공해도 아무 일도 일어나지 않고,
	//	설정을 안 건드린 사람은 그것이 버그인지 미구현인지 알 수 없다.
	//	(네이티브 태그는 모듈 로드 시 등록되므로 CDO 생성 시점에 이미 유효하다.)
	EphemeralAttackCardId = BG::Card::Id::ShadowStrike.GetTag();
}
