#include "postfilestrategy.h"

#include <stdio.h>

#include "utils.h"
#include "compression.h"
#include "constants.h"
#include "fileinfo.h"
#include "filesystem.h"
#include "credentialsmanager.h"
#include "connectionhandler.h"

#include "rollsum.h"
#include "logutils.h"
#include "chunkbuffer.h"
#include "chunkrequest.h"
#include "envelope.h"
#include "censushandler.h"
#include "chunktransform.h"
#include "posthandler.h"
#include "filehandler.h"
#include "folderhandler.h"

#include "sleep.h"

namespace attic { 

int PostFileStrategy::Execute(FileManager* fm, CredentialsManager* cm) {
    int status = ret::A_OK;
    status = InitInstance(fm, cm);
    // Initialize meta post
    post_path_ = GetConfigValue("post_path");
    posts_feed_ = GetConfigValue("posts_feed");
    std::string filepath = GetConfigValue("filepath");
    std::string entity = GetConfigValue("entity");
    std::string post_id = GetConfigValue("post_id");

    std::cout<<" starting upload : " << filepath << std::endl;
    // Check Master Key before doing anything else
    if(!ValidMasterKey()) return ret::A_FAIL_INVALID_MASTERKEY;

    if(fs::CheckFilepathExists(filepath)) {
        FileInfo fi;
        if(!RetrieveFileInfo(filepath, fi))
            return ret::A_FAIL_INVALID_FILE_INFO;

        std::string file_post_id = fi.post_id();
        status = UpdateFilePostTransitState(file_post_id, true); // Set file to in transit

        // Verify key credentials
        if(!fi.file_credentials().key_empty()) {
            FileHandler fh(file_manager_);
            if(status == ret::A_OK && !file_post_id.empty()) {
                // Retrieve Chunk posts
                event::RaiseEvent(event::Event::PUSH, event::Event::START, filepath, NULL);
                ChunkPostList chunk_posts;
                RetrieveChunkPosts(entity, file_post_id, chunk_posts);
                // Extract Chunk info
                FileInfo::ChunkMap chunk_map;
                ExtractChunkInfo(chunk_posts, chunk_map);
                // begin chunking
                status = ChunkFile(filepath, 
                                   fi.file_credentials(), 
                                   file_post_id, 
                                   chunk_posts, 
                                   chunk_map);
                if(status == ret::A_OK) { 
                    std::string plaintext_hash;
                    fh.RollFileMac(filepath, plaintext_hash);

                    // Update file info
                    fi.set_chunks(chunk_map);
                    fi.set_chunk_count(chunk_map.size());
                    fi.set_file_size(utils::CheckFilesize(filepath));
                    fi.set_plaintext_hash(plaintext_hash);
                    fh.UpdateFileInfo(fi);
                    // Update meta data post
                    status = UpdateFilePost(fi);
                    if(status == ret::A_OK) {
                        // Update meta data transit state
                        status = UpdateFilePostTransitState(file_post_id, false);
                    }
                    // Last thing that should happen
                    if(!UpdateFilePostVersion(&fi, file_post_id)){
                        std::string error = " Failed to update file post version";
                        log::LogString("MASDF@12934", error);
                    }
                }
                else {
                    // TODO :: Undo the file to the last good version, or delete if no last good version
                }
                event::RaiseEvent(event::Event::PUSH, event::Event::DONE, filepath, NULL);
            }
            else if(status == ret::A_OK && file_post_id.empty()) {
                status = ret::A_FAIL_INVALID_POST_ID;
                std::cout<<" EXCECUTE sTATUS : " << status << std::endl;

            }
        }
        else {
            std::ostringstream err;
            err<< "Invalid file key during post file  " << std::endl;
            err<< "\t filename : " << filepath << std::endl;
            err<< "\t postid : " << file_post_id << std::endl;
            log::LogString("KJASDmmm++234", err.str());
            status = ret::A_FAIL_INVALID_FILE_KEY;
        }
    }
    else {
        status = ret::A_FAIL_PATH_DOESNT_EXIST;
    }

    std::cout<<" finishing upload : " << filepath <<" status : " << status << std::endl;
    return status;
}

int PostFileStrategy::RetrieveChunkPosts(const std::string& entity,
                                         const std::string& post_id,
                                         ChunkPostList& out) {
    int status = ret::A_OK;
    std::string posts_feed = GetConfigValue("posts_feed");
    UrlParams params;
    params.AddValue(std::string("mentions"), entity + "+" + post_id);
    params.AddValue(std::string("type"), std::string(cnst::g_attic_chunk_type));

    Response response;
    netlib::HttpGet(posts_feed,
                    &params,
                    &access_token_,
                    response);
    if(response.code == 200) {
        Envelope pp;
        jsn::DeserializeObject(&pp, response.body);

        Json::Value chunk_post_arr(Json::arrayValue);
        jsn::DeserializeJson(pp.data(), chunk_post_arr);
        Json::ValueIterator itr = chunk_post_arr.begin();
        for(; itr != chunk_post_arr.end(); itr++) {
            ChunkPost p;
            jsn::DeserializeObject(&p, (*itr));
            // There should never be more than one post in the same group
            if(out.find(p.group()) == out.end())
                out[p.group()] = p;
            else 
                std::cout<<" DUPLICATE GROUP CHUNK POST, RESOLVE " << std::endl;
        }
    }
    else { 
        log::LogHttpResponse("FA332JMA3", response);
        status = ret::A_FAIL_NON_200;
    }
    return status;
}

void PostFileStrategy::ExtractChunkInfo(ChunkPostList& list,
                                        FileInfo::ChunkMap& out) {
    ChunkPostList::iterator itr = list.begin();
    for(;itr != list.end(); itr++) {
        ChunkPost::ChunkInfoList::iterator cp_itr = itr->second.chunk_info_list()->begin();
        for(;cp_itr != itr->second.chunk_info_list()->end(); cp_itr++) {
            ChunkInfo ci = cp_itr->second;
            if(ci.group() == -1) { // debug
                std::cout<<" CHUNK INFO GROUP NOT SET " << std::endl;
                ci.set_group(itr->first);
            }
            out[ci.chunk_name()] = ci;
        }
    }
}

int PostFileStrategy::ChunkFile(const std::string& filepath,
                                const Credentials& file_credentials,
                                const std::string& file_post_id,
                                ChunkPostList& chunk_list,
                                FileInfo::ChunkMap& chunk_map) {
    // To Chunk a file
    //  - Chunk and upload sequentially
    //  - Check if a chunk group(post) exists, if it does update that post, else new post
    int status = ret::A_OK;
    ChunkBuffer cb;
    if(cb.OpenFile(filepath)) {
        std::string file_key = file_credentials.key();
        unsigned int total_read = 0;
        unsigned int chunk_count = 0;
        unsigned int group_count = 0;
        std::string chunk;
        bool new_group = false;
        ChunkPost* cp = NULL;

        std::string entity = GetConfigValue("entity");
        ChunkRequest* cr = NULL;
        while(cb.ReadChunk(chunk)) {
            // Find Chunk Group
            if(chunk_count == 0) {
                cr = new ChunkRequest(entity, 
                                      posts_feed_, 
                                      post_path_, 
                                      file_post_id, 
                                      access_token_, 
                                      group_count);
                // Begin new chunk post
                if(chunk_list.find(group_count) != chunk_list.end()){
                    ChunkPost parent = chunk_list.find(group_count)->second;
                    cr->set_parent_post(parent);
                }
                else {
                    new_group = true;
                }
                cr->BeginRequest();
            }
            // Transform chunk
            ChunkInfo ci;
            ChunkTransform ct(chunk, file_key);
            cr->ProcessTransform(ct, chunk_count, ci);
            verification_map_[ci.digest()] = true;

            chunk_map[ci.chunk_name()] = ci;
            chunk_count++;
            if(cb.BufferEmpty() || chunk_count >= 30) {
                // End chunk post
                chunk_count = 0;
                group_count++;

                Response response;
                cr->EndRequest(response);
                if(cr) {
                    delete cr;
                    cr = NULL;
                }
                if(response.code == 200) { 
                    // Verifiy chunks made it to the server
                    Envelope env;
                    jsn::DeserializeObject(&env, response.body);
                    ChunkPost cp;
                    post::DeserializePostIntoObject(env.post(), &cp);
                    if(!VerifyChunks(cp, filepath)) {
                        status = ret::A_FAIL_ATTACHMENT_VERIFICATION;
                        break;
                    }
                }
                else {
                    std::cout<<" CHUNK POST FAILED AT GROUP : " << group_count << std::endl;
                    log::LogHttpResponse("KJASDF321", response);
                }
            }
            chunk.clear();
        }
    }

    return status;
}

bool PostFileStrategy::VerifyChunks(ChunkPost& cp, const std::string& filepath) {
    Post::AttachmentMap::iterator itr_cp = cp.attachments()->begin();
    for(;itr_cp != cp.attachments()->end(); itr_cp++) {
        std::cout<< itr_cp->second.digest << std::endl;
        std::string decoded;
        std::cout<<" verifying digest : " << itr_cp->second.digest << std::endl;
        if(verification_map_.find(itr_cp->second.digest) == verification_map_.end()){
            std::string error = "Failed to validate attachment integrity.\n";
            error += "\t filepath : " + filepath + "\n";
            error += "\t attachment name : " + itr_cp->second.name + "\n";
            char buf[256] = {'\0'};
            snprintf(buf, 256, "%u", itr_cp->second.size);
            error += "\t size : " + std::string(buf) + "\n";
            error += "verification map contents : \n";

            std::map<std::string, bool>::iterator v_itr = verification_map_.begin();
            for(; v_itr!= verification_map_.end(); v_itr++) {
                error += v_itr->first;
                error += "\n";
            }
            log::LogString("MAS021n124", error);
            return false;
        }
    }
    return true;
}

bool PostFileStrategy::RetrieveFileInfo(const std::string& filepath, FileInfo& out) {
    bool ret = false;
    FileHandler fh(file_manager_);
    if(fh.RetrieveFileInfo(filepath, out)) {
        // Decrypt file key
        std::string mk;
        if(GetMasterKey(mk)) {
            std::string file_key;
            ret = fh.DecryptFileKey(out.encrypted_key(), out.file_credentials_iv(), mk, file_key);
            out.set_file_credentials_key(file_key);
        }
    }
    return ret;
}

int PostFileStrategy::UpdateFilePost(FileInfo& fi) {
    int status = ret::A_OK;
    if(!fi.post_id().empty()) {
        FilePost fp;
        status = RetrieveFilePost(fi.post_id(), fp);
        if(status == ret::A_OK) {
            FileHandler fh(file_manager_);
            std::string mk = GetMasterKey();
            fh.PrepareFilePost(fi, mk, fp);
            std::string posturl;
            utils::FindAndReplace(post_path_, "{post}", fi.post_id(), posturl);
            PostHandler<FilePost> ph(access_token_);
            status = ph.Put(posturl, NULL, fp);
            if(status != ret::A_OK) {
                log::LogHttpResponse("mao194", ph.response());
            }
        }
    }
    else {
        status = ret::A_FAIL_INVALID_POST_ID;
    }
    std::cout<<" Update fIle Post status : " << status << std::endl;
    return status;
}

int PostFileStrategy::RetrieveFilePost(const std::string& post_id, FilePost& out) {
    int status = ret::A_OK;
    if(!post_id.empty()) {
        std::string posturl;
        utils::FindAndReplace(post_path_, "{post}", post_id, posturl);

        PostHandler<FilePost> ph(access_token_);
        status = ph.Get(posturl, NULL, out);
    }
    else {
        status = ret::A_FAIL_INVALID_POST_ID;
    }
    std::cout<<" RETRIEVE FILE POST STATUS : " << status << std::endl;
    return status;
}

int PostFileStrategy::UpdateFilePostTransitState(const std::string& post_id, bool in_transit) {
    int status = ret::A_OK;
    if(!post_id.empty()) {
        std::string posturl;
        utils::FindAndReplace(post_path_, "{post}", post_id, posturl);

        PostHandler<FilePost> ph(access_token_);
        FilePost p;
        status = ph.Get(posturl, NULL, p);
        if(status == ret::A_OK) {
            p.clear_fragment();
            if(in_transit)
                p.set_fragment(cnst::g_transit_fragment);
            ph.Flush();
            status = ph.Put(posturl, NULL, p);
            if(status != ret::A_OK) {
                status = ret::A_FAIL_NON_200;
                log::LogHttpResponse("PO1090MASDF", ph.response());
            }
        }
        else {
            status = ret::A_FAIL_NON_200;
            log::LogHttpResponse("nmasd981", ph.response());
        }
    }
    else {
        status = ret::A_FAIL_INVALID_POST_ID;
    }
    std::cout<< " UPDATE FILE POST TRANSIT STATE : " << status << std::endl;
    return status;
}

bool PostFileStrategy::UpdateFilePostVersion(const FileInfo* fi, const std::string& meta_post_id) {
    std::string posturl;
    utils::FindAndReplace(post_path_, "{post}", meta_post_id, posturl);
    // Get latest post and update cache
    PostHandler<FilePost> ph(access_token_);
    FilePost p;
    int status = ph.Get(posturl, NULL, p);
    if(status == ret::A_OK) {
        FileHandler fh(file_manager_);
        fh.UpdatePostVersion(fi->post_id(), p.version().id());
        return true;
    }
    return false;
}

}//namespace

