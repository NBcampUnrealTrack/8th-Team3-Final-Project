using UnrealBuildTool;
public class BGEndingTools : ModuleRules
{
    public BGEndingTools(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new [] {"Core", "CoreUObject", "Engine"});
        PrivateDependencyModuleNames.AddRange(new [] {"UnrealEd", "AssetRegistry", "GeometryCollectionEngine", "GeometryCollectionEditor", "Chaos", "ChaosSolverEngine", "ChaosCaching", "FractureEngine", "PlanarCut", "DataflowCore", "PhysicsCore"});
    }
}

