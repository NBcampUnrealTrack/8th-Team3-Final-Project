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
            "StateTreeModule"
			});

		PrivateDependencyModuleNames.AddRange(new string[] {  });

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
