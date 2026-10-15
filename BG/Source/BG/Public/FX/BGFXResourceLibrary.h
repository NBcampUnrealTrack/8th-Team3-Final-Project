#pragma once

#include "CoreMinimal.h"

class UNiagaraSystem;

namespace BGFX
{
    /** Load the asset and finish its render prerequisites before creating a Niagara component. */
    BG_API UNiagaraSystem* LoadSystemReady(const TSoftObjectPtr<UNiagaraSystem>& Effect);
}
