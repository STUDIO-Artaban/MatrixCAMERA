#include "Picture.h"

#include <libeng/Features/Camera/Camera.h>
#include <iostream>
#include <fstream>

#ifdef __ANDROID__
#include <boost/filesystem.hpp>
#include <gst/gst.h>
#include "Wifi/Connexion.h"

#else
#include <libGST/libGST.h>
#include "Connexion.h"

#endif

#ifndef PAID_VERSION
#ifdef __ANDROID__
#include "Level/PanelCoords.h"
#else
#include "PanelCoords.h"
#endif

#define LOGO_CORNER_POS             7 // In pixel (from the bottom right)
#endif

//////
Picture::Picture() : mStatus(STATUS_EXTRACT), mSize(0), mFolder(NULL), mWalk(NULL), mAbort(true), mThread(NULL),
mServer(false), mLandscape(true), mLand(NULL) {
    
    LOGV(LOG_LEVEL_PICTURE, 0, LOG_FORMAT(), __PRETTY_FUNCTION__, __LINE__);
#ifndef PAID_VERSION
    mLogoBuffer = NULL;
#endif
    mData = new char[static_cast<int>(CAM_TEX_WIDTH * CAM_TEX_HEIGHT) * 3];
    mRGB = new char[CAM_WIDTH * CAM_HEIGHT * 3];
}
Picture::Picture(bool server) : mServer(server), mStatus(STATUS_COMPRESS), mSize(0), mFolder(NULL), mData(NULL),
        mWalk(NULL), mAbort(true), mThread(NULL), mLandscape(true), mRGB(NULL), mLand(NULL) {

    LOGV(LOG_LEVEL_PICTURE, 0, LOG_FORMAT(), __PRETTY_FUNCTION__, __LINE__);
#ifndef PAID_VERSION
    mLogoBuffer = NULL;
#endif
}
Picture::Picture(unsigned int size) : mStatus(STATUS_FILL), mSize(static_cast<int>(size)), mFolder(NULL), mAbort(true),
        mThread(NULL), mServer(false), mLandscape(true), mRGB(NULL), mLand(NULL) {

    LOGV(LOG_LEVEL_PICTURE, 0, LOG_FORMAT(" - s:%d"), __PRETTY_FUNCTION__, __LINE__, size);
    assert(mSize > 0);
    mData = new char[size];
    mWalk = mData;
#ifndef PAID_VERSION
    mLogoBuffer = NULL;
#endif
}
Picture::Picture(const std::string* folder) : mStatus(STATUS_RECORD), mServer(false), mSize(0), mFolder(folder),
        mData(NULL), mWalk(NULL), mAbort(true), mThread(NULL), mLandscape(true), mRGB(NULL), mLand(NULL) {

    LOGV(LOG_LEVEL_PICTURE, 0, LOG_FORMAT(" - f:%x (%s)"), __PRETTY_FUNCTION__, __LINE__, folder,
            (folder)? folder->c_str():"null");
#ifndef PAID_VERSION
    mLogoBuffer = NULL;
#endif
}
Picture::~Picture() {

    LOGV(LOG_LEVEL_PICTURE, 0, LOG_FORMAT(), __PRETTY_FUNCTION__, __LINE__);
    if (mThread) {

        mAbort = true;
        mThread->join();
        delete mThread;
    }
    if (mData)
        delete [] mData;
    if (mRGB)
        delete [] mRGB;
    if (mLand)
        delete [] mLand;
}

std::string Picture::getFileName(const std::string* folder, const char* extension, unsigned char client) {

    LOGV(LOG_LEVEL_PICTURE, 0, LOG_FORMAT(" - c:%d"), __PRETTY_FUNCTION__, __LINE__, client);
    std::string fileName(*folder);
    fileName.append(MCAM_SUB_FOLDER);
    fileName.append(PIC_FILE_NAME);
    switch (DIGIT_COUNT(client)) {
        case 1: fileName.append("00"); break;
        case 2: fileName += '0'; break;
    }
    fileName.append(numToStr<short>(static_cast<short>(client)));
    fileName.append(extension);
    return fileName;
}

#ifdef DEBUG
bool Picture::gstLaunch(const std::string &pipeline, bool crash) {

    LOGV(LOG_LEVEL_PICTURE, 0, LOG_FORMAT(" - p:%s; c:%s"), __PRETTY_FUNCTION__, __LINE__, pipeline.c_str(),
            (crash)? "true":"false");
#else
bool Picture::gstLaunch(const std::string &pipeline) {

    LOGV(LOG_LEVEL_PICTURE, 0, LOG_FORMAT(" - p:%s"), __PRETTY_FUNCTION__, __LINE__, pipeline.c_str());
#endif

#ifdef __ANDROID__
    GError* error = NULL;
    GstElement* launch = gst_parse_launch(pipeline.c_str(), &error);
    if (error) {

        LOGE(LOG_FORMAT(" - gStreamer error: %s"), __PRETTY_FUNCTION__, __LINE__, error->message);
        g_clear_error(&error);
#ifdef DEBUG
        if (crash)
            assert(NULL);
#endif
        return false;
    }
    gst_element_set_state(launch, GST_STATE_PAUSED);
    gst_element_get_state(launch, NULL, NULL, -1);
    gst_element_set_state(launch, GST_STATE_PLAYING);

    // Wait EOS
    GstMessage* msg = gst_bus_poll(gst_element_get_bus(launch), (GstMessageType)(GST_MESSAGE_ERROR | GST_MESSAGE_EOS), -1);
    switch (GST_MESSAGE_TYPE(msg)) {

        case GST_MESSAGE_EOS:
            break; // EOS

        case GST_MESSAGE_ERROR: {

            GError* err = NULL;
            gchar* dbg = NULL;
            gst_message_parse_error(msg, &err, &dbg);
            if (err) {

                LOGE(LOG_FORMAT(" - gStreamer error: %s (%s)"), __PRETTY_FUNCTION__, __LINE__, err->message,
                        (dbg)? dbg:"none");
                g_error_free(err);
            }
            gst_message_unref(msg);
            gst_element_set_state(launch, GST_STATE_NULL);
            gst_object_unref(GST_OBJECT(launch));

#ifdef DEBUG
            if (crash)
                assert(NULL);
#endif
            return false;
        }
    }
    gst_message_unref(msg);
    g_usleep(1000); // Why not...

    gst_element_set_state(launch, GST_STATE_NULL);
    gst_object_unref(GST_OBJECT(launch));

#else
    if (!lib_gst_launch(pipeline.c_str())) {

        LOGE(LOG_FORMAT(" - GStreamer error: %s"), __PRETTY_FUNCTION__, __LINE__, pipeline.c_str());
#ifdef DEBUG
        if (crash)
            assert(NULL);
#endif
        return false;
    }
#endif
    return true;
}

signed char Picture::fill(const ClientMgr* mgr) {

    LOGV(LOG_LEVEL_PICTURE, 0, LOG_FORMAT(" - m:%x"), __PRETTY_FUNCTION__, __LINE__, mgr);
    LOGI(LOG_LEVEL_PICTURE, 0, LOG_FORMAT(" - Receive length: %d (size:%d)"), __PRETTY_FUNCTION__, __LINE__,
            mgr->getRcvLength() - CHECKSUM_LEN - SECURITY_LEN, mSize);

    unsigned short len = mgr->getRcvLength() - CHECKSUM_LEN - SECURITY_LEN;
#ifdef DEBUG
    if (mStatus != STATUS_FILL) {
        LOGE(LOG_FORMAT(" - Wrong %d status"), __PRETTY_FUNCTION__, __LINE__, mStatus);
        assert(NULL);
    }
    if (len < 1) {
        LOGE(LOG_FORMAT(" - Wrong length to fill: %d"), __PRETTY_FUNCTION__, __LINE__, len);
        assert(NULL);
    }
#endif
    if ((!Connexion::verifyCheckSum(mgr)) || (!Connexion::verifySecurity(mgr))) {

        LOGE(LOG_FORMAT(" - Checksum/Security error"), __PRETTY_FUNCTION__, __LINE__);
        //mStatus = STATUS_ERROR;
        return LIBENG_NO_DATA; // < 0
    }
    memcpy(mWalk, mgr->getRcvBuffer(), len);

    mWalk += len;
    mSize -= len;
    if (mSize < 0) {

        LOGE(LOG_FORMAT(" - Wrong %d size"), __PRETTY_FUNCTION__, __LINE__, mSize);
        mStatus = STATUS_ERROR;
        return LIBENG_NO_DATA;
    }
    return (!mSize)? 0:1;
}

void Picture::orientation(bool land2port) {

    LOGV(LOG_LEVEL_PICTURE, 0, LOG_FORMAT(" - l:%s"), __PRETTY_FUNCTION__, __LINE__, (land2port)? "true":"false");

    char* buffer = mData;
    unsigned char pixel = 4;
    if (mStatus == STATUS_EXTRACT) { // RGB

        assert(mRGB);
        assert(mSize == (CAM_WIDTH * CAM_HEIGHT * 3));

        buffer = mRGB;
        pixel = 3;
    }
#ifdef DEBUG
    else { // RGBA

        assert(mData);
        assert(mSize == (CAM_WIDTH * CAM_HEIGHT * 4));
    }
#endif
    if (!mLand)
        mLand = new char[mSize];
    std::memcpy(mLand, buffer, mSize);

    for (short xPort = 0, xLand = 0; xPort < CAM_WIDTH; ++xPort, ++xLand) {
        for (short yPort = 0, yLand = (CAM_HEIGHT - 1); yPort < CAM_HEIGHT; ++yPort, --yLand) {

            int iPort = ((xPort * CAM_HEIGHT) + yPort) * pixel;
            int iLand = ((yLand * CAM_WIDTH) + xLand) * pixel;

            if (!land2port) // From portrait to landscape
                std::swap<int>(iPort, iLand);
            //else // From landscape to portrait

            buffer[iPort + 0] = mLand[iLand + 0];
            buffer[iPort + 1] = mLand[iLand + 1];
            buffer[iPort + 2] = mLand[iLand + 2];
            //buffer[i + 3] = 0xff;
        }
    }
}

#ifndef PAID_VERSION
void Picture::insert() {

    LOGV(LOG_LEVEL_PICTURE, 0, LOG_FORMAT(), __PRETTY_FUNCTION__, __LINE__);
    assert(mData);
    assert(mSize == (CAM_WIDTH * CAM_HEIGHT * 4));

    short camWidth = CAM_WIDTH;
    short camHeight = CAM_HEIGHT;
    if (!mLandscape)
        std::swap<short>(camWidth, camHeight);

    int xData = ((camHeight - LOGO_CORNER_POS - LOGO_HEIGHT) * camWidth * 4) + ((camWidth - LOGO_CORNER_POS -
            LOGO_WIDTH) * 4);
    int xLogo = (LOGO_Y0 * static_cast<int>(FONT_TEX_WIDTH) * 4) + (LOGO_X0 * 4);
    for (short y = 0; y < LOGO_HEIGHT; ++y) {
        for (short x = 0; x < LOGO_WIDTH; ++x) {

            int i = ((y * LOGO_WIDTH) + x) * 4;

            float alpha = mLogoBuffer[i + xLogo + 3] / MAX_COLOR;
            mData[i + xData + 0] = mData[i + xData + 0] - (mData[i + xData + 0] * alpha) + (mLogoBuffer[i + xLogo + 0] * alpha);
            mData[i + xData + 1] = mData[i + xData + 1] - (mData[i + xData + 1] * alpha) + (mLogoBuffer[i + xLogo + 1] * alpha);
            mData[i + xData + 2] = mData[i + xData + 2] - (mData[i + xData + 2] * alpha) + (mLogoBuffer[i + xLogo + 2] * alpha);
            //mData[i + xData + 3] = 0xff;
        }
        xData += (camWidth - LOGO_WIDTH) * 4;
        xLogo += (static_cast<int>(FONT_TEX_WIDTH) - LOGO_WIDTH) * 4;
    }
}
#endif

bool Picture::removePath(const std::string* folder) {

    LOGV(LOG_LEVEL_PICTURE, 0, LOG_FORMAT(" - f:%x (%s)"), __PRETTY_FUNCTION__, __LINE__, folder,
         (folder)? folder->c_str():"null");
    std::string path(*folder);
    path.append(MCAM_SUB_FOLDER);
#ifdef __ANDROID__
    return boost::filesystem::remove_all(path.c_str()); //, system::error_code& ec);
#else
    return static_cast<bool>([[NSFileManager defaultManager] removeItemAtPath:[NSString stringWithUTF8String:path.c_str()]
                                                                               error:nil]);
#endif
}
bool Picture::createPath(const std::string* folder) {

    LOGV(LOG_LEVEL_PICTURE, 0, LOG_FORMAT(" - f:%x (%s)"), __PRETTY_FUNCTION__, __LINE__, folder,
         (folder)? folder->c_str():"null");
    std::string path(*folder);
    path.append(MCAM_SUB_FOLDER);
#ifdef __ANDROID__
    return boost::filesystem::create_directory(path.c_str()); //, system::error_code& ec);
#else
    return static_cast<bool>([[NSFileManager defaultManager] createDirectoryAtPath:[NSString stringWithUTF8String:path.c_str()]
                              withIntermediateDirectories:NO attributes:nil error:nil]);
#endif
}
bool Picture::store(const char* extension, size_t size, short client) const {

    LOGV(LOG_LEVEL_PICTURE, 0, LOG_FORMAT(" - e:%s; s:%d; c:%d (s:%d)"), __PRETTY_FUNCTION__, __LINE__, extension, size,
            client, mStatus);
    assert(mFolder);

    std::string fileName;
    if (mStatus != STATUS_RECORD)
        fileName.assign(getFileName(mFolder, extension, static_cast<unsigned char>(client)));

    else { // STATUS_RECORD

        fileName.assign(*mFolder);
        fileName.append(MCAM_SUB_FOLDER);
        fileName.append(PIC_FILE_NAME);
        fileName.append(numToStr<short>(client));
        fileName.append(extension);
    }
    FILE* file = fopen(fileName.c_str(), "wb");
    if (!file) {

        LOGE(LOG_FORMAT(" - Failed to create file %s"), __PRETTY_FUNCTION__, __LINE__, fileName.c_str());
        assert(NULL);
        return false;
    }
    if (fwrite(mData, sizeof(char), size, file) != size) {

        fclose(file);
        LOGE(LOG_FORMAT(" - Failed to write %d bytes into file %s"), __PRETTY_FUNCTION__, __LINE__, size,
                fileName.c_str());
        assert(NULL);
        return false;
    }
    fclose(file);
    return true;
}

#ifndef PAID_VERSION
void Picture::save(const unsigned char* logo, bool landscape, unsigned char client) {

    LOGV(LOG_LEVEL_PICTURE, 0, LOG_FORMAT(" - l:%x; l:%s; c:%d (s:%d)"), __PRETTY_FUNCTION__, __LINE__, logo,
            (landscape)? "true":"false", client, mStatus);
#else
void Picture::save(bool landscape, unsigned char client) {

    LOGV(LOG_LEVEL_PICTURE, 0, LOG_FORMAT(" - l:%s; c:%d (s:%d)"), __PRETTY_FUNCTION__, __LINE__,
            (landscape)? "true":"false", client, mStatus);
#endif
    switch (mStatus) {

        case STATUS_COMPRESS: {

            mLandscape = landscape;
#ifndef PAID_VERSION
            assert(logo);
            mLogoBuffer = logo;
#endif
            assert(!mThread);
            mAbort = false;
            mThread = new boost::thread(Picture::startProcessThread, this);
            break;
        }
        case STATUS_FILL: {

            assert(mFolder);
            assert(!mSize);
            assert((mWalk - mData) > 0);
            assert(mData);

            // Save to JPEG file
            createPath(mFolder);
            if (!store(JPEG_FILE_EXTENSION, mWalk - mData, static_cast<short>(client + 1))) { // + 1 -> Server will have 0 index

                mStatus = STATUS_ERROR;
                break; // Error
            }
            mStatus = STATUS_OK; // Done
            break;
        }
#ifdef DEBUG
        default: {

            LOGE(LOG_FORMAT(" - Unexpected %d status"), __PRETTY_FUNCTION__, __LINE__, mStatus);
            assert(NULL);
            break;
        }
#endif
    }
}

#ifndef PAID_VERSION
bool Picture::record(const unsigned char* logo, bool landscape, short client) {

    LOGV(LOG_LEVEL_PICTURE, 0, LOG_FORMAT(" - l:%x; l:%s; c:%d (s:%d)"), __PRETTY_FUNCTION__, __LINE__, logo,
            (landscape)? "true":"false", client, mStatus);
    mLogoBuffer = logo;
#else
bool Picture::record(bool landscape, unsigned char client) {

    LOGV(LOG_LEVEL_PICTURE, 0, LOG_FORMAT(" - l:%s; c:%d (s:%d)"), __PRETTY_FUNCTION__, __LINE__,
            (landscape)? "true":"false", client, mStatus);
#endif
    assert(mStatus == STATUS_RECORD);

    mLandscape = landscape;

    // Fill buffer from BIN file (BGRA/RGBA)
    std::string fileName(*mFolder);
    fileName.append(MCAM_SUB_FOLDER);
    fileName.append(PIC_FILE_NAME);
    fileName.append(numToStr<short>(client));
    fileName.append(BIN_FILE_EXTENSION);
    if (!open(fileName)) {

        remove(fileName.c_str()); // Delete BIN file
        return false;
    }

#ifndef __ANDROID__
    LOGI(LOG_LEVEL_PICTURE, 0, LOG_FORMAT(" - Convert BGRA to RGBA"), __PRETTY_FUNCTION__, __LINE__);

    char* rgba;
    try { rgba = new char[CAM_HEIGHT * CAM_WIDTH * 4]; }
    catch (const std::bad_alloc &e) {

        LOGW(LOG_FORMAT(" - Failed to allocate RGBA buffer"), __PRETTY_FUNCTION__, __LINE__);
        return false;
    }
    // Convert from BGRA to RGBA (ARGB)
    for (int y = 0, pix = 0; y < CAM_HEIGHT; ++y)
        for (int x = 0; x < CAM_WIDTH; ++x, pix += 4) {

            rgba[pix] = mData[pix + 2];
            rgba[pix + 1] = mData[pix + 1];
            rgba[pix + 2] = mData[pix];
            rgba[pix + 3] = mData[pix + 3];
        }

    delete [] mData;
    mData = rgba;
#endif

    if (!mLandscape)
        orientation(true); // From landscape to portrait

#ifndef PAID_VERSION
    // Add logo
    insert();
#endif

    // Save into BIN
    if (!store(BIN_FILE_EXTENSION, static_cast<size_t>(mSize), client)) {

        remove(fileName.c_str());
        return false;
    }
    LOGI(LOG_LEVEL_PICTURE, 0, LOG_FORMAT(" - Convert BIN into JPEG"), __PRETTY_FUNCTION__, __LINE__);
    std::string pipeline("filesrc location=");
    pipeline.append(fileName);
    pipeline.append(" blocksize=");
    pipeline.append(numToStr<int>(mSize)); // Size
    pipeline.append(" ! video/x-raw,format=RGBA,width=");
    if (mLandscape)
        pipeline.append(numToStr<short>(CAM_WIDTH));
    else
        pipeline.append(numToStr<short>(CAM_HEIGHT));
    pipeline.append(",height=");
    if (mLandscape)
        pipeline.append(numToStr<short>(CAM_HEIGHT));
    else
        pipeline.append(numToStr<short>(CAM_WIDTH));
    pipeline.append(",framerate=1/1 ! videoconvert ! video/x-raw,format=RGB,framerate=1/1 ! jpegenc ! filesink location=");
    pipeline.append(*mFolder);
    pipeline.append(MCAM_SUB_FOLDER);
    pipeline.append(PIC_FILE_NAME);
    pipeline.append(numToStr<short>(client));
    pipeline.append(JPEG_FILE_EXTENSION);

    if (!gstLaunch(pipeline)) {

        remove(fileName.c_str());
        return false;
    }
    LOGI(LOG_LEVEL_PICTURE, 0, LOG_FORMAT(" - Delete BIN file (%s)"), __PRETTY_FUNCTION__, __LINE__, fileName.c_str());
    remove(fileName.c_str());
    return true;
}

bool Picture::open(const std::string &fileName) {

#ifdef DEBUG
    LOGV(LOG_LEVEL_PICTURE, 0, LOG_FORMAT(" - f:%s (s:%d)"), __PRETTY_FUNCTION__, __LINE__, fileName.c_str(), mStatus);
    if (mStatus != STATUS_EXTRACT)
        assert(!mData);
#endif
    std::ifstream ifs(fileName.c_str(), std::ifstream::binary);
    if (!ifs.is_open()) {

        LOGE(LOG_FORMAT(" - Failed to open file %s"), __PRETTY_FUNCTION__, __LINE__, fileName.c_str());
        mStatus = STATUS_ERROR;
        assert(NULL);
        return false;
    }
    std::filebuf* pbuf = ifs.rdbuf();
    mSize = static_cast<int>(pbuf->pubseekoff(0, ifs.end, ifs.in));
#ifdef DEBUG
    LOGI(LOG_LEVEL_PICTURE, 0, LOG_FORMAT(), __PRETTY_FUNCTION__, __LINE__);
    if (mStatus == STATUS_EXTRACT)
        assert(mSize == (CAM_WIDTH * CAM_HEIGHT * 3)); // RGB
#endif
    if (mSize < 1) {

        ifs.close();
        LOGE(LOG_FORMAT(" - Wrong %s file size (%d)"), __PRETTY_FUNCTION__, __LINE__, fileName.c_str(), mSize);
        mStatus = STATUS_ERROR;
        assert(NULL);
        return false;
    }
    pbuf->pubseekpos(0, ifs.in);
    if (!mData) {

        try { mData = new char[mSize]; }
        catch (const std::bad_alloc &e) {

            LOGW(LOG_FORMAT(" - Failed to allocate data buffer"), __PRETTY_FUNCTION__, __LINE__);
            mStatus = STATUS_ERROR;
            ifs.close();
            return false;
        }
    }
#ifdef DEBUG
    else
        assert(mStatus == STATUS_EXTRACT);
#endif
    pbuf->sgetn(mData, mSize);
    ifs.close();
    return true;
}

bool Picture::extract(bool landscape, short frame) {

    LOGV(LOG_LEVEL_PICTURE, 0, LOG_FORMAT(" - l:%s; f:%d (s:%d; d:%x; l:%d; r:%x)"), __PRETTY_FUNCTION__, __LINE__,
         (landscape)? "true":"false", frame, mStatus, mData, mSize, mRGB);
    assert(mStatus == STATUS_EXTRACT);
    assert(mFolder);
    assert(mData);
    assert(mRGB);

    std::string fileName(getFileName(mFolder, JPEG_FILE_EXTENSION));
    fileName.resize(fileName.size() - 7); // '000.jpg' contains 7 characters
    fileName.append(numToStr<short>(frame));
    fileName.append(JPEG_FILE_EXTENSION);

    // Uncompress from JPEG to RGB (to BIN file)
    std::string pipeline("filesrc location=");
    pipeline.append(fileName);
    pipeline.append(" ! jpegdec ! videoconvert ! video/x-raw,format=RGB ! filesink location=");
    fileName.resize(fileName.size() - sizeof(JPEG_FILE_EXTENSION) + 1);
    fileName.append(BIN_FILE_EXTENSION);
    pipeline.append(fileName);

    if (!gstLaunch(pipeline))
        return false;

    // Open BIN file (RGB)
    if (!open(fileName))
        return false;

    LOGI(LOG_LEVEL_PICTURE, 0, LOG_FORMAT(), __PRETTY_FUNCTION__, __LINE__);
    assert(mSize == (CAM_WIDTH * CAM_HEIGHT * 3));

    std::memcpy(mRGB, mData, CAM_WIDTH * CAM_HEIGHT * 3);
    if (!landscape)
        orientation(false); // From portrait to landscape

    // Put RGB buffer into a video texture buffer (64 texels)
    int xLag = 0;
    for (short y = 0; y < CAM_HEIGHT; ++y) {
        for (short x = 0; x < CAM_WIDTH; ++x) {

            int i = ((y * CAM_WIDTH) + x) * 3;
            mData[i + xLag + 0] = mRGB[i + 0];
            mData[i + xLag + 1] = mRGB[i + 1];
            mData[i + xLag + 2] = mRGB[i + 2];
        }
        xLag += static_cast<int>(CAM_TEX_WIDTH - CAM_WIDTH) * 3;
    }
    mSize = static_cast<int>(CAM_TEX_WIDTH * CAM_TEX_HEIGHT) * 3;

    // Save it into BIN file
    mStatus = STATUS_RECORD;
    if (!store(BIN_FILE_EXTENSION, static_cast<size_t>(mSize), frame)) {

        mStatus = STATUS_EXTRACT;
        return false;
    }
    mStatus = STATUS_EXTRACT;

    // Delete JPEG file
    fileName.resize(fileName.size() - sizeof(BIN_FILE_EXTENSION) + 1);
    fileName.append(JPEG_FILE_EXTENSION);
    remove(fileName.c_str());

    return true;
}

void Picture::processThreadRunning() {

    LOGV(LOG_LEVEL_PICTURE, 0, LOG_FORMAT(" - Begin (s:%d)"), __PRETTY_FUNCTION__, __LINE__, mStatus);
    assert(mStatus == STATUS_COMPRESS);
    assert(mFolder);

    while (!mAbort) {

        Camera* camera = Camera::getInstance();
        if (!camera->isBuffered()) {

            boost::this_thread::sleep(boost::posix_time::milliseconds(100));
            continue;
        }

        // Camera buffer is ready so convert it into JPEG
        mData = const_cast<char*>(camera->getBuffer());
        mSize = camera->getBufferLen();

        LOGI(LOG_LEVEL_PICTURE, 0, LOG_FORMAT(" - Convert RGBA to JPEG"), __PRETTY_FUNCTION__, __LINE__);
        if (!mLandscape)
            orientation(true); // From landscape to portrait

#ifndef PAID_VERSION
        // Add logo
        insert();
#endif
        createPath(mFolder);
        if (!store(BIN_FILE_EXTENSION, camera->getBufferLen())) { // Save into BIN file

            mAbort = true;
            mStatus = STATUS_ERROR;
            break; // Error
        }
        std::string pipeline("filesrc location=");
        pipeline.append(*mFolder);
        pipeline.append(MCAM_SUB_FOLDER);
        pipeline.append(PIC_FILE_NAME);
        pipeline.append("000.bin blocksize="); // Client + Extension
        pipeline.append(numToStr<int>(mSize)); // Size
        pipeline.append(" ! video/x-raw,format=RGBA,width=");
        if (mLandscape)
            pipeline.append(numToStr<short>(CAM_WIDTH));
        else
            pipeline.append(numToStr<short>(CAM_HEIGHT));
        pipeline.append(",height=");
        if (mLandscape)
            pipeline.append(numToStr<short>(CAM_HEIGHT));
        else
            pipeline.append(numToStr<short>(CAM_WIDTH));
        pipeline.append(",framerate=1/1 ! videoconvert ! video/x-raw,format=RGB,framerate=1/1 ! jpegenc ! filesink location=");
        pipeline.append(*mFolder);
        pipeline.append(MCAM_SUB_FOLDER);
        pipeline.append(PIC_FILE_NAME);
        pipeline.append("000.jpg");

        if (!gstLaunch(pipeline)) {

            mAbort = true;
            mStatus = STATUS_ERROR;
            break; // Error
        }
        mData = NULL; // Avoid to delete buffer (let's camera delete it)
        if ((!mServer) && (!open(getFileName(mFolder, JPEG_FILE_EXTENSION)))) // Do not open it for server
            break;

        mAbort = true;
        mStatus = STATUS_OK; // Done
    }
    LOGI(LOG_LEVEL_PICTURE, 0, LOG_FORMAT(" - Finished"), __PRETTY_FUNCTION__, __LINE__);
}
void Picture::startProcessThread(Picture* pic) {

    LOGV(LOG_LEVEL_PICTURE, 0, LOG_FORMAT(" - p:%x"), __PRETTY_FUNCTION__, __LINE__, pic);
    pic->processThreadRunning();
}
