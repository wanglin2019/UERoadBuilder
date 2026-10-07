// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Tools/LegacyEdModeInterfaces.h"
#include "Tools/UEdMode.h"

#include "RoadToolsMode.generated.h"

class ARoadScene;
class ARoadActor;
class AGroundActor;
class AJunctionActor;
class FEditorViewportClient;
class HHitProxy;
class URoadInteractiveTool;
class URoadToolsModeContextObject;
struct FViewportClick;

/**
 * Road editing mode on the new editor-mode framework (UEdMode + InteractiveToolsFramework).
 *
 * This is the editor-side host of the shared tool layer. It owns only what is genuinely
 * editor-specific: mode registration, the toolkit, the editor implementation of IRoadEditorContext and
 * the palette-to-tool mapping. The tools themselves live in RoadBuilderTools and stay host-agnostic,
 * which is what allows the same implementations to be driven by the runtime host later.
 *
 * Contrast with the legacy FEdModeRoad, which had to hand-build its details panels and hand-order the
 * tool array. Here the framework supplies both: UEdMode::Enter() creates the tools contexts and calls
 * the two-argument toolkit Init() - the same call that makes the built-in details views work - and tool
 * identity is a string, so registering tools in a different order changes nothing.
 *
 * The second base is what lets the right mouse button reach a tool at all. A tool's click behaviour has
 * to claim the press to see the release, and a claimed button is taken away from the viewport for the
 * whole drag - which costs the editor right-drag orbit, the flight camera and WASD for as long as a tool
 * is active. The right button is therefore left unclaimed and arrives through the legacy click path
 * instead: HandleClick() below is that path, and it only fires for a press that did not become a drag,
 * so a right-drag keeps going to the camera. See HandleClick() for the mechanics.
 */
UCLASS()
class URoadToolsMode : public UEdMode, public ILegacyEdModeViewportInterface
{
	GENERATED_BODY()

public:
	/** Mode id, distinct from the legacy EM_Road so both modes can coexist in the mode dropdown. */
	static const FEditorModeID EM_RoadToolsModeId;

	URoadToolsMode();

	/** UEdMode implementation */
	virtual void Enter() override;
	virtual void Exit() override;
	virtual void CreateToolkit() override;
	virtual TMap<FName, TArray<TSharedPtr<FUICommandInfo>>> GetModeCommands() const override;

	/**
	 * Recomputes generated geometry after an undo or redo.
	 *
	 * The undo record restores the road data - the alignment arrays and the scene's road list - but not
	 * anything derived from it, because the derived geometry is not in the snapshot. Undoing a click that
	 * gave a road its second point therefore leaves the old two-point curve on screen over a one-point
	 * road, and undoing the last point leaves a road drawn along a curve it no longer has. Refitting and
	 * rebuilding is what puts the two back in step.
	 *
	 * Called by FEditorModeTools::PostUndo, which is itself an FEditorUndoClient, so this fires for redo
	 * through the same entry point and only while this mode is active.
	 */
	virtual void PostUndo() override;

	/**
	 * Legacy viewport click entry point - the right button's way into a tool.
	 *
	 * The left button arrives through the tool's own input behaviour instead. The two paths are mutually
	 * exclusive rather than redundant: a press the tools context claims makes the viewport return before
	 * it ever reaches here, and a press it does not claim is the only one that gets this far. Delivering
	 * both buttons here would give the left button a second, competing path.
	 */
	virtual bool HandleClick(FEditorViewportClient* InViewportClient, HHitProxy* HitProxy,
		const FViewportClick& Click) override;

	/** DIAGNOSTIC (remove when the click path is confirmed): reports which tool actually became active. */
	virtual void OnToolStarted(UInteractiveToolManager* Manager, UInteractiveTool* Tool) override;
	virtual void OnToolEnded(UInteractiveToolManager* Manager, UInteractiveTool* Tool) override;

	/** The host capability object handed to the tools through UContextObjectStore. */
	URoadToolsModeContextObject* GetRoadEditorContextObject() const { return ContextObject; }

	/**
	 * The tool a palette should start on, as a registered tool identity (RoadToolIds).
	 *
	 * Null for a palette that owes no tool, which today means a name that is not one of the palettes.
	 * Returns the identity rather than selecting anything so the caller decides when the switch happens;
	 * the toolkit does it from OnToolPaletteChanged, where the mode is already being handed a palette.
	 */
	static const TCHAR* GetDefaultToolForPalette(FName PaletteName);

	/**
	 * Starts one of this mode's tools by its registered identity.
	 *
	 * The mode owns the tool manager, so this is the mode's own way of doing what the palette buttons do
	 * through the framework. SelectActiveToolType() returns false for an identity that was never
	 * registered, which is reported rather than swallowed - a palette that silently starts nothing looks
	 * identical to one that works until the user tries to use it.
	 */
	void SelectActiveTool(const TCHAR* ToolId);

	/**
	 * The road / ground / junction being edited, owned by the mode itself.
	 *
	 * The legacy FEdModeRoad carried the same three members (SelectedRoad / SelectedGround /
	 * SelectedJunction) and its tools drove them directly. Owning them here - rather than borrowing the
	 * level editor's actor selection - keeps a click in the outliner from silently changing what a tool
	 * is editing, and keeps a tool's pick from fighting the user's own actor selection. The context
	 * object delegates to these, so the tools still see one IRoadEditorContext. Weak pointers, because
	 * an undo can destroy the very actor a tool has selected.
	 */
	ARoadActor* GetSelectedRoad() const { return SelectedRoad.Get(); }
	void SetSelectedRoad(ARoadActor* Road) { SelectedRoad = Road; }
	AGroundActor* GetSelectedGround() const { return SelectedGround.Get(); }
	void SetSelectedGround(AGroundActor* Ground) { SelectedGround = Ground; }
	AJunctionActor* GetSelectedJunction() const { return SelectedJunction.Get(); }
	void SetSelectedJunction(AJunctionActor* Junction) { SelectedJunction = Junction; }

private:
	/** The road scene of the edited world, or null. Resolved rather than cached; see Enter(). */
	ARoadScene* FindRoadScene() const;

	/** The active tool, when it is one of ours. Null for no tool, or an active tool from another host. */
	URoadInteractiveTool* GetActiveRoadTool() const;

	/** Created in Enter(), released in Exit(). Owned by the mode so its lifetime matches the editing session. */
	UPROPERTY(Transient)
	TObjectPtr<URoadToolsModeContextObject> ContextObject;

	/** The editing selection; see GetSelectedRoad(). Cleared in Exit(). */
	TWeakObjectPtr<ARoadActor> SelectedRoad;
	TWeakObjectPtr<AGroundActor> SelectedGround;
	TWeakObjectPtr<AJunctionActor> SelectedJunction;
};
