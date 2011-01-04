#include "martycam.h"

int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    QApplication app(nCmdShow, &lpCmdLine);
    QCoreApplication::setOrganizationName("CanAssist");
    QCoreApplication::setOrganizationDomain("canassist.ca");
    QCoreApplication::setApplicationName("MartyCam");
    MartyCam *tracker = new MartyCam();
    tracker->show();
    return app.exec();
}
