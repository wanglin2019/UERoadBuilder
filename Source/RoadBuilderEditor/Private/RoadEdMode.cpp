// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

#include "RoadEdMode.h"
#include "EditorModes.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Editor/TransBuffer.h"
#include "DynamicMeshBuilder.h"
#include "ScopedTransaction.h"
#include "RoadEdModeToolkit.h"
#include "RoadToolbars/RoadToolbar.h"
#include "Toolkits/ToolkitManager.h"
#include "RoadInspector.h"
#include "Settings.h"

#include "RoadTools/RoadTool.h"
#include "RoadTools/RoadTool_File.h"
#include "RoadTools/RoadTool_RoadPlan.h"
#include "RoadTools/RoadTool_RoadHeight.h"
#include "RoadTools/RoadTool_RoadChop.h"
#include "RoadTools/RoadTool_RoadSplit.h"
#include "RoadTools/RoadTool_JunctionLink.h"
#include "RoadTools/RoadTool_LaneEdit.h"
#include "RoadTools/RoadTool_LaneCarve.h"
#include "RoadTools/RoadTool_LaneWidth.h"
#include "RoadTools/RoadTool_MarkingLane.h"
#include "RoadTools/RoadTool_MarkingPoint.h"
#include "RoadTools/RoadTool_MarkingCurve.h"
#include "RoadTools/RoadTool_GroundEdit.h"
#include "RoadTools/RoadTool_Settings.h"

IMPLEMENT_HIT_PROXY(HRoadProxy, HHitProxy);
IMPLEMENT_HIT_PROXY(HGroundProxy, HHitProxy);
IMPLEMENT_HIT_PROXY(HJunctionProxy, HHitProxy);
IMPLEMENT_HIT_PROXY(HRoadCurveProxy, HHitProxy);
IMPLEMENT_HIT_PROXY(HRoadMarkingProxy, HHitProxy);

#define LOCTEXT_NAMESPACE "RoadBuilder"

FEdModeRoad::FEdModeRoad()
{
	// Tool registration. Order no longer matters: identity is reported by each tool's GetToolType(), lookups go through ToolsById.
	// To reorder tools here (or insert new ones in between), just move the lines — nothing else is affected.
	auto Register = [this](FRoadTool* Tool)
	{
		const ERoadToolType ToolType = Tool->GetToolType();
		checkf(!ToolsById.Contains(ToolType), TEXT("Duplicate tool identity registered: %d"), (int32)ToolType);
		Tools.Add(Tool);
		ToolsById.Add(ToolType, Tool);
	};

	Register(new FRoadTool_File);
	Register(new FRoadTool_RoadPlan);
	Register(new FRoadTool_RoadHeight);
	Register(new FRoadTool_RoadChop);
	Register(new FRoadTool_RoadSplit);
	Register(new FRoadTool_JunctionLink);
	Register(new FRoadTool_LaneEdit);
	Register(new FRoadTool_LaneCarve);
	Register(new FRoadTool_LaneWidth);
	Register(new FRoadTool_MarkingLane);
	Register(new FRoadTool_MarkingPoint);
	Register(new FRoadTool_MarkingCurve);
	Register(new FRoadTool_GroundEdit);
	Register(new FRoadTool_Settings);
}

FEdModeRoad::~FEdModeRoad()
{
	for (FModeTool* Tool : Tools)
		delete Tool;
}

ERoadToolType FEdModeRoad::GetCurrentToolType() const
{
	return CurrentTool ? static_cast<FRoadTool*>(CurrentTool)->GetToolType() : ERoadToolType::None;
}

FRoadTool* FEdModeRoad::FindRoadTool(ERoadToolType ToolType) const
{
	FRoadTool* const* Found = ToolsById.Find(ToolType);
	return Found ? *Found : nullptr;
}

void FEdModeRoad::SetCurrentToolByType(ERoadToolType ToolType)
{
	FRoadTool* Tool = FindRoadTool(ToolType);
	if (!Tool)
		return;

	Tool->Reset();
	SetCurrentTool(Tool);
	FEditorViewportClient* Client = GLevelEditorModeTools().GetFocusedViewportClient();
	Client->Invalidate();

	// Switching tools also swaps the settings panel: the settings object is tool metadata and follows the tool identity
	if (Inspector)
		Inspector->SetSettings(GetToolSettings(ToolType));
}

UObject* FEdModeRoad::GetToolSettings(ERoadToolType ToolType)
{
	switch (ToolType)
	{
	case ERoadToolType::File:		return GetMutableDefault<USettings_File>();
	case ERoadToolType::RoadPlan:	return GetMutableDefault<USettings_RoadPlan>();
	case ERoadToolType::RoadSplit:	return GetMutableDefault<USettings_RoadSplit>();
	case ERoadToolType::Settings:	return GetMutableDefault<USettings_Global>();
	default:						return nullptr;
	}
}

void FEdModeRoad::RefreshInspectorSettings()
{
	if (Inspector)
		Inspector->SetSettings(GetToolSettings(GetCurrentToolType()));
}

void FEdModeRoad::OnUndo(const FTransactionContext& InTransactionContext, bool bSucceeded)
{
	Scene->Rebuild();
}

void FEdModeRoad::OnRedo(const FTransactionContext& InTransactionContext, bool bSucceeded)
{
	Scene->Rebuild();
}

void FEdModeRoad::Enter()
{
	FEdMode::Enter();
	UTransBuffer* TransBuffer = CastChecked<UTransBuffer>(GEditor->Trans);
	TransBuffer->OnUndo().AddRaw(this, &FEdModeRoad::OnUndo);
	TransBuffer->OnRedo().AddRaw(this, &FEdModeRoad::OnRedo);
	if (!Toolkit.IsValid())
	{
		// Panel is created before the toolkit: SRoadEdit::Construct borrows it to lay the three views out
		Inspector = MakeShared<FRoadInspector>();
		Inspector->Init(this);

		Toolkit = MakeShareable(new FRoadEdModeToolkit);
		Toolkit->Init(Owner->GetToolkitHost());
		Toolkit->SetCurrentPalette(PaletteName_Road);

		// Once the panel is ready, apply the current tool's settings panel (later tool switches go through SetCurrentToolByType)
		RefreshInspectorSettings();
	}
	GLevelEditorModeTools().SetCoordSystem(COORD_Local);
	Scene = Cast<ARoadScene>(UGameplayStatics::GetActorOfClass(GetWorld(), ARoadScene::StaticClass()));
	if (!Scene)
		Scene = GetWorld()->SpawnActor<ARoadScene>();
}

void FEdModeRoad::Exit()
{
	Scene = nullptr;
	SelectedRoad = nullptr;
	SelectedJunction = nullptr;
	FToolkitManager::Get().CloseToolkit(Toolkit.ToSharedRef());
	Toolkit.Reset();
	// Widgets were destroyed in the previous step; no view references the panel anymore, safe to release
	Inspector.Reset();
	UTransBuffer* TransBuffer = CastChecked<UTransBuffer>(GEditor->Trans);
	TransBuffer->OnUndo().RemoveAll(this);
	TransBuffer->OnRedo().RemoveAll(this);
	FEdMode::Exit();
}

void FEdModeRoad::NotifyPreChange(FProperty* PropertyAboutToChange)
{
	const FScopedTransaction Transaction(LOCTEXT("NotifyPreChange", "NotifyPreChange"));
	static_cast<FRoadTool*>(CurrentTool)->NotifyPreChange(PropertyAboutToChange);
}

void FEdModeRoad::PostUndo()
{
	static_cast<FRoadTool*>(CurrentTool)->Reset();
}

bool FEdModeRoad::GetCursor(EMouseCursor::Type& OutCursor) const
{
	FEditorViewportClient* Client = GLevelEditorModeTools().GetFocusedViewportClient();
	HHitProxy* HitProxy = Client->Viewport->GetHitProxy(Client->GetCachedMouseX(), Client->GetCachedMouseY());
	if (HitProxy)
	{
		OutCursor = HitProxy->IsA(HActor::StaticGetType()) ? EMouseCursor::Default : HitProxy->GetMouseCursor();
		return true;
	}
	return false;
}

bool FEdModeRoad::ShouldDrawWidget() const
{
	if (((FRoadTool*)CurrentTool)->ShouldDrawWidget())
		return true;
	return FEdMode::ShouldDrawWidget();
}

FVector FEdModeRoad::GetWidgetLocation() const
{
	return ((FRoadTool*)CurrentTool)->GetWidgetLocation();
}

bool FEdModeRoad::GetCustomDrawingCoordinateSystem(FMatrix& InMatrix, void* InData)
{
	return ((FRoadTool*)CurrentTool)->GetCustomDrawingCoordinateSystem(InMatrix, InData);
}

EAxisList::Type FEdModeRoad::GetWidgetAxisToDraw(UE::Widget::EWidgetMode InWidgetMode) const
{
	if (InWidgetMode != UE::Widget::WM_Translate)
		return EAxisList::None;
	return ((FRoadTool*)CurrentTool)->GetWidgetAxisToDraw();
}

bool FEdModeRoad::HandleClick(FEditorViewportClient* InViewportClient, HHitProxy* HitProxy, const FViewportClick& Click)
{
	if (((FRoadTool*)CurrentTool)->HandleClick(InViewportClient, HitProxy, Click))
		return true;
#if 0
	if (Click.GetKey() == EKeys::RightMouseButton)
	{
		TSharedPtr<SEditorViewport> ViewportWidget = InViewportClient->GetEditorViewportWidget();
		TSharedPtr<SWidget> MenuContents = ((FRoadTool*)CurrentTool)->GenerateContextMenu();
		FSlateApplication::Get().PushMenu(ViewportWidget.ToSharedRef(), FWidgetPath(), MenuContents.ToSharedRef(), Click.GetClickPos(), FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu));
		return true;
	}
#endif
	return FEdMode::HandleClick(InViewportClient, HitProxy, Click);
}
#undef LOCTEXT_NAMESPACE