#include "DialogueScript.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

bool FDialogueScript::LoadRawLines(const FString& RelativePath, TArray<FString>& OutLines)
{
	const FString FullPath = FPaths::ProjectContentDir() / RelativePath;
	if (!FFileHelper::LoadFileToStringArray(OutLines, *FullPath))
	{
		UE_LOG(LogTemp, Error, TEXT("DialogueScript - Failed to load file: %s"), *FullPath);
		return false;
	}
	return true;
}

bool FDialogueScript::Load(const FString& RelativePath)
{
	SourcePath = RelativePath;
	Sections.Empty();

	TArray<FString> RawLines;
	if (!LoadRawLines(RelativePath, RawLines))
	{
		return false;
	}

	TArray<FDialogueLine>* Current = nullptr;
	for (const FString& Raw : RawLines)
	{
		FString Line = Raw.TrimStartAndEnd();
		if (Line.IsEmpty() || Line.StartsWith(TEXT("#")))
		{
			continue;
		}

		// `== SectionName ==` header
		if (Line.StartsWith(TEXT("==")) && Line.EndsWith(TEXT("==")) && Line.Len() > 4)
		{
			const FString Name = Line.Mid(2, Line.Len() - 4).TrimStartAndEnd();
			if (Sections.Contains(Name))
			{
				UE_LOG(LogTemp, Error, TEXT("DialogueScript - Duplicate section '%s' in %s"), *Name, *RelativePath);
			}
			Current = &Sections.FindOrAdd(Name);
			continue;
		}

		if (!Current)
		{
			UE_LOG(LogTemp, Error, TEXT("DialogueScript - Line before any section header in %s: %s"), *RelativePath, *Line);
			continue;
		}

		// `[Tag]` attaches an action cue to the preceding dialogue line.
		if (Line.StartsWith(TEXT("[")) && Line.EndsWith(TEXT("]")))
		{
			if (Current->Num() == 0)
			{
				UE_LOG(LogTemp, Error, TEXT("DialogueScript - Tag '%s' with no preceding line in %s"), *Line, *RelativePath);
				continue;
			}
			Current->Last().Tag = Line.Mid(1, Line.Len() - 2).TrimStartAndEnd();
			continue;
		}

		// Optional `Speaker:` prefix — only when the prefix is a single word,
		// so lines containing a colon mid-sentence don't lose their head.
		FDialogueLine Parsed;
		int32 ColonIndex;
		if (Line.FindChar(TEXT(':'), ColonIndex) && ColonIndex > 0)
		{
			const FString Prefix = Line.Left(ColonIndex).TrimStartAndEnd();
			if (!Prefix.Contains(TEXT(" ")))
			{
				Parsed.Speaker = Prefix;
				Parsed.Text = Line.Mid(ColonIndex + 1).TrimStartAndEnd();
			}
		}
		if (Parsed.Text.IsEmpty())
		{
			Parsed.Text = Line;
		}
		Current->Add(Parsed);
	}

	UE_LOG(LogTemp, Log, TEXT("DialogueScript - Loaded %d sections from %s"), Sections.Num(), *RelativePath);
	return true;
}

const TArray<FDialogueLine>* FDialogueScript::FindSection(const FString& SectionName) const
{
	const TArray<FDialogueLine>* Found = Sections.Find(SectionName);
	if (!Found)
	{
		UE_LOG(LogTemp, Error, TEXT("DialogueScript - Unknown section '%s' in %s"), *SectionName, *SourcePath);
	}
	return Found;
}
