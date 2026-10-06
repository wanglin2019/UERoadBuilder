// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interface/RoadEditorContext.h"
#include "UObject/Object.h"

#include "RoadToolsModeContextObject.generated.h"

class URoadToolsMode;

/**
 * Editor-side implementation of IRoadEditorContext.
 *
 * It is a plain UObject rather than part of the mode so that the capability the tools consume has a
 * lifetime and an identity of its own: the mode injects it through UContextObjectStore in Enter() and
 * removes it in Exit(), and a tool that outlives the mode simply stops finding it. The runtime host
 * supplies a sibling implementation with the same interface - that symmetry is the whole point of
 * routing host access through an interface instead of through FEdModeRoad.
 */
UCLASS()
class ROADBUILDERTOOLSEDITOR_API URoadToolsModeContextObject : public UObject, public IRoadEditorContext
{
	GENERATED_BODY()

public:
	/** Set by URoadToolsMode immediately after construction. */
	void SetOwnerMode(URoadToolsMode* InOwnerMode);

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
	/** The mode that owns this object; it supplies the world. */
	TWeakObjectPtr<URoadToolsMode> OwnerMode;
};
