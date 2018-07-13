#include "lpp.h"


#ifdef _DEBUG
#include <windows.h>
#include <D:/LivePP/API/LPP_API.h>
#define LPP_PATH L"D:/LivePP"
#endif

static void* g_lpp = nullptr;

void lppStartup() {
#ifdef _DEBUG
   if (HMODULE livePP = lpp::lppLoadAndRegister(LPP_PATH, "lootgame")) {
      lpp::lppEnableAllCallingModulesSync(livePP);
      lpp::lppInstallExceptionHandler(livePP);
      g_lpp = livePP;
   }
#endif
}
void lppSync() {
#ifdef _DEBUG
   lpp::lppSyncPoint((HMODULE)g_lpp);
#endif
}