// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TR : ModuleRules
{
	public TR(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput",
            "Niagara",
            "UMG",
            "Slate",
            "SlateCore",
            "CommonUI",
            "MassCore",
            "MassEntity",
            "MassCommon",
            "MassMovement",
            "MassSpawner",
            "MassRepresentation",
            "MassNavigation",
            "MassActors",     // 액터 표현 / 풀링 인터페이스
            "MassLOD",        // LOD 수집기 및 가변 틱
            "MassSignals",    // StateTree를 깨우는 신호
            "MassAIBehavior",
            "DeveloperSettings",   // 프로젝트 세팅에 노출되는 UTRVisualToneSettings
            "StateTreeModule"
			});

		// 세션(방 생성·검색·조인·친구 초대). OnlineSubsystemSteam 은 uproject 플러그인으로 켠다 —
		// 여기서는 어느 서브시스템이 붙든 같은 인터페이스만 쓴다.
		PrivateDependencyModuleNames.AddRange(new string[] {
			"OnlineSubsystem",
			"OnlineSubsystemUtils"
			});
	}
}
