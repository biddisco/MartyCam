#include "IPCameraForm.h"
//
#include <QSettings>
//

#include <QtGui>

QStandardItemModel* createModel(QObject* parent, stringpairlist &CameraList)
{
  QStandardItemModel* model = new QStandardItemModel();
  QList<QStandardItem*> itemlist;
  for (stringpairlist::iterator it=CameraList.begin(); it!=CameraList.end(); ++it) {
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
  model->setHorizontalHeaderItem( 0, new QStandardItem( "Name" ));
  model->setHorizontalHeaderItem( 1, new QStandardItem( "URL" ) );
  return model;
}

//----------------------------------------------------------------------------
IPCameraForm::IPCameraForm(QWidget* parent) : QDialog(parent) 
{
  ui.setupUi(this);
  this->loadSettings();
}
//----------------------------------------------------------------------------
void IPCameraForm::seupModelView() 
{
  QTableView *table = this->ui.tableView; // new QTableView(this);
  table->setModel(createModel(this, this->CameraList));
  //
  QVBoxLayout *mainLayout = new QVBoxLayout();
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
  QString settingsFileName = QCoreApplication::applicationDirPath() + "/MartyCam.ini";
  QSettings settings(settingsFileName, QSettings::IniFormat);
  //
  settings.beginWriteArray("UserCameras");
  int i=0;
  for (stringpairlist::iterator it=this->CameraList.begin(); it!=this->CameraList.end(); ++it,++i) {
    settings.setArrayIndex(i);
    settings.setValue("CameraName", QVariant(it->first.c_str()));
    settings.setValue("URL", QVariant(it->second.c_str()));
  }
  settings.endArray();  
}
//----------------------------------------------------------------------------
void IPCameraForm::loadSettings()
{
  QString settingsFileName = QCoreApplication::applicationDirPath() + "/MartyCam.ini";
  QSettings settings(settingsFileName, QSettings::IniFormat);
  //
  this->CameraList.clear();
  int size = settings.beginReadArray("UserCameras");
  for (int i=0; i<size; ++i) {
    settings.setArrayIndex(i);
    stringpair camera;
    camera.first = settings.value("CameraName").toString().toAscii().data();
    camera.second = settings.value("URL").toString().toAscii().data();
    this->CameraList[camera.first] = camera.second;
  }
  settings.endArray();
}
//----------------------------------------------------------------------------
