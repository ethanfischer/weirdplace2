#pragma once

#include "CoreMinimal.h"

// One parsed dialogue line from a sectioned dialogue file.
struct FDialogueLine
{
	// Speaker prefix ("Rick: ..."), or empty when the line has none — the NPC
	// owning the file speaks it.
	FString Speaker;

	FString Text;

	// Action cue attached to this line via a following `[Tag]` line, or empty.
	FString Tag;
};

// Shared parser for the sectioned dialogue format under Content/Dialogue/:
//
//   == SectionName ==
//   # comment
//   You found it?
//   Rick: And of course that took me a week to research
//   [Give key]
//
// Blank lines are ignored, `#` starts a comment line, `[Tag]` attaches to the
// preceding dialogue line. Lint/preview tooling: scripts/dq.py.
class WEIRDPLACE2_API FDialogueScript
{
public:
	// Load + parse a dialogue file (path relative to Content/). Returns false
	// and logs an Error if the file can't be read.
	bool Load(const FString& RelativePath);

	// Section lookup. Logs an Error and returns nullptr on unknown section.
	const TArray<FDialogueLine>* FindSection(const FString& SectionName) const;

	// Raw line loader (shared with the MovieComments keyed-table parser).
	// Returns false and logs an Error if the file can't be read.
	static bool LoadRawLines(const FString& RelativePath, TArray<FString>& OutLines);

private:
	FString SourcePath;
	TMap<FString, TArray<FDialogueLine>> Sections;
};
