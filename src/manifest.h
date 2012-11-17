// Manifest file structure
// Within the manifest file itself the structure of data to read in
// is as follows. Line by line
//  Manifest specific: entry count, 
//      entry : file name, file path, chunk name, chunk count, file size
//      entry : ...

#ifndef MANIFEST_H_
#define MANIFEST_H_
#pragma once

#include <fstream>
#include <string>
#include <map>

class FileInfo;

class Manifest
{
    bool WriteOutManifestHeader(std::ofstream &ofs);
public:
    Manifest();
    ~Manifest();

    bool CreateEmptyManifest();
    bool WriteOutManifest();    

    bool InsertFileInfo(FileInfo* fi);
    FileInfo* RetrieveFileInfo(std::string &s);

    void SetFilePath(std::string &filePath) { m_filePath = filePath; }
    
    unsigned int GetEntryCount() { return m_entryCount; }
    void SetEntryCount(unsigned int count) { m_entryCount = count; }

private:
    typedef std::map<std::string, FileInfo*> EntriesMap;
    EntriesMap            m_entries;  // Do not delete entries, just clear the map.
                                                    // FileInfoFactory will take care of deletion.

    std::ifstream       m_ifStream;
    std::ofstream       m_ofStream;

    // Manifest specific data
    std::string         m_filePath;     // path to manifest file
    std::string         m_fileName;     // filename of the manifest
    unsigned int        m_entryCount;   // Number of entries in the manifest
};

#endif
