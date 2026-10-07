// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

#include "RoadToolsModeContextObject.h"

#include "Editor.h"
#include "Kismet/GameplayStatics.h"
#include "RoadActor.h"
#include "RoadScene.h"
#include "RoadToolsMode.h"

void URoadToolsModeContextObject::SetOwnerMode(URoadToolsMode* InOwnerMode)
{
	OwnerMode = InOwnerMode;
}

UWorld* URoadToolsModeContextObject::GetEditingWorld() const
{
	URoadToolsMode* RoadToolsMode = OwnerMode.Get();
	return RoadToolsMode != nullptr ? RoadToolsMode->GetWorld() : nullptr;
}

ARoadScene* URoadToolsModeContextObject::GetRoadScene() const
{
	UWorld* World = GetEditingWorld();
	if (World == nullptr)
	{
		return nullptr;
	}

	// A pure query: finding the scene is this method's job, creating it is URoadToolsMode::Enter()'s -
	// the tool layer must not spawn actors. Resolving it on every call rather than caching it keeps a
	// level switch, or a scene added or deleted mid-session, from needing any cache invalidation; the
	// lookup is one pass over the level's actor list, which is nothing next to the per-frame drawing the
	// tools already do with the result.
	return Cast<ARoadScene>(UGameplayStatics::GetActorOfClass(World, ARoadScene::StaticClass()));
}

ARoadActor* URoadToolsModeContextObject::GetSelectedRoad() const
{
	// The selection is the mode's own, mirroring what the legacy FEdModeRoad did with its SelectedRoad
	// member: the tools drive it, and the level editor's actor selection is not consulted - a click in
	// the outliner must not silently change what a tool is editing.
	URoadToolsMode* RoadToolsMode = OwnerMode.Get();
	return RoadToolsMode != nullptr ? RoadToolsMode->GetSelectedRoad() : nullptr;
}

void URoadToolsModeContextObject::SetSelectedRoad(ARoadActor* Road)
{
	if (URoadToolsMode* RoadToolsMode = OwnerMode.Get())
	{
		RoadToolsMode->SetSelectedRoad(Road);
	}
}

AGroundActor* URoadToolsModeContextObject::GetSelectedGround() const
{
	URoadToolsMode* RoadToolsMode = OwnerMode.Get();
	return RoadToolsMode != nullptr ? RoadToolsMode->GetSelectedGround() : nullptr;
}

void URoadToolsModeContextObject::SetSelectedGround(AGroundActor* Ground)
{
	if (URoadToolsMode* RoadToolsMode = OwnerMode.Get())
	{
		RoadToolsMode->SetSelectedGround(Ground);
	}
}

AJunctionActor* URoadToolsModeContextObject::GetSelectedJunction() const
{
	URoadToolsMode* RoadToolsMode = OwnerMode.Get();
	return RoadToolsMode != nullptr ? RoadToolsMode->GetSelectedJunction() : nullptr;
}

void URoadToolsModeContextObject::SetSelectedJunction(AJunctionActor* Junction)
{
	if (URoadToolsMode* RoadToolsMode = OwnerMode.Get())
	{
		RoadToolsMode->SetSelectedJunction(Junction);
	}
}

void URoadToolsModeContextObject::RequestRedraw()
{
	// The tools draw their overlay inside the viewport's draw pass, so asking for a repaint is all that
	// is needed for a selection highlight or a preview to appear.
	if (GEditor != nullptr)
	{
		GEditor->RedrawLevelEditingViewports();
	}
}

void URoadToolsModeContextObject::RequestRebuild()
{
	// S0.5 turned this from a redraw into a real rebuild: the tools now mutate road data, so the
	// generated geometry has to be brought back in step before the viewports are redrawn. This is the
	// same pair of calls the legacy tools made after an edit (Scene->Rebuild() then an invalidate).
	//
	// It is expensive, so tools defer it across an interactive drag and call it once the drag ends;
	// URoadTool_RoadHeight shows the pattern.
	if (ARoadScene* Scene = GetRoadScene())
	{
		Scene->Rebuild();
	}

	RequestRedraw();
}

void URoadToolsModeContextObject::NotifyActiveToolChanged(FName ToolId, bool bActive)
{
	// Intentionally empty on the editor side. FModeToolkit::OnToolStarted/OnToolEnded already switches
	// the details panel to the active tool's property sets, and UEdMode subscribes those hooks itself.
	// The runtime host - which has no toolkit - is the host that actually needs this notification.
}
