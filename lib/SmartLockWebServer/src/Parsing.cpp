/*
  Parsing.cpp - HTTP request parsing.

  Copyright (c) 2015 Ivan Grokhotkov. All rights reserved.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
  Modified 8 May 2015 by Hristo Gochkov (proper post and file upload handling)
*/

#include <Arduino.h>
#include <esp32-hal-log.h>
#include "WiFiServer.h"
#include "WiFiClient.h"
#include "WebServer.h"
#include "detail/mimetable.h"
#include "detail/RequestDeadline.h"

#ifndef WEBSERVER_MAX_POST_ARGS
#define WEBSERVER_MAX_POST_ARGS 32
#endif

#define __STR(a) #a
#define _STR(a) __STR(a)
const char * _http_method_str[] = {
#define XX(num, name, string) _STR(name),
  HTTP_METHOD_MAP(XX)
#undef XX
};

static const char Content_Type[] PROGMEM = "Content-Type";
static const char filename[] PROGMEM = "filename";

// SmartLock limits apply before String/body allocation. No request secrets
// are printed even when verbose core logging is enabled.
static bool boundedLine(WiFiClient& client, String& out, size_t limit,
                        const RequestDeadline& deadline) {
  out=""; if(!out.reserve(limit)) return false;
  const uint32_t started=millis();
  while(!deadline.expired()&&uint32_t(millis()-started)<2000) {
    if(!client.available()){delay(1);continue;}
    int c=client.read();
    if(c=='\r') { while(!client.available()&&!deadline.expired()&&uint32_t(millis()-started)<2000)delay(1); return !deadline.expired()&&client.read()=='\n'; }
    if(c<0||(c<0x20&&c!='\t')||c==0x7f||out.length()>=limit)return false;
    out+=(char)c;
  }
  return false;
}
static bool boundedArguments(const String& data) {
  unsigned count=1;
  for(size_t i=0;i<data.length();++i)if(data[i]=='&'&&++count>16)return false;
  return true;
}

bool WebServer::_parseRequest(WiFiClient& client) {
  const RequestDeadline deadline(millis());
  // Read the first line of HTTP request
  String req; if(!boundedLine(client,req,1024,deadline))return false;
  unsigned headerCount=0;
  //reset header value
  for (int i = 0; i < _headerKeysCount; ++i) {
    _currentHeaders[i].value =String();
  }

  // First line of HTTP request looks like "GET /path HTTP/1.1"
  // Retrieve the "/path" part by finding the spaces
  int addr_start = req.indexOf(' ');
  int addr_end = req.indexOf(' ', addr_start + 1);
  if (addr_start == -1 || addr_end == -1) {
    return false;
  }

  String methodStr = req.substring(0, addr_start);
  String url = req.substring(addr_start + 1, addr_end);
  String versionEnd = req.substring(addr_end + 8);
  _currentVersion = atoi(versionEnd.c_str());
  String searchStr = "";
  int hasSearch = url.indexOf('?');
  if(hasSearch!=-1&&url.substring(0,hasSearch)=="/api/registration/reconcile")return false;
  if (hasSearch != -1){
    searchStr = url.substring(hasSearch + 1);
    url = url.substring(0, hasSearch);
  }
  _currentUri = url;
  _chunked = false;
  _clientContentLength = 0;  // not known yet, or invalid

  HTTPMethod method = HTTP_ANY;
  size_t num_methods = sizeof(_http_method_str) / sizeof(const char *);
  for (size_t i=0; i<num_methods; i++) {
    if (methodStr == _http_method_str[i]) {
      method = (HTTPMethod)i;
      break;
    }
  }
  if (method == HTTP_ANY) {
    return false;
  }
  _currentMethod = method;


  //attach handler
  RequestHandler* handler;
  for (handler = _firstHandler; handler; handler = handler->next()) {
    if (handler->canHandle(_currentMethod, _currentUri))
      break;
  }
  _currentHandler = handler;

  String formData;
  // below is needed only when POST type request
  if (method == HTTP_POST || method == HTTP_PUT || method == HTTP_PATCH || method == HTTP_DELETE){
    String boundaryStr;
    String headerName;
    String headerValue;
    bool isForm = false;
    bool isEncoded = false;
    bool lengthSeen=false;
    //parse headers
    while(1){
      if(++headerCount>24||!boundedLine(client,req,1024,deadline))return false;
      if (req == "") break;//no moar headers
      int headerDiv = req.indexOf(':');
      if (headerDiv == -1){
        break;
      }
      headerName = req.substring(0, headerDiv);
      headerValue = req.substring(headerDiv + 1);
      headerValue.trim();
       _collectHeader(headerName.c_str(),headerValue.c_str());


      if (headerName.equalsIgnoreCase(FPSTR(Content_Type))){
        using namespace mime;
        headerValue.toLowerCase();
        if (headerValue.startsWith(FPSTR(mimeTable[txt].mimeType))){
          isForm = false;
        } else if (headerValue.startsWith(F("application/x-www-form-urlencoded"))){
          isForm = false;
          isEncoded = true;
        } else if (headerValue.startsWith(F("multipart/"))){
          boundaryStr = headerValue.substring(headerValue.indexOf('=') + 1);
          boundaryStr.replace("\"","");
          isForm = true;
        }
      } else if (headerName.equalsIgnoreCase(F("Content-Length"))){
        if(lengthSeen||!headerValue.length()||headerValue.length()>5)return false;
        lengthSeen=true;uint32_t contentLength=0;
        for(size_t i=0;i<headerValue.length();++i){if(headerValue[i]<'0'||headerValue[i]>'9')return false;contentLength=contentLength*10+headerValue[i]-'0';}
        const size_t maximum=url=="/api/line/maintenance/commit"?2048:
            url=="/api/setup/complete"?1024:512;
        if(contentLength>maximum)return false;
        _clientContentLength=contentLength;
      } else if (headerName.equalsIgnoreCase(F("Transfer-Encoding"))){
        return false;
      } else if (headerName.equalsIgnoreCase(F("Host"))){
        _hostHeader = headerValue;
      }
    }

    if(isForm || !lengthSeen) return false; // multipart/upload is deliberately gated
    if (!isForm && _currentHandler && _currentHandler->canRaw(_currentUri)){
      _currentRaw.reset(new HTTPRaw());
      _currentRaw->status = RAW_START;
      _currentRaw->totalSize = 0;
      _currentRaw->currentSize = 0;
      _currentHandler->raw(*this, _currentUri, *_currentRaw);
      _currentRaw->status = RAW_WRITE;

      while (_currentRaw->totalSize < _clientContentLength) {
        while(!client.available()&&!deadline.expired())delay(1);
        if(deadline.expired()) { _currentRaw->status=RAW_ABORTED;_currentHandler->raw(*this,_currentUri,*_currentRaw);return false; }
        size_t wanted=_clientContentLength-_currentRaw->totalSize;
        if(wanted>HTTP_RAW_BUFLEN)wanted=HTTP_RAW_BUFLEN;
        _currentRaw->currentSize=0;
        while(_currentRaw->currentSize<wanted&&client.available()&&!deadline.expired()) {
          int value=client.read();if(value<0)break;
          _currentRaw->buf[_currentRaw->currentSize++]=static_cast<uint8_t>(value);
        }
        _currentRaw->totalSize += _currentRaw->currentSize;
        if (_currentRaw->currentSize == 0) {
          _currentRaw->status = RAW_ABORTED;
          _currentHandler->raw(*this, _currentUri, *_currentRaw);
          return false;
        }
        _currentHandler->raw(*this, _currentUri, *_currentRaw);
      }
      _currentRaw->status = RAW_END;
      _currentHandler->raw(*this, _currentUri, *_currentRaw);
    } else if (!isForm) {
      size_t plainLength;
      char* plainBuf = readRequestBody(client, _clientContentLength, plainLength, deadline);
      if (plainLength < _clientContentLength || deadline.expired()) {
        free(plainBuf);
        return false;
      }
      if (_clientContentLength > 0) {
        if(isEncoded){
          //url encoded form
          if (searchStr != "") searchStr += '&';
          searchStr += plainBuf;
        }
        if(!boundedArguments(searchStr)){free(plainBuf);return false;}
        _parseArguments(searchStr);
        if(!isEncoded){
          //plain post json or other data
          RequestArgument& arg = _currentArgs[_currentArgCount++];
          arg.key = F("plain");
          arg.value = String(plainBuf);
        }

        free(plainBuf);
      } else {
        // No content - but we can still have arguments in the URL.
        if(!boundedArguments(searchStr))return false;
        _parseArguments(searchStr);
      }
    } else {
      // it IS a form
      if(!boundedArguments(searchStr))return false;
      _parseArguments(searchStr);
      if (!_parseForm(client, boundaryStr, _clientContentLength)) {
        return false;
      }
    }
  } else {
    String headerName;
    String headerValue;
    //parse headers
    while(1){
      if(++headerCount>24||!boundedLine(client,req,1024,deadline))return false;
      if (req == "") break;//no moar headers
      int headerDiv = req.indexOf(':');
      if (headerDiv == -1){
        break;
      }
      headerName = req.substring(0, headerDiv);
      headerValue = req.substring(headerDiv + 2);
      _collectHeader(headerName.c_str(),headerValue.c_str());


	  if (headerName.equalsIgnoreCase("Host")){
        _hostHeader = headerValue;
      }
    }
        if(!boundedArguments(searchStr))return false;
      _parseArguments(searchStr);
  }
  if(deadline.expired())return false;
  client.flush();


  return true;
}

bool WebServer::_collectHeader(const char* headerName, const char* headerValue) {
  for (int i = 0; i < _headerKeysCount; i++) {
    if (_currentHeaders[i].key.equalsIgnoreCase(headerName)) {
            _currentHeaders[i].value=headerValue;
            return true;
        }
  }
  return false;
}

void WebServer::_parseArguments(String data) {
  if (_currentArgs)
    delete[] _currentArgs;
  _currentArgs = 0;
  if (data.length() == 0) {
    _currentArgCount = 0;
    _currentArgs = new RequestArgument[1];
    return;
  }
  _currentArgCount = 1;

  for (int i = 0; i < (int)data.length(); ) {
    i = data.indexOf('&', i);
    if (i == -1)
      break;
    ++i;
    ++_currentArgCount;
  }

  _currentArgs = new RequestArgument[_currentArgCount+1];
  int pos = 0;
  int iarg;
  for (iarg = 0; iarg < _currentArgCount;) {
    int equal_sign_index = data.indexOf('=', pos);
    int next_arg_index = data.indexOf('&', pos);
    if ((equal_sign_index == -1) || ((equal_sign_index > next_arg_index) && (next_arg_index != -1))) {
      log_e("arg missing value: %d", iarg);
      if (next_arg_index == -1)
        break;
      pos = next_arg_index + 1;
      continue;
    }
    RequestArgument& arg = _currentArgs[iarg];
    arg.key = urlDecode(data.substring(pos, equal_sign_index));
    arg.value = urlDecode(data.substring(equal_sign_index + 1, next_arg_index));
    ++iarg;
    if (next_arg_index == -1)
      break;
    pos = next_arg_index + 1;
  }
  _currentArgCount = iarg;

}

void WebServer::_uploadWriteByte(uint8_t b){
  if (_currentUpload->currentSize == HTTP_UPLOAD_BUFLEN){
    if(_currentHandler && _currentHandler->canUpload(_currentUri))
      _currentHandler->upload(*this, _currentUri, *_currentUpload);
    _currentUpload->totalSize += _currentUpload->currentSize;
    _currentUpload->currentSize = 0;
  }
  _currentUpload->buf[_currentUpload->currentSize++] = b;
}

int WebServer::_uploadReadByte(WiFiClient& client) {
  int res = client.read();

  if (res < 0) {
    while(!client.available() && client.connected())
      delay(2);

    res = client.read();
  }

  return res;
}

bool WebServer::_parseForm(WiFiClient& client, String boundary, uint32_t len){
  (void) len;
  String line;
  int retry = 0;
  do {
    line = client.readStringUntil('\r');
    ++retry;
  } while (line.length() == 0 && retry < 3);

  client.readStringUntil('\n');
  //start reading the form
  if (line == ("--"+boundary)){
   if(_postArgs) delete[] _postArgs;
    _postArgs = new RequestArgument[WEBSERVER_MAX_POST_ARGS];
    _postArgsLen = 0;
    while(1){
      String argName;
      String argValue;
      String argType;
      String argFilename;
      bool argIsFile = false;

      line = client.readStringUntil('\r');
      client.readStringUntil('\n');
      if (line.length() > 19 && line.substring(0, 19).equalsIgnoreCase(F("Content-Disposition"))){
        int nameStart = line.indexOf('=');
        if (nameStart != -1){
          argName = line.substring(nameStart+2);
          nameStart = argName.indexOf('=');
          if (nameStart == -1){
            argName = argName.substring(0, argName.length() - 1);
          } else {
            argFilename = argName.substring(nameStart+2, argName.length() - 1);
            argName = argName.substring(0, argName.indexOf('"'));
            argIsFile = true;
            //use GET to set the filename if uploading using blob
            if (argFilename == F("blob") && hasArg(FPSTR(filename)))
              argFilename = arg(FPSTR(filename));
          }
          using namespace mime;
          argType = FPSTR(mimeTable[txt].mimeType);
          line = client.readStringUntil('\r');
          client.readStringUntil('\n');
          if (line.length() > 12 && line.substring(0, 12).equalsIgnoreCase(FPSTR(Content_Type))){
            argType = line.substring(line.indexOf(':')+2);
            //skip next line
            client.readStringUntil('\r');
            client.readStringUntil('\n');
          }
          if (!argIsFile){
            while(1){
              line = client.readStringUntil('\r');
              client.readStringUntil('\n');
              if (line.startsWith("--"+boundary)) break;
              if (argValue.length() > 0) argValue += "\n";
              argValue += line;
            }

            RequestArgument& arg = _postArgs[_postArgsLen++];
            arg.key = argName;
            arg.value = argValue;

            if (line == ("--"+boundary+"--")){
              break;
            } else if (_postArgsLen >= WEBSERVER_MAX_POST_ARGS) {
              log_e("Too many PostArgs (max: %d) in request.", WEBSERVER_MAX_POST_ARGS);
              return false;
            }
          } else {
            _currentUpload.reset(new HTTPUpload());
            _currentUpload->status = UPLOAD_FILE_START;
            _currentUpload->name = argName;
            _currentUpload->filename = argFilename;
            _currentUpload->type = argType;
            _currentUpload->totalSize = 0;
            _currentUpload->currentSize = 0;
            if(_currentHandler && _currentHandler->canUpload(_currentUri))
              _currentHandler->upload(*this, _currentUri, *_currentUpload);
            _currentUpload->status = UPLOAD_FILE_WRITE;

            int fastBoundaryLen = 4 /* \r\n-- */ + boundary.length() + 1 /* \0 */;
            char fastBoundary[ fastBoundaryLen ];
            snprintf(fastBoundary, fastBoundaryLen, "\r\n--%s", boundary.c_str());
            int boundaryPtr = 0;
            while ( true ) {
                int ret = _uploadReadByte(client);
                if (ret < 0) {
                    // Unexpected, we should have had data available per above
                    return _parseFormUploadAborted();
                }
                char in = (char) ret;
                if (in == fastBoundary[ boundaryPtr ]) {
                    // The input matched the current expected character, advance and possibly exit this file
                    boundaryPtr++;
                    if (boundaryPtr == fastBoundaryLen - 1) {
                        // We read the whole boundary line, we're done here!
                        break;
                    }
                } else {
                    // The char doesn't match what we want, so dump whatever matches we had, the read in char, and reset ptr to start
                    for (int i = 0; i < boundaryPtr; i++) {
                        _uploadWriteByte( fastBoundary[ i ] );
                    }
                    if (in == fastBoundary[ 0 ]) {
                       // This could be the start of the real end, mark it so and don't emit/skip it
                       boundaryPtr = 1;
                    } else {
                      // Not the 1st char of our pattern, so emit and ignore
                      _uploadWriteByte( in );
                      boundaryPtr = 0;
                    }
                }
            }
            // Found the boundary string, finish processing this file upload
            if (_currentHandler && _currentHandler->canUpload(_currentUri))
                _currentHandler->upload(*this, _currentUri, *_currentUpload);
            _currentUpload->totalSize += _currentUpload->currentSize;
            _currentUpload->status = UPLOAD_FILE_END;
            if (_currentHandler && _currentHandler->canUpload(_currentUri))
                _currentHandler->upload(*this, _currentUri, *_currentUpload);
            if (!client.connected()) return _parseFormUploadAborted();
            line = client.readStringUntil('\r');
            client.readStringUntil('\n');
            if (line == "--") {     // extra two dashes mean we reached the end of all form fields
                break;
            }
            continue;
          }
        }
      }
    }

    int iarg;
    int totalArgs = ((WEBSERVER_MAX_POST_ARGS - _postArgsLen) < _currentArgCount)?(WEBSERVER_MAX_POST_ARGS - _postArgsLen):_currentArgCount;
    for (iarg = 0; iarg < totalArgs; iarg++){
      RequestArgument& arg = _postArgs[_postArgsLen++];
      arg.key = _currentArgs[iarg].key;
      arg.value = _currentArgs[iarg].value;
    }
    if (_currentArgs) delete[] _currentArgs;
    _currentArgs = new RequestArgument[_postArgsLen];
    for (iarg = 0; iarg < _postArgsLen; iarg++){
      RequestArgument& arg = _currentArgs[iarg];
      arg.key = _postArgs[iarg].key;
      arg.value = _postArgs[iarg].value;
    }
    _currentArgCount = iarg;
    if (_postArgs) {
      delete[] _postArgs;
      _postArgs=nullptr;
      _postArgsLen = 0;
    }
    return true;
  }
  return false;
}

String WebServer::urlDecode(const String& text)
{
	String decoded = "";
	char temp[] = "0x00";
	unsigned int len = text.length();
	unsigned int i = 0;
	while (i < len)
	{
		char decodedChar;
		char encodedChar = text.charAt(i++);
		if ((encodedChar == '%') && (i + 1 < len))
		{
			temp[2] = text.charAt(i++);
			temp[3] = text.charAt(i++);

			decodedChar = strtol(temp, NULL, 16);
		}
		else {
			if (encodedChar == '+')
			{
				decodedChar = ' ';
			}
			else {
				decodedChar = encodedChar;  // normal ascii char
			}
		}
		decoded += decodedChar;
	}
	return decoded;
}

bool WebServer::_parseFormUploadAborted(){
  _currentUpload->status = UPLOAD_FILE_ABORTED;
  if(_currentHandler && _currentHandler->canUpload(_currentUri))
    _currentHandler->upload(*this, _currentUri, *_currentUpload);
  return false;
}
