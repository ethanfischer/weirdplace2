#include "Tunable.h"

// weird.Tunables — dump every weird.* console variable with its current value.
// '*' prefix marks variables changed via console this session: the list of values
// to bake back into their WP_TUNABLE defaults after a tuning session.
static FAutoConsoleCommand GWpTunablesCmd(
	TEXT("weird.Tunables"),
	TEXT("List all weird.* console variables; '*' marks values changed via console this session."),
	FConsoleCommandDelegate::CreateLambda([]()
	{
		int32 Count = 0;
		IConsoleManager::Get().ForEachConsoleObjectThatStartsWith(
			FConsoleObjectVisitor::CreateLambda([&Count](const TCHAR* Name, IConsoleObject* Obj)
			{
				IConsoleVariable* Var = Obj->AsVariable();
				if (!Var)
				{
					return; // commands (like wp.Tunables itself) aren't tunables
				}
				const bool bConsoleSet = (Var->GetFlags() & ECVF_SetByMask) == ECVF_SetByConsole;
				UE_LOG(LogTemp, Display, TEXT("weird.Tunables: %s%s = %s"),
					bConsoleSet ? TEXT("*") : TEXT(" "), Name, *Var->GetString());
				++Count;
			}),
			TEXT("weird."));
		UE_LOG(LogTemp, Display, TEXT("weird.Tunables: %d variable(s); '*' = changed via console"), Count);
	}));
