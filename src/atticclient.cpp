#include "atticclient.h"

#include <iostream>

#include "utils.h"
#include "constants.h"
#include "filesystem.h"
#include "clientutils.h"

namespace attic { 

Client::Client(const std::string& workingdir, 
               const std::string& configdir, 
               const std::string& tempdir, 
               const std::string& entityurl,
               const AccessToken& at) {
    working_directory_ = workingdir;
    config_directory_ = configdir;
    temp_directory_ = tempdir;
    entity_url_ = entityurl;
    access_token_ = at;
}

Client::~Client() {}

int Client::Initialize() {
    int status = ret::A_OK;
    if(!status)
        status = LoadAppFromFile();
    if(!status)
        status = LoadEntity();
    // Optional
    if(!status)
        LoadPhraseToken();
    return status;
}

int Client::Shutdown() { 
    int status = ret::A_OK;
    return status;
}

int Client::LoadPhraseToken() { 
    int status = ret::A_OK;
    std::string path;
    ConstructPhraseTokenFilepath(path);
    std::cout<<" phrase token path : " << path << std::endl;
    status = phrase_token_.LoadFromFile(path);
    /*
    if(status != ret::A_OK) {
        // Assume no file
        // Extract info from entity
        status = ExtractPhraseToken(phrase_token_);
        if(status == ret::A_OK) {
            phrase_token_.SaveToFile(path);
        }

    }
    */
    return status;
}

void Client::set_phrase_key(const std::string& key) {
    std::string path;
    ConstructPhraseTokenFilepath(path);
    phrase_token_.set_phrase_key(key);
    phrase_token_.SaveToFile(path);
}

int Client::ExtractPhraseToken(PhraseToken& out) {
   // TODO :: this with app post 

    //return status;
    return 0;
}

int Client::LoadAppFromFile() { 
    int status = ret::A_OK;
    std::string savepath;
    ConstructAppPath(savepath);
    std::cout<<" save path : " << savepath << std::endl;
    status = tent_app_.LoadFromFile(savepath);
    return status;
}

int Client::LoadEntity(bool override) {
    int status = ret::A_OK;
    std::string entpath;
    ConstructEntityFilepath(entpath);
    
    // Attempt to load from file
    status = entity_.LoadFromFile(entpath);

    // If not or overrride, lets attempt discovery
    if(status != ret::A_OK || override) {
        status = client::Discover(entity_url_, &access_token_, entity_);
        if(status == ret::A_OK)
            entity_.WriteToFile(entpath);
    }

    return status;
}

int Client::SaveAppToFile() { 
    int status = ret::A_OK;
    std::string savepath;
    ConstructAppPath(savepath);
    status = tent_app_.SaveToFile(savepath);

    return status;
}

int Client::SavePhraseToken() { 
    int status = ret::A_OK;
    std::string path;
    ConstructPhraseTokenFilepath(path);
    status = phrase_token_.SaveToFile(path);
    return status;
}
/*
void Client::StartupAppInstance(const std::string& appName,
                                const std::string& appDescription,
                                const std::string& url,
                                const std::string& icon,
                                std::vector<std::string>& uris,
                                std::vector<std::string>& scopes)
{
    tent_app_.SetAppName(appName);
    tent_app_.SetAppDescription(appDescription);
    tent_app_.SetAppURL(url);
    tent_app_.SetAppIcon(icon);

    tent_app_.SetRedirectUris(uris);
    tent_app_.SetScopes(scopes);
}
*/

void Client::ConstructAppPath(std::string& out) {
    // Construct path
    out = config_directory_;
    utils::CheckUrlAndAppendTrailingSlash(out);
    out.append(cnst::g_szAppDataName);
}

void Client::ConstructEntityFilepath(std::string& out) {
    out = config_directory_;
    utils::CheckUrlAndAppendTrailingSlash(out);
    out.append(cnst::g_szEntityName);
}

void Client::ConstructPhraseTokenFilepath(std::string& out) {
    out = config_directory_;
    utils::CheckUrlAndAppendTrailingSlash(out);
    out += cnst::g_szPhraseTokenName;
}

}
