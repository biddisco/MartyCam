#include "martycam.h"

#pragma comment(linker, "/NODEFAULTLIB:atlthunk.lib")

int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
#ifdef WIN32
 AllocConsole();
 freopen("conin$", "r", stdin);
 freopen("conout$", "w", stdout);
 freopen("conout$", "w", stderr);
#endif
  QApplication app(nCmdShow, &lpCmdLine);
  QCoreApplication::setOrganizationName("MartyCam");
  QCoreApplication::setOrganizationDomain("MartyCam");
  QCoreApplication::setApplicationName("MartyCam");
  MartyCam *tracker = new MartyCam();
  tracker->show();
  return app.exec();
}
