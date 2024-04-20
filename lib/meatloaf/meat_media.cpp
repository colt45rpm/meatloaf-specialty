#include "meat_media.h"

// Utility Functions

std::string MImageStream::decodeType(uint8_t file_type, bool show_hidden)
{
    //bool hide = false;
    std::string type = file_type_label[ file_type & 0b00000111 ];
    //if ( file_type == 0 )
    //    hide = true;

    switch ( file_type & 0b11000000 )
    {
        case 0xC0:			// Bit 6: Locked flag (Set produces "<" locked files)
            type += "<";
            //hide = false;
            break;

        case 0x00:
            type += "*";	// Bit 7: Closed flag  (Not  set  produces  "*", or "splat" files)
            //hide = true;
            break;
    }

    return type;
}

/********************************************************
 * Istream impls
 ********************************************************/

// std::string MImageStream::seekNextEntry() {
//     // Implement this to skip a queue of file streams to start of next file and return its name
//     // this will cause the next read to return bytes of "next" file in D64 image
//     // might not have sense in this case, as D64 is kinda random access, not a stream.
//     return "";
// };

bool MImageStream::open() {
    // return true if we were able to read the image and confirmed it is valid.
    // it's up to you in what state the stream will be after open. Could be either:
    // 1. EOF-like state (0 available) and the state will be cleared only after succesful seekNextEntry or seekPath
    // 2. non-EOF-like state, and ready to send bytes of first file, because you did immediate seekNextEntry here

    return false;
};

void MImageStream::close() {

};

uint32_t MImageStream::seekFileSize( uint8_t start_track, uint8_t start_sector )
{
    // Calculate file size
    seekSector(start_track, start_sector);

    size_t blocks = 0; 
    do
    {
        //Debug_printv("t[%d] s[%d]", t, s);
        readContainer(&start_track, 1);
        readContainer(&start_sector, 1);
        blocks++;
        if ( start_track > 0 )
            if ( !seekSector( start_track, start_sector ) )
                break;
    } while ( start_track > 0 );
    blocks--;
    return (blocks * (block_size - 2)) + start_sector - 1;
};


uint16_t MImageStream::readContainer(uint8_t *buf, uint16_t size)
{
    return containerStream->read(buf, size);
}


uint32_t MImageStream::write(const uint8_t *buf, uint32_t size) {
    return -1;
}

uint32_t MImageStream::read(uint8_t* buf, uint32_t size) {
    uint32_t bytesRead = 0;

    //Debug_printv("seekCalled[%d]", seekCalled);
    if ( _position >= _size )
        return 0;

    if(seekCalled) {
        // if we have the stream set to a specific file already, either via seekNextEntry or seekPath, return bytes of the file here
        // or set the stream to EOF-like state, if whle file is completely read.
        bytesRead = readFile(buf, size);

    }
    else {
        // seekXXX not called - just pipe image bytes, so it can be i.e. copied verbatim
        bytesRead = readContainer(buf, size);
    }

    _position += bytesRead;

    return bytesRead;
};

bool MImageStream::isOpen() {

    return _is_open;
};


std::unordered_map<std::string, MImageStream*> ImageBroker::repo;
