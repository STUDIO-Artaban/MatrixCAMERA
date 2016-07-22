#ifndef PICTURE_H_
#define PICTURE_H_

#include "Global.h"

#include <libeng/Log/Log.h>
#include <libeng/Tools/Tools.h>
#include <boost/thread.hpp>
#include <string>

#ifdef __ANDROID__
#include "Wifi/ClientMgr.h"
#else
#include "ClientMgr.h"
#endif

#define MCAM_SUB_FOLDER         "/MCAM"
#define PIC_FILE_NAME           "/img_"

#define JPEG_FILE_EXTENSION     ".jpg"
#define BIN_FILE_EXTENSION      ".bin"
#ifdef __ANDROID__
#define GP3_FILE_EXTENSION      ".3gp"
#else
#define AAC_FILE_EXTENSION      ".aac"
#endif

#define CHECKSUM_LEN            3 // In byte (1024 * 255 = 261120 = 3FC00 -> 3 bytes)
#define SECURITY_LEN            2 // ... (65535 = FFFF -> 2 bytes)

using namespace eng;

//////
class Picture {

private:
    const std::string* mFolder;
    char* mData;
    char* mWalk;
    char* mRGB;
    char* mLand;

    int mSize; // Max signed integer > 640*480*4*((255*2*2)+(7*9)+(3*9)) == 1'363'968'000

    bool mServer;
    bool mLandscape;

    volatile bool mAbort;
    boost::thread* mThread;

    void processThreadRunning();
    static void startProcessThread(Picture* pic);

    enum {

        STATUS_ERROR = 0,
        STATUS_OK, // Done

        STATUS_COMPRESS, // Compress from camera buffer to JPEG file (see constructors)
        STATUS_FILL, // Create JPEG file from downloaded client buffer (see constructors & 'retry' method)
        STATUS_RECORD,
        // -> Fill buffer from BIN file (BGRA/RGBA)
        // -> Convert BGRA to RGBA (if needed)
        // -> Add logo (if needed)
        // -> Save into BIN
        // -> Convert BIN into JPEG
        // -> Delete BIN file

        STATUS_EXTRACT // See constructors
        // -> Uncompress JPEG file into a BIN file
        // -> Read BIN file to get RGB buffer
        // -> Apply orientation (into RGB buffer)
        // -> Convert RGB buffer into a 64 texels buffer
        // -> Save it into BIN file
        // -> Delete JPEG file
    };
    unsigned char mStatus;

public:
    Picture();
    Picture(bool server);
    Picture(unsigned int size);
    Picture(const std::string* folder);
    virtual ~Picture();

    static bool removePath(const std::string* folder);
    static bool createPath(const std::string* folder);

#ifdef DEBUG
    static bool gstLaunch(const std::string &pipeline, bool crash = true);
#else
    static bool gstLaunch(const std::string &pipeline);
#endif

    inline void setFolder(const std::string* folder) { mFolder = folder; }
    inline bool isDone() const { return (mStatus == STATUS_OK); }
    inline bool isError() const { return (mStatus == STATUS_ERROR); }

private:
#ifndef PAID_VERSION
    const unsigned char* mLogoBuffer;

    void insert(); // Insert MCAM logo into buffer
#endif
    void orientation(bool land2port); // Convert buffer from portrait/landscape to landscape/portrait

    bool store(const char* extension, size_t size, short client = 0) const;
    bool open(const std::string &fileName); // Fill buffer from local JPEG/BIN file (no passing parameter by reference)

    //////
public:
    static std::string getFileName(const std::string* folder, const char* extension, unsigned char client = 0);

    inline unsigned int getSize() const { return static_cast<unsigned int>(mSize); }
    inline const char* getBuffer() const { return mData; }

    signed char fill(const ClientMgr* mgr);
    inline void retry(unsigned int size) {

        LOGV(LOG_LEVEL_PICTURE, 0, LOG_FORMAT(" - s:%d"), __PRETTY_FUNCTION__, __LINE__, size);
        assert(mStatus == STATUS_ERROR);
        assert(static_cast<int>(size) > 0);

        mStatus = STATUS_FILL;
        mSize = static_cast<int>(size);
    };

#ifndef PAID_VERSION
    void save(const unsigned char* logo, bool landscape, unsigned char client = 0);
    bool record(const unsigned char* logo, bool landscape, short client);
#else
    void save(bool landscape, unsigned char client = 0);
    bool record(bool landscape, short client);
#endif
    bool extract(bool landscape, short frame);

};

#endif // PICTURE_H_
