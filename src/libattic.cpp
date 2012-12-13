#include "libattic.h"

#include <fstream>
#include <string>

#include "errorcodes.h"
#include "connectionmanager.h"
#include "tentapp.h"
#include "jsonserializable.h"
#include "urlparams.h"
#include "post.h"
#include "filemanager.h"
#include "utils.h"

#include "taskarbiter.h"
#include "pulltask.h"
#include "pushtask.h"
#include "deletetask.h"
#include "syncposttask.h"

#include "constants.h"

// TODO :: 
// Things to wrap with mutexes
//  - app
//  - filemanager
//  - connectionmanager
//  - access token? (perhaps provide a copy per operation)

// TODO :: introduce queue'ing mechanism, to protect against multiple
//         file operation spam
//         probably map string enum::state
//         on each operation lock file set state
//         return new error codes if file is in the process of being processed.
//
// TODO :: Set up methods to set the chunk size and whatnot

static TentApp* g_pApp = 0;
static FileManager* g_pFileManager = 0;

static TaskArbiter g_Arb;
static AccessToken g_at;

// Consider making these volatile
static std::string g_WorkingDirectory;
static std::string g_ConfigDirectory;
static std::string g_TempDirectory;

static std::string g_Entity;
static std::string g_AuthorizationURL;

// Local utility functions
static int PostFile(const char* szUrl, const char* szFilePath, FileInfo* fi);
static int PutFile(const char* szUrl, const char* szFilePath, FileInfo* fi);
static ret::eCode DeletePost(const std::string& szPostID);
//////// API start

int InitializeFileManager();
int ShutdownFileManager();
int ShutdownAppInstance();

int InitLibAttic( const char* szWorkingDirectory, 
                  const char* szConfigDirectory,
                  const char* szTempDirectory,
                  const char* szEntityURL)
{
    SetConfigDirectory(szConfigDirectory);
    SetWorkingDirectory(szWorkingDirectory);
    SetTempDirectory(szTempDirectory);

    int status = SetEntityUrl(szEntityURL);
    if(status != ret::A_OK)
    {
        std::cout<<"FAILED : " << status << std::endl;
    }

    status = InitializeFileManager();
    if(status != ret::A_OK)
    {
            std::cout<<"FAILED : " << status << std::endl;
    }

    return status;
}

int ShutdownLibAttic()
{
    int status = ShutdownFileManager();

    if(status != ret::A_OK)
    {
        std::cout<<"FAILED : " << status << " failed to shutdown filemanger" << std::endl;
    }

    status = ShutdownAppInstance();

    if(status != ret::A_OK)
    {
        std::cout<<"FAILED : " << status << " failed to shutdown app instance" << std::endl;
    }

    return status;
}

int InitializeFileManager()
{
    // Construct path
    std::string szFilePath(g_ConfigDirectory);
    utils::CheckUrlAndAppendTrailingSlash(szFilePath);
    szFilePath.append(g_szManifest);

    if(!g_pFileManager)
    {
        g_pFileManager = new FileManager(szFilePath, g_ConfigDirectory);
        g_pFileManager->SetTempDirectory(g_TempDirectory);

        if(!g_pFileManager->StartupFileManager())
            return ret::A_FAIL_TO_LOAD_FILE;
    }

    return ret::A_OK;
}

int ShutdownFileManager()
{
    // Blind shutdown
    if(g_pFileManager)
    {
        g_pFileManager->ShutdownFileManager();
        delete g_pFileManager;
        g_pFileManager = NULL;
    }

    return ret::A_OK;
}

int ShutdownAppInstance()
{
    if(g_pApp)
    {
        delete g_pApp;
        g_pApp = NULL;
    }
    else
    {
        return ret::A_FAIL_INVALID_PTR;
    }

    ConnectionManager::GetInstance()->Shutdown();

    return ret::A_OK;
}

int StartupAppInstance( const char* szAppName, 
                        const char* szAppDescription, 
                        const char* szUrl, 
                        const char* szIcon, 
                        char* redirectUris[], 
                        unsigned int uriCount, 
                        char* scopes[], 
                        unsigned int scopeCount)
{
    g_pApp = new TentApp();                                                

    if(szAppName)
        g_pApp->SetAppName(std::string(szAppName));                    
    if(szAppDescription)
        g_pApp->SetAppDescription(std::string(szAppDescription));   
    if(szIcon)
        g_pApp->SetAppIcon(std::string(szIcon));
    if(szUrl)
        g_pApp->SetAppURL(std::string(szUrl));        

    if(redirectUris)
    {
        for(unsigned int i=0; i < uriCount; i++)
        {
            g_pApp->SetRedirectURI(std::string(redirectUris[i]));
        }
    }

    if(scopes)
    {
        for(unsigned int i=0;i<scopeCount; i++)
        {
            g_pApp->SetScope(std::string(scopes[i]));
        }
    }
    
    return ret::A_OK;
}

int RegisterApp(const char* szPostPath)
{
    if(!szPostPath)
        return ret::A_FAIL_INVALID_PTR;

    if(!g_pApp)
        return ret::A_LIB_FAIL_INVALID_APP_INSTANCE;

    std::string path(szPostPath);
    ConnectionManager* pCm = ConnectionManager::GetInstance();

    // serialize app;
    std::string serialized;
    if(!JsonSerializer::SerializeObject(g_pApp, serialized))
        return ret::A_FAIL_TO_SERIALIZE_OBJECT;

    std::cout << " JSON : " << serialized << std::endl;
    std::string response;

    pCm->HttpPost( path, 
                   NULL,
                   serialized,
                   response);

    std::cout<< " RESPONSE " << response << std::endl;

    // Deserialize new data into app
    if(!JsonSerializer::DeserializeObject(g_pApp, response))
        return ret::A_FAIL_TO_DESERIALIZE_OBJECT;

    return ret::A_OK;
}

int RequestAppAuthorizationURL(const char* szApiRoot)
{
    if(!g_pApp)
        return ret::A_LIB_FAIL_INVALID_APP_INSTANCE;

    UrlParams val;
    val.AddValue(std::string("client_id"), g_pApp->GetAppID());

    if(g_pApp->GetRedirectURIs())
    {
        TentApp::RedirectVec* pUris = g_pApp->GetRedirectURIs();
        TentApp::RedirectVec::iterator itr = pUris->begin();

        for(;itr!=pUris->end();itr++)
        {
            val.AddValue(std::string("redirect_uri"), *itr);
        }
    }

    if(g_pApp->GetScopes())
    {
        TentApp::ScopeVec* pScopes = g_pApp->GetScopes();
        TentApp::ScopeVec::iterator itr = pScopes->begin();

        for(;itr!=pScopes->end();itr++)
        {
            val.AddValue(std::string("scope"), *itr);
        }
    }

    val.AddValue("tent_profile_info_types", "all");
    val.AddValue("tent_post_types", "all");
    //val.AddValue("tent_post_types", "https://tent.io/types/posts/status/v0.1.0");

    g_AuthorizationURL.clear();
    g_AuthorizationURL.append(szApiRoot);

    utils::CheckUrlAndAppendTrailingSlash(g_AuthorizationURL);

    g_AuthorizationURL.append("oauth/authorize");

    std::string params;
    val.SerializeToString(params);

    std::cout<<"PARAMS : " << params << std::endl;

    // TODO:: encode these parameters

    g_AuthorizationURL.append(params);

    return ret::A_OK;
}

const char* GetAuthorizationURL()
{
    return g_AuthorizationURL.c_str();
}


int RequestUserAuthorizationDetails(const char* szApiRoot, const char* szCode)
{
    if(!szCode)
        return ret::A_FAIL_INVALID_CSTR;

    // Build redirect code
    RedirectCode rcode;
    rcode.SetCode(std::string(szCode));
    rcode.SetTokenType(std::string("mac"));

    std::string path(szApiRoot);
    utils::CheckUrlAndAppendTrailingSlash(path);
    path.append("apps/");
    path.append(g_pApp->GetAppID());
    path.append("/authorizations");

    std::cout<< " PATH : " << path << std::endl;
    // serialize RedirectCode
    std::string serialized;
    if(!JsonSerializer::SerializeObject(&rcode, serialized))
        return ret::A_FAIL_TO_SERIALIZE_OBJECT;

    std::string response;
    ConnectionManager* pCm = ConnectionManager::GetInstance();
    pCm->HttpPostWithAuth( path, 
                           NULL,
                           serialized, 
                           response, 
                           g_pApp->GetMacAlgorithm(), 
                           g_pApp->GetMacKeyID(), 
                           g_pApp->GetMacKey(), 
                           false);

    std::cout<< " RESPONSE : " << response << std::endl;

    // Should have an auth token
    // deserialize auth token
    if(!JsonSerializer::DeserializeObject(&g_at, response))
        return ret::A_FAIL_TO_DESERIALIZE_OBJECT;
    // perhaps save it out

    // Construct path
    std::string szSavePath(g_ConfigDirectory);
    utils::CheckUrlAndAppendTrailingSlash(szSavePath);
    szSavePath.append(g_szAuthToken);
    
    g_at.SaveToFile(std::string(szSavePath));

    return ret::A_OK;
}

int LoadAccessToken()
{
    // Construct path
    std::string szFilePath(g_ConfigDirectory);
    utils::CheckUrlAndAppendTrailingSlash(szFilePath);
    szFilePath.append(g_szAuthToken);

    return g_at.LoadFromFile(szFilePath);
}

int SaveAppToFile()
{
    if(!g_pApp)
        return ret::A_LIB_FAIL_INVALID_APP_INSTANCE;

    std::string szSavePath(g_ConfigDirectory);
    utils::CheckUrlAndAppendTrailingSlash(szSavePath);
    szSavePath.append(g_szAppData);

    return g_pApp->SaveToFile(szSavePath);
}

int LoadAppFromFile()
{
    if(!g_pApp)
        g_pApp = new TentApp();                                                

    // Construct path
    std::string szSavePath(g_ConfigDirectory);
    utils::CheckUrlAndAppendTrailingSlash(szSavePath);
    szSavePath.append(g_szAppData);

    g_pApp->LoadFromFile(szSavePath);

    std::string buffer;
    JsonSerializer::SerializeObject(g_pApp, buffer);
    //std::cout<<" BUFFER : " << buffer << std::endl;

    return ret::A_OK;
}

int PushFile(const char* szFilePath, void (*callback)(int, void*) )
{
    PushTask* t = new PushTask( g_pApp, 
                                g_pFileManager, 
                                ConnectionManager::GetInstance(),
                                g_at,
                                g_Entity,
                                szFilePath,
                                g_TempDirectory,
                                g_WorkingDirectory,
                                g_ConfigDirectory,
                                callback);

    g_Arb.SpinOffTask(t);

    return ret::A_OK;
}

int PullFile(const char* szFilePath, void (*callback)(int, void*))
{

    PullTask* t = new PullTask( g_pApp, 
                                g_pFileManager, 
                                ConnectionManager::GetInstance(),
                                g_at,
                                g_Entity,
                                szFilePath,
                                g_TempDirectory,
                                g_WorkingDirectory,
                                g_ConfigDirectory,
                                callback);

    g_Arb.SpinOffTask(t);

    return ret::A_OK;
}

int DeleteFile(const char* szFileName, void (*callback)(int, void*) )
{
    DeleteTask* t = new DeleteTask( g_pApp, 
                                    g_pFileManager, 
                                    ConnectionManager::GetInstance(),
                                    g_at,
                                    g_Entity,
                                    szFileName,
                                    g_TempDirectory,
                                    g_WorkingDirectory,
                                    g_ConfigDirectory,
                                    callback);

    g_Arb.SpinOffTask(t);

    return ret::A_OK;
}

int SyncAtticMetaData( void (*callback)(int, void*) )
{

    return ret::A_OK;
}

int SyncAtticPostsMetaData(void (*callback)(int, void*))
{
    SyncPostsTask* t = new SyncPostsTask( g_pApp, 
                                          g_pFileManager, 
                                          ConnectionManager::GetInstance(),
                                          g_at,
                                          g_Entity,
                                          "",
                                          g_TempDirectory,
                                          g_WorkingDirectory,
                                          g_ConfigDirectory,
                                          callback);

    g_Arb.SpinOffTask(t);

    return ret::A_OK;
}

int PullAllFiles()
{
    if(!g_pApp)
        return ret::A_LIB_FAIL_INVALID_APP_INSTANCE;

    if(!g_pFileManager)
        return ret::A_LIB_FAIL_INVALID_FILEMANAGER_INSTANCE;

    Manifest::EntriesMap* pEntryMap = g_pFileManager->GetManifestEntries();
    Manifest::EntriesMap::iterator itr = pEntryMap->begin();

    std::string filepath = g_WorkingDirectory;
    utils::CheckUrlAndAppendTrailingSlash(filepath);

    for(;itr != pEntryMap->end(); itr++)
    {
        std::string fn = itr->first;
        PullFile((filepath + fn).c_str(), NULL);
    }
    
    return ret::A_OK;
}

int SaveChanges()
{
    // Use this method to force a system wide save
    
    if(!g_pApp)
        return ret::A_LIB_FAIL_INVALID_APP_INSTANCE;
    
    if(!g_pFileManager)
        return ret::A_LIB_FAIL_INVALID_FILEMANAGER_INSTANCE;

    ret::eCode status = ret::A_OK;

    if(!g_pFileManager->WriteOutChanges())
        status = ret::A_FAIL_TO_WRITE_OUT_MANIFEST;

    return status;
}

int SetWorkingDirectory(const char* szDir)
{
    if(!szDir)
        return ret::A_FAIL_INVALID_CSTR;

    g_WorkingDirectory.append(szDir);

    return ret::A_OK;
}

int SetConfigDirectory(const char* szDir)
{
    if(!szDir)
        return ret::A_FAIL_INVALID_CSTR;

    g_ConfigDirectory.append(szDir);

    return ret::A_OK;
}

int SetTempDirectory(const char* szDir)
{
    if(!szDir)
        return ret::A_FAIL_INVALID_CSTR;

    g_TempDirectory.append(szDir);

    return ret::A_OK;
}

int SetEntityUrl(const char* szUrl)
{
    if(!szUrl)
        return ret::A_FAIL_INVALID_CSTR;

    g_Entity.append(szUrl);

    return ret::A_OK;
}

const char* GetWorkingDirectory() { return g_WorkingDirectory.c_str(); }
const char* GetConfigDirectory() { return g_ConfigDirectory.c_str(); }
const char* GetEntityUrl() { return g_Entity.c_str(); }

