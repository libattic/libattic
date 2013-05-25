#include "getfilestrategy.h"

#include <iostream>
#include <boost/timer/timer.hpp>

#include "utils.h"
#include "netlib.h"
#include "compression.h"
#include "crypto.h"
#include "filesystem.h"
#include "event.h"
#include "logutils.h"
#include "connectionhandler.h"
#include "pagepost.h"
#include "filehandler.h"
#include "posthandler.h"
#include "chunktransform.h"

namespace attic { 

GetFileStrategy::GetFileStrategy() {}
GetFileStrategy::~GetFileStrategy() {}

int GetFileStrategy::Execute(FileManager* pFileManager,
                             CredentialsManager* pCredentialsManager) {
    int status = ret::A_OK;
    status = InitInstance(pFileManager, pCredentialsManager);

    post_path_ = GetConfigValue("post_path");
    std::string filepath = GetConfigValue("filepath");
    std::string post_attachment = GetConfigValue("post_attachment");
    std::string entity = GetConfigValue("entity");

    if(!ValidMasterKey()) return ret::A_FAIL_INVALID_MASTERKEY;

    if(!filepath.empty()) { 
        FileHandler fh(file_manager_);
        FileInfo fi;
        if(fh.RetrieveFileInfo(filepath, fi)) {
            std::cout<<" FILE : " << filepath << " DELETED : " << fi.deleted() << std::endl;
            if(fi.deleted()) return ret::A_FAIL_PULL_DELETED_FILE;
            // Retrieve file metadata
            FilePost meta_post;
            status = RetrieveFilePost(fi.post_id(), meta_post);
            if(status == ret::A_OK) {
                Credentials file_cred;
                // Get file credentials
                status = ExtractCredentials(meta_post, file_cred);
                if(status == ret::A_OK) {
                    if(!file_cred.key_empty()) {
                        // Query for chunkposts mentioning metadata
                        ChunkPostList cp_list;
                        RetrieveChunkPosts(entity, meta_post.id(), cp_list);
                        // put file posts in order
                        // pull chunks
                        status = ConstructFile(cp_list, file_cred, fi);
                        if(status == ret::A_OK) {
                            // Retrieve associated folder entries and create local cache 
                            // entries for them
                            ValidateFolderEntries(meta_post);
                        }
                    }
                    else {
                        std::string error = "Invalid file key on get file";
                        log::LogString("MAOP8091", error);
                        status = ret::A_FAIL_INVALID_FILE_KEY;
                    }
                }
            }
        }
        else {
            status = ret::A_FAIL_FILE_NOT_IN_MANIFEST;
        }
    }
    else {
        status = ret::A_FAIL_FILE_NOT_IN_MANIFEST;
        std::cout<<" FILEPATH EMPTY : " << filepath << std::endl;
    }
    
    return status;
}
 
int GetFileStrategy::RetrieveFilePost(const std::string& post_id, FilePost& out) { 
    int status = ret::A_OK;
    std::cout<<" RETRIEVE FILE POST " << std::endl;;
    std::string posturl;
    std::cout<<" POST ID : " << post_id << std::endl;
    utils::FindAndReplace(post_path_, "{post}", post_id, posturl);

    // Get Metadata post
    Response resp;
    PostHandler<FilePost> ph(access_token_);
    status = ph.Get(posturl, NULL, out, resp);

    std::cout<<" POST URL : "<< posturl << std::endl;
    std::cout<<" CODE : " << resp.code << std::endl;
    std::cout<<" BODY : " << resp.body << std::endl;

    return status;
}

int GetFileStrategy::RetrieveChunkPosts(const std::string& entity,
                                        const std::string& post_id,
                                        ChunkPostList& out) {
    int status = ret::A_OK;
    std::cout<<" retrieving chunk posts ... " << std::endl;
    std::string posts_feed = GetConfigValue("posts_feed");
    UrlParams params;
    params.AddValue(std::string("mentions"), entity + " " + post_id);
    params.AddValue(std::string("types"), std::string(cnst::g_attic_chunk_type));

    std::string prm;
    params.SerializeToString(prm);
    std::cout<<" RetrieveChunkPosts params : " << prm << std::endl;

    Response response;
    PostHandler<PagePost> ph(access_token_);
    PagePost pp;
    status = ph.Get(posts_feed, &params, pp, response);
    std::cout<<" CODE : " << response.code << std::endl;
    std::cout<<" BODY : " << response.body << std::endl;

    if(status == ret::A_OK) {
        Json::Value chunk_post_arr(Json::arrayValue);
        jsn::DeserializeJson(pp.data(), chunk_post_arr);

        std::cout<<" TOTAL POST COUNT : " << chunk_post_arr.size() << std::endl;
        Json::ValueIterator itr = chunk_post_arr.begin();
        for(; itr != chunk_post_arr.end(); itr++) {
            Post gp;
            jsn::DeserializeObject(&gp, (*itr));
            // There should never be more than one post in the same group
            std::cout<<" CHUNK POST TYPE : " << gp.type() << std::endl;
            if(gp.type().find(cnst::g_attic_chunk_type) != std::string::npos) {
                ChunkPost p;
                jsn::DeserializeObject(&p, (*itr));
                std::cout<<"TYPE : " << p.type() << std::endl;
                std::cout<<" PUSHING BACK GROUP : " << p.group() << std::endl;
                if(out.find(p.group()) == out.end()) {
                    out[p.group()] = p;
                    std::cout<<" CHUNK INFO LIST SIZE : " << p.chunk_info_list_size() << std::endl;
                }
                else 
                    std::cout<<" DUPLICATE GROUP CHUNK POST, RESOLVE " << std::endl;
            }
        }
    }
    else { 
        log::LogHttpResponse("FA332ASDF3", response);
        status = ret::A_FAIL_NON_200;
    }
    return status;
}

void GetFileStrategy::GetMasterKey(std::string& out) {
    MasterKey mKey;
    credentials_manager_->GetMasterKeyCopy(mKey);
    mKey.GetMasterKey(out);
}

int GetFileStrategy::ExtractCredentials(FilePost& in, Credentials& out) {
    int status = ret::A_OK;
    
    std::string key, iv;
    key = in.key_data();
    iv = in.iv_data();

    std::string mk;
    GetMasterKey(mk);
    std::cout << "MASTER KEY : " << mk << std::endl;
    Credentials FileKeyCred;
    FileKeyCred.set_key(mk);
    FileKeyCred.set_iv(iv);

    // Decrypt File Key
    std::string filekey;
    //crypto::DecryptStringCFB(key, FileKeyCred, filekey);
    status = crypto::DecryptStringGCM(key, FileKeyCred, filekey);
    std::cout<<" ORIGINAL KEY : " << key << std::endl;
    std::cout<<" FILE KEY : " << filekey << std::endl;
    if(status == ret::A_OK) {
        out.set_key(filekey);
        out.set_iv(iv);
    }
    else { 
        std::cout<<" FAILED TO BUILD FILE KEY " << std::endl;
    }

    return status;
}

int GetFileStrategy::ConstructFile(ChunkPostList& chunk_posts, 
                                   const Credentials& file_cred, 
                                   FileInfo& fi) {
    int status = ret::A_OK;
    unsigned int post_count = chunk_posts.size();
    std::cout<<" # CHUNK POSTS : " << post_count << std::endl;

    if(post_count > 0) {
        std::string temp_path;
        FileHandler fh(file_manager_);
        fh.GetTemporaryFilepath(fi, temp_path);

        std::ofstream ofs;
        ofs.open(temp_path.c_str(), std::ios::out | std::ios::trunc | std::ios::binary);
        if(ofs.is_open()) {
            std::string post_attachment = GetConfigValue("post_attachment");
            for(unsigned int i=0; i < post_count; i++) {
                if(chunk_posts.find(i) != chunk_posts.end()) {
                    ChunkPost cp = chunk_posts.find(i)->second;
                    unsigned int chunk_count = cp.chunk_info_list_size();
                    std::cout<<" CHUNK COUNT : " << chunk_count << std::endl;
                    for(unsigned int j=0; j<chunk_count; j++) {
                        ChunkPost::ChunkInfoList::iterator itr = cp.chunk_info_list()->find(j);
                        if(itr != cp.chunk_info_list()->end()) {
                            std::cout<<" FOUND CHUNK # : " << i << std::endl;
                            // Get attachment
                            if(cp.has_attachment(itr->second.chunk_name())) {
                                Attachment attch = cp.get_attachment(itr->second.chunk_name());
                                std::string attachment_path;
                                utils::FindAndReplace(post_attachment,
                                                      "{digest}",
                                                      attch.digest,
                                                      attachment_path);
                                std::cout<<" attachment path : " << attachment_path << std::endl;
                                std::string buffer;
                                status = RetrieveAttachment(attachment_path, buffer);
                                if(status == ret::A_OK) {
                                    std::cout<<" Fetching chunk : ... " << itr->second.chunk_name() << std::endl;
                                    //ChunkInfo* ci = fi.GetChunkInfo(itr->second.chunk_name());
                                    ChunkInfo* ci = cp.GetChunkInfo(itr->second.chunk_name());
                                    std::string chunk;
                                    status = TransformChunk(ci, file_cred.key(), buffer, chunk);
                                    if(status == ret::A_OK) {
                                        // Append to file 
                                        ofs.write(chunk.c_str(), chunk.size());
                                    }
                                    else {
                                        std::cout<<" FAILED TRANSFORM " << std::endl;
                                        std::cout<<" STATUS : "<< status << std::endl;
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }
                else {
                    std::cout<<" INVALID CHUNK POST " << std::endl;
                }
            }
            ofs.close();
        }
        if(status == ret::A_OK) {
            std::string path;
            if(fh.GetCanonicalFilepath(fi.filepath(), path)) {
                fs::MoveFile(temp_path, path);
            }
            else {
                std::cout<<" FAILED TO FORM CANONICAL FILEPATH : " << path << std::endl;
            }
        }
        // delete temp file 
        fs::DeleteFile(temp_path);
    }

    return status;
}

int GetFileStrategy::RetrieveAttachment(const std::string& url, std::string& outBuffer) {
    int status = ret::A_OK;

    std::cout<<" RETRIEVING ATTACHEMNT : " << url << std::endl;

    boost::timer::cpu_timer::cpu_timer t;
    Response response;
    status = netlib::HttpGetAttachment(url, NULL, &access_token_, response);
    boost::timer::cpu_times time = t.elapsed();
    boost::timer::nanosecond_type const elapsed(time.system + time.user);

    if(response.code == 200) {
        outBuffer = response.body;
    }
    else {
        std::cout<<" RETREIVE ATTACHMENT FAILED " << std::endl;
        std::cout<<" FAILED BODY : " << response.body << std::endl;
        status = ret::A_FAIL_NON_200;
    }
    if(elapsed > 0) {
        std::cout<<" elapsed : "<< elapsed << std::endl;
        std::cout<<" buffer size : "<< outBuffer.size() << std::endl;

        // Raise event
        char szSpeed[256] = {'\0'};
        //snprintf(szSpeed, 256, "%u", bps);
        event::RaiseEvent(event::Event::DOWNLOAD_SPEED, std::string(szSpeed), NULL);
    }
    return status;                                                                        
}

int GetFileStrategy::TransformChunk(const ChunkInfo* ci, 
                                    const std::string& filekey,
                                    const std::string& chunkBuffer, 
                                    std::string& out) {
    int status = ret::A_OK;
    if(ci) {
        ChunkTransform ct(chunkBuffer, filekey);
        if(ct.TransformIn(ci)) {
            if(ct.ciphertext_hash() == ci->ciphertext_mac() && 
               ct.plaintext_hash() == ci->plaintext_mac()) {
                    std::cout<<" ^^^---- HASHES ARE THE SAME ----^^^" << std::endl;
                    out = ct.finalized_data();
                    std::cout<<" decomp chunk : " << out.size() << std::endl;
                    std::cout<<" size of chunk : " << out.size() << std::endl;
            }
            else {
                status = ret::A_FAIL_INVALID_CHUNK_HASH;
            }
        }
    }
    else { 
        status = ret::A_FAIL_INVALID_PTR;
    }
    return status;
}

int GetFileStrategy::ConstructPostTree(FileInfo* fi, PostTree& tree) { 
    int status = ret::A_OK;
    if(fi) {
        std::cout<<"BUILDING POST TREE " << std::endl;
        std::string posturl;
        std::string postid = fi->post_id();
        status = RetrieveAndInsert(postid, tree);
    }
    return status;
}

int GetFileStrategy::RetrieveAndInsert(const std::string& postid, PostTree& tree) {
    int status = ret::A_OK;
    std::string posturl;
    utils::FindAndReplace(post_path_, "{post}", postid, posturl);

    Response resp;
    FilePost fp;
    PostHandler<FilePost> ph(access_token_);
    status = ph.Get(posturl, NULL, fp, resp);

    std::cout<<" POST URL : "<< posturl << std::endl;
    std::cout<<" CODE : " << resp.code << std::endl;
    std::cout<<" BODY : " << resp.body << std::endl;

    if(status == ret::A_OK)
        tree.PushBackPost(&fp);
    return status;
}

bool GetFileStrategy::ValidMasterKey() {
    std::string mk;
    GetMasterKey(mk);
    if(mk.empty()) {
        std::string error = "Invalid master key, it is empty!";
        log::LogString("MASDP2823", error);
        return false;
    }
    return true;
}

void GetFileStrategy::ValidateFolderEntries(FilePost& fp) {
    std::cout<<" Validating folder entires ... " << std::endl;
    std::deque<FolderPost> folder_list;
    RetrieveFolderPosts(fp, folder_list);
    std::cout<< " retrieved " << folder_list.size() << " folder posts " << std::endl;
    if(folder_list.size()) {
        FileHandler fh(file_manager_);
        // Validate folder entries exist
        std::deque<FolderPost>::iterator itr = folder_list.begin();
        for(;itr!= folder_list.end(); itr++) { 
            std::cout<<" updating folder entries " << std::endl;
            fh.UpdateFolderEntry((*itr));
        }
    }
}

void GetFileStrategy::RetrieveFolderPosts(FilePost& fp, std::deque<FolderPost>& out) {
    Post::MentionsList::iterator itr = fp.mentions()->begin();
    for(;itr!=fp.mentions()->end(); itr++) {
        FolderPost fp;
        if(RetrieveFolderPost((*itr).post, fp))
            out.push_back(fp);
    }
}

bool GetFileStrategy::RetrieveFolderPost(const std::string& post_id, FolderPost& out) {
    std::string posturl;
    utils::FindAndReplace(post_path_, "{post}", post_id, posturl);

    Response resp;
    FolderPost fp;
    PostHandler<FolderPost> ph(access_token_);
    int status = ph.Get(posturl, NULL, fp, resp);

    std::cout<<" POST URL : "<< posturl << std::endl;
    std::cout<<" CODE : " << resp.code << std::endl;
    std::cout<<" BODY : " << resp.body << std::endl;

    if(status == ret::A_OK) {
        if(fp.type().find(cnst::g_attic_folder_type) != std::string::npos) 
            return true;
    }
    else {
        log::LogHttpResponse("MASDKF111", resp);
    }
    return false;
}

}//namespace
