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
#ifdef QT_GUI_LIB
#include <QApplication>
#include <QDesktopWidget>
#endif
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <QEventLoop>
#include <QDebug>

/*!
 * send google tracking data according to
 * https://developers.google.com/analytics/devguides/collection/protocol/v1/reference
 */

class GAnalytics : public QObject {
  Q_OBJECT
private:
  typedef std::map<QNetworkReply *, bool> reply_map;
public:
  GAnalytics(QString trackingID,
             QString clientID = "",
             bool useGET = false)
    : _qnam(this), _trackingID(trackingID), _clientID(clientID), _useGET(useGET), _isFail(false)
  {
    connect(&_qnam, SIGNAL(finished(QNetworkReply *)), this, SLOT(postFinished(QNetworkReply *)));
    if (!_qnam.networkAccessible())
      qDebug() << "error: network inaccessible\n";
  }
  ~GAnalytics() {
    // wait up to half a second to let replies finish up
    for (int ii = 0; ii < 100; ii++) {
      bool unfinished = false;
      for (reply_map::iterator it = _replies.begin(); it != _replies.end(); it++) {
        QNetworkReply * reply = it->first;
        if (reply->isRunning()) {
          unfinished = true;
          break;
        }
      }
      if (!unfinished)
        break;
      QEventLoop eventLoop;
      eventLoop.processEvents();
      usleep(0.005 * 1000000);
    }
#if 1 // not sure if this is necessary? does ~_qnam() delete all its replies? i guess it should
    for (reply_map::iterator it = _replies.begin(); it != _replies.end(); it++) {
      QNetworkReply * reply = it->first;
      qDebug() << "~GAnalytics, request still running: " << reply->url().toString() << ", aborting.\n";
      delete reply;
      //reply->abort();
      //reply->deleteLater();
    }
#endif
  }
  // manual ip and user agent
  void setClientID(QString clientID) { _clientID = clientID; }
  void setUserIPAddr(QString userIPAddr) { _userIPAddr = userIPAddr; }
  void setUserAgent(QString userAgent) { _userAgent = userAgent; }
  void setAppName(QString appName) { _appName = appName; }
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
    if (_appName.size())
      params.addQueryItem("an", _appName); // mobile event app tracking
    else if (!_appName.size())
      params.addQueryItem("an", appName);
    if (appVersion.size()) params.addQueryItem("av", appVersion);
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
    QString system = QLocale::system().name().toLower().replace("_", "-");
    _userAgent = "Mozilla/5.0 (" + system + ")";
    QNetworkRequest req;
    qDebug() << "User-Agent:" << req.rawHeader("User-Agent").constData() << "->" << _userAgent;

#ifdef QT_GUI_LIB
    QString geom = QString::number(QApplication::desktop()->screenGeometry().width()) 
      + "x" + QString::number(QApplication::desktop()->screenGeometry().height());
    _screenResolution = geom;
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
      qDebug() << "http response code: " << httpStatus;
      if (httpStatus < 200 || httpStatus > 299)
        _isFail = true;
    }
    _replies.erase(reply);
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
    if (_userIPAddr.size())
      params.addQueryItem("uip", _userIPAddr);
    if (_screenResolution.size())
      params.addQueryItem("sr", _screenResolution);
    if (_viewportSize.size())
      params.addQueryItem("vp", _viewportSize);
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
    _replies[reply] = true;
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
  // etc...

  // internal
  bool _isFail;
  mutable reply_map _replies;
};

