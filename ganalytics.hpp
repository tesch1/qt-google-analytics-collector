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
  GAnalytics(std::string trackingID,
             std::string clientID = "",
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
  void setClientID(std::string clientID) { _clientID = clientID; }
  void setUserIPAddr(std::string userIPAddr) { _userIPAddr = userIPAddr; }
  void setUserAgent(std::string userAgent) { _userAgent = userAgent; }
  void setAppName(std::string appName) { _appName = appName; }
  std::string getClientID() const { return _clientID; }
  //
  // hit types
  //
  // - https://developers.google.com/analytics/devguides/collection/protocol/v1/devguide
  //

public Q_SLOTS:

  // pageview
  void sendPageview(std::string docHostname, std::string page, std::string title) const {
    QUrl params = build_metric("pageview");
    params.addQueryItem("dh", docHostname.c_str() );    // document hostname
    params.addQueryItem("dp", page.c_str());            // page
    params.addQueryItem("dt", title.c_str());           // title
    send_metric(params);
  }

  // event
  void sendEvent(std::string eventCategory = "", std::string eventAction = "", 
                 std::string eventLabel = "", std::string eventValue = "") const {
    QUrl params = build_metric("event");
    if (_appName.size()) params.addQueryItem("an", _appName.c_str()); // mobile event app tracking
    if (eventCategory.size()) params.addQueryItem("ec", eventCategory.c_str());
    if (eventAction.size()) params.addQueryItem("ea", eventAction.c_str());
    if (eventLabel.size()) params.addQueryItem("el", eventLabel.c_str());
    if (eventValue.size()) params.addQueryItem("ev", eventValue.c_str());
    send_metric(params);
  }

  // transaction
  void sendTransaction(std::string transactionID, std::string transactionAffiliation = "" /*...todo...*/) const {
    QUrl params = build_metric("transaction");
    params.addQueryItem("ti", transactionID.c_str());
    if (transactionAffiliation.size()) params.addQueryItem("ta", transactionAffiliation.c_str());
    send_metric(params);
  }

  // item
  void sendItem(std::string itemName) const {
    QUrl params = build_metric("item");
    params.addQueryItem("in", itemName.c_str());
    //if (appName.size()) params.addQueryItem("an", appName.c_str());
    send_metric(params);
  }

  // social
  void sendSocial(std::string socialNetwork, std::string socialAction, std::string socialActionTarget) const {
    QUrl params = build_metric("social");
    params.addQueryItem("sn", socialNetwork.c_str());
    params.addQueryItem("sa", socialAction.c_str());
    params.addQueryItem("st", socialActionTarget.c_str());
    send_metric(params);
  }

  // exception
  void sendException(std::string exceptionDescription, bool exceptionFatal = true) const {
    QUrl params = build_metric("exception");
    if (exceptionDescription.size()) params.addQueryItem("exd", exceptionDescription.c_str());
    if (!exceptionFatal) params.addQueryItem("exf", "0");
    send_metric(params);
  }

  // timing
  void sendTiming(/* todo */) const {
    QUrl params = build_metric("timing");
    //if (appName.size()) params.addQueryItem("an", appName.c_str());
    send_metric(params);
  }

  // appview
  void sendAppview(std::string appName, std::string appVersion = "", std::string screenName = "") const {
    QUrl params = build_metric("appview");
    if (!_appName.size()) params.addQueryItem("an", appName.c_str());
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
    _userAgent = "Mozilla/5.0 (" + system.toStdString() + ")";
    QNetworkRequest req;
    qDebug() << "User-Agent:" << req.rawHeader("User-Agent").constData() << "->" << _userAgent.c_str();

#ifdef QT_GUI_LIB
    QString geom = QString::number(QApplication::desktop()->screenGeometry().width()) 
      + "x" + QString::number(QApplication::desktop()->screenGeometry().height());
    _screenResolution = geom.toStdString();
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
  QUrl build_metric(std::string hitType) const {
    QUrl params;
    // required in v1
    params.addQueryItem("v", "1" ); // version
    params.addQueryItem("tid", _trackingID.c_str());
    params.addQueryItem("cid", _clientID.c_str());
    params.addQueryItem("t", hitType.c_str());
    if (_userIPAddr.size())
      params.addQueryItem("uip", _userIPAddr.c_str());
    if (_screenResolution.size())
      params.addQueryItem("sr", _screenResolution.c_str());
    if (_viewportSize.size())
      params.addQueryItem("vp", _viewportSize.c_str());
    //if (_userAgent.size())
    //  params.addQueryItem("ua", _userAgent.c_str());
    return params;
  }
  void send_metric(QUrl & params) const {
    // when google has err'd us, then stop sending events!
    if (_isFail)
      return;
    QUrl collect_url("http://www.google-analytics.com/collect");
    QNetworkRequest request;
    if (_userAgent.size())
      request.setRawHeader("User-Agent", _userAgent.c_str());
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
  std::string _trackingID;
  std::string _clientID;
  bool _useGET; // true=GET, false=POST
  std::string _userID;

  // various parameters:
  bool _anonymizeIP;
  bool _cacheBust;
  //
  std::string _userIPAddr;
  std::string _userAgent;
  std::string _appName;
#if 0 // todo
  // traffic sources
  std::string _documentReferrer;
  std::string _campaignName;
  std::string _campaignSource;
  std::string _campaignMedium;
  std::string _campaignKeyword;
  std::string _campaignContent;
  std::string _campaignID;
  std::string _adwordsID;
  std::string _displayAdsID;
#endif
  // system info
  std::string _screenResolution;
  std::string _viewportSize;
  // etc...

  // internal
  bool _isFail;
  mutable reply_map _replies;
};

