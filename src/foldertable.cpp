#include "foldertable.h"
#include "logutils.h"

namespace attic { 

bool FolderTable::CreateTable() {
    std::string exc;
    exc += "CREATE TABLE IF NOT EXISTS ";
    exc += table_name();
    exc += " (foldername TEXT, alias TEXT, post_id TEXT, parent_post_id TEXT, deleted INT,";
    exc += " PRIMARY KEY(foldername ASC, alias ASC, post_id ASC, parent_post_id ASC));";
    std::string error;
    bool ret = Exec(exc, error);
    if(!ret)
        log::LogString("manifest_09214", error);
    return ret; 
}

bool FolderTable::InsertFolderInfo(const std::string& foldername, 
                                   const std::string& alias,
                                   const std::string& folderpostid,
                                   const std::string& parentpostid,
                                   const bool deleted) {
    bool ret = false;
    std::string exc;

    if(!IsFolderInManifest(folderpostid))
        exc += "INSERT OR REPLACE INTO ";
    else
        exc += "REPLACE INTO ";
    exc += table_name();
    exc += " (foldername, alias, post_id, parent_post_id, deleted) VALUES (?,?,?,?,?);";  
    
    std::string error;
    ret = PrepareStatement(exc, error);     if(!ret) {log::ls("m_240s",error);return ret;}
    ret = BindText(1, foldername, error);   if(!ret) {log::ls("m_241s",error);return ret;}
    ret = BindText(2, alias, error);        if(!ret) {log::ls("m_241.1s",error);return ret;}
    ret = BindText(3, folderpostid, error); if(!ret) {log::ls("m_242s",error);return ret;}
    ret = BindText(4, parentpostid, error); if(!ret) {log::ls("m_243s",error);return ret;}
    ret = BindInt(5, deleted, error);       if(!ret) {log::ls("m_244s",error);return ret;}
    ret = StepStatement(error);             if(!ret) {log::ls("m_253s",error);return ret;}
    ret = FinalizeStatement(error);         if(!ret) {log::ls("m_254s",error);return ret;}
    return ret;
}

bool FolderTable::RemoveFolderData(const std::string& foldername, 
                                   const std::string& parent_post_id) {
    std::string exc;
    exc += "DELETE FROM ";
    exc += table_name();
    exc += " WHERE foldername=\"";
    exc += foldername;
    exc += "\"";
    exc += " AND";
    exc += " parent_post_id=\"";
    exc += parent_post_id;
    exc += "\";";

    std::string error;
    bool ret = Exec(exc, error);
    if(!ret)                                       
        log::LogString("manifest_91005", error);
    return ret;
}

bool FolderTable::IsFolderInManifest(const std::string& foldername, 
                                     const std::string& parent_post_id) {
    bool ret = false;
    std::string query;
    query += "SELECT EXISTS(SELECT * FROM ";
    query += table_name();
    query += " WHERE foldername=\"";
    query += foldername;
    query += "\"";
    query += " AND";
    query += " parent_post_id=\"";
    query += parent_post_id;
    query += "\");";
     
    std::string error;
    SelectResult res;
    if(Select(query, res, error)) {
        int step = 0;
        for(int i=0; i<res.row()+1; i++) {
            step = i*res.col();
            if(step > 0) {
                std::string r = res.results()[0+step];
                if(r == "1") {
                    ret = true;
                    break;
                }
            }
        }
    }
    else {
        log::LogString("manifest_1901010", error);
    }
    return ret;
}
bool FolderTable::IsFolderInManifest(const std::string& post_id) {
    bool ret = false;
    std::string query;
    query += "SELECT EXISTS(SELECT * FROM ";
    query += table_name();
    query += " WHERE post_id=\"";
    query += post_id;
    query += "\");";
     
    std::string error;
    SelectResult res;
    if(Select(query, res, error)) {
        int step = 0;
        for(int i=0; i<res.row()+1; i++) {
            step = i*res.col();
            if(step > 0) {
                std::string r = res.results()[0+step];
                if(r == "1") {
                    ret = true;
                    break;
                }
            }
        }
    }
    else {
        log::LogString("manifest_24893jgs", error);
    }
    return ret;
}

bool FolderTable::QueryForFolder(const std::string& foldername, 
                                 const std::string parent_post_id, 
                                 Folder& out) {
    bool ret = false;
    std::string query;
    query += "SELECT * FROM ";
    query += table_name();
    query += " WHERE foldername=\"";
    query += foldername;
    query += "\" AND";
    query += " parent_post_id=\"";
    query += parent_post_id;
    query += "\";";

    std::string error;
    SelectResult res;
    if(Select(query, res, error)) {
        int step = 0;
        for(int i=0; i<res.row()+1; i++) {
            step = i*res.col();
            if(step > 0)
                ExtractFolderInfoResults(res, step, out);
        }
        if(step) ret = true;
    }
    else {
        log::LogString("manifest_qqqdg92", error);
    }
    return ret;
}

bool FolderTable::QueryForFolderWithAlias(const std::string& aliased_foldername,
                                          const std::string& parent_post_id,
                                          Folder& out) {
    bool ret = false;
    std::string query;
    query += "SELECT * FROM ";
    query += table_name();
    query += " WHERE alias=\"";
    query += aliased_foldername;
    query += "\" AND";
    query += " parent_post_id=\"";
    query += parent_post_id;
    query += "\";";

    std::string error;
    SelectResult res;
    if(Select(query, res, error)) {
        int step = 0;
        for(int i=0; i<res.row()+1; i++) {
            step = i*res.col();
            if(step > 0)
                ExtractFolderInfoResults(res, step, out);
        }
        if(step) ret = true;
    }
    else {
        log::LogString("manifest_qqqdg92", error);
    }
    return ret;
}

bool FolderTable::QueryForFolder(const std::string& foldername, Folder& out) { // depricated
    bool ret = false;
    std::string query;
    query += "SELECT * FROM ";
    query += table_name();
    query += " WHERE foldername=\"";
    query += foldername;
    query += "\";";

    std::string error;
    SelectResult res;
    if(Select(query, res, error)) {
        int step = 0;
        for(int i=0; i<res.row()+1; i++) {
            step = i*res.col();
            if(step > 0)
                ExtractFolderInfoResults(res, step, out);
        }
        if(step) ret = true;
    }
    else {
        log::LogString("manifest_pmasdg92", error);
    }

    return ret;
}

bool FolderTable::QueryForFolderByPostId(const std::string& post_id, Folder& out) {
    bool ret = false;
    std::string exc;
    exc += "SELECT * FROM ";
    exc += table_name();
    exc += " WHERE post_id=\"";
    exc += post_id;
    exc += "\";";

    std::string error;
    SelectResult res;
    if(Select(exc, res, error)) {
        int step = 0;
        for(int i=0; i<res.row()+1; i++) {
            step = i*res.col();
            if(step > 0)
                ExtractFolderInfoResults(res, step, out);
        }
        ret = true;
    }
    else {
        log::LogString("manifest_-951-05", error);
    }
    return ret;
}

bool FolderTable::QueryAllFoldersForFolder(const std::string& folderid, FolderList& out) {
    bool ret = false;
    std::string query;
    query += "SELECT * FROM ";
    query += table_name();
    query += " WHERE parent_post_id=\"";
    query += folderid;
    query += "\";";

    std::string error;
    SelectResult res;
    if(Select(query, res, error)) {
        int step = 0;
        for(int i=0; i<res.row()+1; i++) {
            step = i*res.col();
            if(step > 0) {
                Folder folder;
                ExtractFolderInfoResults(res, step, folder);
                out.push_back(folder);
            }
        }
        ret = true;
    }
    else {
        log::LogString("manifest_8915", error);
    }
    return ret;
}

bool FolderTable::set_folder_alias(const std::string& post_id, const std::string& alias) {
    std::string exc;
    exc += "UPDATE ";
    exc += table_name();
    exc += " SET alias=\"";
    exc += alias;
    exc += "\" WHERE post_id=\"";
    exc += post_id;
    exc += "\";";

    std::string error;
    bool ret = Exec(exc, error);
    if(!ret)                                       
        log::LogString("manifest_293collapse5", error);
    return ret;

}

bool FolderTable::set_folder_post_id(const std::string& post_id,
                                     const std::string& new_post_id) {
    std::string exc;
    exc += "UPDATE ";
    exc += table_name();
    exc += " SET post_id=\"";
    exc += new_post_id;
    exc += "\" WHERE post_id=\"";
    exc += post_id;
    exc += "\";";

    std::string error;
    bool ret = Exec(exc, error);
    if(!ret)                                       
        log::LogString("manifest_01asgg125", error);
    return ret;
}

bool FolderTable::set_folder_parent_post_id(const std::string& post_id, 
                                            const std::string& parent_post_id) {
    std::string exc;
    exc += "UPDATE ";
    exc += table_name();
    exc += " SET parent_post_id=\"";
    exc += parent_post_id;
    exc += "\" WHERE post_id=\"";
    exc += post_id;
    exc += "\";";

    std::string error;
    bool ret = Exec(exc, error);
    if(!ret)                                       
        log::LogString("manifest_01asgg125", error);
    return ret;
}
bool FolderTable::set_foldername(const std::string& post_id, const std::string& foldername) {
    std::string exc;
    exc += "UPDATE ";
    exc += table_name();
    exc += " SET foldername=\"";
    exc += foldername;
    exc += "\" WHERE post_id=\"";
    exc += post_id;
    exc += "\";";

    std::string error;
    bool ret = Exec(exc, error);
    if(!ret)                                       
        log::LogString("manifest_01asgg125", error);
    return ret;
}

bool FolderTable::set_folder_deleted(const std::string& post_id, bool del) {
    char b[256] = {'\0'};
    snprintf(b, 256, "%d", (int)del);

    std::string exc;
    exc += "UPDATE ";
    exc += table_name();
    exc += " SET deleted=\"";
    exc += b;
    exc += "\" WHERE post_id=\"";
    exc += post_id;
    exc += "\";";

    std::string error;
    bool ret = Exec(exc, error);
    if(!ret) {
        error += " | " + exc;
        log::LogString("manifest_01hasdg125", error);
    }
    return ret;
}

bool FolderTable::IsFolderDeleted(const std::string& post_id) {
    bool ret = false;
    std::string query;
    query += "SELECT deleted FROM ";
    query += table_name();
    query += " WHERE post_id=\"";
    query += post_id;
    query += "\";";

    std::string error;
    SelectResult res;
    if(Select(query.c_str(), res, error)) {
        int step = 0;
        for(int i=0; i<res.row()+1; i++) {
            step = i*res.col();
            if(step > 0) { 
                if(atoi(res.results()[0+step]) > 0) {
                    ret = true;
                }
            }
        }
    }
    else {
        log::LogString("manifest_1958sag", error);
    }
    return ret;

}

bool FolderTable::GetParentPostId(const std::string& post_id, std::string& id_out) {
    bool ret = false;
    std::string query;
    query += "SELECT parent_post_id FROM ";
    query += table_name();
    query += " WHERE post_id=\"";
    query += post_id;
    query += "\";";

    std::string error;
    SelectResult res;
    if(Select(query.c_str(), res, error)) {
        int step = 0;
        for(int i=0; i<res.row()+1; i++) {
            step = i*res.col();
            if(step > 0)
                id_out = res.results()[0+step];
        }
        ret = true;
    }
    else {
        log::LogString("manifest_1958sag", error);
    }
    return ret;
}

bool FolderTable::GetFoldername(const std::string& post_id, std::string& out) {
    bool ret = false;
    std::string query;
    query += "SELECT foldername FROM ";
    query += table_name();
    query += " WHERE post_id=\"";
    query += post_id;
    query+= "\"";

    std::string error;
    SelectResult res;
    if(Select(query, res, error)) {
        int step = 0;
        for(int i=0; i<res.row()+1; i++) {
            step = i*res.col();
            if(step > 0)
                out = res.results()[0+step];
        }
        ret = true;
    }
    else {
        log::LogString("manifest_masg91435", error);
    }

    return ret;
}

void FolderTable::ExtractFolderInfoResults(const SelectResult& res, const int step, Folder& out) {
    out.set_foldername(res.results()[0+step]);
    out.set_local_alias(res.results()[1+step]);
    out.set_folder_post_id(res.results()[2+step]);
    out.set_parent_post_id(res.results()[3+step]);
    std::string deleted = res.results()[4+step];
    if(deleted == "1") out.set_deleted(true);
    else out.set_deleted(false);
}

} // namespace

