// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interface/RoadEditorContext.h"
#include "UObject/Object.h"

#include "RoadRuntimeContextObject.generated.h"

class ARoadActor;
class AGroundActor;
class AJunctionActor;
class ARoadScene;

/**
 * Game-side implementation of IRoadEditorContext.
 *
 * The sibling of URoadToolsModeContextObject (RoadBuilderToolsEditor). Both answer the same questions -
 * which world, which scene, what is selected, please redraw, please rebuild - but they get the answers
 * from completely different places: the editor object reads GEditor's actor selection and invalidates
 * level viewports, this one keeps its own selection and rebuilds immediately.
 *
 * That symmetry is the whole point of the interface. The 14 tools are written against IRoadEditorContext
 * and never learn which host is driving them, so a tool that works in the editor works in the game with
 * no second implementation. This file is the runtime half of that claim.
 *
 * Selection is stored rather than read from a global: a game has no editor selection set, and the road
 * tools need "the road being edited" as a stable, settable notion. SetSelectedRoad() is how a tool that
 * picks a road under the cursor tells the host what it picked.
 */
UCLASS()
class ROADBUILDERTOOLSRUNTIME_API URoadRuntimeContextObject : public UObject, public IRoadEditorContext
{
	GENERATED_BODY()

public:
	/** World this host edits. Set once by the subsystem that owns the context. */
	void SetEditingWorld(UWorld* InWorld);

	/**
	 * Called by the host after the user undoes or redoes, so the same rebuild path runs as for an edit.
	 * Set by the owning subsystem; optional.
	 */
	void SetRebuildHandler(TFunction<void()> InHandler);

	/** IRoadEditorContext implementation */
	virtual UWorld* GetEditingWorld() const override;
	virtual ARoadScene* GetRoadScene() const override;
	virtual ARoadActor* GetSelectedRoad() const override;
	virtual void SetSelectedRoad(ARoadActor* Road) override;
	virtual AGroundActor* GetSelectedGround() const override;
	virtual void SetSelectedGround(AGroundActor* Ground) override;
	virtual AJunctionActor* GetSelectedJunction() const override;
	virtual void SetSelectedJunction(AJunctionActor* Junction) override;
	virtual void RequestRedraw() override;
	virtual void RequestRebuild() override;
	virtual void NotifyActiveToolChanged(FName ToolId, bool bActive) override;

private:
	/** Weak: the world belongs to the game and outlives nothing here. */
	TWeakObjectPtr<UWorld> World;

	/**
	 * The road/ground/junction the user is editing. Weak, because a tool may destroy the actor it was
	 * editing (road chop, ground join) and the selection has to go invalid rather than dangle - which is
	 * exactly what TWeakObjectPtr gives, and what IRoadEditorContext's "or null" contract promises.
	 */
	TWeakObjectPtr<ARoadActor> SelectedRoad;
	TWeakObjectPtr<AGroundActor> SelectedGround;
	TWeakObjectPtr<AJunctionActor> SelectedJunction;

	/** Host-supplied rebuild callback. May be unset, in which case rebuild falls back to a redraw. */
	TFunction<void()> RebuildHandler;
};
