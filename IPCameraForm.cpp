#include <QSettings>
#include <QtGui>
//
#include "IPCameraForm.h"
#include "debug/logging.hpp"

// ----------------------------------------------------------------------------
static auto ipc_log = martycam::log::create("IPCamera");

QStandardItemModel* createModel(QObject* parent, stringpairlist& CameraList)
{
  MARTY_LOG_SCOPE(ipc_log, "{} {}", (void*) (parent), __func__);
  QStandardItemModel* model = new QStandardItemModel();
  QList<QStandardItem*> itemlist;
  for (stringpairlist::iterator it = CameraList.begin(); it != CameraList.end(); ++it)
  {
    itemlist.clear();
    //
    QString text = QString(it->first.c_str());
    QStandardItem* item = new QStandardItem(text);
    itemlist.append(item);
    //
    text = QString(it->second.c_str());
    QStandardItem* child = new QStandardItem(text);
    itemlist.append(child);
    model->appendRow(itemlist);
  }
  model->setHorizontalHeaderItem(0, new QStandardItem("Name"));
  model->setHorizontalHeaderItem(1, new QStandardItem("URL"));
  return model;
}

//----------------------------------------------------------------------------
IPCameraForm::IPCameraForm(QWidget* parent)
  : QDialog(parent)
{
  MARTY_LOG_SCOPE(ipc_log, "{} {}", (void*) (this), __func__);
  ui.setupUi(this);
  this->loadSettings();
}

//----------------------------------------------------------------------------
void IPCameraForm::seupModelView()
{
  MARTY_LOG_SCOPE(ipc_log, "{} {}", (void*) (this), __func__);
  QTableView* table = this->ui.tableView;    // new QTableView(this);
  table->setModel(createModel(this, this->CameraList));
  //
  QVBoxLayout* mainLayout = new QVBoxLayout();
  //
  table->setEditTriggers(QAbstractItemView::NoEditTriggers);
  table->setSelectionBehavior(QAbstractItemView::SelectRows);
  table->setSelectionMode(QAbstractItemView::SingleSelection);
  table->resizeColumnsToContents();
  table->resizeRowsToContents();
  //
  ui.gridLayout->addWidget(table);
}

//----------------------------------------------------------------------------
void IPCameraForm::saveSettings()
{
  MARTY_LOG_SCOPE(ipc_log, "{} {}", (void*) (this), __func__);
  QString settingsFileName = QCoreApplication::applicationDirPath() + "/MartyCam.ini";
  QSettings settings(settingsFileName, QSettings::IniFormat);
  //
  settings.beginWriteArray("UserCameras");
  int i = 0;
  for (stringpairlist::iterator it = this->CameraList.begin(); it != this->CameraList.end();
       ++it, ++i)
  {
    settings.setArrayIndex(i);
    settings.setValue("CameraName", QVariant(it->first.c_str()));
    settings.setValue("URL", QVariant(it->second.c_str()));
  }
  settings.endArray();
}

//----------------------------------------------------------------------------
void IPCameraForm::loadSettings()
{
  MARTY_LOG_SCOPE(ipc_log, "{} {}", (void*) (this), __func__);
  QString settingsFileName = QCoreApplication::applicationDirPath() + "/MartyCam.ini";
  QSettings settings(settingsFileName, QSettings::IniFormat);
  //
  this->CameraList.clear();
  int size = settings.beginReadArray("UserCameras");
  for (int i = 0; i < size; ++i)
  {
    settings.setArrayIndex(i);
    stringpair camera;
    camera.first = settings.value("CameraName").toString().toLatin1().data();
    camera.second = settings.value("URL").toString().toLatin1().data();
    MARTY_LOG_INFO(ipc_log, "{:<20} Camera: {}={}", "IPCameraForm", camera.first, camera.second);
    this->CameraList[camera.first] = camera.second;
  }
  settings.endArray();
}

// [UserCameras]
// CameraName=MartyCamGarden
// CameraURL="rtsp://gardencam:gardencam@192.168.1.34:554/stream1"
//----------------------------------------------------------------------------
