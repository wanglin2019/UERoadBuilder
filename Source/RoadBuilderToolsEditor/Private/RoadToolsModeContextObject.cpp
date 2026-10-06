// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

#include "RoadToolsModeContextObject.h"

#include "Editor.h"
#include "Engine/Selection.h"
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
	if (GEditor == nullptr)
	{
		return nullptr;
	}

	// S0 reads the level editor's actor selection. S1 moves the notion of "the road being edited" onto
	// the mode itself, mirroring what the legacy FEdModeRoad did with its own SelectedRoad member -
	// the tools then drive that instead of the editor selection.
	for (FSelectionIterator It(GEditor->GetSelectedActorIterator()); It; ++It)
	{
		if (ARoadActor* Road = Cast<ARoadActor>(*It))
		{
			return Road;
		}
	}

	return nullptr;
}

void URoadToolsModeContextObject::SetSelectedRoad(ARoadActor* Road)
{
	if (GEditor == nullptr)
	{
		return;
	}

	// Null is a meaningful request ("nothing is selected"): the tool layer clears its own selection when
	// a click lands on empty space, and the editor's selection has to follow it there.
	// Skip the notify while clearing so the change is reported once, for the new actor.
	GEditor->SelectNone(false, true, false);
	if (Road != nullptr)
	{
		GEditor->SelectActor(Road, true, true, true);
	}
}

AGroundActor* URoadToolsModeContextObject::GetSelectedGround() const
{
	if (GEditor == nullptr)
	{
		return nullptr;
	}

	for (FSelectionIterator It(GEditor->GetSelectedActorIterator()); It; ++It)
	{
		if (AGroundActor* Ground = Cast<AGroundActor>(*It))
		{
			return Ground;
		}
	}

	return nullptr;
}

void URoadToolsModeContextObject::SetSelectedGround(AGroundActor* Ground)
{
	if (GEditor == nullptr)
	{
		return;
	}

	// Grounds and roads are edited by different tools, never at once, so the shared actor selection can
	// carry either one. The selection is filtered by type on the way back out, which is what keeps a
	// ground selection from reading as a road one.
	GEditor->SelectNone(false, true, false);
	if (Ground != nullptr)
	{
		GEditor->SelectActor(Ground, true, true, true);
	}
}

AJunctionActor* URoadToolsModeContextObject::GetSelectedJunction() const
{
	if (GEditor == nullptr)
	{
		return nullptr;
	}

	for (FSelectionIterator It(GEditor->GetSelectedActorIterator()); It; ++It)
	{
		if (AJunctionActor* Junction = Cast<AJunctionActor>(*It))
		{
			return Junction;
		}
	}

	return nullptr;
}

void URoadToolsModeContextObject::SetSelectedJunction(AJunctionActor* Junction)
{
	if (GEditor == nullptr)
	{
		return;
	}

	GEditor->SelectNone(false, true, false);
	if (Junction != nullptr)
	{
		GEditor->SelectActor(Junction, true, true, true);
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
