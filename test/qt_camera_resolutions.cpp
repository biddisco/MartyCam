#include <QApplication>
#include <QDialog>
#include <QHBoxLayout>
#include <QPushButton>
#include <QRadioButton>
#include <QVBoxLayout>

#include "martycam/widgets/CameraSelectorWidget.h"
#include "martycam/core/camera_utils.h"

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

  QObject::connect(closeButton, &QPushButton::clicked, [&]() {
    QList<QRadioButton*> radios = selectorWidget->findChildren<QRadioButton*>();
    for (QRadioButton* radio : radios)
    {
      if (radio->isChecked())
      {
        QString configJson = radio->property("camera_config_json").toString();
        int resolutionIndex = radio->property("resolution_index").toInt();

        CameraConfig config = CameraConfig::from_json(configJson.toStdString());
        if (resolutionIndex >= 0 && resolutionIndex < static_cast<int>(config.resolutions.size()))
        {
          camera_resolution const& camRes = config.resolutions[resolutionIndex];
          qDebug() << "Selected Camera Path:" << QString::fromStdString(config.camera_path);
          qDebug() << "Selected Resolution:" << camRes.resolution.width << "x"
                   << camRes.resolution.height;
          qDebug() << "Selected FPS:" << (camRes.fpsList.empty() ? 30 : camRes.fpsList[0]);
          qDebug() << "Selected FOURCC:" << fourCCToString(camRes.fourcc).c_str();
        }
        break;
      }
    }
    dialog.accept();
  });

  dialog.show();

  return app.exec();
}