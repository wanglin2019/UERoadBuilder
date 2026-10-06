// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

#include "Tools/RoadTool_JunctionLink.h"

#include "RoadBoundary.h"
#include "RoadCurve.h"
#include "RoadScene.h"
#include "SceneManagement.h"
#include "Tools/RoadArrayChange.h"
#include "Tools/RoadPicking.h"
#include "UObject/UnrealType.h"

#define LOCTEXT_NAMESPACE "RoadTool_JunctionLink"

namespace
{
	/**
	 * Reports the segments a junction draws before one of its links is picked: the cross-line at each gate,
	 * and the centreline of the ramp the gate owns.
	 *
	 * Every segment is tagged as the junction as a whole, because that is what the legacy drawing pass did
	 * - it put one whole-junction hit proxy around all of it.
	 *
	 * The legacy code also resolved the next gate's destination-side road edge here and never used it; the
	 * walk below omits that lookup rather than carrying a dead local along.
	 */
	template <typename FSegment>
	void ForEachJunctionOutline(AJunctionActor* Junction, FSegment&& OnSegment)
	{
		if (Junction == nullptr)
		{
			return;
		}

		const TArray<FJunctionGate>& Gates = Junction->Gates;
		for (int32 GateIndex = 0; GateIndex < Gates.Num(); ++GateIndex)
		{
			const FJunctionGate& Gate = Gates[GateIndex];
			if (Gate.Road == nullptr)
			{
				continue;
			}

			// The cross-line spans the road, from the edge on its far side to the edge on its near side.
			const int32 SrcSide = Gate.Sign > 0 ? 0 : 1;
			URoadBoundary* SrcBoundary = Gate.Road->GetRoadEdge(SrcSide);
			URoadBoundary* PrevBoundary = Gate.Road->GetRoadEdge(!SrcSide);
			if (SrcBoundary != nullptr && PrevBoundary != nullptr)
			{
				OnSegment(PrevBoundary->GetPos(Gate.Dist), SrcBoundary->GetPos(Gate.Dist), INDEX_NONE, INDEX_NONE);
			}

			// The ramp the gate owns is drawn as its own centreline.
			if (Gate.Links.IsValidIndex(1) && Gate.Links[1].Road != nullptr &&
				Gate.Links[1].Road->BaseCurve != nullptr)
			{
				const FPolyline& Curve = Gate.Links[1].Road->BaseCurve->Curve;
				for (int32 Point = 0; Point < Curve.Points.Num() - 1; ++Point)
				{
					OnSegment(Curve.Points[Point].Pos, Curve.Points[Point + 1].Pos, INDEX_NONE, INDEX_NONE);
				}
			}
		}
	}

	/**
	 * Reports the segments of every link curve of one junction, tagged with the gate and link they belong
	 * to - which is the identity the per-link hit proxies carried, and what a click needs to select one.
	 *
	 * A gate's links are the roads continuing through the junction; the second one is drawn as its
	 * centreline and the others as their right lane, which is the pairing the legacy pass used.
	 */
	template <typename FSegment>
	void ForEachJunctionLink(AJunctionActor* Junction, FSegment&& OnSegment)
	{
		if (Junction == nullptr)
		{
			return;
		}

		const TArray<FJunctionGate>& Gates = Junction->Gates;
		for (int32 GateIndex = 0; GateIndex < Gates.Num(); ++GateIndex)
		{
			const TArray<FJunctionLink>& Links = Gates[GateIndex].Links;
			for (int32 LinkIndex = 0; LinkIndex < Links.Num(); ++LinkIndex)
			{
				ARoadActor* Road = Links[LinkIndex].Road;
				if (Road == nullptr || Road->BaseCurve == nullptr)
				{
					continue;
				}

				URoadCurve* Curve = (LinkIndex == 1)
					? static_cast<URoadCurve*>(Road->BaseCurve)
					: static_cast<URoadCurve*>(Road->BaseCurve->RightLane);
				if (Curve == nullptr)
				{
					continue;
				}

				for (int32 Point = 0; Point < Curve->Curve.Points.Num() - 1; ++Point)
				{
					OnSegment(Curve->Curve.Points[Point].Pos, Curve->Curve.Points[Point + 1].Pos,
						GateIndex, LinkIndex);
				}
			}
		}
	}
}

UInteractiveTool* URoadTool_JunctionLinkBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	URoadTool_JunctionLink* NewTool = NewObject<URoadTool_JunctionLink>(SceneState.ToolManager);
	NewTool->SetEditingWorld(SceneState.World);
	return NewTool;
}

void URoadTool_JunctionLink::Setup()
{
	UInteractiveTool::Setup();

	// Only the left button: the legacy tool had no right-button behaviour, and registering one here would
	// swallow the context menu the editor shows when a tool declines a click.
	AddClickBehavior();

	Properties = NewObject<URoadTool_JunctionLinkProperties>(this, TEXT("JunctionLinkSettings"));
	AddToolPropertySource(Properties);

	// A junction may already be selected when the tool starts, and this tool reads that selection rather
	// than owning it - so the panel is synchronised against it, with nothing written back.
	SyncProperties();
}

FJunctionLink* URoadTool_JunctionLink::GetCurrentLink() const
{
	AJunctionActor* Junction = GetSelectedJunction();
	if (Junction == nullptr || !Junction->Gates.IsValidIndex(GateIndex))
	{
		return nullptr;
	}

	TArray<FJunctionLink>& Links = Junction->Gates[GateIndex].Links;
	return Links.IsValidIndex(LinkIndex) ? &Links[LinkIndex] : nullptr;
}

void URoadTool_JunctionLink::SelectLink(AJunctionActor* Junction, int32 Gate, int32 Link)
{
	// Reading the selection out of the host rather than caching it keeps the tool honest about a junction
	// that was destroyed or merged while it was open; only a change is written back.
	if (GetSelectedJunction() != Junction)
	{
		SetSelectedJunction(Junction);
	}

	const bool bValid = (Junction != nullptr && Junction->Gates.IsValidIndex(Gate) &&
		Junction->Gates[Gate].Links.IsValidIndex(Link));
	GateIndex = bValid ? Gate : INDEX_NONE;
	LinkIndex = bValid ? Link : INDEX_NONE;

	SyncProperties();
	RequestRedraw();
}

void URoadTool_JunctionLink::SyncProperties()
{
	AJunctionActor* Junction = GetSelectedJunction();
	const FJunctionLink* Link = GetCurrentLink();
	if (Junction == nullptr || Link == nullptr)
	{
		Properties->GateIndex = INDEX_NONE;
		Properties->LinkIndex = INDEX_NONE;
		Properties->Road = nullptr;
		Properties->InputRoad = nullptr;
		Properties->OutputRoad = nullptr;
		Properties->Radius = 1000.0;
		return;
	}

	Properties->GateIndex = GateIndex;
	Properties->LinkIndex = LinkIndex;
	Properties->Road = Link->Road;
	Properties->InputRoad = Link->InputRoad;
	Properties->OutputRoad = Link->OutputRoad;
	Properties->Radius = Link->Radius;
}

void URoadTool_JunctionLink::ApplyProperties(FProperty* Property)
{
	AJunctionActor* Junction = GetSelectedJunction();
	FJunctionLink* Link = GetCurrentLink();
	if (Junction == nullptr || Link == nullptr || Property == nullptr)
	{
		return;
	}

	// Only the radius is the user's to change - the three roads are the link's identity.
	if (Property->GetFName() != GET_MEMBER_NAME_CHECKED(URoadTool_JunctionLinkProperties, Radius))
	{
		return;
	}

	TUniquePtr<FRoadArrayChange> Change = FRoadArrayChange::CaptureBefore(Junction, GetGatesProperty());
	if (Change == nullptr)
	{
		return;
	}

	Link->Radius = Properties->Radius;

	Change->CaptureAfter();
	EmitArrayChange(Junction, MoveTemp(Change), LOCTEXT("EditJunctionLink", "Edit Junction Link"));

	// The link curve is regenerated from the radius, so the geometry has to be rebuilt - which is what the
	// legacy NotifyHook asked the scene for.
	RequestRebuild();
}

void URoadTool_JunctionLink::OnPropertyModified(UObject* PropertySet, FProperty* Property)
{
	UInteractiveTool::OnPropertyModified(PropertySet, Property);

	if (PropertySet != Properties || Property == nullptr)
	{
		return;
	}

	ApplyProperties(Property);
}

void URoadTool_JunctionLink::PickUnderRay(const FRay& Ray, AJunctionActor*& OutJunction, int32& OutGate,
	int32& OutLink) const
{
	OutJunction = nullptr;
	OutGate = INDEX_NONE;
	OutLink = INDEX_NONE;

	FRoadHitCollector Collector(Ray);
	ARoadScene* Scene = GetRoadScene();
	AJunctionActor* Selected = GetSelectedJunction();
	if (Selected != nullptr)
	{
		// A junction is picked, so its link curves are what is on screen and what the ray may choose from.
		ForEachJunctionLink(Selected, [&Collector, Selected](const FVector& Start, const FVector& End,
			int32 Gate, int32 Link)
		{
			Collector.ConsiderSegment(Start, End, Selected, Gate, Link);
		});
	}
	else if (Scene != nullptr)
	{
		// Nothing is picked yet, so whole junctions are on offer.
		for (AJunctionActor* Junction : Scene->Junctions)
		{
			if (Junction == nullptr)
			{
				continue;
			}

			ForEachJunctionOutline(Junction, [&Collector, Junction](const FVector& Start, const FVector& End,
				int32 Gate, int32 Link)
			{
				Collector.ConsiderSegment(Start, End, Junction);
			});
		}
	}

	const FRoadRayHit Hit = Collector.Resolve();
	if (Hit.bHit)
	{
		OutJunction = Cast<AJunctionActor>(Hit.Owner);
		OutGate = Hit.Index;
		OutLink = Hit.SubIndex;
	}
}

void URoadTool_JunctionLink::OnRoadClicked(const FInputDeviceRay& ClickPos, bool bRightButton)
{
	if (bRightButton)
	{
		return;
	}

	AJunctionActor* HitJunction = nullptr;
	int32 HitGate = INDEX_NONE;
	int32 HitLink = INDEX_NONE;
	PickUnderRay(ClickPos.WorldRay, HitJunction, HitGate, HitLink);

	// A click that hits neither a junction nor one of its links leaves the selection alone, which is what
	// the legacy tool did by simply not claiming the click.
	if (HitJunction != nullptr)
	{
		SelectLink(HitJunction, HitGate, HitLink);
	}
}

void URoadTool_JunctionLink::SelectParent()
{
	// The legacy tool did not override InputKey, so its Escape went straight through the base class: no
	// road selected, a junction selected - and the junction was cleared in one step, link and all, since
	// the tool's own selection lived on the junction. Calling SelectLink(nullptr) reproduces that exactly
	// (it clears the junction too), and the base then finds nothing left to step up.
	SelectLink(nullptr, INDEX_NONE, INDEX_NONE);
	Super::SelectParent();
}

FArrayProperty* URoadTool_JunctionLink::GetGatesProperty()
{
	return FindFProperty<FArrayProperty>(AJunctionActor::StaticClass(),
		GET_MEMBER_NAME_CHECKED(AJunctionActor, Gates));
}

void URoadTool_JunctionLink::Render(IToolsContextRenderAPI* RenderAPI)
{
	UInteractiveTool::Render(RenderAPI);

	FPrimitiveDrawInterface* PDI = (RenderAPI != nullptr) ? RenderAPI->GetPrimitiveDrawInterface() : nullptr;
	if (PDI == nullptr)
	{
		return;
	}

	AJunctionActor* Selected = GetSelectedJunction();
	if (Selected != nullptr)
	{
		ForEachJunctionLink(Selected, [this, PDI](const FVector& Start, const FVector& End, int32 Gate, int32 Link)
		{
			const bool bSelectedLink = (Gate == GateIndex) && (Link == LinkIndex);
			PDI->DrawLine(Start, End,
				bSelectedLink ? RoadToolStyle::Color_Select : RoadToolStyle::Color_Road,
				SDPG_Foreground, RoadToolStyle::Thickness_Road,
				bSelectedLink ? RoadToolStyle::DepthBias_Select : 0.0f, true);
		});
		return;
	}

	ARoadScene* Scene = GetRoadScene();
	if (Scene == nullptr)
	{
		return;
	}

	// Every junction is drawn while none is picked, so that one can be picked.
	AJunctionActor* SelectedJunction = GetSelectedJunction();
	for (AJunctionActor* Junction : Scene->Junctions)
	{
		if (Junction == nullptr)
		{
			continue;
		}

		const FLinearColor Color = (Junction == SelectedJunction)
			? RoadToolStyle::Color_Select
			: RoadToolStyle::Color_Road;
		ForEachJunctionOutline(Junction, [PDI, &Color](const FVector& Start, const FVector& End,
			int32 Gate, int32 Link)
		{
			PDI->DrawLine(Start, End, Color, SDPG_Foreground, RoadToolStyle::Thickness_Road, 0.0f, true);
		});
	}
}

#undef LOCTEXT_NAMESPACE
