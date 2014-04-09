#include <signal.h>
#include <QCoreApplication>
#ifdef QT_GUI_LIB
#include <QApplication>
#endif
#include <QSettings>
#include "ganalytics.hpp"

QCoreApplication * qca;
void sigint(int sig)
{
  qca->quit();
}

int main(int argc, char * argv[])
{
  if (argc != 2) {
    qDebug() << "usage: " << argv[0] << " <Google Analytics Tracking/Property ID>";
    return -1;
  }
#ifdef QT_GUI_LIB
  QApplication a(argc, argv);
#else
  QCoreApplication a(argc, argv);
#endif

  qca = &a;
  signal(SIGINT, &sigint);

  QCoreApplication::setOrganizationName("ganalytics-demo");
  QCoreApplication::setApplicationName("ganalytics");
  QCoreApplication::setApplicationVersion("0.0.1");


  GAnalytics & analytics = *new GAnalytics(&a, argv[1] /*"UA-00000000-0"*/);
  analytics.generateUserAgentEtc();
  //analytics.sendPageview(std::string docHostname, std::string page, std::string title);
  analytics.sendPageview("mydemo.com", "/home", "homepage");
  analytics.setAppName("ganalytics-demo");
  analytics.sendEvent("event category 1", "event action 1", "event label 1", 1);
  //analytics.sendTransaction(std::string transactionID, std::string transactionAffiliation = "" /*...todo...*/);
  //analytics.sendItem(std::string itemName);
  //analytics.sendSocial(std::string socialNetwork, std::string socialAction, std::string socialActionTarget);
  //analytics.sendException(std::string exceptionDescription, bool exceptionFatal = true);
  //analytics.sendTiming(/* todo */);
  analytics.sendAppview("ganalytics-demo", "v1", "ganalytics demo screen");
  analytics.endSession();

  a.exec();
}
