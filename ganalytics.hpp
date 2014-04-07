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
*/
#include <unistd.h>
#include <map>
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
    : _qnam(this), _trackingID(trackingID), _clientID(clientID), _useGET(useGET)
  {
    connect(&_qnam, SIGNAL(finished(QNetworkReply *)), this, SLOT(postFinished(QNetworkReply *)));
    if (!_qnam.networkAccessible())
      qDebug() << "error: network inaccessible\n";
  }
  ~GAnalytics() {
    _eventLoop.processEvents();
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
      _eventLoop.processEvents();
      usleep(0.005 * 1000000);
    }
    for (reply_map::iterator it = _replies.begin(); it != _replies.end(); it++) {
      QNetworkReply * reply = it->first;
      qDebug() << "~GAnalytics, request still running: " << reply->url().toString() << ", aborting.\n";
      delete reply;
      //reply->abort();
      //reply->deleteLater();
    }
  }
  // manual ip and user agent
  void setClientID(std::string clientID) {
    _clientID = clientID;
  }
  void setUserIPAddr(std::string userIPAddr) {
    _userIPAddr = userIPAddr;
  }
  void setUserAgent(std::string userAgent) {
    _userAgent = userAgent;
  }
  // hit types
  // - https://developers.google.com/analytics/devguides/collection/protocol/v1/devguide
  // pageview
  void sendPageview(std::string docHostname, std::string page, std::string title) {
    QUrl params = build_metric("pageview");
    params.addQueryItem("dh", docHostname.c_str() );    // document hostname
    params.addQueryItem("dp", page.c_str());            // page
    params.addQueryItem("dt", title.c_str());           // title
    send_metric(params);
  }
  // event
  void sendEvent(std::string eventCategory, std::string eventAction, std::string eventLabel, std::string eventValue) {
  }
  // transaction
  // item
  // social
  // exception
  // timing
  // appview
  void sendAppview(std::string appName, std::string appVersion, std::string screenName = "") {
  }
private Q_SLOTS:
  void postFinished(QNetworkReply * reply) {
    //qDebug() << "reply:" << reply->readAll().toHex();
    if (QNetworkReply::NoError != reply->error()) {
      qDebug() << "postFinished error: " << reply->errorString() << "\n";
    }
    else {
      qDebug() << "http response code: " << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
    }
    _replies.erase(reply);
    reply->deleteLater();
  }
  void postError(QNetworkReply::NetworkError code) {
    qDebug() << "network error signal: " << code << "\n";
  }

private:
  GAnalytics(const GAnalytics &); // disable copy const constructor
  QUrl build_metric(std::string hitType) {
    QUrl params;
    // required in v1
    params.addQueryItem("v", "1" ); // version
    params.addQueryItem("tid", _trackingID.c_str());
    params.addQueryItem("cid", _clientID.c_str());
    params.addQueryItem("t", hitType.c_str());
    if (_userIPAddr.size())
      params.addQueryItem("uip", _userIPAddr.c_str());
    //if (_userAgent.size())
    //  params.addQueryItem("ua", _userAgent.c_str());
    return params;
  }
  void send_metric(QUrl & params) {
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
  QNetworkAccessManager _qnam;
  std::string _trackingID;
  std::string _clientID;
  std::string _userIPAddr;
  std::string _userAgent;
  bool _useGET;
  reply_map _replies;
  QEventLoop _eventLoop;
};

