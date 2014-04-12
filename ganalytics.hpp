/*

  Copyright (c) 2014, Michael Tesch
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

  * Redistributions of source code must retain the above copyright notice, this
    list of conditions and the following disclaimer.

  * Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions and the following disclaimer in the documentation
    and/or other materials provided with the distribution.

  * Neither the name of Michael Tesch nor the names of other
    contributors may be used to endorse or promote products derived from
    this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

  https://github.com/tesch1/qt-google-analytics-collector

*/
#include <unistd.h>
#include <map>
#include <QCoreApplication>
#include <QSettings>
#ifdef QT_GUI_LIB
#include <QApplication>
#include <QDesktopWidget>
#endif
#include <QUuid>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <QDebug>
#include <QCryptographicHash>

/*!
 * send google tracking data according to
 * https://developers.google.com/analytics/devguides/collection/protocol/v1/reference
 */

class GAnalytics : public QObject {
  Q_OBJECT
private:
public:
  GAnalytics(QCoreApplication * parent,
             QString trackingID,
             QString clientID = "",
             bool useGET = false)
    : QObject(parent), _qnam(this), _trackingID(trackingID), _clientID(clientID), _useGET(useGET), _isFail(false)
  {
    if (parent) {
      setAppName(parent->applicationName());
      setAppVersion(parent->applicationVersion());
#ifdef QT_GUI_LIB
      parent->dumpObjectTree();
#endif
    }
    if (!_clientID.size()) {
      // load client id from settings
      QSettings settings;
      if (!settings.contains("GAnalytics-cid")) {
        settings.setValue("GAnalytics-cid", QUuid::createUuid().toString());
      }
      _clientID = settings.value("GAnalytics-cid").toString();
    }
    connect(&_qnam, SIGNAL(finished(QNetworkReply *)), this, SLOT(postFinished(QNetworkReply *)));
#if QT_VERSION >= 0x040800
    if (!_qnam.networkAccessible())
      qDebug() << "error: network inaccessible\n";
#endif
  }

  ~GAnalytics() {
    // this generally happens after the event-loop is done, so no more network processing
#if 1
    QList<QNetworkReply *> replies = _qnam.findChildren<QNetworkReply *>();
    for (QList<QNetworkReply *>::iterator it = replies.begin(); it != replies.end(); it++) {
      if ((*it)->isRunning())
        qDebug() << "~GAnalytics, request still running: " << (*it)->url().toString() << ", aborting.";
      //reply->deleteLater();
    }
#endif
  }

  // manual config of static fields
  void setClientID(QString clientID) { _clientID = clientID; }
  void setUserID(QString userID) {
    if (userID.size())
      _userID = userID;
    else {
#ifdef Q_WS_WIN
      QString user = qgetenv("USERNAME");
#else
      QString user = qgetenv("USER");
#endif
      _userID = QCryptographicHash::hash(user.toUtf8(), QCryptographicHash::Md5).toHex();
    }
  }
  void setUserIPAddr(QString userIPAddr) { _userIPAddr = userIPAddr; }
  void setUserAgent(QString userAgent) { _userAgent = userAgent; }
  void setAppName(QString appName) { _appName = appName; }
  void setAppVersion(QString appVersion) { _appVersion = appVersion; }
  void setScreenResolution(QString screenResolution) { _screenResolution = screenResolution; }
  void setViewportSize(QString viewportSize) { _viewportSize = viewportSize; }
  void setUserLanguage(QString userLanguage) { _userLanguage = userLanguage; }
  QString getClientID() const { return _clientID; }

  //
  // hit types
  //
  // - https://developers.google.com/analytics/devguides/collection/protocol/v1/devguide
  //

public Q_SLOTS:

  // pageview
  void sendPageview(QString docHostname, QString page, QString title) const {
    QUrl params = build_metric("pageview");
    params.addQueryItem("dh", docHostname);    // document hostname
    params.addQueryItem("dp", page);            // page
    params.addQueryItem("dt", title);           // title
    send_metric(params);
  }

  // event
  void sendEvent(QString eventCategory = "", QString eventAction = "", 
                 QString eventLabel = "", int eventValue = 0) const {
    QUrl params = build_metric("event");
    if (_appName.size()) params.addQueryItem("an", _appName); // mobile event app tracking
    if (_appVersion.size()) params.addQueryItem("av", _appVersion); // mobile event app tracking
    if (eventCategory.size()) params.addQueryItem("ec", eventCategory);
    if (eventAction.size()) params.addQueryItem("ea", eventAction);
    if (eventLabel.size()) params.addQueryItem("el", eventLabel);
    if (eventValue) params.addQueryItem("ev", QString::number(eventValue));
    send_metric(params);
  }

  // transaction
  void sendTransaction(QString transactionID, QString transactionAffiliation = "" /*...todo...*/) const {
    QUrl params = build_metric("transaction");
    params.addQueryItem("ti", transactionID);
    if (transactionAffiliation.size()) params.addQueryItem("ta", transactionAffiliation);
    send_metric(params);
  }

  // item
  void sendItem(QString itemName) const {
    QUrl params = build_metric("item");
    params.addQueryItem("in", itemName);
    //if (appName.size()) params.addQueryItem("an", appName);
    send_metric(params);
  }

  // social
  void sendSocial(QString socialNetwork, QString socialAction, QString socialActionTarget) const {
    QUrl params = build_metric("social");
    params.addQueryItem("sn", socialNetwork);
    params.addQueryItem("sa", socialAction);
    params.addQueryItem("st", socialActionTarget);
    send_metric(params);
  }

  // exception
  void sendException(QString exceptionDescription, bool exceptionFatal = true) const {
    QUrl params = build_metric("exception");
    if (exceptionDescription.size()) params.addQueryItem("exd", exceptionDescription);
    if (!exceptionFatal) params.addQueryItem("exf", "0");
    send_metric(params);
  }

  // timing
  void sendTiming(/* todo */) const {
    QUrl params = build_metric("timing");
    //if (appName.size()) params.addQueryItem("an", appName);
    send_metric(params);
  }

  // appview
  void sendAppview(QString appName, QString appVersion = "", QString screenName = "") const {
    QUrl params = build_metric("appview");
    if (_appName.size()) params.addQueryItem("an", _appName);
    else if (appName.size()) params.addQueryItem("an", appName);
    if (_appVersion.size()) params.addQueryItem("av", _appVersion);
    else if (appVersion.size()) params.addQueryItem("av", appVersion);
    if (screenName.size()) params.addQueryItem("cd", screenName);
    send_metric(params);
  }

  // custom dimensions / metrics
  // todo

  // experiment id / variant
  // todo

  //void startSession();
  void endSession() const {
    QUrl params = build_metric("event");
    params.addQueryItem("sc", "end");
    send_metric(params);
  }

public:

  void generateUserAgentEtc() {
    QString locale = QLocale::system().name().toLower().replace("_", "-");
#ifdef __APPLE__
    QString machine = "Macintosh; Intel Mac OS X 10.9; ";
#endif
#ifdef __linux__
    QString machine = "X11; Linux ";
#if defined(__amd64__) || defined(__x86_64__) || defined(__ppc64__)
    machine += "x86_64; ";
#else
    machine += "i686; ";
#endif
#endif
#ifdef Q_WS_WIN
    QString machine = "Windows; ";
#endif
    _userAgent = "Mozilla/5.0 (" + machine + locale + ") GAnalytics/1.0 (Qt/" QT_VERSION_STR " )";
    _userLanguage = locale;
#ifdef QT_GUI_LIB
    QString geom = QString::number(QApplication::desktop()->screenGeometry().width()) 
      + "x" + QString::number(QApplication::desktop()->screenGeometry().height());
    _screenResolution = geom;
#endif
#if 0
    qDebug() << "User-Agent:" << _userAgent;
    qDebug() << "Language:" << _userLanguage;
    qDebug() << "Screen Resolution:" << _screenResolution;
#endif
  }

private Q_SLOTS:
  void postFinished(QNetworkReply * reply) {
    //qDebug() << "reply:" << reply->readAll().toHex();
    if (QNetworkReply::NoError != reply->error()) {
      qDebug() << "postFinished error: " << reply->errorString() << "\n";
    }
    else {
      int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
      //qDebug() << "http response code: " << httpStatus;
      if (httpStatus < 200 || httpStatus > 299) {
        _isFail = true;
      }
    }
    reply->deleteLater();
  }
  void postError(QNetworkReply::NetworkError code) {
    qDebug() << "network error signal: " << code << "\n";
  }

private:
  GAnalytics(const GAnalytics &); // disable copy const constructor
  QUrl build_metric(QString hitType) const {
    QUrl params;
    // required in v1
    params.addQueryItem("v", "1" ); // version
    params.addQueryItem("tid", _trackingID);
    params.addQueryItem("cid", _clientID);
    params.addQueryItem("t", hitType);
    // optional
    if (_userID.size())
      params.addQueryItem("uid", _userID);
    if (_userIPAddr.size())
      params.addQueryItem("uip", _userIPAddr);
    if (_screenResolution.size())
      params.addQueryItem("sr", _screenResolution);
    if (_viewportSize.size())
      params.addQueryItem("vp", _viewportSize);
    if (_userLanguage.size())
      params.addQueryItem("ul", _userLanguage);
    //if (_userAgent.size())
    //  params.addQueryItem("ua", _userAgent);
    return params;
  }
  void send_metric(QUrl & params) const {
    // when google has err'd us, then stop sending events!
    if (_isFail)
      return;
    QUrl collect_url("http://www.google-analytics.com/collect");
    QNetworkRequest request;
    if (_userAgent.size())
      request.setRawHeader("User-Agent", _userAgent.toUtf8());
    QNetworkReply * reply;
    if (_useGET) {
      // add cache-buster "z" to params
      //params.addQueryItem("z", QString::number(qrand()) );
      collect_url.setQueryItems(params.queryItems());
      request.setUrl(collect_url);
      reply = _qnam.get(request);
    }
    else {
      request.setUrl(collect_url);
      request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
      reply = _qnam.post(request, params.encodedQuery());
    }
    connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), 
            this, SLOT(postError(QNetworkReply::NetworkError)));
    reply->setParent(&_qnam);
  }
  mutable QNetworkAccessManager _qnam;
  QString _trackingID;
  QString _clientID;
  bool _useGET; // true=GET, false=POST
  QString _userID;

  // various parameters:
  bool _anonymizeIP;
  bool _cacheBust;
  //
  QString _userIPAddr;
  QString _userAgent;
  QString _appName;
  QString _appVersion;
#if 0 // todo
  // traffic sources
  QString _documentReferrer;
  QString _campaignName;
  QString _campaignSource;
  QString _campaignMedium;
  QString _campaignKeyword;
  QString _campaignContent;
  QString _campaignID;
  QString _adwordsID;
  QString _displayAdsID;
#endif
  // system info
  QString _screenResolution;
  QString _viewportSize;
  QString _userLanguage;
  // etc...

  // internal
  bool _isFail;
};

