using UnrealBuildTool;

public class VerseGallery : ModuleRules
{
	public VerseGallery(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"ProceduralMeshComponent",
			"Json",
			"Slate",
			"SlateCore"
		});
	}
}
