#include "Debug/BGCardLog.h"

#if !UE_BUILD_SHIPPING
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "MaterialShared.h"
#include "Materials/MaterialInterface.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "RenderingThread.h"
#include "UObject/UObjectIterator.h"

namespace
{
FAutoConsoleCommandWithWorldAndArgs PrepareFXCommand(
    TEXT("BG.FXPrepare"), TEXT("Complete active FX material resources. Optional system name filter."),
    FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
    {
        for (TObjectIterator<UNiagaraComponent> It; It; ++It)
        {
            UNiagaraComponent* Component = *It;
            UNiagaraSystem* System = Component->GetAsset();
            if (Component->GetWorld() != World || !System || (!Args.IsEmpty() && !System->GetName().Contains(Args[0]))) continue;
#if WITH_EDITOR
            System->WaitForCompilationComplete(true, false);
#endif
            TArray<UMaterialInterface*> Materials;
            Component->GetUsedMaterials(Materials);
            for (UMaterialInterface* Material : Materials)
            {
                if (!Material) continue;
                Material->EnsureIsComplete();
                const FMaterialResource* Resource = Material->GetMaterialResource(GMaxRHIShaderPlatform);
                UE_LOG(LogBGCard, Display, TEXT("[FXPrepare] Material=%s Complete=%d"),
                    *Material->GetPathName(), Resource && Resource->IsGameThreadShaderMapComplete());
            }
            FlushRenderingCommands();
            Component->MarkRenderStateDirty();
        }
    }));

FAutoConsoleCommandWithWorldAndArgs InspectFXCommand(
    TEXT("BG.FXInspect"), TEXT("Inspect active FX materials and textures. Optional system name filter."),
    FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
    {
        for (TObjectIterator<UNiagaraComponent> It; It; ++It)
        {
            UNiagaraComponent* Component = *It;
            UNiagaraSystem* System = Component->GetAsset();
            if (Component->GetWorld() != World || !System || (!Args.IsEmpty() && !System->GetName().Contains(Args[0]))) continue;
            TArray<UMaterialInterface*> Materials;
            Component->GetUsedMaterials(Materials);
            UE_LOG(LogBGCard, Display, TEXT("[FXInspect] %s Active=%d Ready=%d Materials=%d"),
                *Component->GetPathName(), Component->IsActive(), System->IsReadyToRun(), Materials.Num());
            for (UMaterialInterface* Material : Materials)
            {
                if (!Material) continue;
                const FMaterialResource* Resource = Material->GetMaterialResource(GMaxRHIShaderPlatform);
                UE_LOG(LogBGCard, Display, TEXT("[FXInspect] Material=%s Compiling=%d Complete=%d Resource=%d"),
                    *Material->GetPathName(), Material->IsCompiling(), Resource && Resource->IsGameThreadShaderMapComplete(), Resource != nullptr);
                TArray<UTexture*> Textures;
                Material->GetUsedTextures(Textures);
                for (UTexture* Texture : Textures)
                {
                    UTexture2D* Texture2D = Cast<UTexture2D>(Texture);
                    UE_LOG(LogBGCard, Display, TEXT("[FXInspect] Texture=%s Resource=%d FullyStreamed=%d Mips=%d/%d"),
                        *Texture->GetPathName(), Texture->GetResource() != nullptr, Texture->IsFullyStreamedIn(),
                        Texture2D ? Texture2D->GetNumResidentMips() : -1, Texture2D ? Texture2D->GetNumMips() : -1);
                }
            }
        }
    }));
}
#endif
