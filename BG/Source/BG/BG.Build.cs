// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class BG : ModuleRules
{
	public BG(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput",
			// --- GAS (Team4Project 의 GAS 시스템과 동일 구성) ---
			"GameplayAbilities",   // UGameplayAbility / UGameplayEffect / UAttributeSet
			"GameplayTags",        // NativeGameplayTags (UE_DEFINE_GAMEPLAY_TAG)
			"GameplayTasks",        // UAbilityTask (응답 창 WaitForResolution 등)
			"UMG",
			//	버프/디버프 이펙트와 연출 이펙트. Public/ 헤더가 UNiagaraSystem(DT 행) 과
			//	UNiagaraComponent(부착 핸들)를 직접 들고 있어 Private 로는 부족하다.
			"Niagara",
			// --- 세션(방코드 온라인, M6) ---
			"OnlineSubsystem",      // IOnlineSessionPtr / FOnlineSessionSettings 를 헤더에서 사용
			"DeveloperSettings"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Slate", "SlateCore",
			"OnlineSubsystemUtils",  // Online::GetSubsystem 헬퍼 (.cpp 전용)
			"ApplicationCore",
			"LevelSequence",         // 엔딩 연출 — 승리 진영별 성 시퀀스를 각 클라가 재생한다
			"MovieScene",            // ULevelSequencePlayer 재생 설정
			"CommonUI"               // UCommonTextStyle — 코드로 만드는 텍스트의 기본 서체
		});

		//	⚠️ PublicIncludePaths.Add(ModuleDirectory) 를 다시 넣지 말 것.
		//	  그 한 줄이 있으면 Private/ 안의 헤더까지 include 가능해져 Public/Private 분리가
		//	  컴파일러 차원에서 무의미해진다. UBT 가 Public/ 과 Private/ 를 자동으로 include
		//	  경로에 넣으므로 "Core/BaamGameState.h" 같은 경로는 그 줄 없이도 해석된다.
		//	  예전에 Source/Baam/UI/ 가 Public/Private 없이 평평하게 존재할 수 있었던 것이
		//	  바로 이 줄 때문이었다.
	}
}
