#ifndef ATTICCLIENT_H_
#define ATTICCLIENT_H_

#include <string>

#include "tentapp.h"
#include "filemanager.h"
#include "credentialsmanager.h"
#include "entity.h"
#include "accesstoken.h"
#include "phrasetoken.h"
#include "connectionhandler.h"

namespace attic { 

class Client {
    void ConstructAppPath(std::string& out);
    void ConstructEntityFilepath(std::string& out);
    void ConstructPhraseTokenFilepath(std::string& out);

    int InitializeFileManager();
    int InitializeCredentialsManager();

    int ShutdownFileManager();
    int ShutdownCredentialsManager();

    int ExtractPhraseToken(PhraseToken& out);
public:
    Client(const std::string& workingdir, 
           const std::string& configdir, 
           const std::string& tempdir, 
           const std::string& entityurl,
           const AccessToken& at);

    ~Client();

    int Initialize();
    int Shutdown();

    int LoadPhraseToken();
    int LoadAppFromFile();
    int LoadEntity(bool override=false);

    int SaveAppToFile();
    int SavePhraseToken();

//    FileManager* file_manager()                 { return file_manager_; }
 //   CredentialsManager* credentials_manager()   { return credentials_manager_; }

    const TentApp& tent_app() const             { return tent_app_; }
    const Entity& entity() const                { return entity_; }
    const PhraseToken& phrase_token() const     { return phrase_token_; }
    const AccessToken& access_token() const     { return access_token_; }

    void set_tent_app(const TentApp& app) { tent_app_ = app; }
    void set_phrase_key(const std::string& key);
    void set_phrase_token(const PhraseToken& pt) { phrase_token_ = pt; }

    const std::string& working_directory() const { return working_directory_; }
    const std::string& config_directory() const  { return config_directory_; }
    const std::string& temp_directory() const    { return temp_directory_; } 
    const std::string& entity_url() const        { return entity_url_; }

    void set_working_directory(const std::string& dir) { working_directory_ = dir; }
    void set_config_directory(const std::string& dir) { config_directory_ = dir; }
    void set_temp_directory(const std::string& dir) { temp_directory_ = dir; }
    void set_entity_url(const std::string& url) { entity_url_ = url; }
    void set_access_token(const AccessToken& at) {access_token_ = at; }

private:
    TentApp             tent_app_;
    Entity              entity_;
    AccessToken         access_token_;
    PhraseToken         phrase_token_;

    //ConnectionHandler   conh_;

    std::string working_directory_;
    std::string config_directory_;
    std::string temp_directory_;
    std::string entity_url_;
};

} // namespace
#endif

