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
            "CommonUI"
			});

		// 세션(방 생성·검색·조인·친구 초대). OnlineSubsystemSteam 은 uproject 플러그인으로 켠다 —
		// 여기서는 어느 서브시스템이 붙든 같은 인터페이스만 쓴다.
		PrivateDependencyModuleNames.AddRange(new string[] {
			"OnlineSubsystem",
			"OnlineSubsystemUtils"
			});
	}
}
