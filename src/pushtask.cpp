
#include "pushtask.h"

#include <list>

#include "filemanager.h"
#include "connectionmanager.h"
#include "chunkinfo.h"
#include "errorcodes.h"
#include "utils.h"
#include "netlib.h"
#include "compression.h"
#include "constants.h"
#include "conoperations.h"
#include "jsonserializable.h"

#include "log.h"

PushTask::PushTask( TentApp* pApp, 
                    FileManager* pFm, 
                    CredentialsManager* pCm,
                    TaskArbiter* pTa,
                    TaskFactory* pTf,
                    const AccessToken& at,
                    const Entity& entity,
                    const std::string& filepath,
                    const std::string& tempdir,
                    const std::string& workingdir,
                    const std::string& configdir,
                    void (*callback)(int, void*))
                    :
                    TentTask ( Task::PUSH,
                               pApp,
                               pFm,
                               pCm,
                               pTa,
                               pTf,
                               at,
                               entity,
                               filepath,
                               tempdir,
                               workingdir,
                               configdir,
                               callback )
{

}

PushTask::~PushTask()
{

}

void PushTask::RunTask()
{
    std::cout<<" running push task ... " << std::endl;
    // Run the task
    std::string filepath;
    GetFilepath(filepath);

    int status = PushFile(filepath);

    std::cout<<" finishing push task ... " << std::endl;
    
    // Callback
    Callback(status, NULL);
    SetFinishedState();
}

int PushTask::PushFile(const std::string& filepath)
{
    int status = ret::A_OK;

    // Verify file exists
    if(utils::CheckFileExists(filepath)) {
        // Begin the chunking pipeline
        std::string posturl;
        ConstructPostUrl(posturl);

        // Retrieve file info if already exists
        FileInfo* fi = RetrieveFileInfo(filepath);
        std::string chunkPostId;
        fi->GetChunkPostID(chunkPostId);

        Credentials fileCredentials;
        // Initiate Chunk Post request
        std::string requestType;
        if(chunkPostId.empty()) {
            requestType = "POST";
            // This is a new file
            // Generate Credentials
            crypto::GenerateCredentials(fileCredentials);
        }
        else {
            requestType = "PUT";
            utils::CheckUrlAndAppendTrailingSlash(posturl);
            posturl += chunkPostId;
            std::string encryptedkey, fileiv; 

            fi->GetEncryptedKey(encryptedkey);
            fi->GetIv(fileiv);
            // Decrypt file key
            Credentials masterCred;
            MasterKey mKey;
            GetCredentialsManager()->GetMasterKeyCopy(mKey);
            std::string mk;
            mKey.GetMasterKey(mk);
            masterCred.SetKey(mk);
            masterCred.SetIv(fileiv);

            std::string decryptedkey;
            crypto::DecryptStringCFB(encryptedkey, masterCred, decryptedkey);
            fi->SetFileKey(decryptedkey);
            std::cout<<" DECRYPTED KEY : " << decryptedkey << std::endl;
            fi->GetFileCredentials(fileCredentials);
        }
        
        // Check for key
        if(fileCredentials.KeyEmpty() || fileCredentials.IvEmpty()) {
            status = ret::A_FAIL_INVALID_FILE_KEY;
        }

        Response resp;
        if(status == ret::A_OK) {
            status = ProcessFile( requestType,
                                  posturl,
                                  filepath,
                                  fileCredentials,
                                  fi,
                                  resp);
        }

        if(status == ret::A_OK) {
            std::cout<< "RESPONSE CODE : " << resp.code << std::endl;
            std::cout<< "RESPONSE BODY : " << resp.body << std::endl;
            if(resp.code == 200) {
                // On success 
                FileInfo::ChunkMap* pList = fi->GetChunkInfoList();
                ChunkPost p;
                // Deserialize basic post data
                JsonSerializer::DeserializeObject((Post*)&p, resp.body);
                InitChunkPost(p, *pList);
                // Setup post url
                if(chunkPostId.empty()) {
                    p.GetID(chunkPostId);
                    if(chunkPostId.empty()) {
                        status = ret::A_FAIL_INVALID_POST_ID;
                    }
                    utils::CheckUrlAndAppendTrailingSlash(posturl);
                    posturl += chunkPostId;
                }

                if(status == ret::A_OK) {
                    // update chunk post with chunk info metadata
                    // use non multipart to just update the post body
                    // leaving existing attachment in-tact

                    std::string bodyBuffer;
                    JsonSerializer::SerializeObject(&p, bodyBuffer);
                    
                    Response metaResp;
                    AccessToken* at = GetAccessToken();
                    status = conops::HttpPut( posturl,
                                              NULL,
                                              bodyBuffer,
                                              *at,
                                              metaResp);

                    std::cout<< " META RESPONSE CODE : " << metaResp.code << std::endl;
                    std::cout<< " META RESPONSE BODY : " << metaResp.body << std::endl;
                }

                std::string filename;
                utils::ExtractFileName(filepath, filename);
                unsigned int filesize = utils::CheckFilesize(filepath);

                // Encrypt File Key
                MasterKey mKey;
                GetCredentialsManager()->GetMasterKeyCopy(mKey);

                std::string mk;
                mKey.GetMasterKey(mk);

                // Insert File Data
                fi->SetChunkPostID(chunkPostId);
                fi->SetFilepath(filepath);
                fi->SetFilename(filename);
                fi->SetFileSize(filesize);
                fi->SetFileCredentials(fileCredentials);

                // Encrypt File Key
                std::string fileKey, fileIv;
                fileCredentials.GetKey(fileKey);
                fileCredentials.GetIv(fileIv);

                Credentials fCred;
                fCred.SetKey(mk);
                fCred.SetIv(fileIv);

                std::string encryptedKey;
                crypto::EncryptStringCFB(fileKey, fCred, encryptedKey);

                fi->SetIv(fileIv);
                fi->SetFileKey(fileKey);
                fi->SetEncryptedKey(encryptedKey);

                // Insert file info to manifest
                GetFileManager()->InsertToManifest(fi);
                // create attic file metadata post
                status = SendAtticPost(fi, filepath);
                // Send Attic Post
                int trycount = 0;
                for(status = SendAtticPost(fi, filepath); status != ret::A_OK; trycount++) {
                    status = SendAtticPost(fi, filepath);
                    std::cout<<" RETRYING .................................." << std::endl;
                    if(trycount > 2)
                        break;
                }

            }
        }
    }
    else {
        status = ret::A_FAIL_OPEN_FILE;
    }

    return status;
}

int PushTask::SendAtticPost( FileInfo* fi, const std::string& filepath)
{
    int status = ret::A_OK;
    // Create Attic Post
    if(!fi)
        std::cout<<"invalid file info"<<std::endl;

    std::string filename;
    utils::ExtractFileName(filepath, filename);

    // Check for existing post
    std::string postid;
    fi->GetPostID(postid);


    // Get ChunkInfo List
    FileInfo::ChunkMap* pList = fi->GetChunkInfoList();

    // Construct post url
    // TODO :: abstract this common functionality somewhere else, utils?

    std::string posturl;
    ConstructPostUrl(posturl);

    std::string chunkname;
    fi->GetChunkName(chunkname);

    bool post = true;
    Response response;
    if(postid.empty()) {
        // New Post
        std::cout<< " POST URL : " << posturl << std::endl;

                unsigned int size = utils::CheckFilesize(filepath);
        AtticPost p;
        InitAtticPost(p,
                      false,
                      filepath,
                      filename,
                      chunkname,
                      size,
                      pList);

        std::string postBuffer;
        JsonSerializer::SerializeObject(&p, postBuffer);

        std::cout<<"\n\n Attic Post Buffer : " << postBuffer << std::endl;

        AccessToken* at = GetAccessToken();

        status = conops::HttpPost( posturl,
                                   NULL,
                                   postBuffer,
                                   *at,
                                   response );
    }
    else {
        post = false;
        // Modify Post
        posturl += "/";
        posturl += postid;

        std::cout<< " PUT URL : " << posturl << std::endl;
        
        unsigned int size = utils::CheckFilesize(filepath);
        AtticPost p;
        InitAtticPost(p,
                      false,
                      filepath,
                      filename,
                      chunkname,
                      size,
                      pList);

        std::string postBuffer;
        JsonSerializer::SerializeObject(&p, postBuffer);

        AccessToken* at = GetAccessToken();
        status = conops::HttpPut( posturl,
                                   NULL,
                                   postBuffer,
                                   *at,
                                   response );
   }

    // Handle Response
    if(response.code == 200) {
        AtticPost p;
        JsonSerializer::DeserializeObject(&p, response.body);

        std::string postid;
        p.GetID(postid);

        if(!postid.empty()) {
            FileManager* fm = GetFileManager();
            fi->SetPostID(postid); 
            if(post){
                std::string filepath;
                fi->SetPostVersion(0); // temporary for now, change later
                fi->GetFilepath(filepath);

                fm->SetFilePostId(filepath, postid);
            }
        }
    }
    else {
        status = ret::A_FAIL_NON_200;
    }

    return status;
}

int PushTask::InitAtticPost( AtticPost& post,
                             bool pub,
                             const std::string& filepath,
                             const std::string& filename, 
                             const std::string& chunkname, // Depricated
                             unsigned int size,
                             FileInfo::ChunkMap* pList)
{
    int status = ret::A_OK;

    if(pList) {
        post.SetPublic(pub);
        post.AtticPostSetFilepath(filepath);
        post.AtticPostSetFilename(filename);
        post.AtticPostSetSize(size);
        post.AtticPostSetChunkName(chunkname); // Depricated
        
        FileInfo::ChunkMap::iterator itr = pList->begin();

        std::string identifier, postids;
        for(;itr != pList->end(); itr++) {
            identifier.clear();
            postids.clear();

            itr->second.GetChecksum(identifier);
            post.PushBackChunkIdentifier(identifier);
        }

        FileManager* fm = GetFileManager();
        FileInfo* fi = fm->GetFileInfo(filepath);

        std::string chunkpostid;
        if(fi) {
            std::string encKey, iv;
            fi->GetEncryptedKey(encKey);
            fi->GetIv(iv);

            post.AtticPostSetKeyData(encKey);
            post.AtticPostSetIvData(iv);

            fi->GetChunkPostID(chunkpostid);
            post.PushBackChunkPostId(chunkpostid);
        }
    }
    else {
        status = ret::A_FAIL_INVALID_PTR;
    }

    return status;
}


int PushTask::InitChunkPost(ChunkPost& post, FileInfo::ChunkMap& List)
{
    int status = ret::A_OK;

    post.SetChunkInfoList(List);

    return status;
}

int PushTask::GetUploadSpeed()
{
    int speed = -1;
    if(GetConnectionHandle())
        speed = GetConnectionHandle()->GetUploadSpeed();
    return speed;
}

FileInfo* PushTask::RetrieveFileInfo(const std::string& filepath)
{
    FileInfo* fi = GetFileManager()->GetFileInfo(filepath);

    if(!fi)
        fi = GetFileManager()->CreateFileInfo();

    return fi;
}




int PushTask::ProcessFile( const std::string& requestType,
                           const std::string& url,
                           const std::string& filepath,
                           const Credentials& fileCredentials,
                           FileInfo* pFi,
                           Response& resp)
{
    int status = ret::A_OK;

    std::string host, path;
    netlib::ExtractHostAndPath(url, host, path);
            
    boost::asio::io_service io_service; 
    tcp::socket socket(io_service); 
            
    status = netlib::ResolveHost(io_service, socket, host); 
    if(status == ret::A_OK) {
        // Setup SSL handshake
        boost::system::error_code error = boost::asio::error::host_not_found; 
        // setup an ssl context 
        boost::asio::ssl::context ctx( io_service, 
                                       boost::asio::ssl::context::sslv23_client); 
        ctx.set_verify_mode(boost::asio::ssl::context::verify_none);
        boost::asio::ssl::stream<tcp::socket&> ssl_sock(socket, ctx);

        ssl_sock.handshake(boost::asio::ssl::stream_base::client, error);
        if (error) {
            alog::Log( Logger::ERROR, 
                       boost::system::system_error(error).what(), 
                       ret::A_FAIL_SSL_HANDSHAKE);
            status = ret::A_FAIL_SSL_HANDSHAKE;
        }
        if(status != ret::A_OK)
            return status;

        AccessToken* at = GetAccessToken();
        
        std::string boundary;
        utils::GenerateRandomString(boundary, 20);

        std::string fileKey;
        fileCredentials.GetKey(fileKey);

        std::cout << " encryption file key : " << fileKey << std::endl;
        Credentials chunkCred;
        chunkCred.SetKey(fileKey);

        // Build request
        boost::asio::streambuf request;
        std::ostream request_stream(&request);
        //BuildRequestHeader("POST", url, boundary, at, request_stream); 
        netlib::BuildRequestHeader(requestType, url, boundary, at, request_stream); 

        // Build Body Form header
        ChunkPost p;
        std::string body; // we send an empty body for now
        JsonSerializer::SerializeObject(&p, body);

        boost::asio::streambuf requestBody;
        std::ostream part_stream(&requestBody);
        netlib::BuildBodyForm(body, boundary, part_stream);

        // Chunk the Body 
        boost::asio::streambuf chunkedBody;
        std::ostream partbuf(&chunkedBody);
        netlib::ChunkPart(requestBody, partbuf);

        // Start the request
        boost::asio::write(ssl_sock, request); 
        boost::asio::write(ssl_sock, chunkedBody);

        unsigned int filesize = utils::CheckFilesize(filepath);
        // start the process
        std::ifstream ifs;
        ifs.open(filepath.c_str(), std::ifstream::in | std::ifstream::binary);

        unsigned int count = 0;
        unsigned int totalread = 0; // total read count
        if (ifs.is_open()) {
            unsigned int chunksize = cnst::g_unChunkSize;
            char* szChunkBuffer = new char[chunksize];

            chunksize = sizeof(char)*chunksize; // char is always 1, but for good practice calc this

            while(!ifs.eof()) {
                memset(szChunkBuffer, 0, chunksize);
                // read to the buffer
                ifs.read(szChunkBuffer, chunksize);
                unsigned int readcount = ifs.gcount();
                totalread += readcount;

                // append to string
                std::string chunk(szChunkBuffer, chunksize);

                // Calculate plaintext hash
                std::string plaintextHash;
                crypto::GenerateHash(chunk, plaintextHash);

                // create chunk name (hex encoded plaintext hash)
                std::string chunkName;
                utils::StringToHex(plaintextHash, chunkName);

                // Compress
                std::string compressedChunk;
                compress::CompressString(chunk, compressedChunk);

                // Encrypt
                std::string encryptedChunk;
                std::string iv;
                crypto::GenerateIv(iv);
                chunkCred.SetIv(iv);

                std::cout<< " IV : " << iv << std::endl;
                crypto::EncryptStringCFB(compressedChunk, chunkCred, encryptedChunk);
                //crypto::EncryptStringGCM(compressedChunk, chunkCred, encryptedChunk);

                // Base64 Encode
                std::string finishedChunk;
                crypto::Base64EncodeString(encryptedChunk, finishedChunk);

                std::string ciphertextHash;
                crypto::GenerateHash(encryptedChunk, ciphertextHash);

                // Fill Out Chunk info object
                ChunkInfo ci;
                ci.SetChunkName(chunkName);
                ci.SetPlainTextMac(plaintextHash);
                ci.SetCipherTextMac(ciphertextHash);
                ci.SetIv(iv);
                if(pFi) pFi->PushChunkBack(ci);

                // Push chunk back into fileinfo

                // Build Attachment
                boost::asio::streambuf attachment;
                std::ostream attachmentstream(&attachment);
                netlib::BuildAttachmentForm(chunkName, finishedChunk, boundary, count, attachmentstream);

                // create multipart post
                if(totalread >= filesize) {
                    // Add end part
                    netlib::AddEndBoundry(attachmentstream, boundary);

                    // Chunk the end
                    boost::asio::streambuf partEnd;
                    std::ostream partendstream(&partEnd);
                    netlib::ChunkEnd(attachment, partendstream);

                    boost::system::error_code errorcode;
                    static int breakcount = 0;
                    do {
                        boost::asio::write(ssl_sock, partEnd, errorcode); 
                        if(errorcode)
                            std::cout<<errorcode.message()<<std::endl;

                        if(breakcount > 20)
                            break;
                        breakcount++;
                    }
                    while(errorcode);
                    break;

                }
                else {
                    // carry on
                    // Chunk the part
                    boost::asio::streambuf part;
                    std::ostream chunkpartbuf(&part);
                    netlib::ChunkPart(attachment, chunkpartbuf);
                    
                    std::cout<<" write to socket " << std::endl;

                    boost::system::error_code errorcode;
                    static int breakcount = 0;
                    do
                    {
                        boost::asio::write(ssl_sock, part, errorcode); 
                        if(errorcode)
                            std::cout<<errorcode.message()<<std::endl;

                        if(breakcount > 20)
                            break;
                        breakcount++;
                    }
                    while(errorcode);
                }

                // send
                count++;
            }

            boost::asio::streambuf response;
            boost::asio::read_until(ssl_sock, response, "\r\n");
            netlib::InterpretResponse(response, ssl_sock, resp);
        }
        else {
            status = ret::A_FAIL_OPEN_FILE;
        }
    }

    return status;
}

