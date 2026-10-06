// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class RoadBuilderTools : ModuleRules
	{
		public RoadBuilderTools(ReadOnlyTargetRules Target) : base(Target)
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
					"InteractiveToolsFramework",
					// GroundActor.h pulls in PCGGraph.h for AGroundActor's PCG spline, and the ground and
					// marking tools need the full type. Listed explicitly rather than relied on
					// transitively through RoadBuilder, so a change there cannot break this module.
					"PCG",
					"RoadBuilder",
				}
				);

			// BOUNDARY RULE - never add an editor-only dependency to this list.
			//
			// This module is the shared layer under two different hosts: the UEdMode host in
			// RoadBuilderToolsEditor and the game-side ToolsContext host in RoadBuilderToolsRuntime.
			// Anything placed here must therefore compile in a game target, so UnrealEd / Slate /
			// PropertyEditor / LevelEditor are off limits - host UI belongs to the host modules, which
			// reach the tools through interfaces (IRoadEditorContext) instead. Adding an editor module
			// here silently kills the runtime host, and the link error would show up much later.
		}
	}
}
