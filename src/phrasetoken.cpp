#include "phrasetoken.h"

#include <fstream>

#include "utils.h"
#include "errorcodes.h"

namespace attic {

void PhraseToken::Serialize(Json::Value& root) {
    root["phrase_key"] = phrase_key_;
    root["salt"] = salt_;
    root["iv"] = iv_;
}

void PhraseToken::Deserialize(Json::Value& root) {
    phrase_key_ = root.get("phrase_key", "").asString();
    salt_ = root.get("salt", "").asString();
    iv_ = root.get("iv", "").asString();
}

int PhraseToken::SaveToFile(const std::string& filepath) {
    std::ofstream ofs;
    ofs.open(filepath.c_str(), std::ofstream::out | std::ofstream::binary);

    if(!ofs.is_open())
        return ret::A_FAIL_OPEN_FILE;

    std::string serialized;
    jsn::SerializeObject(this, serialized);

    ofs.write(serialized.c_str(), serialized.size());
    ofs.close();

    return ret::A_OK;
}

int PhraseToken::LoadFromFile(const std::string& filepath) {
    std::ifstream ifs;                                                                            
    ifs.open(filepath.c_str(), std::ifstream::in | std::ifstream::binary);                        
                                                                                                  
    if(!ifs.is_open())
        return ret::A_FAIL_OPEN_FILE;
                                                                                                  
    unsigned int size = utils::CheckIStreamSize(ifs);
    char* pBuf = new char[size+1];
    pBuf[size] = '\0';
                                                                                                  
    ifs.read(pBuf, size);                                                                         
                                                                                                  
    // sanity check size and readcount should be the same                                         
    int readcount = ifs.gcount();
    if(readcount != size)
        std::cout<<"READCOUNT NOT EQUAL TO SIZE\n";                                               
    
    std::string loaded(pBuf);                                                                     
    
    if(pBuf) {
        delete[] pBuf;                                                                            
        pBuf = 0;                                                                                 
    }                                                                                             
    // Deserialize into self.                                                                     
    jsn::DeserializeObject(this, loaded);                                              
    
    return ret::A_OK;                                                                             
}

}//namespace
