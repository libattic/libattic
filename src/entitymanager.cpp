#include "entitymanager.h"

#include <iostream> // TODO :: remove this

#include "entity.h"
#include "utils.h"
#include "constants.h"
#include "errorcodes.h"
#include "netlib.h"
#include "tentclient.h"

#include "log.h"

EntityManager::EntityManager()
{

}

EntityManager::~EntityManager()
{

}

int EntityManager::Initialize()
{
    // Load entity info from somewhere ... if it exists

    return ret::A_OK;
}

int EntityManager::Shutdown()
{
    // Clear out Entity vector


    return ret::A_OK;
}


int EntityManager::Discover(const std::string& entityurl, const AccessToken& at, Entity& entOut)
{
    int status = ret::A_OK;
    status = client::Discover(entityurl, &at, entOut);

    if(status == ret::A_OK) {
        // Grab entity api root etc
        RetrieveEntityProfiles(entOut, at);
        
        // Set Api root
        Profile* pProf = entOut.GetActiveProfile();
        if(pProf) {
            std::string apiroot;
            pProf->GetApiRoot(apiroot);
            entOut.SetApiRoot(apiroot);
            entOut.SetEntityUrl(entityurl);
        }
        else {
            status = ret::A_FAIL_INVALID_PTR;
        }
    }

    return status; 
}

void EntityManager::RetrieveEntityProfiles(Entity& ent, const AccessToken& at)
{
    unsigned int profcount = ent.GetProfileCount();
    if(profcount)
    {
        const Entity::UrlList* ProfUrlList = ent.GetProfileUrlList();
        Entity::UrlList::const_iterator itr = ProfUrlList->begin();

        while(itr != ProfUrlList->end())
        {
            Response response;
            netlib::HttpGet( *itr, 
                             NULL,
                             &at,
                             response);

            /*
            std::cout<< " resp : " << response.body << std::endl;
            std::cout<< " code : " << response.code << std::endl;
            */
 
            if(response.code == 200)
            {
                // Deserialize into Profile Object
                Profile* pProf = new Profile();
                jsn::DeserializeObject(pProf, response.body);
                
                // Push back into entity
                ent.PushBackProfile(pProf);
            }
            itr++;
        }

        Entity::ProfileList* pProfList = ent.GetProfileList();
        if(pProfList)
            ent.SetActiveProfile(&*pProfList->begin());
   }
}
