#include "RoadInspector.h"
#include "RoadEdMode.h"
#include "RoadObjectDetails.h"
#include "IDetailsView.h"
#include "IStructureDetailsView.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "UObject/StructOnScope.h"

namespace
{
	/** Empty struct data: FStructOnScope(nullptr, 0) cannot be fed to MakeShared directly; template deduction fails. */
	TSharedRef<FStructOnScope> MakeEmptyStructOnScope()
	{
		return MakeShared<FStructOnScope>((const UStruct*)nullptr, (uint8*)nullptr);
	}
}

FRoadInspector::~FRoadInspector() = default;

void FRoadInspector::Init(FEdModeRoad* InNotifyHook)
{
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

	FDetailsViewArgs Args;
	Args.bHideSelectionTip = true;
	Args.bAllowSearch = false;

	SettingsView = PropertyEditorModule.CreateDetailView(Args);
	SettingsView->RegisterInstancedCustomPropertyLayout(UObject::StaticClass(), FOnGetDetailCustomizationInstance::CreateStatic(&FObjectDetails::MakeInstance));

	// Both selection panels need the NotifyHook: before a property changes, FEdModeRoad::NotifyPreChange opens a
	// transaction and forwards to the current tool's Modify(), which is what makes the edit undoable. It must be
	// set BEFORE the views are created -- FDetailsViewArgs is copied into them at creation time.
	// The settings panel is deliberately left out: it edits a config CDO that persists itself via SaveConfig.
	Args.NotifyHook = InNotifyHook;
	ObjectDetailsView = PropertyEditorModule.CreateDetailView(Args);

	FStructureDetailsViewArgs StructureViewArgs;
	StructureViewArgs.bShowObjects = true;
	StructureViewArgs.bShowInterfaces = true;
	StructDetailsView = PropertyEditorModule.CreateStructureDetailView(Args, StructureViewArgs, nullptr);
}

TSharedRef<SWidget> FRoadInspector::GetSettingsWidget() const
{
	return SettingsView.ToSharedRef();
}

TSharedRef<SWidget> FRoadInspector::GetObjectWidget() const
{
	return ObjectDetailsView.ToSharedRef();
}

TSharedRef<SWidget> FRoadInspector::GetStructWidget() const
{
	return StructDetailsView->GetWidget().ToSharedRef();
}

void FRoadInspector::SetSettings(UObject* Settings)
{
	if (SettingsView.IsValid())
		SettingsView->SetObject(Settings);
}

void FRoadInspector::SetStructVisible(bool bVisible)
{
	if (StructDetailsView.IsValid())
		StructDetailsView->GetWidget()->SetVisibility(bVisible ? EVisibility::Visible : EVisibility::Collapsed);
}

void FRoadInspector::SetObjectVisible(bool bVisible)
{
	if (ObjectDetailsView.IsValid())
		ObjectDetailsView->SetVisibility(bVisible ? EVisibility::Visible : EVisibility::Collapsed);
}

void FRoadInspector::ShowStruct(UScriptStruct* Struct, void* Data, FRoadPropertyChanged OnChanged)
{
	if (!StructDetailsView.IsValid())
		return;

	FOnFinishedChangingProperties& Delegate = StructDetailsView->GetOnFinishedChangingPropertiesDelegate();
	Delegate.Clear();

	if (Struct && Data)
	{
		StructDetailsView->SetStructureData(MakeShared<FStructOnScope>(Struct, (uint8*)Data));
		if (OnChanged)
			Delegate.AddLambda([OnChanged](const FPropertyChangedEvent& PropertyChangedEvent) { OnChanged(PropertyChangedEvent); });
	}
	else
	{
		StructDetailsView->SetStructureData(MakeEmptyStructOnScope());
	}

	SetObjectVisible(false);
	SetStructVisible(true);
}

void FRoadInspector::ShowObject(UObject* Object, FRoadPropertyChanged OnChanged)
{
	if (!ObjectDetailsView.IsValid())
		return;

	FOnFinishedChangingProperties& Delegate = ObjectDetailsView->OnFinishedChangingProperties();
	Delegate.Clear();

	ObjectDetailsView->SetObject(Object);
	if (Object && OnChanged)
		Delegate.AddLambda([OnChanged](const FPropertyChangedEvent& PropertyChangedEvent) { OnChanged(PropertyChangedEvent); });

	SetObjectVisible(true);
	SetStructVisible(false);
}

void FRoadInspector::Clear()
{
	if (StructDetailsView.IsValid())
	{
		StructDetailsView->GetOnFinishedChangingPropertiesDelegate().Clear();
		StructDetailsView->SetStructureData(MakeEmptyStructOnScope());
	}
	if (ObjectDetailsView.IsValid())
	{
		ObjectDetailsView->OnFinishedChangingProperties().Clear();
		ObjectDetailsView->SetObject(nullptr);
	}
	// Nothing is selected: hide both selection panels instead of leaving two empty ones behind.
	// The settings panel stays visible -- it is the only content left while the tool is idle.
	SetStructVisible(false);
	SetObjectVisible(false);
}
