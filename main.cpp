#include <QCoreApplication>
#include <QSettings>
#include <QUuid>
#include "ganalytics.hpp"

int main(int argc, char * argv[])
{
  if (argc != 2) {
    qDebug() << "usage: " << argv[0] << " <Google Analytics Tracking/Property ID>";
    return -1;
  }
  QCoreApplication a(argc, argv);

  QCoreApplication::setOrganizationName("ganalytics-demo");
  QCoreApplication::setApplicationName("ganalytics");
  QCoreApplication::setApplicationVersion("0.0.1");

  // create and store a new client id
  QSettings settings;
  if (!settings.contains("cid")) {
    settings.setValue("cid", QUuid::createUuid().toString());
  }

  GAnalytics analytics(argv[1] /*"UA-00000000-0"*/, settings.value("cid").toString().toStdString());
  //analytics.generateUserAgentEtc();
  //analytics.sendPageview(std::string docHostname, std::string page, std::string title);
  analytics.sendPageview("mydemo.com", "/home", "homepage");
  analytics.setAppName("ganalytics-demo");
  //analytics.sendEvent(std::string eventCategory = "", std::string eventAction = "", 
  //                    std::string eventLabel = "", std::string eventValue = "");
  //analytics.sendTransaction(std::string transactionID, std::string transactionAffiliation = "" /*...todo...*/);
  //analytics.sendItem(std::string itemName);
  //analytics.sendSocial(std::string socialNetwork, std::string socialAction, std::string socialActionTarget);
  //analytics.sendException(std::string exceptionDescription, bool exceptionFatal = true);
  //analytics.sendTiming(/* todo */);
  analytics.sendAppview("ganalytics-demo", "v1", "root");

  analytics.endSession();

  a.exec();
}
