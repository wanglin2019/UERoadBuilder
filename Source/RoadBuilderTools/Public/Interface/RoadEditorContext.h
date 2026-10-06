// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "RoadEditorContext.generated.h"

class AGroundActor;
class AJunctionActor;
class ARoadActor;
class ARoadScene;
class UWorld;

/**
 * The one thing the engine does not know about road editing: which road is being edited.
 *
 * Tools never talk to a host type directly. They ask for this interface, and each host supplies its own
 * implementation:
 *   - editor host : URoadToolsModeContextObject (RoadBuilderToolsEditor) - reads the editor's actor
 *                   selection and requests a level viewport redraw.
 *   - runtime host: game-side implementation (RoadBuilderToolsRuntime, S4) - reads the game's own
 *                   selection state and rebuilds immediately.
 *
 * Injection uses the official ITF slot, UContextObjectStore, reached from a tool as
 * GetToolManager()->GetContextObjectStore(). The store lives on the UInteractiveToolsContext and is
 * already created by the time any tool is built, so a host only has to AddContextObject() before
 * registering its tools.
 *
 * Keeping this an interface (not a concrete class) is what lets one tool implementation run in both
 * hosts: the tool layer compiles without a single editor or game-host header.
 */
UINTERFACE(MinimalAPI)
class URoadEditorContext : public UInterface
{
	GENERATED_BODY()
};

class ROADBUILDERTOOLS_API IRoadEditorContext
{
	GENERATED_BODY()

public:
	/** World the host is editing. Tools use it to raycast; may be null on a host with no world yet. */
	virtual UWorld* GetEditingWorld() const = 0;

	/**
	 * Road network of the edited world, or null.
	 *
	 * Added in S0.5, on evidence: the first real tool was written without it and immediately needed it
	 * - every road tool either enumerates roads (to pick one) or highlights the whole network while one
	 * road is being edited. The scene is a world actor, so both hosts can derive it the same way the
	 * legacy mode did, but leaving that lookup to the tools would spread host policy across 14 classes.
	 */
	virtual ARoadScene* GetRoadScene() const = 0;

	/** Road the host currently treats as selected, or null. */
	virtual ARoadActor* GetSelectedRoad() const = 0;

	/** Ask the host to make this road the selected one. */
	virtual void SetSelectedRoad(ARoadActor* Road) = 0;

	/** Ground the host currently treats as selected, or null. */
	virtual AGroundActor* GetSelectedGround() const = 0;

	/** Ask the host to make this ground the selected one. */
	virtual void SetSelectedGround(AGroundActor* Ground) = 0;

	/** Junction the host currently treats as selected, or null. */
	virtual AJunctionActor* GetSelectedJunction() const = 0;

	/** Ask the host to make this junction the selected one. */
	virtual void SetSelectedJunction(AJunctionActor* Junction) = 0;

	/**
	 * Tell the host that the *view* is stale and needs presenting again - cheap, safe to call per frame.
	 *
	 * Split out from RequestRebuild() in S0.5, because the two turned out to be genuinely different
	 * operations and merging them made a tool that only draws pay for a geometry rebuild. This is the
	 * legacy FEditorViewportClient::Invalidate(); RequestRebuild() is the legacy Scene->Rebuild().
	 */
	virtual void RequestRedraw() = 0;

	/**
	 * Tell the host that road *data* changed and the generated geometry is stale. The host regenerates
	 * and then redraws, so this is expensive and must not be called per frame - tools defer it across an
	 * interactive drag.
	 */
	virtual void RequestRebuild() = 0;

	/**
	 * Announce that the active tool changed, so the host can switch its property panel to the new
	 * tool's settings. The editor host does not need this (its toolkit hooks OnToolStarted/OnToolEnded
	 * itself); the runtime host uses it to swap its own panel.
	 */
	virtual void NotifyActiveToolChanged(FName ToolId, bool bActive) = 0;
};
