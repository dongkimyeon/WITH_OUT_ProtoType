#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "ExportLevelObstaclesCommandlet.generated.h"

// Headless export of a level's blocking (Pawn-collision) actors as 2D
// (X/Y) axis-aligned bounding boxes, for the server's approximate wall
// avoidance (see WOP_SERVER's LevelObstacles.h/.cpp). The server has no
// engine/NavMesh access at all, so this is the one-time bridge between "the
// level's actual layout" and "a plain text file the C++ server can load".
//
// Usage (no editor UI needed -- run from the command line):
//   UnrealEditor-Cmd.exe ProtoProject.uproject -run=ExportLevelObstacles
//     -Map=/Game/Maps/L_Stage2 -Out=C:\Proj\WITH_OUT_ProtoType_SERVER\WOP_SERVER\Data\L_Stage2_obstacles.txt
//
// Output format: one obstacle per line, "minX minY maxX maxY" (Unreal
// world units / cm), plain ASCII, no header. Re-run and re-copy to the
// server whenever L_Stage2's blocking geometry changes -- this is a
// snapshot, not something that stays in sync automatically.
UCLASS()
class UExportLevelObstaclesCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	virtual int32 Main(const FString& Params) override;
};
