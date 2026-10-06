// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class RoadBuilderToolsRuntime : ModuleRules
	{
		public RoadBuilderToolsRuntime(ReadOnlyTargetRules Target) : base(Target)
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
					"RoadBuilder",
					"RoadBuilderTools",
					// Runtime host UI. These are Runtime modules themselves (UMG / Slate / SlateCore all
					// live under Engine/Source/Runtime), which is what makes an in-game editing host
					// possible at all. They are declared here and not in RoadBuilderTools on purpose:
					// the shared tool layer stays free of UI so that it keeps compiling in a game target.
					"Slate",
					"SlateCore",
					"UMG",
				}
				);

			// Skeleton plus the runtime rendering adapter (S0.5). What is still missing for the host
			// itself (S4): a URoadToolsContext (UInteractiveToolsContext subclass + IToolsContextQueries
			// / Transactions / Render implementations - the Render half now exists, see Rendering/), a
			// UWorldSubsystem to own its lifetime, an input bridge feeding
			// UInputRouter::PostInputEvent(FInputDeviceState), an undo command stack, and the property
			// UI. See the project notes.
			//
			// Note for S4: gizmo-based tools also need
			// UE::TransformGizmoUtil::RegisterTransformGizmoContextObject() called on this context, the
			// same way URoadToolsMode::Enter() does it for the editor host.
		}
	}
}
