// Another Reddit Console Client
// Copyright (c) 2017, Adalid Claure <aclaure@gmail.com>

#include <ctime>

#include <json.hpp>

#include "Reddit.h"
#include "utils.h"

const std::string DEFAULT_USERAGENT = "User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X x.y; rv:42.0) Gecko/20100101 Firefox/42.0";

namespace arcc
{

void OAuth2Login::start()
{
    _server.resource["^/oauth2$"]["GET"] = 
        [&](std::shared_ptr<HttpServer::Response> response, std::shared_ptr<HttpServer::Request> request) 
        {
            std::string failedReason;

            std::stringstream stream;
            auto query_fields = request->parse_query_string();
            auto errorIt = query_fields.find("error");

            if (errorIt != query_fields.end())
            {
                failedReason = errorIt->second ;
            }
            else
            {
                auto codeIt = query_fields.find("code");
                auto stateIt = query_fields.find("state");

                assert(codeIt != query_fields.end() && stateIt != query_fields.end());

                WebClient client;
                client.setBasicAuth(REDDIT_CLIENT_ID,"");

                const std::string postData = (boost::format("grant_type=authorization_code&code=%1%&redirect_uri=%2%") % codeIt->second % REDDIT_REDIRECT_URL).str();
                auto result = client.doRequest("https://www.reddit.com/api/v1/access_token", postData, WebClient::Method::POST);
                
                if (result.status == 200)
                {
                    auto jreply = nlohmann::json::parse(result.data);                    
                    _reddit = std::make_shared<RedditSession>(jreply["access_token"], jreply["refresh_token"], jreply["expires_in"].get<double>());

                    _loggedIn = true;
                }
                else
                {
                    // TODO: need to support more failures if the retrieval to get the access token files
                    //       see https://github.com/reddit/reddit/wiki/oauth2#authorization
                    
                    if (result.status == 401)
                    {
                        failedReason = "http basic authorization credentials were invalid";
                    }
                    else
                    {
                        failedReason = "authorization failed for an unknown reason";
                    }
                }
            }

            if (_loggedIn)
            {
                stream << "<h1>Login Successful!</h1>";
                stream << "<p>You are now logged in to <b>Reddit</b> through <b>arcc</b> and may close this browser.";
            }
            else
            {
                stream << "<h1>ERROR!</h1>";
                stream << "<big>reason:</big> <b><font style='font-family:monospace;'>" << failedReason << "</font></b>";
                stream << "<p>You need to authorize <b>arcc</b> to use <b>Reddit</b>. You can try again by typing <b><i>login</i></b> in <b>arcc</b>";
            }

            response->write(stream);
            response->send([&](const SimpleWeb::error_code & /*ec*/) 
            { 
                _server.stop(); // When send has finished, successful or not, stop the server
            });
        };

    utils::openBrowser(_loginUrl);
    _server.start();
}

RedditSession::RedditSession(const std::string& accessToken, const std::string& refreshToken, double expiry)
    : _accessToken(accessToken), 
        _refreshToken(refreshToken), 
        _expiry(expiry),
        _lastRefresh(std::chrono::system_clock::now())
{
    _webclient.setUserAgent(DEFAULT_USERAGENT);
    _webclient.setHeader("Authorization: bearer " + _accessToken);

    // _commandMap.insert(std::make_par())
}

std::string RedditSession::doRequest(const std::string& endpoint, WebClient::Method method)
{
    refreshToken();

    std::string endpointUrl = requestUrl + endpoint;

    auto result = _webclient.doRequest(endpointUrl, std::string(), method);
    return result.data;
}

void RedditSession::doRefreshToken()
{
    std::chrono::duration<double> elapsed_seconds = std::chrono::system_clock::now()-_lastRefresh;
    if (elapsed_seconds.count() > _expiry)
    {
        // send refresh request

        _lastRefresh = std::chrono::system_clock::now();
    }
}   


} // namespace arcc