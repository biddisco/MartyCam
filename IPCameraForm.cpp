#include "IPCameraForm.h"
//
#include <QSettings>
//

#include <QtGui>

QStandardItemModel* createModel(QObject* parent, stringpairlist &list)
{
  const int numRows = list.size();

  QStandardItemModel* model = new QStandardItemModel();
  QList<QStandardItem*> itemlist;
  for (int row=0; row<numRows; ++row) {
    itemlist.clear();
    //
    QString text = QString(list[row].first.c_str());
    QStandardItem* item = new QStandardItem(text);
    itemlist.append(item);
    //
    text = QString(list[row].second.c_str());
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
  this->CameraList.clear();
  this->CameraList.push_back(
    std::pair<std::string,std::string>("MartyCam", "http://admin:1234@192.168.1.21/videostream.cgi?req_fps=30&.mjpg")
  );
  //
  QTableView *table = new QTableView(this);
  table->setModel(createModel(this, this->CameraList));

  QVBoxLayout *mainLayout = new QVBoxLayout();

//  std::string line = this->CameraList[0].first + " @@ " + this->CameraList[0].second;
//  this->ui.textEdit->append(line.c_str());

  table->setEditTriggers(QAbstractItemView::NoEditTriggers);    
  table->setSelectionBehavior(QAbstractItemView::SelectRows);
  table->setSelectionMode(QAbstractItemView::SingleSelection);    
  table->resizeColumnsToContents();
  table->resizeRowsToContents();

  ui.gridLayout->addWidget(table);
//  this->setLayout(mainLayout);
}
//----------------------------------------------------------------------------
void IPCameraForm::saveSettings()
{
  QString settingsFileName = QCoreApplication::applicationDirPath() + "/MartyCam.ini";
  QSettings settings(settingsFileName, QSettings::IniFormat);
  //
  //settings.beginGroup("MotionDetection");
  //settings.setValue("threshold",ui.threshold->value()); 
  //settings.setValue("average",ui.average->value()); 
  //settings.setValue("erode",ui.erode->value()); 
  //settings.setValue("dilate",ui.dilate->value()); 
  //settings.endGroup();

  //settings.beginGroup("UserSettings");
  //settings.setValue("resolution_640",ui.res640Radio->isChecked()); 
  //settings.endGroup();
}
//----------------------------------------------------------------------------
void IPCameraForm::loadSettings()
{
  QString settingsFileName = QCoreApplication::applicationDirPath() + "/MartyCam.ini";
  QSettings settings(settingsFileName, QSettings::IniFormat);
  //
  //settings.beginGroup("MotionDetection");
  //ui.threshold->setValue(settings.value("threshold",3).toInt()); 
  //ui.average->setValue(settings.value("average",10).toInt()); 
  //ui.erode->setValue(settings.value("erode",1).toInt()); 
  //ui.dilate->setValue(settings.value("dilate",1).toInt()); 
  //settings.endGroup();

  //settings.beginGroup("UserSettings");
  //ui.res640Radio->setChecked( settings.value("resolution_640",true).toBool());
  //ui.res320Radio->setChecked(!settings.value("resolution_640",true).toBool());
  //settings.endGroup();
}
//----------------------------------------------------------------------------
