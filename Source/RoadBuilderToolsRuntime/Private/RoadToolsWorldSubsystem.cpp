// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

#include "RoadToolsWorldSubsystem.h"

#include "Components/LineBatchComponent.h"
#include "Editing/RoadRuntimeContextObject.h"
#include "Editing/RoadToolsContext.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "Input/RoadRuntimeInputBridge.h"
#include "InteractiveTool.h"
#include "InteractiveToolManager.h"
#include "InputRouter.h"
#include "Rendering/RoadRuntimeRenderAPI.h"
#include "RoadLog.h"
#include "UI/RoadRuntimePropertyPanel.h"
#include "Widgets/SBoxPanel.h"

// Every tool builder lives in RoadBuilderTools, which this module depends on and which never depends on
// this one - the dependency boundary that keeps the tool layer compilable in a game target with no host.
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

bool URoadToolsWorldSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (!Super::ShouldCreateSubsystem(Outer))
	{
		return false;
	}

	// A dedicated server cannot host an editor: no viewport, no local player, no rendering. Creating the
	// subsystem there would allocate a tools context that can never start a tool.
	const UWorld* OuterWorld = Cast<UWorld>(Outer);
	if (OuterWorld == nullptr)
	{
		return false;
	}

	return OuterWorld->IsGameWorld() && OuterWorld->GetNetMode() != NM_DedicatedServer;
}

void URoadToolsWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Everything else is created lazily in StartEditing(), because a world that never enters road editing
	// should not pay for a tools context, an input bridge or an overlay component.
	RoadLog_Debug(TEXT("runtime road subsystem created for world '%s'"),
		GetWorld() != nullptr ? *GetWorld()->GetName() : TEXT("null"));
}

void URoadToolsWorldSubsystem::Deinitialize()
{
	// StopEditing() first: it deactivates tools while the world is still intact, which is what keeps a
	// tool's Shutdown() from reaching into a torn-down level.
	StopEditing();

	Super::Deinitialize();
}

ETickableTickType URoadToolsWorldSubsystem::GetTickableTickType() const
{
	// Never, rather than Conditional: a subsystem that is not editing has nothing to tick, and answering
	// Never lets the tickable registry skip it entirely instead of calling IsTickable() every frame.
	return IsTemplate() ? ETickableTickType::Never : ETickableTickType::Conditional;
}

bool URoadToolsWorldSubsystem::IsTickable() const
{
	return bEditing;
}

TStatId URoadToolsWorldSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(URoadToolsWorldSubsystem, STATGROUP_Tickables);
}

bool URoadToolsWorldSubsystem::StartEditing()
{
	if (bEditing)
	{
		// Idempotent by contract, so a game can call this from a key handler without tracking state.
		return true;
	}

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		// Error: a subsystem without a world is a lifecycle bug, not a user problem.
		RoadLog_Error(TEXT("StartEditing with no world"));
		return false;
	}

	// The overlay has to exist before the render API that draws into it.
	EnsureOverlayTarget();
	if (OverlayTarget == nullptr)
	{
		RoadLog_Error(TEXT("StartEditing: no overlay target; tools would draw nowhere"));
		return false;
	}

	// The context object first: InitializeRoadEditing() installs it into the store, and installing it
	// before any tool is registered is what guarantees a tool finds it while being built.
	RuntimeContext = NewObject<URoadRuntimeContextObject>(this);
	RuntimeContext->SetEditingWorld(World);

	// The rebuild handler is the runtime equivalent of FEdMode::PostUndo()'s job: after any transaction the
	// generated geometry is stale. A game owns how its world is built, so the default here is the scene's
	// own rebuild, and a game can replace it if it builds roads differently.
	RuntimeContext->SetRebuildHandler([this]()
	{
		if (RuntimeContext != nullptr)
		{
			if (ARoadScene* Scene = RuntimeContext->GetRoadScene())
			{
				Scene->Rebuild();
			}
		}
	});

	ToolsContext = NewObject<URoadToolsContext>(this);
	if (!ToolsContext->InitializeRoadEditing(World, RuntimeContext))
	{
		RoadLog_Error(TEXT("StartEditing: tools context failed to initialise"));
		ToolsContext = nullptr;
		RuntimeContext = nullptr;
		return false;
	}

	// Input plumbing, now that the router exists. bAutoInvalidate* are turned on so a hover or a captured
	// drag requests a redraw by itself: the runtime host has no editor tick to repaint on idle, and a
	// preview that only appears when something else happens to change would look broken.
	if (UInputRouter* Router = ToolsContext->InputRouter)
	{
		Router->bAutoInvalidateOnHover = true;
		Router->bAutoInvalidateOnCapture = true;
	}
	else
	{
		// Warning: no router means no tool can receive input. The context is supposed to create one.
		RoadLog_Warn(TEXT("StartEditing: no input router on the tools context; tools will get no input"));
	}

	InputBridge = MakeUnique<FRoadRuntimeInputBridge>();
	InputBridge->Initialize(ToolsContext->InputRouter, World);

	RenderAPI = MakeUnique<FRoadRuntimeRenderAPI>(nullptr, OverlayTarget);

	// The property panel is built once and reused. It has to exist before the first tool starts, because
	// the subscription below fires immediately for the entry tool.
	if (!PropertyPanel.IsValid())
	{
		PropertyPanel = SNew(SRoadRuntimePropertyPanel);
	}

	// Follow the active tool through the tool manager's own lifecycle delegates. This is the runtime
	// counterpart of what FModeToolkit's OnToolStarted/OnToolEnded do for the editor's details panel - and
	// it is the reason IRoadEditorContext::NotifyActiveToolChanged() exists, since a game has no toolkit to
	// hang this off.
	if (ToolsContext->ToolManager != nullptr)
	{
		ToolStartedHandle = ToolsContext->ToolManager->OnToolStarted.AddLambda(
			[this](UInteractiveToolManager*, UInteractiveTool*)
			{
				RefreshPropertyPanel();
			});
		ToolEndedHandle = ToolsContext->ToolManager->OnToolEnded.AddLambda(
			[this](UInteractiveToolManager*, UInteractiveTool*)
			{
				RefreshPropertyPanel();
			});
	}
	else
	{
		RoadLog_Warn(TEXT("StartEditing: no tool manager; the property panel will not follow tool switches"));
	}

	RegisterTools();

	bEditing = true;

	// The entry tool, mirroring what a palette switch does in the editor: entering the host lands on a
	// working tool rather than on nothing.
	StartTool(RoadToolIds::RoadPlan);

	// Info: the host is up. The tool count is reported by RegisterTools() itself, which is where the number
	// is actually known.
	RoadLog_Info(TEXT("runtime road editing started: world=%s panel=%d"),
		*World->GetName(), PropertyPanel.IsValid() ? 1 : 0);

	return true;
}

void URoadToolsWorldSubsystem::StopEditing()
{
	if (!bEditing)
	{
		return;
	}

	// Editing is off first, so nothing ticks while the host is being dismantled - a Tick that ran between
	// the steps below would post input into a half-dead router.
	bEditing = false;

	// Unsubscribe before the manager goes away, and before the panel that the callbacks touch.
	if (ToolsContext != nullptr && ToolsContext->ToolManager != nullptr)
	{
		if (ToolStartedHandle.IsValid())
		{
			ToolsContext->ToolManager->OnToolStarted.Remove(ToolStartedHandle);
		}
		if (ToolEndedHandle.IsValid())
		{
			ToolsContext->ToolManager->OnToolEnded.Remove(ToolEndedHandle);
		}
	}
	ToolStartedHandle.Reset();
	ToolEndedHandle.Reset();

	// Captures before tools: a captured behaviour would otherwise be notified about a tool that is going
	// away underneath it.
	if (InputBridge.IsValid())
	{
		InputBridge->ReleaseCaptures();
		InputBridge->Shutdown();
		InputBridge.Reset();
	}

	// The render API holds a raw component pointer; drop it before the component can be destroyed.
	RenderAPI.Reset();

	if (ToolsContext != nullptr)
	{
		ToolsContext->ShutdownRoadEditing();
		ToolsContext = nullptr;
	}

	RuntimeContext = nullptr;

	// The panel is kept alive (it is a shared Slate widget a game may still have mounted) but pointed at
	// nothing, so it does not read properties off a tool that no longer exists.
	if (PropertyPanel.IsValid())
	{
		PropertyPanel->SetTool(nullptr);
	}

	// The overlay actor belongs to the host only when the host made it; a game-supplied component and its
	// owner are the game's to destroy.
	if (OverlayActor != nullptr)
	{
		OverlayActor->Destroy();
		OverlayActor = nullptr;
		OverlayTarget = nullptr;
	}

	RoadLog_Info(TEXT("runtime road editing stopped"));
}

void URoadToolsWorldSubsystem::RefreshPropertyPanel()
{
	if (!PropertyPanel.IsValid())
	{
		return;
	}

	UInteractiveTool* ActiveTool = nullptr;
	if (ToolsContext != nullptr && ToolsContext->ToolManager != nullptr)
	{
		// EToolSide::Left is the side this host starts its tools on - the same side EToolSide::Mouse names.
		ActiveTool = ToolsContext->ToolManager->GetActiveTool(EToolSide::Left);
	}

	PropertyPanel->SetTool(ActiveTool);

	// The host notification, so a game's own UI can react to the tool change too. The editor implementation
	// of this method is deliberately empty; here it is the point.
	if (RuntimeContext != nullptr && ActiveTool != nullptr)
	{
		RuntimeContext->NotifyActiveToolChanged(ActiveTool->GetClass()->GetFName(), /*bActive*/ true);
	}
}

void URoadToolsWorldSubsystem::EnsureOverlayTarget()
{
	if (OverlayTarget != nullptr)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	// A component needs an actor to live on, so the host spawns a bare one to carry it. Spawned with
	// RF_Transient so it is never saved into a level - it is host plumbing, not game content - and hidden
	// in game so it does not show up as a stray actor in an outliner or a save.
	FActorSpawnParameters SpawnParams;
	SpawnParams.ObjectFlags |= RF_Transient;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	OverlayActor = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, SpawnParams);
	if (OverlayActor == nullptr)
	{
		RoadLog_Error(TEXT("EnsureOverlayTarget: failed to spawn the overlay actor"));
		return;
	}

#if WITH_EDITOR
	OverlayActor->SetActorLabel(TEXT("RoadToolsOverlay"));
#endif
	OverlayActor->SetActorEnableCollision(false);
	OverlayActor->SetHidden(true);

	// Created rather than attached to an existing component so the tools' batch cannot collide with lines
	// the game draws: FRoadLineBatchDrawInterface clears only OverlayBatchId.
	OverlayTarget = NewObject<ULineBatchComponent>(OverlayActor, TEXT("RoadToolsOverlayBatch"));
	if (OverlayTarget != nullptr)
	{
		OverlayTarget->RegisterComponent();

		// The component has to be told to be drawn even when the owning actor is hidden, which is what
		// separates "the actor is invisible" from "the overlay is invisible".
		OverlayTarget->SetVisibility(true);
		OverlayTarget->bCalculateAccurateBounds = false;
	}
	else
	{
		RoadLog_Error(TEXT("EnsureOverlayTarget: failed to create the line batch component"));
	}
}

void URoadToolsWorldSubsystem::SetOverlayTarget(ULineBatchComponent* InOverlayTarget)
{
	if (bEditing)
	{
		// Refused while running: the render API captured the old component, and swapping it mid-session
		// would leave the tools drawing into a component nobody renders. Stop and restart instead.
		RoadLog_Warn(TEXT("SetOverlayTarget ignored while editing is running; stop editing first"));
		return;
	}

	OverlayTarget = InOverlayTarget;
}

void URoadToolsWorldSubsystem::SetViewState(const FViewCameraState& InState)
{
	if (ToolsContext != nullptr)
	{
		if (FRoadRuntimeQueriesAPI* Queries = ToolsContext->GetRoadQueries())
		{
			Queries->SetViewState(InState);
		}
	}
}

void URoadToolsWorldSubsystem::RegisterTools()
{
	// The tool set is written down once per host, exactly as URoadToolsMode::Enter() does for the editor.
	// Order is irrelevant - identity is a string, not an index - so a tool added or removed here cannot
	// shift another one, which was the legacy layer's worst failure mode.
	int32 RegisteredCount = 0;
	auto RegisterOne = [this, &RegisteredCount](const TCHAR* ToolId, UInteractiveToolBuilder* Builder)
	{
		if (ToolsContext->RegisterRoadTool(ToolId, Builder))
		{
			++RegisteredCount;
		}
	};

	RegisterOne(RoadToolIds::RoadPlan, NewObject<URoadTool_RoadPlanBuilder>(this));
	RegisterOne(RoadToolIds::RoadHeight, NewObject<URoadTool_RoadHeightBuilder>(this));
	RegisterOne(RoadToolIds::RoadChop, NewObject<URoadTool_RoadChopBuilder>(this));
	RegisterOne(RoadToolIds::RoadSplit, NewObject<URoadTool_RoadSplitBuilder>(this));
	RegisterOne(RoadToolIds::LaneCarve, NewObject<URoadTool_LaneCarveBuilder>(this));
	RegisterOne(RoadToolIds::LaneEdit, NewObject<URoadTool_LaneEditBuilder>(this));
	RegisterOne(RoadToolIds::LaneWidth, NewObject<URoadTool_LaneWidthBuilder>(this));
	RegisterOne(RoadToolIds::MarkingLane, NewObject<URoadTool_MarkingLaneBuilder>(this));
	RegisterOne(RoadToolIds::MarkingPoint, NewObject<URoadTool_MarkingPointBuilder>(this));
	RegisterOne(RoadToolIds::MarkingCurve, NewObject<URoadTool_MarkingCurveBuilder>(this));
	RegisterOne(RoadToolIds::GroundEdit, NewObject<URoadTool_GroundEditBuilder>(this));
	RegisterOne(RoadToolIds::JunctionLink, NewObject<URoadTool_JunctionLinkBuilder>(this));
	RegisterOne(RoadToolIds::File, NewObject<URoadTool_FileBuilder>(this));
	RegisterOne(RoadToolIds::Settings, NewObject<URoadTool_SettingsBuilder>(this));

	// Debug: the count that proves the loop completed. A missing tool shows up here as a number below 14.
	RoadLog_Debug(TEXT("registerTools: %d builders registered"), RegisteredCount);
}

bool URoadToolsWorldSubsystem::StartTool(const TCHAR* ToolId)
{
	return ToolsContext != nullptr && ToolsContext->StartRoadTool(ToolId);
}

bool URoadToolsWorldSubsystem::UndoEdit()
{
	if (ToolsContext == nullptr)
	{
		return false;
	}

	FRoadRuntimeUndoStack* Stack = ToolsContext->GetUndoStack();
	if (Stack == nullptr)
	{
		return false;
	}

	const FText Description = Stack->Undo();
	if (Description.IsEmpty())
	{
		return false;
	}

	// The rebuild is what makes an undo visible: the changes reverted data, and the geometry derived from
	// it is now stale. Same ordering as the editor's PostUndo() - refit each road's curve, then rebuild the
	// scene - expressed through the host's own rebuild path.
	if (RuntimeContext != nullptr)
	{
		RuntimeContext->RequestRebuild();
	}

	return true;
}

bool URoadToolsWorldSubsystem::RedoEdit()
{
	if (ToolsContext == nullptr)
	{
		return false;
	}

	FRoadRuntimeUndoStack* Stack = ToolsContext->GetUndoStack();
	if (Stack == nullptr)
	{
		return false;
	}

	const FText Description = Stack->Redo();
	if (Description.IsEmpty())
	{
		return false;
	}

	if (RuntimeContext != nullptr)
	{
		RuntimeContext->RequestRebuild();
	}

	return true;
}

void URoadToolsWorldSubsystem::Tick(float DeltaTime)
{
	if (!bEditing)
	{
		return;
	}

	// Input first, then render: a tool that responded to input this frame draws its new state in the same
	// frame, which is what keeps a drag from lagging a frame behind the cursor.
	if (InputBridge.IsValid())
	{
		if (APlayerController* PlayerController = GetWorld() != nullptr
			? GetWorld()->GetFirstPlayerController()
			: nullptr)
		{
			InputBridge->PollMouse(PlayerController);
		}
	}

	// Frame the overlay. The camera state is whatever the game last pushed - zeroed until it does, which
	// only affects tools that orient to the view.
	if (RenderAPI.IsValid() && ToolsContext != nullptr)
	{
		// The draw interface is reset at BeginFrame and published at EndFrame, and the tools render in
		// between, so the batch the game sees is exactly this frame's overlay.
		RenderAPI->BeginFrame(FViewCameraState{}, EViewInteractionState::Focused);
		ToolsContext->RenderTools(*RenderAPI);
		RenderAPI->EndFrame();
	}
}
