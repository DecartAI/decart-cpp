// Copyright 2026 Decart. SPDX-License-Identifier: MIT
#include "detail/http_client.h"

#include <ixwebsocket/IXHttpClient.h>
#include <ixwebsocket/IXNetSystem.h>

namespace decart::detail {
namespace {

// IXWebSocket needs WSAStartup on Windows; harmless and idempotent elsewhere.
struct NetSystemInit {
  NetSystemInit() { ix::initNetSystem(); }
};
void ensureNetSystem() { static NetSystemInit once; }

} // namespace

HttpResponse httpPostJson(const std::string& url, const std::string& body,
                          const std::map<std::string, std::string>& headers) {
  ensureNetSystem();

  ix::HttpClient client(/*async=*/false);
  ix::HttpRequestArgsPtr args = client.createRequest(url, ix::HttpClient::kPost);
  for (const auto& [key, value] : headers) {
    args->extraHeaders[key] = value;
  }
  args->connectTimeout = 30;
  args->transferTimeout = 60;

  ix::HttpResponsePtr resp = client.post(url, body, args);

  HttpResponse out;
  out.status = resp->statusCode;
  out.body = resp->body;
  if (resp->errorCode != ix::HttpErrorCode::Ok) {
    out.transportError = resp->errorMsg.empty() ? "HTTP transport error" : resp->errorMsg;
  }
  return out;
}

} // namespace decart::detail
