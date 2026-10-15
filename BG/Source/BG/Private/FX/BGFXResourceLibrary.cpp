#include "FX/BGFXResourceLibrary.h"

#include "Async/TaskGraphInterfaces.h"
#include "Debug/BGCardLog.h"
#include "Materials/MaterialInterface.h"
#include "Misc/App.h"
#include "NiagaraMeshRendererProperties.h"
#include "NiagaraRendererProperties.h"
#include "NiagaraSystem.h"
#include "RenderingThread.h"

#if WITH_EDITOR
#include "StaticMeshCompiler.h"
#endif

UNiagaraSystem* BGFX::LoadSystemReady(const TSoftObjectPtr<UNiagaraSystem>& Effect)
{
    if (Effect.IsNull() || !FApp::CanEverRender() || IsRunningDedicatedServer()) return nullptr;

    UNiagaraSystem* System = Effect.LoadSynchronous();
    if (!System) return nullptr;

#if WITH_EDITOR
    // UObject loading finishes before Niagara/material/mesh background compilation does.
    System->WaitForCompilationComplete(true, false);
    TArray<UStaticMesh*> Meshes;
    for (const FNiagaraEmitterHandle& Emitter : System->GetEmitterHandles())
    {
        if (!Emitter.GetIsEnabled()) continue;
        Emitter.ForEachEnabledRendererWithIndex([&](const UNiagaraRendererProperties* Renderer, int32)
        {
            if (const UNiagaraMeshRendererProperties* MeshRenderer = Cast<UNiagaraMeshRendererProperties>(Renderer))
            {
                for (const FNiagaraMeshRendererMeshProperties& Entry : MeshRenderer->Meshes)
                {
                    if (Entry.Mesh) Meshes.AddUnique(Entry.Mesh);
                }
            }
        });
    }
    FStaticMeshCompilingManager::Get().FinishCompilation(Meshes);
#endif

    TArray<UMaterialInterface*> Materials;
    for (const FNiagaraEmitterHandle& Emitter : System->GetEmitterHandles())
    {
        if (!Emitter.GetIsEnabled()) continue;
        Emitter.ForEachEnabledRendererWithIndex([&](const UNiagaraRendererProperties* Renderer, int32)
        {
            TArray<UMaterialInterface*> RendererMaterials;
            Renderer->GetUsedMaterials(nullptr, RendererMaterials);
            for (UMaterialInterface* Material : RendererMaterials)
            {
                if (Material) Materials.AddUnique(Material);
            }
        });
    }
    for (UMaterialInterface* Material : Materials)
    {
        // In editor, an incomplete on-demand shader map can have IsCompiling() == false.
        // EnsureIsComplete submits those missing jobs as well as waiting for compilation.
        Material->EnsureIsComplete();
    }

    // Cooked shader bytecode still needs its graphics pipelines prepared on this machine.
    System->PrecacheAssetPSOs();
    const FGraphEventRef PSOsReady = System->GetPrecachePSOsEvent();
    if (PSOsReady.IsValid())
    {
        FTaskGraphInterface::Get().WaitUntilTaskCompletes(PSOsReady, ENamedThreads::GameThread);
    }
    FlushRenderingCommands();
    UE_LOG(LogBGCard, Verbose, TEXT("[FXReady] %s Materials=%d"), *System->GetPathName(), Materials.Num());
    return System;
}
