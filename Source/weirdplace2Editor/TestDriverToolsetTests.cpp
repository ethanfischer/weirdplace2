#include "TestDriverToolset.h"

#if WITH_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "ToolsetRegistry/ToolCallAsyncResultString.h"
#include "ToolsetRegistry/ToolCallAsyncResultVoid.h"

BEGIN_DEFINE_SPEC(
	FTestDriverToolsetSpec,
	"AI.TestDriverToolset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FTestDriverToolsetSpec)

// Success paths require a live PIE session with a ready player and are
// exercised interactively through unreal-mcp; these specs cover the
// no-PIE and bad-input error paths, which run in a bare editor.
void FTestDriverToolsetSpec::Define()
{
	Describe("Without a PIE session", [this]()
	{
		It("PressInputAction raises", [this]()
		{
			AddExpectedError(TEXT("No PIE session is running"), EAutomationExpectedErrorFlags::Contains);
			UTestDriverToolset::PressInputAction(TEXT("Interact"));
		});

		It("SetInputActionPressed raises", [this]()
		{
			AddExpectedError(TEXT("No PIE session is running"), EAutomationExpectedErrorFlags::Contains);
			UTestDriverToolset::SetInputActionPressed(TEXT("Interact"), true);
		});

		It("PressKey raises", [this]()
		{
			AddExpectedError(TEXT("No PIE session is running"), EAutomationExpectedErrorFlags::Contains);
			UTestDriverToolset::PressKey(TEXT("E"));
		});

		It("GetPlayerStatus raises", [this]()
		{
			AddExpectedError(TEXT("No PIE session is running"), EAutomationExpectedErrorFlags::Contains);
			UTestDriverToolset::GetPlayerStatus();
		});

		It("TeleportToWaypoint raises", [this]()
		{
			AddExpectedError(TEXT("No PIE session is running"), EAutomationExpectedErrorFlags::Contains);
			UTestDriverToolset::TeleportToWaypoint(TEXT("AnyTag"));
		});

		It("TeleportNearActor raises", [this]()
		{
			AddExpectedError(TEXT("No PIE session is running"), EAutomationExpectedErrorFlags::Contains);
			UTestDriverToolset::TeleportNearActor(TEXT("AnyLabel"), 200.f);
		});

		It("LookAtActor raises", [this]()
		{
			AddExpectedError(TEXT("No PIE session is running"), EAutomationExpectedErrorFlags::Contains);
			UTestDriverToolset::LookAtActor(TEXT("AnyLabel"));
		});

		It("PressInputSequence completes with an error", [this]()
		{
			AddExpectedError(TEXT("No PIE session is running"), EAutomationExpectedErrorFlags::Contains);
			UToolCallAsyncResultVoid* Result = UTestDriverToolset::PressInputSequence({ TEXT("Interact") }, 0.1f);
			TestTrue(TEXT("Result is complete"), Result->bIsComplete);
			TestFalse(TEXT("Result has an error"), Result->Error.IsEmpty());
		});

		It("WaitForActivityState completes with an error", [this]()
		{
			AddExpectedError(TEXT("No PIE session is running"), EAutomationExpectedErrorFlags::Contains);
			UToolCallAsyncResultVoid* Result = UTestDriverToolset::WaitForActivityState(TEXT("FreeRoaming"), 1.f);
			TestTrue(TEXT("Result is complete"), Result->bIsComplete);
			TestFalse(TEXT("Result has an error"), Result->Error.IsEmpty());
		});

		It("CapturePlayerView completes with an error", [this]()
		{
			AddExpectedError(TEXT("No PIE session is running"), EAutomationExpectedErrorFlags::Contains);
			UToolCallAsyncResultString* Result = UTestDriverToolset::CapturePlayerView();
			TestTrue(TEXT("Result is complete"), Result->bIsComplete);
			TestFalse(TEXT("Result has an error"), Result->Error.IsEmpty());
		});

		It("WaitForMenuPage completes with an error", [this]()
		{
			AddExpectedError(TEXT("No PIE session is running"), EAutomationExpectedErrorFlags::Contains);
			UToolCallAsyncResultVoid* Result = UTestDriverToolset::WaitForMenuPage(TEXT("Tunables"), 1.f);
			TestTrue(TEXT("Result is complete"), Result->bIsComplete);
			TestFalse(TEXT("Result has an error"), Result->Error.IsEmpty());
		});
	});
}

#endif // WITH_AUTOMATION_TESTS
