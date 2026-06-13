#include <QApplication>
#include <QDialog>
#include <QHBoxLayout>
#include <QPushButton>
#include <QVBoxLayout>

#include "utility_widgets/CameraSelectorWidget.h"

int main(int argc, char* argv[])
{
  QApplication app(argc, argv);

  QDialog dialog;
  dialog.setWindowTitle("Hot-Plug Camera Manager");

  QVBoxLayout* dialogLayout = new QVBoxLayout(&dialog);
  CameraSelectorWidget* selectorWidget = new CameraSelectorWidget(&dialog);
  dialogLayout->addWidget(selectorWidget);

  QHBoxLayout* actionLayout = new QHBoxLayout();
  QPushButton* closeButton = new QPushButton("Apply & Exit", &dialog);
  actionLayout->addStretch();
  actionLayout->addWidget(closeButton);
  dialogLayout->addLayout(actionLayout);

  QObject::connect(closeButton, &QPushButton::clicked, &dialog, &QDialog::accept);

  dialog.show();
  QObject::connect(selectorWidget, &CameraSelectorWidget::cameraConfigChanged,
      [](QString cameraPath, cv::Size resolution, int fps, int fourcc) {
        qDebug() << "Selected Camera Path:" << cameraPath;
        qDebug() << "Selected Resolution:" << resolution.width << "x" << resolution.height;
        qDebug() << "Selected FPS:" << fps;
        qDebug() << "Selected FOURCC:" << fourCCToString(fourcc).c_str();
      });

  return app.exec();
}