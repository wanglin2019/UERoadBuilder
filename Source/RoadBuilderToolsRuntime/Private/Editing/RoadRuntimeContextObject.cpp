// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

#include "Editing/RoadRuntimeContextObject.h"

#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "RoadActor.h"
#include "RoadLog.h"
#include "RoadScene.h"

void URoadRuntimeContextObject::SetEditingWorld(UWorld* InWorld)
{
	World = InWorld;
}

void URoadRuntimeContextObject::SetRebuildHandler(TFunction<void()> InHandler)
{
	RebuildHandler = MoveTemp(InHandler);
}

UWorld* URoadRuntimeContextObject::GetEditingWorld() const
{
	return World.Get();
}

ARoadScene* URoadRuntimeContextObject::GetRoadScene() const
{
	UWorld* EditingWorld = World.Get();
	if (EditingWorld == nullptr)
	{
		return nullptr;
	}

	// Resolved on every call rather than cached, for the same reason the editor object does it: a scene
	// added or destroyed mid-session, or a level change, then needs no cache invalidation. The lookup is
	// one pass over the level's actors, which is nothing next to the per-frame drawing that consumes it.
	return Cast<ARoadScene>(UGameplayStatics::GetActorOfClass(EditingWorld, ARoadScene::StaticClass()));
}

ARoadActor* URoadRuntimeContextObject::GetSelectedRoad() const
{
	return SelectedRoad.Get();
}

void URoadRuntimeContextObject::SetSelectedRoad(ARoadActor* Road)
{
	// Selecting a road clears the other two: the three tools that edit them are never active at once, so
	// a shared "what is selected" slot filtered by type is what keeps a road selection from also reading
	// as a junction one.
	SelectedRoad = Road;
	SelectedGround = nullptr;
	SelectedJunction = nullptr;
}

AGroundActor* URoadRuntimeContextObject::GetSelectedGround() const
{
	return SelectedGround.Get();
}

void URoadRuntimeContextObject::SetSelectedGround(AGroundActor* Ground)
{
	SelectedGround = Ground;
	SelectedRoad = nullptr;
	SelectedJunction = nullptr;
}

AJunctionActor* URoadRuntimeContextObject::GetSelectedJunction() const
{
	return SelectedJunction.Get();
}

void URoadRuntimeContextObject::SetSelectedJunction(AJunctionActor* Junction)
{
	SelectedJunction = Junction;
	SelectedRoad = nullptr;
	SelectedGround = nullptr;
}

void URoadRuntimeContextObject::RequestRedraw()
{
	// The tools draw their overlay through FRoadRuntimeRenderAPI into a line batch component, which is
	// re-published every frame the host renders. So there is no per-request repaint to trigger: the next
	// frame picks the change up. This is the one place the runtime host is genuinely simpler than the
	// editor, where Invalidate() had to be called explicitly because the editor does not redraw every
	// frame when idle.
}

void URoadRuntimeContextObject::RequestRebuild()
{
	// Expensive: this regenerates the whole road network's geometry, which is why tools defer it across a
	// drag rather than calling it per motion event. The host supplies the implementation because only it
	// knows how its world is built (spawned actors, a subsystem, a streaming cell).
	if (RebuildHandler)
	{
		RebuildHandler();
		return;
	}

	// No handler: fall back to the editor object's behaviour as closely as possible - rebuild the scene if
	// one is present. Worth a Debug line because a host that forgot the handler gets geometry that is
	// only wrong on a rebuild path, which is easy to misdiagnose.
	RoadLog_Debug(TEXT("RequestRebuild with no handler; rebuilding scene directly"));
	if (ARoadScene* Scene = GetRoadScene())
	{
		Scene->Rebuild();
	}
}

void URoadRuntimeContextObject::NotifyActiveToolChanged(FName ToolId, bool bActive)
{
	// This is the notification the editor object deliberately ignores, because FModeToolkit's
	// OnToolStarted/OnToolEnded already swaps the editor's details panel. The runtime host has no toolkit,
	// so this is where it learns to point its own property panel at the new tool - see the subsystem.
	//
	// Debug: fires twice per tool switch, which is fine at Verbose and lets a stuck panel be traced.
	RoadLog_Debug(TEXT("active tool changed id=%s active=%d"), *ToolId.ToString(), bActive ? 1 : 0);
}
