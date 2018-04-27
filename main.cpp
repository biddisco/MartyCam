#ifdef WIN32
 #include <windows.h>
#endif

#include "martycam.h"

#pragma comment(linker, "/NODEFAULTLIB:atlthunk.lib")

//int __stdcall WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
int main(int argc, char **argv)
{
#ifdef AAWIN32
  AllocConsole();
  freopen("conin$", "r", stdin);                                                `
  freopen("conout$", "w", stdout);
  freopen("conout$", "w", stderr);

  //
  // Prevent machine going to sleep whilst MartyCam is running
  //
  if (SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_AWAYMODE_REQUIRED) == NULL)
  {
     // try XP variant as well just to make sure 
     SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED);
  }  
#endif

//  QApplication app(nCmdShow, NULL);
  QApplication app(argc, argv);
  QCoreApplication::setOrganizationName("MartyCam");
  QCoreApplication::setOrganizationDomain("MartyCam");
  QCoreApplication::setApplicationName("MartyCam");
  //
  // App icon
  //
  QIcon appIcon(":/images/Marty.png");
  app.setWindowIcon(appIcon);
  //
  //
  //
  MartyCam *tracker = new MartyCam();
  tracker->show();
  app.exec();
  //
  // set power state back to normal
  //
#ifdef WIN32
  SetThreadExecutionState(ES_CONTINUOUS);  
#endif
  return 0;
}
