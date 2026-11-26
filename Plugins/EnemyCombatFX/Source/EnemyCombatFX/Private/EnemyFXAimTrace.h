#pragma once

#include "Engine/World.h"

namespace EnemyFXAimTrace
{
    inline bool Trace(UWorld* World, FHitResult& Hit, const FVector& Start,
        const FVector& End, const FCollisionQueryParams& Params)
    {
        // Ragdolls ignore Visibility by default. Query all object types so that
        // physics bodies are selectable while walls and other actors still occlude.
        return World && World->LineTraceSingleByObjectType(Hit, Start, End,
            FCollisionObjectQueryParams(FCollisionObjectQueryParams::AllObjects), Params);
    }
}
