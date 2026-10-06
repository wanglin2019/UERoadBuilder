// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class RoadBuilderToolsEditor : ModuleRules
	{
		public RoadBuilderToolsEditor(ReadOnlyTargetRules Target) : base(Target)
		{
			PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
				}
				);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"CoreUObject",
					"Engine",
					"InputCore",
					"Slate",
					"SlateCore",
					"EditorFramework",
					// UEdMode::GetInteractiveToolsContext() returns a UEditorInteractiveToolsContext, so the
					// host needs its definition to hand it to the ITF helpers (gizmo registration, for
					// instance). UnrealEd depends on this module privately, so it does not come along for
					// free - ModelingToolsEditorMode declares exactly this pair.
					"EditorInteractiveToolsFramework",
					"InteractiveToolsFramework",
					// The property panel of the toolkit is an IDetailsView, and the CallInEditor button
					// strips are added through a custom layout registered on it.
					"PropertyEditor",
					// RoadToolsCallInEditorDetails walks UFunctions and skips editor utility blueprints via
					// FBlueprintEditorUtils, which lives here.
					"BlueprintGraph",
					"RoadBuilder",
					"RoadBuilderTools",
					"UnrealEd",
				}
				);

			// This module is the editor host of the shared tool layer: it owns URoadToolsMode (UEdMode), the
			// toolkit and the editor implementation of IRoadEditorContext. The tools it drives live in
			// RoadBuilderTools and know nothing about it.
			//
			// Note the LoadingPhase in RoadBuilder.uplugin: Default, not PostEngineInit. UEdMode subclasses
			// are discovered by UAssetEditorSubsystem at PostEngineInit, so a module loading later would
			// never have its mode registered.
		}
	}
}
