#ifndef CENSUSHANDLER_H_
#define CENSUSHANDLER_H_
#pragma once

#include <string>

#include "accesstoken.h"
#include "censuspost.h"
#include "event.h"

namespace attic { 

class CensusHandler : public EventListener {
    bool GetCensusPost(CensusPost& out);
    int GetCensusPostCount();
    int RetrieveCensusPost(CensusPost& out);
    int CreateCensusPost(CensusPost& out);

    void PushVersionBump();
public:
    CensusHandler(const std::string& posts_feed, const AccessToken& at);
    ~CensusHandler();

    bool CensusInquiry();

    void OnEventRaised(const Event& event);
private:
    AccessToken access_token_;
    std::string posts_feed_;
    std::string last_known_version_;
};

} //namespace
#endif
