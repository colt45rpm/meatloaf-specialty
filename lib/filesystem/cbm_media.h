


#ifndef MEATLOAF_CBM_MEDIA
#define MEATLOAF_CBM_MEDIA

#include "meat_io.h"

#include <map>
#include <bitset>
#include <unordered_map>

#include "../../../include/debug.h"

#include "string_utils.h"


/********************************************************
 * Streams
 ********************************************************/

class CBMImageStream: public MStream {

public:
    CBMImageStream(std::shared_ptr<MStream> is) {
        this->containerStream = is;
        this->m_isOpen = true;
    }

    // MStream methods
    size_t position() override;
    void close() override;
    bool open() override;
    ~CBMImageStream() {
        //Debug_printv("close");
        close();
    }

    // MStream methods
    bool isBrowsable() override { return false; };
    bool isRandomAccess() override { return true; };

    bool seek(size_t pos) override {
        return true;
    };
     bool seek(size_t pos, int mode) override {
        return true;
    };   

    bool seekPath(std::string path) override { return false; };
    std::string seekNextEntry() override { return ""; };

    size_t available() override;
    size_t size() override;
    size_t read(uint8_t* buf, size_t size) override;
    size_t write(const uint8_t *buf, size_t size);

    bool isOpen();

protected:

    bool seekCalled = false;
    std::shared_ptr<MStream> containerStream;

    bool m_isOpen = false;
    size_t m_length = 0;
    size_t m_bytesAvailable = 0;
    size_t m_position = 0;

    CBMImageStream* decodedStream;

    bool show_hidden = false;

    size_t block_size = 256;
    size_t entry_index = 0;  // Currently selected directory entry
    size_t entry_count = -1; // Directory list entry count (-1 unknown)

    enum open_modes { OPEN_READ, OPEN_WRITE, OPEN_APPEND, OPEN_MODIFY };
    std::string file_type_label[8] = { "del", "seq", "prg", "usr", "rel", "cbm", "dir", "???" };

    virtual void seekHeader() = 0;
    virtual bool seekNextImageEntry() = 0;
    void resetEntryCounter() {
        entry_index = 0;
    }

    // Disks
    virtual uint16_t blocksFree() { return 0; };
	virtual uint8_t speedZone( uint8_t track) { return 0; };

    virtual bool seekEntry( std::string filename ) { return false; };
    virtual bool seekEntry( size_t index ) { return false; };

    virtual size_t readFile(uint8_t* buf, size_t size) = 0;
    std::string decodeType(uint8_t file_type, bool show_hidden = false);

private:

    // File
    friend class P00File;

    // Disk
    friend class D64File;
    friend class D71File;
    friend class D80File;
    friend class D81File;
    friend class D82File;
    friend class D8BFile;
    friend class DNPFile;

    // Tape
    friend class T64File;
    friend class TCRTFile;

    // Cartridge

};



/********************************************************
 * Utility implementations
 ********************************************************/
class ImageBroker {
    static std::unordered_map<std::string, CBMImageStream*> repo;
public:
    template<class T> static T* obtain(std::string url) {
        // obviously you have to supply STREAMFILE.url to this function!
        if(repo.find(url)!=repo.end()) {
            return (T*)repo.at(url);
        }

        // create and add stream to broker if not found
        auto newFile = MFSOwner::File(url);
        T* newStream = (T*)newFile->meatStream();

        // Are we at the root of the pathInStream?
        if ( newFile->pathInStream == "")
        {
            Debug_printv("DIRECTORY [%s]", url.c_str());
        }
        else
        {
            Debug_printv("SINGLE FILE [%s]", url.c_str());
        }

        repo.insert(std::make_pair(url, newStream));
        delete newFile;
        return newStream;
    }

    static CBMImageStream* obtain(std::string url) {
        return obtain<CBMImageStream>(url);
    }

    static void dispose(std::string url) {
        if(repo.find(url)!=repo.end()) {
            auto toDelete = repo.at(url);
            repo.erase(url);
            delete toDelete;
        }
    }
};

#endif // MEATLOAF_CBM_MEDIA
