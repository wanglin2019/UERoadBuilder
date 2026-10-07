// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

#include "Tools/RoadInteractiveTool.h"

#include "BaseBehaviors/ClickDragBehavior.h"
#include "BaseBehaviors/MouseHoverBehavior.h"
#include "BaseBehaviors/SingleClickBehavior.h"
#include "ContextObjectStore.h"
#include "Engine/World.h"
#include "InteractiveToolManager.h"
#include "Interface/RoadEditorContext.h"
#include "RoadActor.h"
#include "RoadBoundary.h"
#include "RoadBuilderTools.h"
#include "RoadLog.h"
#include "RoadCurve.h"
#include "RoadLane.h"
#include "RoadScene.h"
#include "SceneManagement.h"
#include "Tools/RoadArrayChange.h"
#include "Tools/RoadKeyInputBehavior.h"
#include "Tools/RoadPicking.h"
#include "Tools/RoadPropertyChange.h"

bool URoadInteractiveToolBuilder::CanBuildTool(const FToolBuilderState& SceneState) const
{
	// Every road tool raycasts into the world, so a world is a hard prerequisite.
	return SceneState.World != nullptr;
}

void URoadInteractiveTool::AddInputBehavior(UInputBehavior* Behavior, void* Source)
{
	// Fires from inside Setup() of every tool, so one line per behaviour reports which tool was set up and
	// whether the host seam worked for it. A tool whose behaviours never appear here never got that far,
	// and a null manager / context / world is the difference between a tool that works and one that
	// silently does nothing. Verbose rather than Log because it is setup noise, not a user-visible event.
	RoadLog_Debug(TEXT("%s +behavior %s | manager=%d context=%d world=%d"),
		*GetClass()->GetName(),
		Behavior != nullptr ? *Behavior->GetClass()->GetName() : TEXT("null"),
		GetToolManager() != nullptr ? 1 : 0,
		GetRoadEditorContext() != nullptr ? 1 : 0,
		GetEditingWorld() != nullptr ? 1 : 0);

	Super::AddInputBehavior(Behavior, Source);
}

IRoadEditorContext* URoadInteractiveTool::GetRoadEditorContext() const
{
	UInteractiveToolManager* ToolManager = GetToolManager();
	if (ToolManager == nullptr)
	{
		return nullptr;
	}

	UContextObjectStore* ContextStore = ToolManager->GetContextObjectStore();
	if (ContextStore == nullptr)
	{
		return nullptr;
	}

	// FindContext() walks up to the parent store, so a mode-scoped tool still reaches the context
	// object a mode-level host registered.
	//
	// Ask for the C++ interface (IRoadEditorContext), NOT the UINTERFACE's UObject half
	// (URoadEditorContext). FindContext() resolves each candidate with Cast<>, and for a UInterface
	// class Cast<> degrades to an ordinary UObject cast: TIsIInterface<> only answers true for types
	// that do not themselves derive from UObject, and UInterface does (Casts.h:48-60 - the primary
	// template is selected for anything derived from UObject, and it says false). A UObject cast then
	// tests the inheritance chain via IsA(), but an implementing class derives from UObject rather than
	// from the UInterface, so FindContext<URoadEditorContext>() could never match anything and answered
	// nullptr every time - which took the whole host seam, and with it every tool, down with it.
	// Naming the interface type instead routes Cast<> through GetInterfaceAddress(), the lookup that
	// actually consults the object's implemented interfaces (and that IRoadEditorContext::UClassType
	// exists for).
	return ContextStore->FindContext<IRoadEditorContext>();
}

UWorld* URoadInteractiveTool::GetEditingWorld() const
{
	if (IRoadEditorContext* RoadEditorContext = GetRoadEditorContext())
	{
		if (UWorld* ContextWorld = RoadEditorContext->GetEditingWorld())
		{
			return ContextWorld;
		}
	}

	return EditingWorld.Get();
}

void URoadInteractiveTool::SetEditingWorld(UWorld* InWorld)
{
	EditingWorld = InWorld;
}

ARoadActor* URoadInteractiveTool::GetSelectedRoad() const
{
	IRoadEditorContext* RoadEditorContext = GetRoadEditorContext();
	return RoadEditorContext != nullptr ? RoadEditorContext->GetSelectedRoad() : nullptr;
}

void URoadInteractiveTool::SetSelectedRoad(ARoadActor* Road) const
{
	if (IRoadEditorContext* RoadEditorContext = GetRoadEditorContext())
	{
		RoadEditorContext->SetSelectedRoad(Road);
	}
}

ARoadScene* URoadInteractiveTool::GetRoadScene() const
{
	IRoadEditorContext* RoadEditorContext = GetRoadEditorContext();
	return RoadEditorContext != nullptr ? RoadEditorContext->GetRoadScene() : nullptr;
}

AGroundActor* URoadInteractiveTool::GetSelectedGround() const
{
	IRoadEditorContext* RoadEditorContext = GetRoadEditorContext();
	return RoadEditorContext != nullptr ? RoadEditorContext->GetSelectedGround() : nullptr;
}

AJunctionActor* URoadInteractiveTool::GetSelectedJunction() const
{
	IRoadEditorContext* RoadEditorContext = GetRoadEditorContext();
	return RoadEditorContext != nullptr ? RoadEditorContext->GetSelectedJunction() : nullptr;
}

void URoadInteractiveTool::SetSelectedGround(AGroundActor* Ground) const
{
	if (IRoadEditorContext* RoadEditorContext = GetRoadEditorContext())
	{
		RoadEditorContext->SetSelectedGround(Ground);
	}
}

void URoadInteractiveTool::SetSelectedJunction(AJunctionActor* Junction) const
{
	if (IRoadEditorContext* RoadEditorContext = GetRoadEditorContext())
	{
		RoadEditorContext->SetSelectedJunction(Junction);
	}
}

void URoadInteractiveTool::RequestRedraw() const
{
	if (IRoadEditorContext* RoadEditorContext = GetRoadEditorContext())
	{
		RoadEditorContext->RequestRedraw();
	}
}

void URoadInteractiveTool::RequestRebuild() const
{
	if (IRoadEditorContext* RoadEditorContext = GetRoadEditorContext())
	{
		RoadEditorContext->RequestRebuild();
	}
}

FVector URoadInteractiveTool::LineTrace(const FRay& Ray, AActor* IgnoredActor) const
{
	UWorld* World = GetEditingWorld();
	if (World == nullptr)
	{
		return FVector(WORLD_MAX, WORLD_MAX, WORLD_MAX);
	}

	FHitResult Hit;
	FCollisionQueryParams Params;
	if (IgnoredActor != nullptr)
	{
		Params.AddIgnoredActor(IgnoredActor);
	}

	if (World->LineTraceSingleByChannel(Hit, Ray.Origin, Ray.Origin + Ray.Direction * RoadPicking::RayLength,
		ECollisionChannel::ECC_Visibility, Params))
	{
		return Hit.Location;
	}

	// The legacy sentinel, kept because callers depend on being able to tell "hit nothing" from a real
	// position: a road chop projects this point onto the road, and a missed trace must not be projected.
	return FVector(WORLD_MAX, WORLD_MAX, WORLD_MAX);
}

ARoadActor* URoadInteractiveTool::SelectRoadUnderRay(const FRay& Ray) const
{
	ARoadActor* PickedRoad = RoadPicking::PickRoad(GetRoadScene(), Ray);
	if (PickedRoad == nullptr)
	{
		return nullptr;
	}

	if (IRoadEditorContext* RoadEditorContext = GetRoadEditorContext())
	{
		RoadEditorContext->SetSelectedRoad(PickedRoad);
	}
	return PickedRoad;
}

void URoadInteractiveTool::AddClickBehavior()
{
	// The local variant implements IClickBehaviorTarget through lambdas, which is what lets the tool own
	// the binding without a second target object.
	ULocalSingleClickInputBehavior* Behavior = NewObject<ULocalSingleClickInputBehavior>();
	Behavior->Initialize();

	// The single most decisive probe on the click path.
	//
	// The button-state function is read by IsPressed(), which WantsCapture() calls before anything else -
	// so this lambda is invoked exactly when a mouse event has travelled the whole way from the viewport
	// through the mode manager and the input router and has landed on this behaviour. If a left click
	// produces no "poll ... press=1" line at VeryVerbose, the click never reached the tool at all and the
	// fault is upstream of it; if it does appear, everything up to this point is proven good.
	//
	// It spells out the base class's left-button default rather than changing it: it answers with the very
	// same button state, so the binding is unchanged.
	//
	// Mouse-move events also poll this - WantsCapture() runs on every posted mouse event - so the line is
	// gated on an actual button transition. A press or release always carries one; a move never does. That
	// gating is also why this sits at VeryVerbose: on a press-shaped event it is rare, but the lambda is
	// still on the hot path and should stay silent unless a gesture is being traced.
	Behavior->SetUseCustomMouseButton([](const FInputDeviceState& Input)
	{
		const FDeviceButtonState& State = Input.Mouse.Left;
		if (State.bPressed || State.bReleased)
		{
			RoadLog_Trace(TEXT("poll btn=L press=%d down=%d release=%d"),
				State.bPressed ? 1 : 0, State.bDown ? 1 : 0, State.bReleased ? 1 : 0);
		}
		return State;
	});

	Behavior->IsHitByClickFunc = [this](const FInputDeviceRay& ClickPos)
	{
		// Logged with its own result, because the router only ever consults the tool when the behaviour
		// asked for capture - an ignored hit test would otherwise be indistinguishable from a click that
		// never arrived. VeryVerbose: one line per click candidate, useful only while tracing input.
		const FInputRayHit Hit = IsHitByRoadClick(ClickPos);
		RoadLog_Trace(TEXT("hit-test btn=L hit=%d depth=%.1f"),
			Hit.bHit ? 1 : 0, Hit.HitDepth);
		return Hit;
	};
	Behavior->OnClickedFunc = [this](const FInputDeviceRay& ClickPos)
	{
		// Info: a left click did reach the tool and is about to act. This is a real user-visible event, so
		// it stays on at the default Log verbosity.
		RoadLog_Info(TEXT("clicked btn=L"));
		// The result is ignored on this path: the behaviour already claimed the press at hit-test time,
		// so there is nothing left to fall through to.
		OnRoadClicked(ClickPos, /*bRightButton*/ false);
	};

	AddInputBehavior(Behavior);

	// Escape rides along with the click binding, because the two are one gesture set: every interactive
	// tool selects with a click and steps back out with Escape, which is exactly what the legacy
	// FModeTool::InputKey(Escape) did for all of them from the base class. The two panel-only tools
	// (File, Settings) never call this, and so never get the binding - also as in the legacy set.
	AddSelectParentBehavior();
}

bool URoadInteractiveTool::HandleViewportClick(const FRay& WorldRay, bool bRightButton)
{
	// The same entry point the left-button behaviour calls, so a tool keeps one click handler and the
	// button is the only thing that differs between the two paths. The tool's verdict travels back to
	// the host: a click it declined is the editor's again (context menu included).
	return OnRoadClicked(FInputDeviceRay(WorldRay), bRightButton);
}

bool URoadInteractiveTool::OnRoadClicked(const FInputDeviceRay& ClickPos, bool bRightButton)
{
	// Nothing by default: a tool with no click handling simply does not register a click behaviour, and
	// a click it never handled was never consumed.
	return false;
}

FInputRayHit URoadInteractiveTool::IsHitByRoadClick(const FInputDeviceRay& ClickPos)
{
	// Accept anywhere, and at the worst possible depth.
	//
	// Without hit proxies there is nothing to ray-test up front, so the tool has to accept every click and
	// sort out what was hit in OnRoadClicked(). The depth reported here only breaks ties in the input
	// router's capture ranking, and the tool and the gizmos do not in fact tie: gizmo behaviours run at
	// FInputCapturePriority::DEFAULT_GIZMO_PRIORITY (50) and tool behaviours at DEFAULT_TOOL_PRIORITY
	// (100), and the router sorts ascending, so a gizmo always outranks a tool on priority alone.
	//
	// Reporting the maximum depth is therefore belt and braces rather than a fix - it keeps bHit true (the
	// behaviour still claims plain clicks, which is how selection works) while making sure this request
	// loses to any real hit in the event that a future behaviour ever does land on the same priority. The
	// drag failure this was once blamed for had a different cause; see URoadToolsMode::Enter().
	return FInputRayHit(TNumericLimits<float>::Max());
}

void URoadInteractiveTool::AddHoverBehavior()
{
	ULocalMouseHoverBehavior* Behavior = NewObject<ULocalMouseHoverBehavior>();
	Behavior->Initialize();

	// Accept the hover anywhere: the cursor being over the viewport is the only condition, and what it is
	// over is the tool's business.
	Behavior->BeginHitTestFunc = [](const FInputDeviceRay&) { return FInputRayHit(0.0f); };
	Behavior->OnUpdateHoverFunc = [this](const FInputDeviceRay& DevicePos)
	{
		LastCursorRay = DevicePos.WorldRay;
		bHasCursorRay = true;
		// Keep the hover capture alive; returning false would end it after a single move.
		return true;
	};
	Behavior->OnEndHoverFunc = [this]() { bHasCursorRay = false; };

	AddInputBehavior(Behavior);
}

void URoadInteractiveTool::AddSelectParentBehavior()
{
	// Escape steps the selection up one level. Registered as an ordinary key behaviour rather than as a
	// toolkit command, which is what the legacy mode used - a command needs a toolkit to own it, and this
	// keeps the binding next to the tool state it operates on.
	URoadKeyInputBehavior* Behavior = NewObject<URoadKeyInputBehavior>();
	Behavior->Initialize(EKeys::Escape, [this]() { SelectParent(); });
	AddInputBehavior(Behavior);
}

void URoadInteractiveTool::SelectParent()
{
	// The two levels every legacy tool shared: a selected road collapses to its owning junction (if it has
	// one) and then to nothing, and a bare junction selection clears. A tool with a sub-selection of its
	// own overrides this, calls it first, and then clears that sub-selection.
	ARoadActor* Road = GetSelectedRoad();
	if (Road != nullptr)
	{
		AJunctionActor* Junction = Cast<AJunctionActor>(Road->GetAttachParentActor());
		SetSelectedRoad(nullptr);
		if (Junction != nullptr)
		{
			SetSelectedJunction(Junction);
		}
		RequestRedraw();
		return;
	}

	if (GetSelectedJunction() != nullptr)
	{
		SetSelectedJunction(nullptr);
		RequestRedraw();
	}
}

void URoadInteractiveTool::AddDragBehavior()
{
	// The local variant implements IClickDragBehaviorTarget through lambdas, so the tool owns the binding
	// without a second target object - the same reason AddClickBehavior() uses the local single-click
	// behaviour.
	ULocalClickDragInputBehavior* Behavior = NewObject<ULocalClickDragInputBehavior>();
	Behavior->Initialize();

	// Consulted before the plain click behaviour, so a press that lands on a handle becomes a drag rather
	// than a selection. See the header for why the priority is the whole mechanism here.
	Behavior->SetDefaultPriority(FInputCapturePriority(FInputCapturePriority::DEFAULT_TOOL_PRIORITY - 1));

	Behavior->CanBeginClickDragFunc = [this](const FInputDeviceRay& PressPos)
	{
		// Reports what the handle test decided for a press, which is the one fact that separates "the drag
		// behaviour was never asked" from "it was asked and declined". Verbose: per-press, not per-move.
		const FInputRayHit Hit = CanBeginRoadDrag(PressPos);
		RoadLog_Debug(TEXT("drag begin btn=L hit=%d depth=%.1f"),
			Hit.bHit ? 1 : 0, Hit.HitDepth);
		return Hit;
	};
	Behavior->OnClickDragFunc = [this](const FInputDeviceRay& DragPos) { OnRoadDragged(DragPos); };
	Behavior->OnClickReleaseFunc = [this](const FInputDeviceRay& ReleasePos)
	{
		RoadLog_Debug(TEXT("drag release"));
		OnRoadDragEnded();
	};
	// A capture the router takes away mid-drag has to end the drag too, or the tool would be left
	// believing a drag is still running.
	Behavior->OnTerminateFunc = [this]() { OnRoadDragEnded(); };

	AddInputBehavior(Behavior);
}

FInputRayHit URoadInteractiveTool::CanBeginRoadDrag(const FInputDeviceRay& PressPos)
{
	// Nothing to drag by default: a tool that wants this calls AddDragBehavior() and overrides both this
	// and OnRoadDragged().
	return FInputRayHit();
}

void URoadInteractiveTool::OnRoadDragged(const FInputDeviceRay& DragPos)
{
	// Nothing by default, matching CanBeginRoadDrag().
}

void URoadInteractiveTool::OnRoadDragEnded()
{
	// Nothing by default, matching CanBeginRoadDrag().
}

bool URoadInteractiveTool::GetLastCursorRay(FRay& OutRay) const
{
	if (!bHasCursorRay)
	{
		return false;
	}

	OutRay = LastCursorRay;
	return true;
}

FRoadUndoTransaction::FRoadUndoTransaction(UInteractiveToolManager* InToolManager, const FText& Description)
	: ToolManager(InToolManager)
{
	if (ToolManager != nullptr)
	{
		ToolManager->BeginUndoTransaction(Description);
	}
}

FRoadUndoTransaction::~FRoadUndoTransaction()
{
	if (ToolManager != nullptr)
	{
		ToolManager->EndUndoTransaction();
	}
}

void URoadInteractiveTool::DrawRoads(FPrimitiveDrawInterface* PDI, bool bDrawLinks) const
{
	ARoadScene* Scene = GetRoadScene();
	if (PDI == nullptr || Scene == nullptr)
	{
		return;
	}

	ARoadActor* SelectedRoad = GetSelectedRoad();
	for (ARoadActor* Road : Scene->Roads)
	{
		if (Road == nullptr || Road->BaseCurve == nullptr)
		{
			continue;
		}

		const bool bSelected = (Road == SelectedRoad);
		const FLinearColor Color = bSelected ? RoadToolStyle::Color_Select : RoadToolStyle::Color_Road;
		const float DepthBias = bSelected ? RoadToolStyle::DepthBias_Select : 0.0f;

		// A road with a single point has no curve to walk, so it is drawn as the point it is. Faithful to
		// the legacy DrawRoads(), which had the same special case for the same reason.
		if (Road->RoadPoints.Num() == 1)
		{
			PDI->DrawPoint(Road->BaseCurve->GetPos(0.0), Color, RoadToolStyle::Size_Point, SDPG_Foreground);
		}
		else
		{
			DrawCurve(PDI, Road->BaseCurve->Curve, Color, RoadToolStyle::Thickness_Road, DepthBias);
		}
	}

	if (!bDrawLinks)
	{
		return;
	}

	// The legacy link pass, ported verbatim in shape: each gate's links hold the road that continues
	// through the junction, and which curve of it gets drawn is the model's own call - see
	// FJunctionGate::GetLinkCurve(), shared with the junction tool's picking pass so the two can never
	// disagree about what a link looks like.
	for (AJunctionActor* Junction : Scene->Junctions)
	{
		if (Junction == nullptr)
		{
			continue;
		}

		for (FJunctionGate& Gate : Junction->Gates)
		{
			for (int32 LinkIndex = 0; LinkIndex < Gate.Links.Num(); ++LinkIndex)
			{
				URoadCurve* Curve = Gate.GetLinkCurve(LinkIndex);
				if (Curve == nullptr)
				{
					continue;
				}

				const bool bSelected = (Gate.Links[LinkIndex].Road == SelectedRoad);
				DrawCurve(PDI, Curve->Curve, bSelected ? RoadToolStyle::Color_Select : RoadToolStyle::Color_Road,
					RoadToolStyle::Thickness_Road, bSelected ? RoadToolStyle::DepthBias_Select : 0.0f);
			}
		}
	}
}

void URoadInteractiveTool::DrawCurve(FPrimitiveDrawInterface* PDI, const FPolyline& Curve,
	const FLinearColor& Color, float Thickness, float DepthBias)
{
	if (PDI == nullptr)
	{
		return;
	}

	for (int32 PointIndex = 0; PointIndex < Curve.Points.Num() - 1; ++PointIndex)
	{
		const FVector& Start = Curve.Points[PointIndex].Pos;
		const FVector& End = Curve.Points[PointIndex + 1].Pos;
		PDI->DrawLine(Start, End, Color, SDPG_Foreground, Thickness, DepthBias, true);
	}
}

void URoadInteractiveTool::DrawDivider(FPrimitiveDrawInterface* PDI, const URoadLane* Lane, double Dist,
	const FLinearColor& Color)
{
	if (PDI == nullptr || Lane == nullptr || Lane->LeftBoundary == nullptr || Lane->RightBoundary == nullptr)
	{
		return;
	}

	const FVector Start = Lane->RightBoundary->GetPos(Dist);
	const FVector End = Lane->LeftBoundary->GetPos(Dist);
	PDI->DrawLine(Start, End, Color, SDPG_Foreground);
}

void URoadInteractiveTool::DrawPoint(FPrimitiveDrawInterface* PDI, URoadCurve* Curve, double Dist,
	const FLinearColor& Color)
{
	if (PDI == nullptr || Curve == nullptr)
	{
		return;
	}

	PDI->DrawPoint(Curve->GetPos(Dist), Color, RoadToolStyle::Size_Point, SDPG_Foreground);
}

FVector2D URoadInteractiveTool::ToRoadFrame(ARoadActor* Road, double Dist, const FVector& Delta)
{
	if (Road == nullptr)
	{
		// No road to align to: the world's own X/Y is the best available answer, and a tool that reaches
		// here with no road has nothing to edit anyway.
		return FVector2D(Delta.X, Delta.Y);
	}

	const FVector Direction = Road->GetDir(Dist).GetSafeNormal();
	const FVector Right = Road->GetRight(Dist).GetSafeNormal();
	return FVector2D(FVector::DotProduct(Delta, Direction), FVector::DotProduct(Delta, Right));
}

void URoadInteractiveTool::EmitArrayChange(UObject* Owner, TUniquePtr<FRoadArrayChange> Change,
	const FText& Description)
{
	UInteractiveToolManager* ToolManager = GetToolManager();
	if (ToolManager == nullptr || Owner == nullptr || Change == nullptr)
	{
		return;
	}

	// Upcast here rather than at every call site: EmitObjectChange() speaks FToolCommandChange, and every
	// road undo record is one by construction.
	TUniquePtr<FToolCommandChange> BaseChange(Change.Release());
	ToolManager->EmitObjectChange(Owner, MoveTemp(BaseChange), Description);
}

void URoadInteractiveTool::EmitPropertyChange(UObject* Owner, TUniquePtr<FRoadPropertyChange> Change,
	const FText& Description)
{
	UInteractiveToolManager* ToolManager = GetToolManager();
	if (ToolManager == nullptr || Owner == nullptr || Change == nullptr)
	{
		return;
	}

	TUniquePtr<FToolCommandChange> BaseChange(Change.Release());
	ToolManager->EmitObjectChange(Owner, MoveTemp(BaseChange), Description);
}
