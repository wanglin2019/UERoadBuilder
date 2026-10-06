// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

#include "RoadToolsMode.h"

#include "BaseGizmos/TransformGizmoUtil.h"
#include "ContextObjectStore.h"
#include "EdModeInteractiveToolsContext.h"
#include "Editor.h"
#include "EditorViewportClient.h"
#include "Engine/World.h"
#include "InteractiveTool.h"
#include "InteractiveToolManager.h"
#include "Kismet/GameplayStatics.h"
#include "RoadActor.h"
#include "RoadBuilderTools.h"
#include "RoadLog.h"
#include "RoadScene.h"
#include "RoadToolsModeCommands.h"
#include "RoadToolsModeContextObject.h"
#include "RoadToolsModeToolkit.h"
#include "Tools/RoadInteractiveTool.h"
#include "Tools/RoadToolIdentity.h"
#include "Tools/RoadTool_File.h"
#include "Tools/RoadTool_GroundEdit.h"
#include "Tools/RoadTool_JunctionLink.h"
#include "Tools/RoadTool_LaneCarve.h"
#include "Tools/RoadTool_LaneEdit.h"
#include "Tools/RoadTool_LaneWidth.h"
#include "Tools/RoadTool_MarkingCurve.h"
#include "Tools/RoadTool_MarkingLane.h"
#include "Tools/RoadTool_MarkingPoint.h"
#include "Tools/RoadTool_RoadChop.h"
#include "Tools/RoadTool_RoadHeight.h"
#include "Tools/RoadTool_RoadPlan.h"
#include "Tools/RoadTool_RoadSplit.h"
#include "Tools/RoadTool_Settings.h"

#define LOCTEXT_NAMESPACE "RoadToolsMode"

const FEditorModeID URoadToolsMode::EM_RoadToolsModeId = TEXT("EM_RoadTools");

URoadToolsMode::URoadToolsMode()
{
	// Assigned here, in the constructor: UAssetEditorSubsystem reads it off the class default object
	// when it registers the mode at PostEngineInit.
	//
	// The display name sits next to the legacy mode, which the engine shows as "Road". Drop the
	// "(New)" marker once the legacy mode is deleted.
	Info = FEditorModeInfo(URoadToolsMode::EM_RoadToolsModeId,
		LOCTEXT("ModeName", "Road (New)"),
		FSlateIcon(),
		true);
}

void URoadToolsMode::Enter()
{
	// Base Enter() creates the InteractiveToolsContexts - and with them the context object store - and
	// the toolkit, so everything registered below depends on it having run first.
	UEdMode::Enter();

	UInteractiveToolManager* ToolManager = GetToolManager();
	if (ToolManager == nullptr)
	{
		// Warning: the base Enter() is supposed to create the tool manager, so this is a host failure
		// rather than a user error.
		RoadLog_Warn(TEXT("Enter ABORTED: no tool manager"));
		return;
	}

	// Host capabilities before tools: a tool looks the context object up while it is being built.
	ContextObject = NewObject<URoadToolsModeContextObject>(this);
	ContextObject->SetOwnerMode(this);

	bool bContextReachable = false;
	UContextObjectStore* ContextStore = ToolManager->GetContextObjectStore();
	if (ContextStore == nullptr)
	{
		// Warning: same host failure - without a store no tool can reach the world.
		RoadLog_Warn(TEXT("Enter: tool manager has no context object store"));
	}
	else
	{
		ContextStore->AddContextObject(ContextObject);

		// Read the interface back exactly the way a tool does. This single lookup is what every tool
		// depends on to reach the world at all, and when it fails nothing anywhere says so - each tool just
		// sees null and quietly does nothing. Info: one line per mode entry, and the one that says whether
		// the whole host seam is live.
		bContextReachable = (ContextStore->FindContext<IRoadEditorContext>() != nullptr);
		RoadLog_Info(TEXT("Enter: manager=1 store=1 context reachable=%d"),
			bContextReachable ? 1 : 0);
	}

	// Tools that use a transform gizmo reach it through the UE::TransformGizmoUtil helpers, which need a
	// UCombinedTransformGizmoContextObject in the context object store. S0.5 found this the hard way:
	// without it the first gizmo-based tool asserts on gizmo creation. The call is idempotent, so making
	// it on every Enter() is free. The runtime host has to make the same call on its own context.
	//
	// The UGizmoViewContext the factory wires into every sub gizmo is created and owned by this call (or
	// by the gizmo manager's own RegisterDefaultGizmos()), and is always non-null by the time a tool asks
	// for a gizmo, so there is nothing to pre-register here.
	UE::TransformGizmoUtil::RegisterTransformGizmoContextObject(GetInteractiveToolsContext());

	// Let the gizmo ignore the editor's widget mode.
	//
	// A UCombinedTransformGizmo derives the visibility of its handles from
	// IToolsContextQueriesAPI::GetCurrentTransformGizmoMode(), and in the editor that query answers with
	// FEditorModeTools::GetWidgetMode() - the translate/rotate/scale switch in the viewport toolbar.
	// CombinedTransformGizmo::Tick() then hides every sub-gizmo that the current mode does not name, so a
	// mode left on "Select" (WM_None) or parked on rotate hides the translation handles entirely. A
	// hidden handle fails its own hit test, so the drag never even asks for capture: the point keeps its
	// selection highlight - the tool draws that itself - and simply cannot be grabbed.
	//
	// SetForceCombinedGizmoMode(true) makes that query answer "combined" unconditionally, which is what a
	// custom gizmo wants: the handles a tool asked for are then the handles it gets, and the viewport's
	// widget switch stops acting on them. This is the ITF replacement for the legacy arrangement, where
	// FEdModeRoad::GetWidgetAxisToDraw() drew its own axis list and so was never at the toolbar's mercy.
	if (UEditorInteractiveToolsContext* ToolsContext = GetInteractiveToolsContext())
	{
		ToolsContext->SetForceCombinedGizmoMode(true);
	}

	// The whole road network lives in one scene actor, and every tool reaches it through
	// IRoadEditorContext::GetRoadScene(). The legacy FEdModeRoad created that actor in its Enter(), and
	// this mode has to do the same: the tools are deliberately not allowed to spawn actors, so without
	// this an empty level has no scene and every tool silently does nothing - a right click cannot create
	// the first road, and nothing is drawn because the road list it would walk does not exist.
	//
	// Only existence is established here, not ownership: the actor belongs to the level and is looked up
	// again on demand, so a level switch or a scene deleted in between comes out right without any cache
	// to keep in step.
	UWorld* EnterWorld = GetWorld();
	ARoadScene* EnterScene = FindRoadScene();
	const bool bSceneExisted = (EnterScene != nullptr);
	if (EnterWorld != nullptr && EnterScene == nullptr)
	{
		EnterScene = EnterWorld->SpawnActor<ARoadScene>();
	}

	// A null scene here is the "every tool does nothing" case, so knowing whether Enter() ran and whether
	// it found or spawned one shortens the trail. Info: one line per mode entry.
	RoadLog_Info(TEXT("Enter world=%d scene=%d existed=%d spawned=%d roads=%d"),
		EnterWorld != nullptr ? 1 : 0,
		EnterScene != nullptr ? 1 : 0,
		bSceneExisted ? 1 : 0,
		(!bSceneExisted && EnterScene != nullptr) ? 1 : 0,
		EnterScene != nullptr ? EnterScene->Roads.Num() : -1);

	// RegisterTool() pairs the palette command with the identifier the tool manager starts tools by.
	// Order is irrelevant - unlike the legacy layer, identity is a string rather than an array index.
	// DIAGNOSTIC (remove when the click path is confirmed): the count is reported at the end, so a
	// missing registration cannot look like a working one.
	int32 RegisteredToolCount = 0;
	const FRoadToolsModeCommands& ModeCommands = FRoadToolsModeCommands::Get();
	RegisterTool(ModeCommands.RoadPlan, RoadToolIds::RoadPlan, NewObject<URoadTool_RoadPlanBuilder>(this)); ++RegisteredToolCount;
	RegisterTool(ModeCommands.RoadHeight, RoadToolIds::RoadHeight, NewObject<URoadTool_RoadHeightBuilder>(this)); ++RegisteredToolCount;
	RegisterTool(ModeCommands.RoadChop, RoadToolIds::RoadChop, NewObject<URoadTool_RoadChopBuilder>(this)); ++RegisteredToolCount;
	RegisterTool(ModeCommands.RoadSplit, RoadToolIds::RoadSplit, NewObject<URoadTool_RoadSplitBuilder>(this)); ++RegisteredToolCount;
	RegisterTool(ModeCommands.LaneCarve, RoadToolIds::LaneCarve, NewObject<URoadTool_LaneCarveBuilder>(this)); ++RegisteredToolCount;
	RegisterTool(ModeCommands.LaneEdit, RoadToolIds::LaneEdit, NewObject<URoadTool_LaneEditBuilder>(this)); ++RegisteredToolCount;
	RegisterTool(ModeCommands.LaneWidth, RoadToolIds::LaneWidth, NewObject<URoadTool_LaneWidthBuilder>(this)); ++RegisteredToolCount;
	RegisterTool(ModeCommands.MarkingLane, RoadToolIds::MarkingLane, NewObject<URoadTool_MarkingLaneBuilder>(this)); ++RegisteredToolCount;
	RegisterTool(ModeCommands.MarkingPoint, RoadToolIds::MarkingPoint, NewObject<URoadTool_MarkingPointBuilder>(this)); ++RegisteredToolCount;
	RegisterTool(ModeCommands.MarkingCurve, RoadToolIds::MarkingCurve, NewObject<URoadTool_MarkingCurveBuilder>(this)); ++RegisteredToolCount;
	RegisterTool(ModeCommands.GroundEdit, RoadToolIds::GroundEdit, NewObject<URoadTool_GroundEditBuilder>(this)); ++RegisteredToolCount;
	RegisterTool(ModeCommands.JunctionLink, RoadToolIds::JunctionLink, NewObject<URoadTool_JunctionLinkBuilder>(this)); ++RegisteredToolCount;
	RegisterTool(ModeCommands.File, RoadToolIds::File, NewObject<URoadTool_FileBuilder>(this)); ++RegisteredToolCount;
	RegisterTool(ModeCommands.Settings, RoadToolIds::Settings, NewObject<URoadTool_SettingsBuilder>(this)); ++RegisteredToolCount;

	// DIAGNOSTIC (remove when the click path is confirmed): the return value of SelectActiveToolType() is
	// false when the identifier was never registered above, which is exactly the failure that leaves the
	// palette looking fine while no tool is ever built. OnToolStarted() below reports the other half.
	//
	// The startup tool is the Road palette's entry tool, taken from the same mapping the palette tabs use,
	// so the mode opens on the tab the toolkit will also show as current.
	if (const TCHAR* StartupTool = GetDefaultToolForPalette(PaletteName_Road))
	{
		SelectActiveTool(StartupTool);
	}
	// Info: how many tools were registered, and which one is active - the pair that says the palette is
	// wired to real tools rather than merely looking right.
	RoadLog_Info(TEXT("registered=%d active=%s"),
		RegisteredToolCount, *ToolManager->GetActiveToolName(EToolSide::Left));
}

void URoadToolsMode::OnToolStarted(UInteractiveToolManager* Manager, UInteractiveTool* Tool)
{
	// Info: the moment a tool becomes the active one, which is the other half of the lifecycle pair below.
	RoadLog_Info(TEXT("tool started %s"),
		Tool != nullptr ? *Tool->GetClass()->GetName() : TEXT("null"));
}

void URoadToolsMode::OnToolEnded(UInteractiveToolManager* Manager, UInteractiveTool* Tool)
{
	// Info: pairs with the started line above.
	RoadLog_Info(TEXT("tool ended %s"),
		Tool != nullptr ? *Tool->GetClass()->GetName() : TEXT("null"));
}

void URoadToolsMode::Exit()
{
	// Info: pairs with the Enter() lines, so a mode that was re-entered is visible in the log.
	RoadLog_Info(TEXT("Exit"));

	// Release the capability object before the base tears the tools contexts down.
	if (UInteractiveToolManager* ToolManager = GetToolManager())
	{
		if (UContextObjectStore* ContextStore = ToolManager->GetContextObjectStore())
		{
			ContextStore->RemoveContextObject(ContextObject);
		}
	}
	ContextObject = nullptr;

	UEdMode::Exit();
}

void URoadToolsMode::PostUndo()
{
	ARoadScene* Scene = FindRoadScene();
	if (Scene == nullptr)
	{
		return;
	}

	// Scene->Rebuild() below walks the road list and dereferences each road's curve, so a list the undo
	// left inconsistent would fault here. Undoing a click that both spawned a road and edited the scene
	// restores the two halves through different mechanisms - the actor is destroyed, the scene's road array
	// is put back from its snapshot - so this checks the result rather than assuming the two agree. Leaving
	// the geometry stale for one frame is recoverable; dereferencing a destroyed road is not.
	for (ARoadActor* Road : Scene->Roads)
	{
		if (!IsValid(Road) || Road->BaseCurve == nullptr)
		{
			// Warning: the undo left the road list inconsistent. Recoverable for one frame, but it should
			// not be happening, so it is reported rather than swallowed.
			RoadLog_Warn(
				TEXT("undo rebuild skipped: %d roads, one unusable (valid=%d)"),
				Scene->Roads.Num(), IsValid(Road) ? 1 : 0);
			return;
		}
	}

	// The refit has to run before the scene-wide rebuild: UpdateCurve() is what turns the restored
	// alignment points back into segments, lanes and gates (and what rewrites the height profile's station
	// column from the new length), and Rebuild() is what re-derives the junctions those gates feed.
	for (ARoadActor* Road : Scene->Roads)
	{
		Road->UpdateCurve();
	}
	Scene->Rebuild();

	if (GEditor != nullptr)
	{
		GEditor->RedrawLevelEditingViewports();
	}
}

bool URoadToolsMode::HandleClick(FEditorViewportClient* InViewportClient, HHitProxy* HitProxy,
	const FViewportClick& Click)
{
	// The right button only, and only when it did not drag.
	//
	// The "did not drag" half is not checked here because it cannot be: FEditorViewportClient::
	// ProcessClickInViewport() is the only caller of the mode click path, and it already discards a release
	// whose mouse moved further than its click threshold. Handled is therefore exactly "a right click",
	// which is why routing the button here leaves right-drag orbit and the right-button-held flight camera
	// untouched - those never reach this function at all.
	//
	// The left button keeps its own path through the tool's input behaviour and is deliberately not
	// accepted here; see the declaration.
	if (Click.GetKey() != EKeys::RightMouseButton)
	{
		return false;
	}

	// Alt is the editor's own camera modifier. The tools context already steps aside for it on the way in
	// (UModeManagerInteractiveToolsContext::InputKey returns early for a press with Alt down), and the
	// legacy path has no such filter of its own, so the same priority is asserted here to keep the two
	// buttons behaving alike. Without this, Alt-clicking a road would edit it instead of orbiting.
	if (Click.IsAltDown())
	{
		return false;
	}

	URoadInteractiveTool* Tool = GetActiveRoadTool();
	if (Tool == nullptr)
	{
		// No tool of ours is active, so the click is the editor's to do with as it likes.
		return false;
	}

	// The ray is taken at the click position rather than at the current cursor, which is the same ray the
	// input behaviour would have produced for the left button.
	const FRay WorldRay(Click.GetOrigin(), Click.GetDirection());

	// Verbose: the right button's whole route from the viewport down to the tool. Nothing between here and
	// the tool can drop the click silently, so this is the criteria line for a right click.
	RoadLog_Debug(TEXT("legacy click btn=R tool=%s ray=(%.0f,%.0f,%.0f)"),
		*Tool->GetClass()->GetName(), WorldRay.Origin.X, WorldRay.Origin.Y, WorldRay.Origin.Z);

	return Tool->HandleViewportClick(WorldRay, /*bRightButton*/ true);
}

ARoadScene* URoadToolsMode::FindRoadScene() const
{
	UWorld* World = GetWorld();
	return (World != nullptr)
		? Cast<ARoadScene>(UGameplayStatics::GetActorOfClass(World, ARoadScene::StaticClass()))
		: nullptr;
}

URoadInteractiveTool* URoadToolsMode::GetActiveRoadTool() const
{
	UInteractiveToolManager* ToolManager = GetToolManager();
	if (ToolManager == nullptr)
	{
		return nullptr;
	}

	// The left-hand side is the one this mode registers its tools against in Enter(), which is also the
	// side EToolSide::Left in UInteractiveToolManager means for a mode's own tools.
	return Cast<URoadInteractiveTool>(ToolManager->GetActiveTool(EToolSide::Left));
}

void URoadToolsMode::CreateToolkit()
{
	Toolkit = MakeShareable(new FRoadToolsModeToolkit);
}

const TCHAR* URoadToolsMode::GetDefaultToolForPalette(FName PaletteName)
{
	// One tool per palette, the one that palette's switch should land on. A palette whose legacy
	// counterpart had no buttons still names a tool, because the point of that tab is the panel the
	// tool brings up: File and Settings hold no button, so entering the tab is the only way in.
	//
	// This is the only place that maps a palette to its entry tool, so a renamed palette or a moved
	// tool changes here and nowhere else. Every name is a registered tool identity (RoadToolIds).
	if (PaletteName == PaletteName_File) { return RoadToolIds::File; }
	if (PaletteName == PaletteName_Road) { return RoadToolIds::RoadPlan; }
	if (PaletteName == PaletteName_Junction) { return RoadToolIds::JunctionLink; }
	if (PaletteName == PaletteName_Lane) { return RoadToolIds::LaneEdit; }
	if (PaletteName == PaletteName_Marking) { return RoadToolIds::MarkingLane; }
	if (PaletteName == PaletteName_Ground) { return RoadToolIds::GroundEdit; }
	if (PaletteName == PaletteName_Settings) { return RoadToolIds::Settings; }

	return nullptr;
}

void URoadToolsMode::SelectActiveTool(const TCHAR* ToolId)
{
	UInteractiveToolManager* ToolManager = GetToolManager();
	if (ToolManager == nullptr || ToolId == nullptr)
	{
		return;
	}

	// EToolSide::Left is the side Enter() registered every tool against, and the same side the palette
	// buttons drive through the framework, so both routes end at the same active tool.
	if (!ToolManager->SelectActiveToolType(EToolSide::Left, ToolId))
	{
		// A palette pointing at an unregistered identity is a wiring mistake, not a user error: the tab
		// would look fine and start nothing. Warning so it shows up the moment it happens.
		RoadLog_Warn(
			TEXT("SelectActiveTool: no tool registered as '%s'"), ToolId);
	}
}

TMap<FName, TArray<TSharedPtr<FUICommandInfo>>> URoadToolsMode::GetModeCommands() const
{
	// The palette contents the toolkit's BuildToolPalette() renders; the toolkit names the palettes.
	TMap<FName, TArray<TSharedPtr<FUICommandInfo>>> Commands = FRoadToolsModeCommands::Get().GetCommands();

	// FModeToolkit::BuildToolPalette() looks the palette name up in this map and renders nothing when the
	// name is not a key, so which keys exist - and how many buttons each carries - is worth one line per
	// palette. Verbose: called once per palette widget rebuild, not per frame, but still rebuild noise.
	for (const TPair<FName, TArray<TSharedPtr<FUICommandInfo>>>& Pair : Commands)
	{
		RoadLog_Debug(TEXT("GetModeCommands palette=%s commands=%d"),
			*Pair.Key.ToString(), Pair.Value.Num());
	}

	return Commands;
}

#undef LOCTEXT_NAMESPACE
