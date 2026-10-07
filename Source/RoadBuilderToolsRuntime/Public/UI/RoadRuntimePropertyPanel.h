// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class SRoadRuntimePropertyRow;
class UInteractiveTool;
class UObject;

/**
 * Property panel for the runtime editing host.
 *
 * The editor host shows tool properties in an IDetailsView, which is part of UnrealEd's PropertyEditor
 * module - a module that exists only in an editor build. A game therefore has no details view and has to
 * supply its own, which is what this widget is.
 *
 * It is not a re-implementation of the details view and does not try to be. It walks the UObjects a tool
 * hands back from GetToolProperties() and lays out their reflected properties as a flat list of labelled
 * rows. That covers what the road tools actually expose - configuration structs of numbers, booleans and
 * enums drawn from USettings_* - and deliberately stops there:
 *
 *   - no category grouping, no search box, no multi-object editing, no undo integration beyond the edit's
 *     own transaction
 *   - structs and arrays are shown as their leaf properties where they are simple, and skipped where
 *     editing them properly would need the details view's own machinery
 *
 * The panel follows the active tool through IRoadEditorContext::NotifyActiveToolChanged(), which is the
 * one host notification the editor-side implementation ignores precisely because the editor's toolkit
 * already handles it. Here it is the mechanism.
 *
 * Rebuild, not refresh: the row set changes wholesale when the tool changes, and tool switches are rare
 * compared to frames, so the simple thing is also the right thing.
 */
class ROADBUILDERTOOLSRUNTIME_API SRoadRuntimePropertyPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SRoadRuntimePropertyPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/**
	 * Point the panel at a tool's property sets. Pass null to clear it (no tool active).
	 *
	 * Called by the host whenever the active tool changes.
	 */
	void SetTool(UInteractiveTool* InTool);

private:
	/** Rebuild the row list from the current tool's property sets. */
	void RebuildRows();

	/** One row per editable leaf property of every property set the tool exposes. */
	void BuildRowsForObject(UObject* Object);

	/** Build a single labelled row for one property. Returns an empty widget for unsupported types. */
	TSharedRef<class SWidget> BuildRowForProperty(UObject* Owner, class FProperty* Property);

	/** Title line: which tool the rows below belong to. */
	FText GetToolTitle() const;

	/** Weak: the tool is owned by the tool manager and may end at any time. */
	TWeakObjectPtr<UInteractiveTool> Tool;

	/** Container the rows are rebuilt into. */
	TSharedPtr<class SVerticalBox> RowContainer;
};
