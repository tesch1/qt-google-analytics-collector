#include <QCoreApplication>
#include <QSettings>
#include <QUuid>
#include "ganalytics.hpp"

int main(int argc, char * argv[])
{
  QCoreApplication a(argc, argv);

  QCoreApplication::setOrganizationName("ganalytics-demo");
  QCoreApplication::setApplicationName("ganalytics");
  QCoreApplication::setApplicationVersion("0.0.1");

  QSettings settings;
  if (!settings.contains("cid")) {
    settings.setValue("cid", QUuid::createUuid().toString());
  }

  GAnalytics analytics(/*"UA-00000000-0"*/, settings.value("cid").toString().toStdString());
  //GAnalytics analytics("UA-00000000-0", settings.value("cid").toString().toStdString()); // DO NOT USE
  analytics.sendPageview("mydemo.com", "/home", "homepage");

  a.exec();
}
