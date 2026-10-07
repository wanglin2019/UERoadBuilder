// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

#include "Tools/RoadPointGizmo.h"

#include "BaseGizmos/CombinedTransformGizmo.h"
#include "BaseGizmos/GizmoBaseComponent.h"
#include "BaseGizmos/GizmoMath.h"
#include "BaseGizmos/GizmoRenderingUtil.h"
#include "BaseGizmos/GizmoViewContext.h"
#include "BaseGizmos/TransformGizmoUtil.h"
#include "BaseGizmos/TransformProxy.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "ContextObjectStore.h"
#include "Engine/World.h"
#include "InteractiveGizmoManager.h"
#include "InteractiveTool.h"
#include "InteractiveToolManager.h"
#include "RoadBuilderTools.h"
#include "RoadLog.h"
#include "SceneManagement.h"
#include "Tools/RoadInteractiveTool.h"
#include "Tools/RoadPicking.h"
#include "ToolContextInterfaces.h"

void URoadPointGizmo::Initialize(UInteractiveTool* InOwningTool, ETransformGizmoSubElements InElements,
	TFunction<FTransform()> InGetTransform,
	TFunction<void(const FTransform&)> InSetTransform,
	TFunction<void()> InOnDragEnded)
{
	OwningTool = InOwningTool;
	Elements = InElements;
	GetTransform = MoveTemp(InGetTransform);
	SetTransform = MoveTemp(InSetTransform);
	OnDragEnded = MoveTemp(InOnDragEnded);

	if (OwningTool == nullptr || !GetTransform || !SetTransform)
	{
		// Without both halves there is nothing worth wiring; Update() then does nothing at all.
		return;
	}

	// See the member comment on HandleComponent: the proxy needs a component, and a road point is not one.
	HandleComponent = NewObject<USceneComponent>(this, TEXT("RoadPointGizmoHandle"));

	TransformProxy = NewObject<UTransformProxy>(this);
	TransformProxy->AddComponentCustom(HandleComponent,
		[this]() { return GetTransform(); },
		[this](const FTransform& NewTransform) { SetTransform(NewTransform); },
		/*UserDefinedIndex*/ 0,
		/*bModifyComponentOnTransform*/ false);

	// The gizmo brackets a drag with these, which is where the deferred rebuild hangs off. This is the ITF
	// counterpart of the legacy LazyRebuild flag plus EndModify().
	TransformProxy->OnBeginTransformEdit.AddUObject(this, &URoadPointGizmo::OnBeginTransformEdit);
	TransformProxy->OnEndTransformEdit.AddUObject(this, &URoadPointGizmo::OnEndTransformEdit);
}

void URoadPointGizmo::Shutdown()
{
	if (TransformGizmo != nullptr)
	{
		// Gizmos are owned by the gizmo manager, not by the tool, so they have to be handed back.
		if (UInteractiveGizmoManager* GizmoManager = TransformGizmo->GetGizmoManager())
		{
			GizmoManager->DestroyGizmo(TransformGizmo);
		}
		TransformGizmo = nullptr;
	}

	TransformProxy = nullptr;
	HandleComponent = nullptr;
	bDragging = false;
}

void URoadPointGizmo::EnsureGizmo()
{
	if (TransformGizmo != nullptr || TransformProxy == nullptr || OwningTool == nullptr)
	{
		return;
	}

	TransformGizmo = UE::TransformGizmoUtil::CreateCustomTransformGizmo(OwningTool->GetToolManager(),
		Elements, OwningTool);

	// The single piece of evidence that tells a gizmo that was never created apart from one that was
	// created and then hidden. A null here means the pairwise gizmo manager has no
	// UCombinedTransformGizmoContextObject in its store, and no amount of visibility work further down can
	// help. Verbose: once per gizmo, and only meaningful while the gizmo path is being debugged.
	RoadLog_Debug(TEXT("EnsureGizmo created=%d elements=%d"),
		TransformGizmo != nullptr ? 1 : 0, static_cast<int32>(Elements));

	if (TransformGizmo != nullptr)
	{
		// No explicit transaction provider: the gizmo falls back to its own manager, which is the
		// ITF-correct source of undo for a transform edit.
		TransformGizmo->SetActiveTarget(TransformProxy);

		// Ignore the host's widget mode entirely, and keep the gizmo's own handle selection instead.
		//
		// UCombinedTransformGizmo::Tick() re-forces the visibility of every sub-gizmo every frame from
		// IToolsContextQueriesAPI::GetCurrentTransformGizmoMode(), and in the editor that query answers
		// with FEditorModeTools::GetWidgetMode() - the translate/rotate/scale switch in the viewport
		// toolbar. A mode left on "Select" (WM_None) or parked on rotate therefore hides the translation
		// handles on the very next tick, no matter what SetVisibility(true) said a moment earlier: the
		// actor is unhidden but every handle component inside it is turned back off.
		//
		// URoadToolsMode::Enter() asks the context to answer "combined" unconditionally, which handles
		// that; setting bUseContextGizmoMode = false is the belt to that pair of braces, because it stops
		// the query from being consulted at all. ActiveGizmoMode then keeps its default of Combined, and
		// the handles this tool asked for are the handles it gets - the ITF replacement for the legacy
		// arrangement, where FEdModeRoad::GetWidgetAxisToDraw() drew its own axis list and so was never at
		// the toolbar's mercy.
		TransformGizmo->bUseContextGizmoMode = false;
		TransformGizmo->ActiveGizmoMode = EToolContextTransformGizmoMode::Combined;

		// DIAGNOSTIC: reports the two facts that are observable from outside the gizmo, since the decisive
		// one - UGizmoBaseComponent::bIsViewDependent, which decides whether GetWorldCorners() scales the
		// handle to a fixed screen size - is a protected member and cannot be read here.
		//
		//   storeCtx=1 -> a UGizmoViewContext was present in the tool manager's store when this gizmo was
		//                 created. It always is: the gizmo manager's RegisterDefaultGizmos() creates one
		//                 on demand and FCombinedTransformGizmoActorFactory hard-checks for it, so every
		//                 sub gizmo gets a non-null context and bIsViewDependent is always true. This line
		//                 exists only to rule that out; it is not the fault.
		//   handle=0   -> the plane handle component was not found on the actor at all, which would be a
		//                 different fault entirely (the actor was built without TranslateXY).
		//
		// Both healthy (which is what the reports show) leaves the view-dependent render cull as the cause:
		// UGizmoRectangleComponent::GetWorldCorners() skips drawing when |Dot(PlaneNormal, ViewDirection)|
		// <= 0.25, and a ground-flush XY plane is edge-on - and therefore culled - from any camera below
		// ~15 degrees of elevation. That is why the fix is to ask for the axis arrows as well, not to touch
		// the view context.
		bool bStoreHasViewContext = false;
		if (const UInteractiveToolManager* ToolManager = OwningTool->GetToolManager())
		{
			if (const UContextObjectStore* Store = ToolManager->GetContextObjectStore())
			{
				bStoreHasViewContext = (Store->FindContext<UGizmoViewContext>() != nullptr);
			}
		}
		if (const ACombinedTransformGizmoActor* CreatedActor = TransformGizmo->GetGizmoActor())
		{
			RoadLog_Debug(TEXT("EnsureGizmo handle=%d storeCtx=%d"),
				CreatedActor->TranslateXY != nullptr ? 1 : 0,
				bStoreHasViewContext ? 1 : 0);
		}
	}
}

void URoadPointGizmo::Update(bool bHasTarget)
{
	if (!bHasTarget)
	{
		// Hide rather than destroy, so moving between points does not churn gizmos through the manager on
		// every click. Nothing to hide means nothing to do.
		if (TransformGizmo != nullptr)
		{
			TransformGizmo->SetVisibility(false);
		}
		return;
	}

	EnsureGizmo();
	if (TransformGizmo == nullptr)
	{
		return;
	}

	TransformGizmo->SetVisibility(true);

	// Moves the gizmo onto the target. This also reaches the write callback, which is why every caller's
	// write callback has to be a no-op when the point is already where the transform says it is.
	TransformProxy->SetTransform(GetTransform());

	// A one-shot diagnostic used to stand here: a throttled (1 Hz) ray cast that reproduced the gizmo's
	// own hit test by hand and logged the view context, the cull dots and the pixel-to-world scale. It
	// answered why the handles drew but would not grab - UGizmoComponentHitTarget shares the
	// GetWorldEndpoints/GetWorldCorners cull with the render proxy, so a handle the cull reports as
	// invisible refuses every click even while the tool draws it. The finding was the same view-dependent
	// cull documented in EnsureGizmo() above, and the fix (ask for the axis arrows as well) is in the
	// tools' element flags, so the probe has no ongoing job and was removed rather than tiered.
}

void URoadPointGizmo::Render(FPrimitiveDrawInterface* PDI, const FSceneView* View) const
{
	if (PDI == nullptr || View == nullptr || TransformGizmo == nullptr || !TransformGizmo->GetGizmoActor())
	{
		return;
	}

	// Only draw while the gizmo is meant to be showing. Update() is what turns visibility on and off, and
	// the tool calls Update() from its own OnTick, so this reads the same state the actor would have used.
	const ACombinedTransformGizmoActor* GizmoActor = TransformGizmo->GetGizmoActor();
	if (GizmoActor->IsHidden() || GizmoActor->IsTemporarilyHiddenInEditor())
	{
		return;
	}

	// Shared with the hit test, so the drawn handles and the grabbable handles are the same geometry by
	// construction and cannot drift apart when one of them is tuned.
	const double PixelToWorldScale = GetPixelToWorldScale(View);

	// Kept for the press-time handle hit test, which runs from an input event that carries a ray but no
	// view; the frame the user aimed at is the frame this was measured in. See GetLastPixelToWorld().
	LastPixelToWorld = PixelToWorldScale;

	FHandleGeometry Geometry;
	if (!GetHandleGeometry(PixelToWorldScale, Geometry))
	{
		return;
	}

	const FVector Origin = Geometry.Origin;
	const FVector LocalX = Geometry.AxisX;
	const FVector LocalY = Geometry.AxisY;
	const float Thickness = View->IsPerspectiveProjection() ? (3.0f * View->FOV / 90.0f) : 3.0f;

	// The engine's own handle dimensions (CombinedTransformGizmo.cpp:105-146): arrows start 20px out and
	// are 60px long, the plane square is 30px on a side. The head and centre box are decoration on top of
	// that, and deliberately not part of the hit test.
	const float ArrowGap = Geometry.ArrowStart;
	const float ArrowEnd = Geometry.ArrowEnd;
	const float PlaneSize = Geometry.PlaneHalf;
	const float HeadLength = PixelToWorldScale * 18.0f;
	const float HeadRadius = PixelToWorldScale * 7.0f;
	const float CentreSize = PixelToWorldScale * 6.0f;

	// A cone at the tip of an axis, the way the engine widget ends its arrows. Fanning four base points
	// around the axis reads as a cone at any distance and costs three short lines.
	auto DrawArrowHead = [&](const FVector& Direction, const FLinearColor& Color)
	{
		// Two vectors perpendicular to the axis, so the head needs no reference frame of its own.
		const FVector Up = (FMath::Abs(Direction.Z) > 0.9f) ? FVector::YAxisVector : FVector::ZAxisVector;
		const FVector Side = FVector::CrossProduct(Direction, Up).GetSafeNormal();
		const FVector Side2 = FVector::CrossProduct(Direction, Side).GetSafeNormal();

		const FVector Tip = Origin + ArrowEnd * Direction;
		const FVector Base = Tip - HeadLength * Direction;
		const FVector BasePoints[4] = {
			Base + HeadRadius * Side,
			Base + HeadRadius * Side2,
			Base - HeadRadius * Side,
			Base - HeadRadius * Side2
		};
		for (int32 Index = 0; Index < 4; ++Index)
		{
			PDI->DrawLine(BasePoints[Index], Tip, Color, SDPG_Foreground, Thickness, 0.0f, true);
			PDI->DrawLine(BasePoints[Index], BasePoints[(Index + 1) % 4], Color, SDPG_Foreground, Thickness, 0.0f, true);
		}
	};

	// Only the elements the tool asked for, so what is drawn is exactly what is grabbable. A single-axis
	// tool therefore draws one arrow, and this stays correct if the axis it uses changes.
	const bool bHasX = EnumHasAnyFlags(Elements, ETransformGizmoSubElements::TranslateAxisX);
	const bool bHasY = EnumHasAnyFlags(Elements, ETransformGizmoSubElements::TranslateAxisY);
	const bool bHasPlane = EnumHasAnyFlags(Elements, ETransformGizmoSubElements::TranslatePlaneXY);

	if (bHasX)
	{
		PDI->DrawLine(Origin + ArrowGap * LocalX, Origin + ArrowEnd * LocalX,
			RoadToolStyle::Color_HandleX, SDPG_Foreground, Thickness, 0.0f, true);
		DrawArrowHead(LocalX, RoadToolStyle::Color_HandleX);
	}
	if (bHasY)
	{
		PDI->DrawLine(Origin + ArrowGap * LocalY, Origin + ArrowEnd * LocalY,
			RoadToolStyle::Color_HandleY, SDPG_Foreground, Thickness, 0.0f, true);
		DrawArrowHead(LocalY, RoadToolStyle::Color_HandleY);
	}
	if (bHasPlane)
	{
		// Only the two outer edges, as the engine widget draws them, which is what reads as "grab me".
		const FVector PlaneCorner = Origin + PlaneSize * LocalX + PlaneSize * LocalY;
		PDI->DrawLine(Origin + PlaneSize * LocalX, PlaneCorner,
			RoadToolStyle::Color_HandleXY, SDPG_Foreground, Thickness, 0.0f, true);
		PDI->DrawLine(Origin + PlaneSize * LocalY, PlaneCorner,
			RoadToolStyle::Color_HandleXY, SDPG_Foreground, Thickness, 0.0f, true);
	}

	// A small centre box, so the handles visually meet at the point they move.
	const FVector CentreCorners[4] = {
		Origin - CentreSize * LocalX - CentreSize * LocalY,
		Origin + CentreSize * LocalX - CentreSize * LocalY,
		Origin + CentreSize * LocalX + CentreSize * LocalY,
		Origin - CentreSize * LocalX + CentreSize * LocalY
	};
	for (int32 Index = 0; Index < 4; ++Index)
	{
		PDI->DrawLine(CentreCorners[Index], CentreCorners[(Index + 1) % 4],
			RoadToolStyle::Color_HandleXY, SDPG_Foreground, Thickness, 0.0f, true);
	}
}

bool URoadPointGizmo::GetHandleGeometry(double InPixelToWorld, FHandleGeometry& Out) const
{
	if (InPixelToWorld <= 0.0 || !GetTransform)
	{
		// A degenerate or unmeasured scale gives every handle zero size, which would make the whole gizmo
		// a single unpickable pixel. Refusing here keeps the caller from reading that as "nothing hit".
		return false;
	}

	const FTransform GizmoTransform = GetTransform();
	Out.Origin = GizmoTransform.GetLocation();
	Out.AxisX = GizmoTransform.GetUnitAxis(EAxis::X);
	Out.AxisY = GizmoTransform.GetUnitAxis(EAxis::Y);
	Out.PixelToWorld = InPixelToWorld;

	// The very same pixel sizes Render() draws with, so what is visible is what is grabbable.
	Out.ArrowStart = Out.PixelToWorld * 20.0;
	Out.ArrowEnd = Out.PixelToWorld * (20.0 + 60.0);
	Out.PlaneHalf = Out.PixelToWorld * 30.0;
	return true;
}

double URoadPointGizmo::GetPixelToWorldScale(const FSceneView* View) const
{
	if (View == nullptr || !GetTransform)
	{
		return 0.0;
	}

	const double Scale = GizmoRenderingUtil::CalculateLocalPixelToWorldScale(View, GetTransform().GetLocation());

	// Measure once here so both the render pass and the hit test agree on what "one pixel" means; a
	// non-positive answer means the view cannot size the handles and the gizmo is not interactive.
	return (Scale > 0.0) ? Scale : 0.0;
}

FRoadGizmoHit URoadPointGizmo::HitTestHandle(const FRay& Ray, double InPixelToWorld) const
{
	FRoadGizmoHit Result;

	FHandleGeometry Geometry;
	if (!GetHandleGeometry(InPixelToWorld, Geometry))
	{
		return Result;
	}

	// Anchored on the gizmo's own depth rather than the ray's: one pixel scale for the whole test, which
	// is what "the handles are a fixed size on screen" means. Using the ray would let a handle's tolerance
	// swell as the cursor slid along it.
	const double PixelToWorld = Geometry.PixelToWorld;

	// The pick tolerance, in pixels. Generous on purpose - a handle is 3px thick and the cursor is not
	// asked to be more precise than the eye - and still far smaller than the 20px gap between the origin
	// and the first arrow, so the centre stays grabbable by nothing but the plane.
	static constexpr double HandlePixelTolerance = 10.0;

	// Where the ray comes nearest to each handle, and how far that is in pixels. The ray is walked at the
	// tool layer's shared length rather than a local figure, so picking, tracing and this test can never
	// quietly disagree about how far "along the ray" reaches.
	auto PixelDistanceToSegment = [&Ray, PixelToWorld](const FVector& Start, const FVector& End) -> double
	{
		FVector RayPoint;
		FVector SegmentPoint;
		FMath::SegmentDistToSegmentSafe(Ray.Origin, Ray.Origin + Ray.Direction * RoadPicking::RayLength,
			Start, End, RayPoint, SegmentPoint);
		return FVector::Dist(RayPoint, SegmentPoint) / FMath::Max(PixelToWorld, SMALL_NUMBER);
	};

	const bool bHasX = EnumHasAnyFlags(Elements, ETransformGizmoSubElements::TranslateAxisX);
	const bool bHasY = EnumHasAnyFlags(Elements, ETransformGizmoSubElements::TranslateAxisY);
	const bool bHasPlane = EnumHasAnyFlags(Elements, ETransformGizmoSubElements::TranslatePlaneXY);

	// The arrows are tested first and remembered as "an arrow was hit". The plane square is 30px on a
	// side and the arrows start 20px out, so the two bands do overlap; an arrow is the far more precise
	// target there, and a user aiming at an arrow shaft means the arrow, not the plane underneath it.
	bool bArrowHit = false;

	if (bHasX)
	{
		const double Distance = PixelDistanceToSegment(Geometry.Origin + Geometry.ArrowStart * Geometry.AxisX,
			Geometry.Origin + Geometry.ArrowEnd * Geometry.AxisX);
		if (Distance <= HandlePixelTolerance)
		{
			Result.bHit = true;
			Result.Handle = ERoadGizmoHandle::AxisX;
			Result.PixelDistance = Distance;
			bArrowHit = true;
		}
	}
	if (bHasY)
	{
		const double Distance = PixelDistanceToSegment(Geometry.Origin + Geometry.ArrowStart * Geometry.AxisY,
			Geometry.Origin + Geometry.ArrowEnd * Geometry.AxisY);
		if (Distance <= HandlePixelTolerance && (!bArrowHit || Distance < Result.PixelDistance))
		{
			Result.bHit = true;
			Result.Handle = ERoadGizmoHandle::AxisY;
			Result.PixelDistance = Distance;
			bArrowHit = true;
		}
	}
	if (bHasPlane && !bArrowHit)
	{
		// The square, tested as a solid rather than as the two edges Render() strokes: aiming inside a
		// 30px square and being told "you missed, the line is 3px wide" would be a hostile way to grab it.
		// The ray is intersected with the plane the square lies in, and the meeting point is measured in
		// the square's own axes.
		const FVector Normal = FVector::CrossProduct(Geometry.AxisX, Geometry.AxisY).GetSafeNormal();
		const double Denominator = Ray.Direction | Normal;
		if (FMath::Abs(Denominator) > SMALL_NUMBER)
		{
			const double DistanceAlongRay = ((Geometry.Origin - Ray.Origin) | Normal) / Denominator;
			if (DistanceAlongRay > 0.0)
			{
				const FVector PlanePoint = Ray.Origin + Ray.Direction * DistanceAlongRay;
				const FVector Local = PlanePoint - Geometry.Origin;
				const double AlongX = Local | Geometry.AxisX;
				const double AlongY = Local | Geometry.AxisY;
				if (FMath::Abs(AlongX) <= Geometry.PlaneHalf && FMath::Abs(AlongY) <= Geometry.PlaneHalf)
				{
					Result.bHit = true;
					Result.Handle = ERoadGizmoHandle::PlaneXY;
					Result.PixelDistance = 0.0;
				}
			}
		}
	}

	return Result;
}

void URoadPointGizmo::BeginDrag(ERoadGizmoHandle Handle, const FRay& PressRay)
{
	if (Handle == ERoadGizmoHandle::None || !GetTransform)
	{
		return;
	}

	// The anchor every later frame measures against. Taken from the data, not from the cursor, so a drag
	// that was clamped on the very first move still springs back to where the point really is.
	DragStartOrigin = GetTransform().GetLocation();

	// Where the cursor was inside the drag subspace at the press, in the same subspace every later frame
	// will be measured in - the engine's UAxisPositionGizmo::OnClickPress / UPlanePositionGizmo::
	// OnClickPress do exactly this with exactly these helpers.
	const FVector AxisX = GetTransform().GetUnitAxis(EAxis::X);
	const FVector AxisY = GetTransform().GetUnitAxis(EAxis::Y);

	// The subspace is fixed at the press, directions included: for a gizmo rotated onto the data it
	// edits - a boundary tangent, a road frame - the directions rotate as this very drag moves the data,
	// and a per-frame re-read would measure the cursor in a different frame than the anchor was taken in.
	DragStartAxisX = AxisX;
	DragStartAxisY = AxisY;

	if (Handle == ERoadGizmoHandle::AxisX || Handle == ERoadGizmoHandle::AxisY)
	{
		const FVector Axis = (Handle == ERoadGizmoHandle::AxisX) ? AxisX : AxisY;
		FVector NearestLinePoint;
		float LineParameter;
		FVector NearestRayPoint;
		float RayParameter;
		GizmoMath::NearestPointOnLineToRay(DragStartOrigin, Axis, PressRay.Origin, PressRay.Direction,
			NearestLinePoint, LineParameter, NearestRayPoint, RayParameter);
		DragStartCursor = NearestLinePoint;
	}
	else
	{
		const FVector Normal = FVector::CrossProduct(AxisX, AxisY).GetSafeNormal();
		FVector HitPoint;
		bool bIntersects = false;
		GizmoMath::RayPlaneIntersectionPoint(DragStartOrigin, Normal,
			PressRay.Origin, PressRay.Direction, bIntersects, HitPoint);
		// A miss leaves the cursor anchor at the plane origin, which makes the first frame read a false
		// motion. A press accepted by HitTestHandle hit the plane square, so this is defensive only -
		// but a false motion is worse than none, so fall back to the origin.
		DragStartCursor = bIntersects ? HitPoint : DragStartOrigin;
	}

	// Brackets the drag for the tool exactly the way the gizmo's own OnBeginTransformEdit would have.
	bDragging = true;
	TransformProxy->BeginTransformEditSequence();
}

void URoadPointGizmo::EndDrag()
{
	if (!bDragging)
	{
		return;
	}

	bDragging = false;

	// Fires OnEndTransformEdit, which is where the tool's deferred geometry rebuild hangs. The undo step
	// is the tool's business, not this bracket's: a tool that stores a station records one array change
	// for the whole drag, which is not something a transform proxy can express.
	TransformProxy->EndTransformEditSequence();
}

void URoadPointGizmo::DragHandle(ERoadGizmoHandle Handle, const FRay& CurrentRay)
{
	if (Handle == ERoadGizmoHandle::None || !GetTransform || !SetTransform)
	{
		return;
	}

	// The drag subspace is fixed at the press: the same line, or the same plane, through DragStartOrigin,
	// in the directions captured there. GetTransform()'s data-derived rotation may well have moved since -
	// this drag is what moved it - so the press-time directions are the ones the anchor was measured with,
	// and the ones every later frame has to be measured with too.
	const FVector AxisX = DragStartAxisX;
	const FVector AxisY = DragStartAxisY;

	// Where the cursor is now, in the very same subspace BeginDrag() measured it in.
	FVector CursorNow;
	if (Handle == ERoadGizmoHandle::AxisX || Handle == ERoadGizmoHandle::AxisY)
	{
		const FVector Axis = (Handle == ERoadGizmoHandle::AxisX) ? AxisX : AxisY;
		FVector NearestLinePoint;
		float LineParameter;
		FVector NearestRayPoint;
		float RayParameter;
		GizmoMath::NearestPointOnLineToRay(DragStartOrigin, Axis, CurrentRay.Origin, CurrentRay.Direction,
			NearestLinePoint, LineParameter, NearestRayPoint, RayParameter);
		CursorNow = NearestLinePoint;
	}
	else
	{
		const FVector Normal = FVector::CrossProduct(AxisX, AxisY).GetSafeNormal();
		FVector HitPoint;
		bool bIntersects = false;
		GizmoMath::RayPlaneIntersectionPoint(DragStartOrigin, Normal,
			CurrentRay.Origin, CurrentRay.Direction, bIntersects, HitPoint);
		if (!bIntersects)
		{
			// Looking edge-on along the drag plane: there is no intersection to read a position from,
			// and the honest answer is to leave the point alone for this frame rather than guess.
			return;
		}
		CursorNow = HitPoint;
	}

	// The cursor's motion since the press, projected onto the directions the handle may move along. For
	// an axis both anchors sit on the line, so the difference is already axial; for the plane the double
	// projection is what keeps the motion in-plane even if a future tool's frame is not orthonormal.
	const FVector CursorMotion = CursorNow - DragStartCursor;
	FVector Motion;
	if (Handle == ERoadGizmoHandle::AxisX)
	{
		Motion = AxisX * (CursorMotion | AxisX);
	}
	else if (Handle == ERoadGizmoHandle::AxisY)
	{
		Motion = AxisY * (CursorMotion | AxisY);
	}
	else
	{
		Motion = AxisX * (CursorMotion | AxisX) + AxisY * (CursorMotion | AxisY);
	}

	// Hand the write callback an absolute transform, the same shape a gizmo-driven drag produces: the
	// anchor plus how far the cursor has travelled since the press.
	FTransform NewTransform = GetTransform();
	NewTransform.SetLocation(DragStartOrigin + Motion);
	SetTransform(NewTransform);
}

void URoadPointGizmo::NotifyDragEnded()
{
	if (OnDragEnded)
	{
		OnDragEnded();
	}
}

void URoadPointGizmo::OnBeginTransformEdit(UTransformProxy* Proxy)
{
	bDragging = true;
}

void URoadPointGizmo::OnEndTransformEdit(UTransformProxy* Proxy)
{
	bDragging = false;
	if (OnDragEnded)
	{
		OnDragEnded();
	}
}
