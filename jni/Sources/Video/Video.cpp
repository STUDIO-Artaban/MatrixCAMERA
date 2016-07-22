#include "Video.h"

#include <libeng/Storage/Storage.h>
#include <libeng/Player/Player.h>
#include <stdio.h>
#include <time.h>
#include <iostream>
#include <fstream>

#ifdef __ANDROID__
#include <boost/filesystem.hpp>
#include "Wifi/Connexion.h"
#include "Share/Share.h"

#else
#include "Connexion.h"
#include "Share.h"

#endif

#define VIDEO_FILENAME              "/MCAM_" // MCAM_YYYYMMDD_HHMMSS.webm/.mov
#define WEBM_FILE_EXTENSION         ".webm"
#define MOV_FILE_EXTENSION          ".mov"
#define WAV_FILE_EXTENSION          ".wav"
#define OGG_FILE_EXTENSION          ".ogg"

#ifdef __ANDROID__
#define STORE_MEDIA_SUCCEEDED       "Video saved into '/Movies/MCAM' folder"
#else
#define STORE_MEDIA_SUCCEEDED       "Video added into your album!"
#define STORE_MEDIA_FAILED          "ERROR: Failed to add video into album! Share it to be able to save it."

#define MOV_SUB_FOLDER              "/MOVIES"
#endif
#define SAVE_VIDEO_ERROR            "ERROR: Failed to create video! Please to retry."

#define REC_AFTER_IDX               700 // > (255 frame * 2) + (7 * 9)

#define MCAM_MIC_FILENAME           "/MCAMmicFile"
#define WAV_HEADER_SIZE             44
#ifdef __ANDROID__
#define BYTES_PER_SECOND            88200.f // = SampleRate * Channels * BitsPerSample / 8 = 44100 * 1 * 16 / 8
#define BYTES_PER_BLOC              2 // = Channels * BitsPerSample / 8 = 1 * 16 / 8
#else
#define BYTES_PER_SECOND            176400.f // = SampleRate * Channels * BitsPerSample / 8 = 44100 * 1 * 32 / 8
#define BYTES_PER_BLOC              4 // = Channels * BitsPerSample / 8 = 1 * 32 / 8
#endif

#define BULLET_TIME_LAG             (FREEZE_CAMERA_DURATION + 150) // Time lag between GO and bullet time (in milliseconds)

#define FILM_TEXTURE_ID             4 // Video texture ID
#define FILM_TEXTURE_IDX            8 // Video texture index (4 + 5 - 1)

#define SOUND_ID_FILM               (SOUND_ID_LOGO + 1)

//////
Recorder::Recorder(const std::string* folder) : mLandscape(true), mFolder(folder), mAbort(true), mThread(NULL) {

    LOGV(LOG_LEVEL_VIDEO, 0, LOG_FORMAT(" - f:%x (%s)"), __PRETTY_FUNCTION__, __LINE__, folder,
            (folder)? folder->c_str():"null");
#ifndef PAID_VERSION
    mLogo = NULL;
#endif
}
Recorder::~Recorder() {

    LOGV(LOG_LEVEL_VIDEO, 0, LOG_FORMAT(), __PRETTY_FUNCTION__, __LINE__);
    clear();
}

time_t Recorder::add(const unsigned char* rgba, bool before) {

    LOGV(LOG_LEVEL_VIDEO, 0, LOG_FORMAT(" - r:%x; b:%s (b:%d; a:%d)"), __PRETTY_FUNCTION__, __LINE__, rgba,
            (before)? "true":"false", static_cast<short>(mBefore.size()), static_cast<short>(mAfter.size()));
    assert(rgba);

    RecFrame* frame;
    try { frame = new RecFrame; }
    catch (std::bad_alloc e) {

        LOGE(LOG_FORMAT(" - Avoid heap memory corruption (lock/unlock screen operation)"), __PRETTY_FUNCTION__, __LINE__);
        return 0;
    }
    frame->elapsed = time(NULL);
    frame->status = STATUS_PROGRESS;
    frame->index = (before)? static_cast<short>(mBefore.size()):static_cast<short>(mAfter.size() + REC_AFTER_IDX);

    std::string fileName(*mFolder);
    fileName.append(MCAM_SUB_FOLDER);
    fileName.append(PIC_FILE_NAME);
    fileName.append(numToStr<short>(static_cast<short>(frame->index)));
    fileName.append(BIN_FILE_EXTENSION);

    // Save BIN file now!
    FILE* file = fopen(fileName.c_str(), "wb");
    if (!file) {

        LOGE(LOG_FORMAT(" - Failed to create file %s"), __PRETTY_FUNCTION__, __LINE__, fileName.c_str());
        assert(NULL);
        delete frame;
        return 0;
    }
    if (fwrite(rgba, sizeof(char), (CAM_HEIGHT * CAM_WIDTH * 4), file) != (CAM_HEIGHT * CAM_WIDTH * 4)) {

        LOGE(LOG_FORMAT(" - Failed to write %d bytes into file %s"), __PRETTY_FUNCTION__, __LINE__, (CAM_HEIGHT *
                CAM_WIDTH * 4), fileName.c_str());
        assert(NULL);
        fclose(file);
        delete frame;
        return 0;
    }
    fclose(file);

    mMutex.lock();
    (before)? mBefore.push_back(frame):mAfter.push_back(frame);
    mMutex.unlock();

    return ((before) && (mBefore.size() == (RECORD_DURATION_BEFORE * MAX_VIDEO_FPS))) ||
            ((!before) && (mAfter.size() == (RECORD_DURATION_AFTER * MAX_VIDEO_FPS)))? 0:frame->elapsed;
}
#ifndef PAID_VERSION
void Recorder::start(const unsigned char* logo, bool landscape) {

    LOGV(LOG_LEVEL_VIDEO, 0, LOG_FORMAT(" - l:%x; l:%s"), __PRETTY_FUNCTION__, __LINE__, logo, (landscape)? "true":"false");
    mLogo = logo;
#else
void Recorder::start(bool landscape) {

    LOGV(LOG_LEVEL_VIDEO, 0, LOG_FORMAT(" - l:%s"), __PRETTY_FUNCTION__, __LINE__, (landscape)? "true":"false");
#endif
    mLandscape = landscape;

    assert(!mThread);
    mAbort = false;
    mThread = new boost::thread(Recorder::startProcessThread, this);
}
void Recorder::clear() {

    LOGV(LOG_LEVEL_VIDEO, 0, LOG_FORMAT(), __PRETTY_FUNCTION__, __LINE__);
    if (mThread) {

        mAbort = true;
        mThread->join();
        delete mThread;

        mThread = NULL;
    }
    for (std::vector<RecFrame*>::iterator iter = mBefore.begin(); iter != mBefore.end(); ++iter)
        delete (*iter);
    mBefore.clear();
    for (std::vector<RecFrame*>::iterator iter = mAfter.begin(); iter != mAfter.end(); ++iter)
        delete (*iter);
    mAfter.clear();
}

void Recorder::processThreadRunning() {

    LOGV(LOG_LEVEL_VIDEO, 0, LOG_FORMAT(" - Begin"), __PRETTY_FUNCTION__, __LINE__);
    while (!mAbort) {

        boost::this_thread::sleep(boost::posix_time::milliseconds(20));
        mMutex.lock();
        unsigned char idx = 0;
        for ( ; idx < static_cast<unsigned char>(mBefore.size()); ++idx)
            if (!mBefore[idx]->status) // STATUS_PROGRESS
                break;

        RecFrame* frame = NULL;
        if (idx == static_cast<unsigned char>(mBefore.size())) {
            idx = 0;
            for ( ; idx < static_cast<unsigned char>(mAfter.size()); ++idx)
                if (!mAfter[idx]->status)
                    break;

            if (idx == static_cast<unsigned char>(mAfter.size())) {

                mMutex.unlock();
                continue; // Nothing to do
            }
            frame = mAfter[idx];
        }
        else
            frame = mBefore[idx];
        mMutex.unlock();

        LOGI(LOG_LEVEL_VIDEO, 0, LOG_FORMAT(" - Process file %d"), __PRETTY_FUNCTION__, __LINE__, frame->index);
        Picture picture(mFolder);
#ifndef PAID_VERSION
        frame->status = (picture.record(mLogo, mLandscape, frame->index))? STATUS_DONE:STATUS_ERROR;
#else
        frame->status = (picture.record(mLandscape, frame->index))? STATUS_DONE:STATUS_ERROR;
#endif
    }
    LOGI(LOG_LEVEL_VIDEO, 0, LOG_FORMAT(" - Finished"), __PRETTY_FUNCTION__, __LINE__);
}
void Recorder::startProcessThread(Recorder* recorder) {

    LOGV(LOG_LEVEL_VIDEO, 0, LOG_FORMAT(" - r:%x"), __PRETTY_FUNCTION__, __LINE__, recorder);
    recorder->processThreadRunning();
}

//////
Video::Video() : mPicIdx(0), mPicCount(0), mBuffer(NULL), mBufferLen(0), mAbort(true), mThread(NULL), mStatus(0),
        mRcvLen(0), mFilm(false), mPlaying(false), mLandscape(true), mFPS(0), mTexGen(false), mClientCount(0),
        mDelay(0) {

    LOGV(LOG_LEVEL_VIDEO, 0, LOG_FORMAT(), __PRETTY_FUNCTION__, __LINE__);
#ifdef __ANDROID__
    mPicFolder.assign(Storage::getFolder(FOLDER_TYPE_APPLICATION));
    mMovFolder.assign(Storage::getFolder(FOLDER_TYPE_MOVIES));
    mMovFolder.append(MCAM_SUB_FOLDER);
    if (!boost::filesystem::exists(mMovFolder))
        boost::filesystem::create_directory(mMovFolder.c_str());

    miOS = false;

    mBufferWEBM = NULL;
    mBufferMOV = NULL;
    mBufferLenWEBM = 0;
    mBufferLenMOV = 0;
#else
    mPicFolder.assign(Storage::getFolder(FOLDER_TYPE_DOCUMENTS));
    mMovFolder.assign(Storage::getFolder(FOLDER_TYPE_DOCUMENTS));

    mMovFolder.append(MOV_SUB_FOLDER);
    [[NSFileManager defaultManager] removeItemAtPath:[NSString stringWithUTF8String:mMovFolder.c_str()] error:nil];
    [[NSFileManager defaultManager] createDirectoryAtPath:[NSString stringWithUTF8String:mMovFolder.c_str()]
                                                        withIntermediateDirectories:NO attributes:nil error:nil];
#endif
    mRecorder = new Recorder(&mPicFolder);
    mTexBuffer = new char[static_cast<int>(CAM_TEX_WIDTH * CAM_TEX_HEIGHT) * 3];
    std::memset(mTexBuffer, 0, static_cast<size_t>(CAM_TEX_WIDTH * CAM_TEX_HEIGHT * 3));
}
Video::~Video() {

    LOGV(LOG_LEVEL_VIDEO, 0, LOG_FORMAT(), __PRETTY_FUNCTION__, __LINE__);
    clear();
    delete [] mTexBuffer;
    delete mRecorder;
}

void Video::initialize(const Game2D* game) {

    LOGV(LOG_LEVEL_VIDEO, 0, LOG_FORMAT(" - g:%x"), __PRETTY_FUNCTION__, __LINE__, game);
    mFilm.initialize(game);
    mFilm.start(FILM_TEXTURE_IDX);

    static const float texCoords[8] = { 0.f, 0.f, 0.f, CAM_HEIGHT / CAM_TEX_HEIGHT, CAM_WIDTH / CAM_TEX_WIDTH,
            CAM_HEIGHT / CAM_TEX_HEIGHT, CAM_WIDTH / CAM_TEX_WIDTH, 0.f };
    mFilm.setTexCoords(texCoords);

    short screenW = (game->getScreen()->width >> 1) * SCREEN_SCALE_RATIO; // Half
    short screenH = screenW * CAM_HEIGHT / CAM_WIDTH;
    if (screenH > (game->getScreen()->height >> 1)) {

        screenH = game->getScreen()->height >> 1;
        screenW = screenH * CAM_WIDTH / CAM_HEIGHT;
    }
    mFilm.setVertices((game->getScreen()->width >> 1) - screenW, (game->getScreen()->height >> 1) + screenH,
            (game->getScreen()->width >> 1) + screenW, (game->getScreen()->height >> 1) - screenH);
}
void Video::resume() {

    LOGV(LOG_LEVEL_VIDEO, 0, LOG_FORMAT(), __PRETTY_FUNCTION__, __LINE__);
    mTexGen = false;
    mFilm.resume(FILM_TEXTURE_IDX);
    mDelay = 0;

#ifndef __ANDROID__
    if (Player::getInstance()->getIndex(SOUND_ID_FILM) == SOUND_IDX_INVALID)
        return;
#endif
    std::string oggFile(mPicFolder);
    oggFile.append(MCAM_SUB_FOLDER);
    oggFile.append(MCAM_MIC_FILENAME);
    oggFile.append(OGG_FILE_EXTENSION);
#ifdef __ANDROID__
    if (boost::filesystem::exists(oggFile))
#else
    if ([[NSFileManager defaultManager] fileExistsAtPath:[NSString stringWithUTF8String:oggFile.c_str()]])
#endif
        loadOGG(oggFile);
}

void Video::add(Picture* picture, unsigned char client) {

    LOGV(LOG_LEVEL_VIDEO, 0, LOG_FORMAT(" - p:%x; c:%d"), __PRETTY_FUNCTION__, __LINE__, picture, client);
    assert(picture);

    picture->setFolder(&mPicFolder);

    Frame* frame = new Frame;
    frame->client = client;
    frame->picture = picture;
    mPictures.push_back(frame);
}

void Video::clear(bool remove) {

    LOGV(LOG_LEVEL_VIDEO, 0, LOG_FORMAT(" - r:%s (a:%s; t:%x)"), __PRETTY_FUNCTION__, __LINE__, (remove)? "true":"false",
            (mAbort)? "true":"false", mThread);
    if (mThread) {

        mAbort = true;
        mThread->join();
        delete mThread;
        mThread = NULL;
    }
    mFileName.clear();
    for (std::vector<Frame*>::iterator iter = mPictures.begin(); iter != mPictures.end(); ++iter) {
        delete (*iter)->picture;
        delete (*iter);
    }
    mPictures.clear();
    if (remove) {

        mRecorder->clear();
        Picture::removePath(&mPicFolder);
    }
#ifdef __ANDROID__
    if ((mBuffer) && (mBuffer != mBufferWEBM) && (mBuffer != mBufferMOV))
        delete [] mBuffer;

    if (mBufferWEBM) {
        delete [] mBufferWEBM;
        mBufferWEBM = NULL;
    }
    if (mBufferMOV) {
        delete [] mBufferMOV;
        mBufferMOV = NULL;
    }
    mBuffer = NULL;
#else
    if (mBuffer) {
        delete [] mBuffer;
        mBuffer = NULL;
    }
#endif
}
#ifdef __ANDROID__
void Video::purge() {

    LOGV(LOG_LEVEL_VIDEO, 0, LOG_FORMAT(" - (f:%s)"), __PRETTY_FUNCTION__, __LINE__, mFileName.c_str());
    assert(mFileName.length());
    if (mFileName.at(mFileName.size() - 5) != '.') // '.mov' file?
        return; // Not a WebM video file just a MOV file to be kept!

    std::string fileName(mMovFolder);
    fileName.append(mFileName);
    fileName.resize(fileName.size() - sizeof(WEBM_FILE_EXTENSION) + 1);
    fileName.append(MOV_FILE_EXTENSION); // '.mov'
    if (boost::filesystem::exists(fileName)) {

        LOGI(LOG_LEVEL_VIDEO, 0, LOG_FORMAT(" - Video '%s' file will be removed"), __PRETTY_FUNCTION__, __LINE__,
             fileName.c_str());
        boost::filesystem::remove(fileName);
    }
}
void Video::free(bool server) {

    LOGV(LOG_LEVEL_VIDEO, 0, LOG_FORMAT(" - s:%s"), __PRETTY_FUNCTION__, __LINE__, (server)? "true":"false");
#else
void Video::free() {

    LOGV(LOG_LEVEL_VIDEO, 0, LOG_FORMAT(), __PRETTY_FUNCTION__, __LINE__);
#endif
    assert(Textures::getInstance()->getIndex(FILM_TEXTURE_ID) == FILM_TEXTURE_IDX);
    Textures* textures = Textures::getInstance();
    textures->delTexture(FILM_TEXTURE_IDX);
    textures->rmvTextures(1);

    mTexGen = false;
    mPlaying = false;
    mDelay = 0;

    Player* player = Player::getInstance();
    unsigned char track = player->getIndex(SOUND_ID_FILM);
    if (track != SOUND_IDX_INVALID) {
        if (player->getStatus(track) == AL_PLAYING)
            player->stop(track);

        player->rmvSound(track);
    }
#ifdef __ANDROID__
    if (server)
        purge(); // Delete MOV file (if any)
#endif
    clear();
}

bool Video::generate() {

    LOGV(LOG_LEVEL_VIDEO, 0, LOG_FORMAT(" - (i:%d)"), __PRETTY_FUNCTION__, __LINE__, mPicIdx);
    Textures* textures = Textures::getInstance();
    if (textures->getIndex(FILM_TEXTURE_ID) != TEXTURE_IDX_INVALID) {

        textures->delTexture(FILM_TEXTURE_IDX);
        textures->rmvTextures(1);
    }
    std::string binFile(mPicFolder);
    binFile.append(MCAM_SUB_FOLDER);
    binFile.append(PIC_FILE_NAME);
    binFile.append(numToStr<short>(mPicIdx));
    binFile.append(BIN_FILE_EXTENSION);

    std::ifstream ifs(binFile.c_str(), std::ifstream::binary);
    if (!ifs.is_open()) {

        LOGE(LOG_FORMAT(" - Failed to open file %s"), __PRETTY_FUNCTION__, __LINE__, binFile.c_str());
        assert(NULL);
        return false;
    }
    ifs.rdbuf()->sgetn(mTexBuffer, static_cast<size_t>(CAM_TEX_WIDTH * CAM_TEX_HEIGHT * 3));
    ifs.close();

    textures->addTexture(FILM_TEXTURE_ID, CAM_TEX_WIDTH, CAM_TEX_HEIGHT,
                         const_cast<unsigned char*>(reinterpret_cast<const unsigned char*>(mTexBuffer)), false);
    textures->genTexture(FILM_TEXTURE_IDX, false, true); // RGB texture buffer

    mTexGen = true;
    return true;
}

void Video::mergeWAV() {

    LOGV(LOG_LEVEL_VIDEO, 0, LOG_FORMAT(" - (cli:%d; fps:%d)"), __PRETTY_FUNCTION__, __LINE__, mClientCount, mFPS);
    std::string fileName(mPicFolder);
    fileName.append(MCAM_SUB_FOLDER);
    fileName.append(RECORD_MIC_FILENAME);
    fileName.append(WAV_FILE_EXTENSION);

    std::ifstream wavFile(fileName.c_str(), std::ifstream::binary);
    wavFile.seekg(0, std::ifstream::end);
    int fileSize = static_cast<int>(wavFile.tellg());
    wavFile.seekg(0, std::ifstream::beg);

    int start = static_cast<int>(mRecorder->getDoneCount(true) * BYTES_PER_SECOND / mFPS) +
            static_cast<int>((BULLET_TIME_LAG / 1000.f) * BYTES_PER_SECOND) + WAV_HEADER_SIZE;
    while (start % BYTES_PER_BLOC) --start; // Must be in bloc byte count

    int duration = static_cast<int>(((mClientCount + 2) * MCAM_FPS_FACTOR(mFPS)) * BYTES_PER_SECOND / mFPS);
    while (duration % BYTES_PER_BLOC) --duration; // ...

    LOGI(LOG_LEVEL_VIDEO, 0, LOG_FORMAT(" - Start:%d Duration:%d (size:%d)"), __PRETTY_FUNCTION__, __LINE__, start,
            duration, fileSize);

    fileName.assign(mPicFolder);
    fileName.append(MCAM_SUB_FOLDER);
    fileName.append(MCAM_MIC_FILENAME);
    fileName.append(WAV_FILE_EXTENSION);

    std::ofstream resFile(fileName.c_str(), std::ifstream::binary);
    for (int i = 0; i < fileSize; ++i) {

        if (i == start) { // Bullet time!

            LOGI(LOG_LEVEL_VIDEO, 0, LOG_FORMAT(" - Add bullet time silent"), __PRETTY_FUNCTION__, __LINE__);
            for (int j = 0; j < duration; ++j)
                resFile << 0x00;
            // -> Silent during bullet time has been added

            continue;
        }
        char readChar;
        wavFile.get(readChar);
        resFile << readChar;
    }
    wavFile.close();

    fileSize = static_cast<int>(resFile.tellp()) - 8;
    resFile.seekp(4, std::ifstream::beg);

#ifdef DEBUG
    enum {

        O32_LITTLE_ENDIAN = 0x03020100ul,
        O32_BIG_ENDIAN = 0x00010203ul,
        O32_PDP_ENDIAN = 0x01000302ul
    };
    static const union {

        unsigned char bytes[4];
        uint32_t value;

    } o32_host_order = { { 0, 1, 2, 3 } };

    LOGI(LOG_LEVEL_VIDEO, 0, LOG_FORMAT(" - Check little endian"), __PRETTY_FUNCTION__, __LINE__);
    assert(o32_host_order.value == O32_LITTLE_ENDIAN); // Must be little endian device
#endif
    resFile << static_cast<char>(fileSize & 0xff);
    resFile << static_cast<char>((fileSize >> 8) & 0xff);
    resFile << static_cast<char>((fileSize >> 16) & 0xff);
    resFile << static_cast<char>((fileSize >> 24) & 0xff);
    // -> File size -8 in WAV header updated

    fileSize += 8; // File size
    fileSize -= WAV_HEADER_SIZE;

    fileSize -= 12; // ???

    resFile.seekp(WAV_HEADER_SIZE - 4, std::ifstream::beg); // -4 -> File size - header size = data size (on 4 bytes)

    resFile << static_cast<char>(fileSize & 0xff);
    resFile << static_cast<char>((fileSize >> 8) & 0xff);
    resFile << static_cast<char>((fileSize >> 16) & 0xff);
    resFile << static_cast<char>((fileSize >> 24) & 0xff);
    // -> Data size in WAV header updated

    resFile.close();
}
unsigned char Video::loadOGG(const std::string &file) {

    LOGV(LOG_LEVEL_VIDEO, 0, LOG_FORMAT(" - f:%s"), __PRETTY_FUNCTION__, __LINE__, file.c_str());
#ifdef __ANDROID__
    std::ifstream ogg(file.c_str(), std::ifstream::binary);
    std::filebuf* pbuf = ogg.rdbuf();
    int length = static_cast<int>(pbuf->pubseekoff(0, ogg.end, ogg.in));
    pbuf->pubseekpos(0, ogg.in);

    LOGI(LOG_LEVEL_VIDEO, 0, LOG_FORMAT(" - OGG file size: %d"), __PRETTY_FUNCTION__, __LINE__, length);
    assert(length > 0);
    char* data = new char[length];
    pbuf->sgetn(data, length);
    ogg.close();

    return Player::getInstance()->addSound(SOUND_ID_FILM, length, reinterpret_cast<unsigned char*>(data), true);

#else
    NSData* oggData = [[NSData alloc] initWithContentsOfFile:[NSString stringWithUTF8String:file.c_str()]];
    unsigned char* data = new unsigned char[oggData.length];
    memcpy(data, (const unsigned char*)[oggData bytes], oggData.length);

    LOGI(LOG_LEVEL_VIDEO, 0, LOG_FORMAT(" - OGG file size: %d"), __PRETTY_FUNCTION__, __LINE__, oggData.length);
    unsigned char track = Player::getInstance()->addSound(SOUND_ID_FILM, oggData.length, data, true);

    [oggData release];
    return track;
#endif
}

void Video::prepare(int size, unsigned char fps) {

    LOGV(LOG_LEVEL_VIDEO, 0, LOG_FORMAT(" - s:%d; f:%d (b:%x)"), __PRETTY_FUNCTION__, __LINE__, size, fps, mBuffer);
    assert(size > 0);
    assert(!mBuffer);

    mFPS = fps;

    mBufferLen = size;
    mBuffer = new char[size];
    mRcvLen = 0;
}
signed char Video::fill(const ClientMgr* mgr) {

    LOGV(LOG_LEVEL_PICTURE, 0, LOG_FORMAT(" - m:%x; s:%d"), __PRETTY_FUNCTION__, __LINE__, mgr);
    LOGI(LOG_LEVEL_PICTURE, 0, LOG_FORMAT(" - Receive length: %d (buffer:%x; size:%d)"), __PRETTY_FUNCTION__, __LINE__,
            mgr->getRcvLength() - CHECKSUM_LEN - SECURITY_LEN, mBuffer, mBufferLen);
    assert(mBuffer);

    unsigned short len = mgr->getRcvLength() - CHECKSUM_LEN - SECURITY_LEN;
#ifdef DEBUG
    if (len < 1) {
        LOGE(LOG_FORMAT(" - Wrong length to fill: %d"), __PRETTY_FUNCTION__, __LINE__, len);
        assert(NULL);
    }
#endif
    //if ((!Connexion::verifyCheckSum(mgr)) || (!Connexion::verifySecurity(mgr))) { // No security check here
    if (!Connexion::verifyCheckSum(mgr)) {                                          // ->  On client side only

        LOGE(LOG_FORMAT(" - Checksum/Security error"), __PRETTY_FUNCTION__, __LINE__);
        return LIBENG_NO_DATA; // < 0
    }
    memcpy(mBuffer + mRcvLen, mgr->getRcvBuffer(), len);

    int prevRcvLen = mRcvLen; // Needed for next try (in error case)
    mRcvLen += len;
    if ((mRcvLen > mBufferLen) || (mRcvLen < 0)) {

        LOGE(LOG_FORMAT(" - Wrong %d received buffer size"), __PRETTY_FUNCTION__, __LINE__, mRcvLen);
        mRcvLen = prevRcvLen; // Back to previous receive length (next try)
        return LIBENG_NO_DATA;
    }
    return (mRcvLen == mBufferLen)? 0:1;
}
#ifdef __ANDROID__
bool Video::store(bool landscape, bool android) {

    LOGV(LOG_LEVEL_VIDEO, 0, LOG_FORMAT(" - l:%s; a:%s (b:%x; r:%d; s:%d)"), __PRETTY_FUNCTION__, __LINE__,
            (landscape)? "true":"false", (android)? "true":"false", mBuffer, mRcvLen, mBufferLen);
    miOS = android;
#else
bool Video::store(bool landscape) {

    LOGV(LOG_LEVEL_VIDEO, 0, LOG_FORMAT(" - l:%s (b:%x; r:%d; s:%d)"), __PRETTY_FUNCTION__, __LINE__,
            (landscape)? "true":"false", mBuffer, mRcvLen, mBufferLen);
#endif
    assert(mBuffer);
    assert(mRcvLen == mBufferLen);

    mLandscape = landscape;
    mFileName.assign(VIDEO_FILENAME);

    time_t curDate = time(NULL);
    struct tm* now = localtime(&curDate);
    mFileName.append(numToStr<short>(now->tm_year + 1900));
    if (now->tm_mon < 9)
        mFileName += '0';
    mFileName.append(numToStr<short>(now->tm_mon + 1));
    if (now->tm_mday < 10)
        mFileName += '0';
    mFileName.append(numToStr<short>(now->tm_mday));
    mFileName += '_';
    if (now->tm_hour < 10)
        mFileName += '0';
    mFileName.append(numToStr<short>(now->tm_hour));
    if (now->tm_min < 10)
        mFileName += '0';
    mFileName.append(numToStr<short>(now->tm_min));
    if (now->tm_sec < 10)
        mFileName += '0';
    mFileName.append(numToStr<short>(now->tm_sec));
#ifdef __ANDROID__
    mFileName.append((android)? WEBM_FILE_EXTENSION:MOV_FILE_EXTENSION);
#else
    mFileName.append(MOV_FILE_EXTENSION);
#endif

    std::string fileName(mMovFolder);
    fileName.append(mFileName);

    LOGI(LOG_LEVEL_VIDEO, 0, LOG_FORMAT(" - Save %s video file"), __PRETTY_FUNCTION__, __LINE__, fileName.c_str());
    FILE* file = fopen(fileName.c_str(), "wb");
    if (!file) {

        LOGE(LOG_FORMAT(" - Failed to create file %s"), __PRETTY_FUNCTION__, __LINE__, fileName.c_str());
        assert(NULL);
#ifdef __ANDROID__
        alertMessage(LOG_LEVEL_VIDEO, SAVE_VIDEO_ERROR);
#else
        alertMessage(LOG_LEVEL_VIDEO, 2.5, SAVE_VIDEO_ERROR);
#endif
        return false;
    }
    if (fwrite(mBuffer, sizeof(char), mBufferLen, file) != mBufferLen) {

        fclose(file);
        LOGE(LOG_FORMAT(" - Failed to write %d bytes into file %s"), __PRETTY_FUNCTION__, __LINE__, mBufferLen,
                fileName.c_str());
        assert(NULL);
#ifdef __ANDROID__
        alertMessage(LOG_LEVEL_VIDEO, SAVE_VIDEO_ERROR);
#else
        alertMessage(LOG_LEVEL_VIDEO, 2.5, SAVE_VIDEO_ERROR);
#endif
        return false;
    }
    fclose(file);

    start(PROC_STORE);
    return true;
}

bool Video::save(const FrameList* clients, bool landscape) {

    LOGV(LOG_LEVEL_VIDEO, 0, LOG_FORMAT(" - c:%x; l:%s (b:%d; a:%d)"), __PRETTY_FUNCTION__, __LINE__, clients,
            (landscape)? "true":"false", static_cast<short>(mRecorder->mBefore.size()),
            static_cast<short>(mRecorder->mAfter.size()));
    mLandscape = landscape;
    mFPS = mRecorder->getFPS();

    //
    LOGI(LOG_LEVEL_VIDEO, 0, LOG_FORMAT(" - Rename JPEG files B4 bullet time effect"), __PRETTY_FUNCTION__, __LINE__);
    std::string prevFile(Picture::getFileName(&mPicFolder, JPEG_FILE_EXTENSION));
    std::string newFile(prevFile);

    newFile.resize(newFile.size() - 7); // '000.jpg' contains 7 characters
    size_t fileSize = newFile.size(); // File size without frame index

    // img_0.jpg -> img_0.jpg
    // img_2.jpg -> img_1.jpg
    // img_4.jpg -> img_2.jpg
    mPicCount = 0;
    for (unsigned char i = 0; i < static_cast<unsigned char>(mRecorder->mBefore.size()); ++i) {
        if (mRecorder->mBefore[i]->status != Recorder::STATUS_DONE)
            continue;

        if (mRecorder->mBefore[i]->index == mPicCount) {
            ++mPicCount;
            continue;
        }
        prevFile.resize(fileSize); // '../img_'
        newFile.assign(prevFile); // ...
        prevFile.append(numToStr<short>(mRecorder->mBefore[i]->index));
        prevFile.append(JPEG_FILE_EXTENSION);
        newFile.append(numToStr<short>(mPicCount++));
        newFile.append(JPEG_FILE_EXTENSION);

        if (rename(prevFile.c_str(), newFile.c_str())) { // Error

            LOGE(LOG_FORMAT(" - Failed to rename file %s to %s"), __PRETTY_FUNCTION__, __LINE__, prevFile.c_str(),
                    newFile.c_str());
            clear();
            assert(NULL);
#ifdef __ANDROID__
            alertMessage(LOG_LEVEL_VIDEO, SAVE_VIDEO_ERROR);
#else
            alertMessage(LOG_LEVEL_VIDEO, 2.5, SAVE_VIDEO_ERROR);
#endif
            return false;
        }
    }
    if (!mPicCount) {

        LOGE(LOG_FORMAT(" - No B4 frame count"), __PRETTY_FUNCTION__, __LINE__);
        clear();
        assert(NULL);
#ifdef __ANDROID__
        alertMessage(LOG_LEVEL_VIDEO, SAVE_VIDEO_ERROR);
#else
        alertMessage(LOG_LEVEL_VIDEO, 2.5, SAVE_VIDEO_ERROR);
#endif
        return false;
    }

    //
    LOGI(LOG_LEVEL_VIDEO, 0, LOG_FORMAT(" - Add lag between GO and bullet time (fps:%d)"), __PRETTY_FUNCTION__, __LINE__,
            mFPS);
    unsigned char lag = static_cast<unsigned char>((BULLET_TIME_LAG / 1000.f) * mFPS) + 1;

    // img_700.jpg -> img_109.jpg
    // img_701.jpg -> img_110.jpg
    // img_703.jpg -> img_111.jpg

    for (unsigned char i = 0; i < static_cast<unsigned char>(mRecorder->mAfter.size()); ++i) {
        if (mRecorder->mAfter[i]->status != Recorder::STATUS_DONE)
            continue;

        if (i == lag)
            break;

        prevFile.resize(fileSize); // '../img_'
        newFile.assign(prevFile); // ...
        prevFile.append(numToStr<short>(mRecorder->mAfter[i]->index));
        prevFile.append(JPEG_FILE_EXTENSION);
        newFile.append(numToStr<short>(mPicCount++));
        newFile.append(JPEG_FILE_EXTENSION);

        if (rename(prevFile.c_str(), newFile.c_str())) { // Error

            LOGE(LOG_FORMAT(" - Failed to rename file %s to %s"), __PRETTY_FUNCTION__, __LINE__, prevFile.c_str(),
                    newFile.c_str());
            clear();
            assert(NULL);
#ifdef __ANDROID__
            alertMessage(LOG_LEVEL_VIDEO, SAVE_VIDEO_ERROR);
#else
            alertMessage(LOG_LEVEL_VIDEO, 2.5, SAVE_VIDEO_ERROR);
#endif
            return false;
        }
    }

    --mPicCount; // Current frame index
    for (unsigned char i = 1; i < MCAM_FPS_FACTOR(mFPS); ++i) { // Repeat server frame

        copyFile(&prevFile, &newFile, fileSize, mPicCount, mPicCount + 1);
        ++mPicCount;
    }
    ++mPicCount; // Next frame index

    //
    LOGI(LOG_LEVEL_VIDEO, 0, LOG_FORMAT(" - Organize JPEG files"), __PRETTY_FUNCTION__, __LINE__);
    // img_001.jpg -> img_98.jpg + img_99.jpg + img_100.jpg (x3)
    // img_003.jpg -> img_101.jpg + img_102.jpg + img_103.jpg (x3)
    // img_004.jpg -> img_104.jpg + img_105.jpg + img_106.jpg (x3)

#ifdef __ANDROID__
    miOS = false;
#endif
    mClientCount = 0;
    for (unsigned char i = 0; i < static_cast<unsigned char>(clients->size()); ++i) { // ...common direction (x3)
#ifdef __ANDROID__
        if (!(*clients)[i]->done)
            continue;

        if (!(*clients)[i]->android)
            miOS = true;
#else
        if (!(*clients)[i])
            continue;
#endif
        LOGI(LOG_LEVEL_VIDEO, 0, LOG_FORMAT(), __PRETTY_FUNCTION__, __LINE__);
        assert(get(i));
        assert(get(i)->isDone());

        prevFile.resize(fileSize); // '../img_'
        newFile.assign(prevFile); // ...
        switch (DIGIT_COUNT(i + 1)) {
            case 1: prevFile.append("00"); break;
            case 2: prevFile += '0'; break;
        }
        prevFile.append(numToStr<short>(static_cast<short>(i + 1)));
        prevFile.append(JPEG_FILE_EXTENSION);
        newFile.append(numToStr<short>(mPicCount));
        newFile.append(JPEG_FILE_EXTENSION);

        if (rename(prevFile.c_str(), newFile.c_str())) { // Error

            LOGE(LOG_FORMAT(" - Failed to rename file %s to %s"), __PRETTY_FUNCTION__, __LINE__, prevFile.c_str(),
                    newFile.c_str());
            clear();
            assert(NULL);
#ifdef __ANDROID__
            alertMessage(LOG_LEVEL_VIDEO, SAVE_VIDEO_ERROR);
#else
            alertMessage(LOG_LEVEL_VIDEO, 2.5, SAVE_VIDEO_ERROR);
#endif
            return false;
        }
        for (unsigned char j = 1; j < MCAM_FPS_FACTOR(mFPS); ++j) { // Repeat bullet time frame(s)

            copyFile(&prevFile, &newFile, fileSize, mPicCount, mPicCount + 1);
            ++mPicCount;
        }
        ++mClientCount;
    }
    clear(false);

    LOGI(LOG_LEVEL_VIDEO, 0, LOG_FORMAT(" - Add JPEG files for back direction (cnt:%d)"), __PRETTY_FUNCTION__, __LINE__,
            mClientCount);
    // img_97.jpg, img_98.jpg, img_99.jpg, img_100.jpg, img_101.jpg, img_102.jpg
    // Add files (x3):
    // img_103.jpg, img_104.jpg, img_105.jpg, img_106.jpg, img_107.jpg, img_108.jpg

    short backCount = mPicCount;
    short bulletCnt = mClientCount;
    while (bulletCnt > LIBENG_NO_DATA) {

        for (unsigned char i = 0; i < MCAM_FPS_FACTOR(mFPS); ++i) // Repeat server frame
            copyFile(&prevFile, &newFile, fileSize, backCount--, ++mPicCount);

        --bulletCnt;
    }

    //
    LOGI(LOG_LEVEL_VIDEO, 0, LOG_FORMAT(" - Rename JPEG files after bullet time effect"), __PRETTY_FUNCTION__, __LINE__);
    // img_700.jpg -> img_109.jpg
    // img_701.jpg -> img_110.jpg
    // img_703.jpg -> img_111.jpg

    for (unsigned char i = 0; i < static_cast<unsigned char>(mRecorder->mAfter.size()); ++i) {
        if ((mRecorder->mAfter[i]->status != Recorder::STATUS_DONE) || (i < lag))
            continue;

        prevFile.resize(fileSize); // '../img_'
        newFile.assign(prevFile); // ...
        prevFile.append(numToStr<short>(mRecorder->mAfter[i]->index));
        prevFile.append(JPEG_FILE_EXTENSION);
        newFile.append(numToStr<short>(++mPicCount)); // Next frame index
        newFile.append(JPEG_FILE_EXTENSION);

        if (rename(prevFile.c_str(), newFile.c_str())) { // Error

            LOGE(LOG_FORMAT(" - Failed to rename file %s to %s"), __PRETTY_FUNCTION__, __LINE__, prevFile.c_str(),
                    newFile.c_str());
            clear();
            assert(NULL);
#ifdef __ANDROID__
            alertMessage(LOG_LEVEL_VIDEO, SAVE_VIDEO_ERROR);
#else
            alertMessage(LOG_LEVEL_VIDEO, 2.5, SAVE_VIDEO_ERROR);
#endif
            return false;
        }
    }

    start(PROC_SAVE);
    return true;
}
void Video::extract() {

    LOGV(LOG_LEVEL_VIDEO, 0, LOG_FORMAT(" - (t:%x)"), __PRETTY_FUNCTION__, __LINE__, mThread);
    assert(mThread);

    mAbort = true;
    mThread->join();
    delete mThread;
    mThread = NULL;

    start(PROC_EXTRACT);
}

#ifdef __ANDROID__
void Video::select(bool android) {

    LOGV(LOG_LEVEL_VIDEO, 0, LOG_FORMAT(" - a:%s"), __PRETTY_FUNCTION__, __LINE__, (android)? "true":"false");
    mBuffer = (android)? mBufferWEBM:mBufferMOV;
    mBufferLen = (android)? mBufferLenWEBM:mBufferLenMOV;
}
#endif
bool Video::open() {

    LOGV(LOG_LEVEL_VIDEO, 0, LOG_FORMAT(" - (m:%s; f:%s)"), __PRETTY_FUNCTION__, __LINE__, mMovFolder.c_str(),
            mFileName.c_str());
    assert(mMovFolder.length());
    assert(mFileName.length());
    assert(!mBuffer);

    std::string fileName(mMovFolder);
    fileName.append(mFileName);
    std::ifstream ifs(fileName.c_str(), std::ifstream::binary);
    if (!ifs.is_open()) {

        LOGE(LOG_FORMAT(" - Failed to open file %s"), __PRETTY_FUNCTION__, __LINE__, fileName.c_str());
        assert(NULL);
        return false;
    }
    std::filebuf* pbuf = ifs.rdbuf();
#ifdef __ANDROID__
    LOGI(LOG_LEVEL_VIDEO, 0, LOG_FORMAT(), __PRETTY_FUNCTION__, __LINE__);
    assert(!mBufferWEBM);

    mBufferLenWEBM = static_cast<int>(pbuf->pubseekoff(0, ifs.end, ifs.in));
    if (mBufferLenWEBM < 1) {
#else
    mBufferLen = static_cast<int>(pbuf->pubseekoff(0, ifs.end, ifs.in));
    if (mBufferLen < 1) {
#endif
        ifs.close();
        LOGE(LOG_FORMAT(" - Wrong %s file size (%d)"), __PRETTY_FUNCTION__, __LINE__, fileName.c_str(), mBufferLen);
        assert(NULL);
        return false;
    }
    pbuf->pubseekpos(0, ifs.in);

#ifdef __ANDROID__
    mBufferWEBM = new char[mBufferLenWEBM];
    pbuf->sgetn(mBufferWEBM, mBufferLenWEBM);
#else
    mBuffer = new char[mBufferLen];
    pbuf->sgetn(mBuffer, mBufferLen);
#endif
    ifs.close();

#ifdef __ANDROID__
    LOGI(LOG_LEVEL_VIDEO, 0, LOG_FORMAT(), __PRETTY_FUNCTION__, __LINE__);
    assert(!mBufferMOV);

    fileName.resize(fileName.size() - sizeof(WEBM_FILE_EXTENSION) + 1);
    fileName.append(MOV_FILE_EXTENSION);

    ifs.open(fileName.c_str(), std::ifstream::binary);
    if (!ifs.is_open()) {

        LOGW(LOG_FORMAT(" - Failed to open file %s"), __PRETTY_FUNCTION__, __LINE__, fileName.c_str());
        //assert(NULL); // MOV file not exists (possible case)
        return true;
    }
    pbuf = ifs.rdbuf();

    mBufferLenMOV = static_cast<int>(pbuf->pubseekoff(0, ifs.end, ifs.in));
    if (mBufferLenMOV < 1) {

        ifs.close();
        LOGW(LOG_FORMAT(" - Wrong %s file size (%d)"), __PRETTY_FUNCTION__, __LINE__, fileName.c_str(), mBufferLen);
        //assert(NULL); // ...
        mBufferLenMOV = 0;
        return true;
    }
    pbuf->pubseekpos(0, ifs.in);

    mBufferMOV = new char[mBufferLenMOV];
    pbuf->sgetn(mBufferMOV, mBufferLenMOV);
    ifs.close();

#endif
    return true;
}

void Video::start(unsigned char process) {

    LOGV(LOG_LEVEL_VIDEO, 0, LOG_FORMAT(" - p:%d (a:%s; t:%x)"), __PRETTY_FUNCTION__, __LINE__, process,
            (mAbort)? "true":"false", mThread);
    assert(mAbort);
    assert(!mThread);

    mStatus = 0; // In progress

    mAbort = false;
    mThread = new boost::thread(Video::startProcessThread, this, process);
}
void Video::processThreadRunning(unsigned char proc) {

    LOGV(LOG_LEVEL_VIDEO, 0, LOG_FORMAT(" - Begin: %d (file:%s; cnt:%d; fps:%d)"), __PRETTY_FUNCTION__, __LINE__, proc,
            mFileName.c_str(), mPicCount, mFPS);

    switch (proc) {
        case PROC_SAVE: { // Save (create & save video) - Server

            //assert(mPicCount < ((MAX_VIDEO_FPS * (RECORD_DURATION_BEFORE + RECORD_DURATION_AFTER)) + (255 * 3))); // x2 x3
            //assert(mPicCount > (3 * 3)); // x2 x3

#ifdef __ANDROID__
            LOGI(LOG_LEVEL_VIDEO, 0, LOG_FORMAT(" - Convert 3GP into WAV (fps:%d)"), __PRETTY_FUNCTION__, __LINE__, mFPS);
            std::string fileName(mPicFolder);
            fileName.append(MCAM_SUB_FOLDER);
            fileName.append(RECORD_MIC_FILENAME);
            fileName.append(GP3_FILE_EXTENSION);

            bool sound = boost::filesystem::exists(fileName); // Existing 3GP sound

            std::string mfsrc("filesrc location=");
            if (sound) {

                mfsrc.append(fileName);
                mfsrc.append(" ! qtdemux ! decodebin ! audioconvert ! audio/x-raw,rate=8000,channels=1 ! audioresample"
                             " ! audio/x-raw,rate=44100 ! wavenc ! filesink location=");
                fileName.resize(fileName.size() - sizeof(GP3_FILE_EXTENSION) + 1);
                fileName.append(WAV_FILE_EXTENSION);
                mfsrc.append(fileName);

                sound = Picture::gstLaunch(mfsrc);
                if (sound) {

                    //
                    if (aborted(__PRETTY_FUNCTION__, __LINE__, proc)) break;
                    mergeWAV();

                    //
                    LOGI(LOG_LEVEL_VIDEO, 0, LOG_FORMAT(" - Create OGG from WAV"), __PRETTY_FUNCTION__, __LINE__);
                    mfsrc.assign("filesrc location=");
                    fileName.assign(mPicFolder);
                    fileName.append(MCAM_SUB_FOLDER);
                    fileName.append(MCAM_MIC_FILENAME);
                    fileName.append(WAV_FILE_EXTENSION);
                    mfsrc.append(fileName);
                    mfsrc.append(" ! wavparse ! audioconvert ! vorbisenc ! oggmux ! filesink location=");
                    fileName.resize(fileName.size() - sizeof(WAV_FILE_EXTENSION) + 1);
                    fileName.append(OGG_FILE_EXTENSION);
                    mfsrc.append(fileName);

                    if ((!Picture::gstLaunch(mfsrc)) && (boost::filesystem::exists(fileName)))
                        boost::filesystem::remove(fileName); // Remove wrong OGG file
                }
            }
            mRecorder->clear();

            //
            if (aborted(__PRETTY_FUNCTION__, __LINE__, proc))
                break;
            LOGI(LOG_LEVEL_VIDEO, 0, LOG_FORMAT(" - Convert all JPEG into WebM (fps:%d)"), __PRETTY_FUNCTION__, __LINE__, mFPS);
            mFileName.assign(VIDEO_FILENAME);

            time_t curDate = time(NULL);
            struct tm* now = localtime(&curDate);
            mFileName.append(numToStr<short>(now->tm_year + 1900));
            if (now->tm_mon < 9)
                mFileName += '0';
            mFileName.append(numToStr<short>(now->tm_mon + 1));
            if (now->tm_mday < 10)
                mFileName += '0';
            mFileName.append(numToStr<short>(now->tm_mday));
            mFileName += '_';
            if (now->tm_hour < 10)
                mFileName += '0';
            mFileName.append(numToStr<short>(now->tm_hour));
            if (now->tm_min < 10)
                mFileName += '0';
            mFileName.append(numToStr<short>(now->tm_min));
            if (now->tm_sec < 10)
                mFileName += '0';
            mFileName.append(numToStr<short>(now->tm_sec));
            mFileName.append(WEBM_FILE_EXTENSION);

            fileName.assign(Picture::getFileName(&mPicFolder, JPEG_FILE_EXTENSION));
            fileName.resize(fileName.size() - 7); // '000.jpg' contains 7 characters
            if (!sound) {

                LOGI(LOG_LEVEL_VIDEO, 0, LOG_FORMAT(" - Without sound"), __PRETTY_FUNCTION__, __LINE__);
                mfsrc.assign("multifilesrc location=");
                mfsrc.append(fileName); // '../img_'
                mfsrc.append("%d.jpg index=0 caps=\"image/jpeg,framerate=");
                mfsrc.append(numToStr<short>(static_cast<short>(mFPS)));
                mfsrc.append("/1\" ! jpegdec ! videoconvert ! vp8enc ! webmmux ! filesink location=");
                mfsrc.append(mMovFolder);
                mfsrc.append(mFileName);
            }
            else {

                LOGI(LOG_LEVEL_VIDEO, 0, LOG_FORMAT(" - With sound"), __PRETTY_FUNCTION__, __LINE__);
                mfsrc.assign("webmmux name=mux ! filesink location=");
                mfsrc.append(mMovFolder);
                mfsrc.append(mFileName);
                mfsrc.append(" multifilesrc location=");
                mfsrc.append(fileName); // '../img_'
                mfsrc.append("%d.jpg index=0 caps=\"image/jpeg,framerate=");
                mfsrc.append(numToStr<short>(static_cast<short>(mFPS)));
                mfsrc.append("/1\" ! jpegdec ! videoconvert ! vp8enc ! queue ! mux.video_0 filesrc location=");
                fileName.assign(mPicFolder);
                fileName.append(MCAM_SUB_FOLDER);
                fileName.append(MCAM_MIC_FILENAME);
                fileName.append(WAV_FILE_EXTENSION);
                mfsrc.append(fileName);
                mfsrc.append(" ! wavparse ! audioconvert ! vorbisenc ! queue ! mux.audio_0");
            }
            if (!Picture::gstLaunch(mfsrc)) {

                // Delete wrong WebM file (if any)
                fileName.assign(mMovFolder);
                fileName.append(mFileName);
                if (boost::filesystem::exists(fileName))
                    boost::filesystem::remove(fileName);

                alertMessage(LOG_LEVEL_VIDEO, SAVE_VIDEO_ERROR);
                mStatus = LIBENG_NO_DATA; // Error
                mAbort = true;
                break;
            }
            alertMessage(LOG_LEVEL_VIDEO, STORE_MEDIA_SUCCEEDED);

            //
            if (aborted(__PRETTY_FUNCTION__, __LINE__, proc))
                break;
            LOGI(LOG_LEVEL_VIDEO, 0, LOG_FORMAT(" - Add WebM video into media album"), __PRETTY_FUNCTION__, __LINE__);
            fileName.assign(mMovFolder);
            fileName.append(mFileName);
            std::string videoTitle(VIDEO_TITLE);
            videoTitle.append(Share::extractDate(mFileName));
            Storage::getInstance()->saveMedia(fileName, WEBM_MIME_TYPE, videoTitle);

            // Check if needed to create MOV video file (existing iOS client)
            if (miOS) {

                //
                if (aborted(__PRETTY_FUNCTION__, __LINE__, proc))
                    break;
                LOGI(LOG_LEVEL_VIDEO, 0, LOG_FORMAT(" - Convert from WebM to MOV"), __PRETTY_FUNCTION__, __LINE__);
                if (!sound) {

                    LOGI(LOG_LEVEL_VIDEO, 0, LOG_FORMAT(" - Without sound"), __PRETTY_FUNCTION__, __LINE__);
                    mfsrc.assign("filesrc location=");
                    mfsrc.append(fileName); // '../MCAM_*.webm'
                    mfsrc.append(" ! matroskademux ! vp8dec ! videoconvert ! x264enc ! video/x-h264,profile=baseline"
                                 " ! qtmux ! filesink location=");
                    mfsrc.append(fileName); // '../MCAM_*.webm'
                    mfsrc.resize(mfsrc.size() - sizeof(WEBM_FILE_EXTENSION) + 1);
                    mfsrc.append(MOV_FILE_EXTENSION); // '.mov'
                }
                else {

                    LOGI(LOG_LEVEL_VIDEO, 0, LOG_FORMAT(" - With sound"), __PRETTY_FUNCTION__, __LINE__);
                    mfsrc.assign("qtmux name=mux ! filesink location=");
                    mfsrc.append(fileName); // '../MCAM_*.webm'
                    mfsrc.resize(mfsrc.size() - sizeof(WEBM_FILE_EXTENSION) + 1);
                    mfsrc.append(MOV_FILE_EXTENSION); // '.mov'
                    mfsrc.append(" filesrc location=");
                    mfsrc.append(fileName); // '../MCAM_*.webm'
                    mfsrc.append(" ! matroskademux ! vp8dec ! videoconvert ! x264enc ! video/x-h264,profile=baseline ! queue"
                                 " ! mux.video_0 filesrc location=");
                    mfsrc.append(mPicFolder);
                    mfsrc.append(MCAM_SUB_FOLDER);
                    mfsrc.append(MCAM_MIC_FILENAME);
                    mfsrc.append(WAV_FILE_EXTENSION);
                    mfsrc.append(" ! wavparse ! audioconvert ! voaacenc ! queue ! mux.audio_0");
                }
                if (!Picture::gstLaunch(mfsrc)) {

                    LOGW(LOG_FORMAT(" - Failed to create MOV video file"), __PRETTY_FUNCTION__, __LINE__);
                    //assert(NULL); // Sorry for all iOS clients!

                    // Delete wrong MOV file (if any)
                    fileName.resize(fileName.size() - sizeof(WEBM_FILE_EXTENSION) + 1);
                    fileName.append(MOV_FILE_EXTENSION); // '.mov'
                    if (boost::filesystem::exists(fileName))
                        boost::filesystem::remove(fileName);
                }
#ifdef DEBUG
                else {
                    LOGI(LOG_LEVEL_VIDEO, 0, LOG_FORMAT(" - MOV video file created"), __PRETTY_FUNCTION__, __LINE__);
                }
            }
            else {
                LOGI(LOG_LEVEL_VIDEO, 0, LOG_FORMAT(" - No need to create MOV video file"), __PRETTY_FUNCTION__, __LINE__);
#endif
            }

#else // iOS

            LOGI(LOG_LEVEL_VIDEO, 0, LOG_FORMAT(" - Convert AAC into WAV"), __PRETTY_FUNCTION__, __LINE__);
            std::string fileName(mPicFolder);
            fileName.append(MCAM_SUB_FOLDER);
            fileName.append(RECORD_MIC_FILENAME);
            fileName.append(AAC_FILE_EXTENSION);

            bool sound = static_cast<bool>([[NSFileManager defaultManager]
                                       fileExistsAtPath:[NSString stringWithUTF8String:fileName.c_str()]]);

            std::string mfsrc("filesrc location=");
            if (sound) {

                mfsrc.append(fileName);
                mfsrc.append(" ! decodebin ! audioresample ! audioconvert ! wavenc ! filesink location=");
                fileName.resize(fileName.size() - sizeof(AAC_FILE_EXTENSION) + 1);
                fileName.append(WAV_FILE_EXTENSION);
                mfsrc.append(fileName);

                sound = Picture::gstLaunch(mfsrc);
                if (sound) {

                    mergeWAV();

                    //
                    LOGI(LOG_LEVEL_VIDEO, 0, LOG_FORMAT(" - Create OGG from WAV"), __PRETTY_FUNCTION__, __LINE__);
                    mfsrc.assign("filesrc location=");
                    fileName.assign(mPicFolder);
                    fileName.append(MCAM_SUB_FOLDER);
                    fileName.append(MCAM_MIC_FILENAME);
                    fileName.append(WAV_FILE_EXTENSION);
                    mfsrc.append(fileName);
                    mfsrc.append(" ! wavparse ! audioconvert ! vorbisenc ! oggmux ! filesink location=");
                    fileName.resize(fileName.size() - sizeof(WAV_FILE_EXTENSION) + 1);
                    fileName.append(OGG_FILE_EXTENSION);
                    mfsrc.append(fileName);

                    if ((!Picture::gstLaunch(mfsrc)) && ([[NSFileManager defaultManager]
                                                fileExistsAtPath:[NSString stringWithUTF8String:fileName.c_str()]]))
                        [[NSFileManager defaultManager] removeItemAtPath:[NSString stringWithUTF8String:fileName.c_str()]
                                                                   error:nil]; // Remove wrong OGG file
                }
            }
            mRecorder->clear();

            //
            if (aborted(__PRETTY_FUNCTION__, __LINE__, proc))
                break;
            LOGI(LOG_LEVEL_VIDEO, 0, LOG_FORMAT(" - Convert all JPEG to MOV"), __PRETTY_FUNCTION__, __LINE__);

            fileName.assign(Picture::getFileName(&mPicFolder, JPEG_FILE_EXTENSION));
            fileName.resize(fileName.size() - 7); // '000.jpg' contains 7 characters
            mFileName.assign(VIDEO_FILENAME);

            time_t curDate = time(NULL);
            struct tm* now = localtime(&curDate);
            mFileName.append(numToStr<short>(now->tm_year + 1900));
            if (now->tm_mon < 9)
                mFileName += '0';
            mFileName.append(numToStr<short>(now->tm_mon + 1));
            if (now->tm_mday < 10)
                mFileName += '0';
            mFileName.append(numToStr<short>(now->tm_mday));
            mFileName += '_';
            if (now->tm_hour < 10)
                mFileName += '0';
            mFileName.append(numToStr<short>(now->tm_hour));
            if (now->tm_min < 10)
                mFileName += '0';
            mFileName.append(numToStr<short>(now->tm_min));
            if (now->tm_sec < 10)
                mFileName += '0';
            mFileName.append(numToStr<short>(now->tm_sec));
            mFileName.append(MOV_FILE_EXTENSION);

            if (!sound) {

                LOGI(LOG_LEVEL_VIDEO, 0, LOG_FORMAT(" - Without sound"), __PRETTY_FUNCTION__, __LINE__);
                mfsrc.assign("multifilesrc location=");
                mfsrc.append(fileName); // '.../img_'
                mfsrc.append("%d.jpg index=0 caps=\"image/jpeg,framerate=");
                mfsrc.append(numToStr<short>(static_cast<short>(mFPS)));
                mfsrc.append("/1\" ! jpegdec ! videoconvert ! x264enc ! video/x-h264,profile=baseline ! qtmux"
                             " ! filesink location=");
                mfsrc.append(mMovFolder);
                mfsrc.append(mFileName);
            }
            else {

                LOGI(LOG_LEVEL_VIDEO, 0, LOG_FORMAT(" - With sound"), __PRETTY_FUNCTION__, __LINE__);
                mfsrc.assign("qtmux name=mux ! filesink location=");
                mfsrc.append(mMovFolder);
                mfsrc.append(mFileName);
                mfsrc.append(" multifilesrc location=");
                mfsrc.append(fileName); // '../img_'
                mfsrc.append("%d.jpg index=0 caps=\"image/jpeg,framerate=");
                mfsrc.append(numToStr<short>(static_cast<short>(mFPS)));
                mfsrc.append("/1\" ! jpegdec ! videoconvert ! x264enc ! video/x-h264,profile=baseline ! queue"
                             " ! mux.video_0 filesrc location=");
                fileName.assign(mPicFolder);
                fileName.append(MCAM_SUB_FOLDER);
                fileName.append(MCAM_MIC_FILENAME);
                fileName.append(WAV_FILE_EXTENSION);
                mfsrc.append(fileName);
                mfsrc.append(" ! wavparse ! audioconvert ! voaacenc ! queue ! mux.audio_0");
            }
            if (!Picture::gstLaunch(mfsrc)) {

                mFileName.clear();
                alertMessage(LOG_LEVEL_VIDEO, 2.5, SAVE_VIDEO_ERROR);
                mStatus = LIBENG_NO_DATA; // Error
                break;
            }
            fileName.assign(mMovFolder);
            fileName.append(mFileName);

            //
            if (aborted(__PRETTY_FUNCTION__, __LINE__, proc))
                break;
            LOGI(LOG_LEVEL_VIDEO, 0, LOG_FORMAT(" - Add MOV video file into media album"), __PRETTY_FUNCTION__, __LINE__);
            Storage::getInstance()->saveVideo(fileName.c_str(), STORE_MEDIA_SUCCEEDED, STORE_MEDIA_FAILED, 3.5);
#endif
            mPicIdx = 0;
            mStatus = 1; // Ok
            break;
        }
        case PROC_STORE: { // Store (save video) - Client

            std::string fileName(mMovFolder);
            fileName.append(mFileName);

#ifdef __ANDROID__
            alertMessage(LOG_LEVEL_VIDEO, STORE_MEDIA_SUCCEEDED);
#else
            Storage::getInstance()->saveVideo(fileName.c_str(), STORE_MEDIA_SUCCEEDED, STORE_MEDIA_FAILED, 3.5);
#endif
            //
            if (aborted(__PRETTY_FUNCTION__, __LINE__, proc))
                break;
            LOGI(LOG_LEVEL_VIDEO, 0, LOG_FORMAT(" - Extract JPEG files from WebM/MOV video file"), __PRETTY_FUNCTION__,
                    __LINE__);
            std::string pipeline("filesrc location=");
            pipeline.append(fileName); // Video file name path
#ifdef __ANDROID__
            if (miOS) // WebM video
                pipeline.append(" ! matroskademux ! vp8dec ! jpegenc ! multifilesink location=");
            else // MOV file
                pipeline.append(" ! qtdemux ! decodebin ! jpegenc ! multifilesink location=");
#else
            // MOV video
            pipeline.append(" ! qtdemux ! decodebin ! jpegenc ! multifilesink location=");
#endif
            std::string filePath(mPicFolder);
            filePath.append(MCAM_SUB_FOLDER);
            pipeline.append(filePath);
            pipeline.append("/img_%d.jpg");

            Picture::createPath(&mPicFolder);
            if (!Picture::gstLaunch(pipeline)) {

                mStatus = LIBENG_NO_DATA; // Error
                mAbort = true;
                break;
            }
#ifdef __ANDROID__ // Get JPEG files count
            mPicCount = static_cast<short>(std::distance(boost::filesystem::directory_iterator(filePath.c_str()),
                    boost::filesystem::directory_iterator()));
#else
            mPicCount = static_cast<short>([[[NSFileManager defaultManager]
                            contentsOfDirectoryAtPath:[NSString stringWithUTF8String:filePath.c_str()] error:nil] count]);
#endif
            //assert(mPicCount < ((MAX_VIDEO_FPS * (RECORD_DURATION_BEFORE + RECORD_DURATION_AFTER)) + (255 * 3))); // x2 x3
            //assert(mPicCount > (3 * 3)); // x2 x3

            //
            if (aborted(__PRETTY_FUNCTION__, __LINE__, proc))
                break;
            LOGI(LOG_LEVEL_VIDEO, 0, LOG_FORMAT(" - Extract OGG file from WebM/MOV video file (if any)"), __PRETTY_FUNCTION__,
                    __LINE__);
            pipeline.assign("filesrc location=");
            pipeline.append(fileName); // Video file name
#ifdef __ANDROID__
            if (miOS) // WebM video
                pipeline.append(" ! matroskademux ! vorbisdec ! audioconvert ! vorbisenc ! oggmux ! filesink location=");
            else // MOV file
                pipeline.append(" ! qtdemux ! faad ! audioconvert ! vorbisenc ! oggmux ! filesink location=");
#else
            // MOV video
            pipeline.append(" ! qtdemux ! faad ! audioconvert ! vorbisenc ! oggmux ! filesink location=");
#endif
            fileName.assign(mPicFolder);
            fileName.append(MCAM_SUB_FOLDER);
            fileName.append(MCAM_MIC_FILENAME);
            fileName.append(OGG_FILE_EXTENSION);
            pipeline.append(fileName);

#ifdef __ANDROID__
            bool sound = true;
#ifdef DEBUG
            sound = Picture::gstLaunch(pipeline, false);
#else
            sound = Picture::gstLaunch(pipeline);
#endif
            if ((!sound) && (boost::filesystem::exists(fileName)))
                boost::filesystem::remove(fileName); // Remove wrong OGG file

            if (!miOS) { // Server iOS

                //
                if (aborted(__PRETTY_FUNCTION__, __LINE__, proc))
                    break;
                LOGI(LOG_LEVEL_VIDEO, 0, LOG_FORMAT(" - Convert video from MOV to WebM (fps:%d)"), __PRETTY_FUNCTION__,
                        __LINE__, mFPS);
                assert((mFPS >= MIN_VIDEO_FPS) || (mFPS <= MAX_VIDEO_FPS));

                mFileName.resize(mFileName.size() - sizeof(MOV_FILE_EXTENSION) + 1);
                mFileName.append(WEBM_FILE_EXTENSION); // '.webm'

                fileName.assign(Picture::getFileName(&mPicFolder, JPEG_FILE_EXTENSION));
                fileName.resize(fileName.size() - 7); // '000.jpg' contains 7 characters
                if (!sound) {

                    LOGI(LOG_LEVEL_VIDEO, 0, LOG_FORMAT(" - Without sound"), __PRETTY_FUNCTION__, __LINE__);
                    pipeline.assign("multifilesrc location=");
                    pipeline.append(fileName); // '../img_'
                    pipeline.append("%d.jpg index=0 caps=\"image/jpeg,framerate=");
                    pipeline.append(numToStr<short>(static_cast<short>(mFPS)));
                    pipeline.append("/1\" ! jpegdec ! videoconvert ! vp8enc ! webmmux ! filesink location=");
                    pipeline.append(mMovFolder);
                    pipeline.append(mFileName); // '.webm'
                }
                else {

                    LOGI(LOG_LEVEL_VIDEO, 0, LOG_FORMAT(" - With sound"), __PRETTY_FUNCTION__, __LINE__);
                    pipeline.assign("webmmux name=mux ! filesink location=");
                    pipeline.append(mMovFolder);
                    pipeline.append(mFileName); // '.webm'
                    pipeline.append(" multifilesrc location=");
                    pipeline.append(fileName); // '../img_'
                    pipeline.append("%d.jpg index=0 caps=\"image/jpeg,framerate=");
                    pipeline.append(numToStr<short>(static_cast<short>(mFPS)));
                    pipeline.append("/1\" ! jpegdec ! videoconvert ! vp8enc ! queue ! mux.video_0 filesrc location=");
                    pipeline.append(mPicFolder);
                    pipeline.append(MCAM_SUB_FOLDER);
                    pipeline.append(MCAM_MIC_FILENAME);
                    pipeline.append(OGG_FILE_EXTENSION);
                    pipeline.append(" ! oggdemux ! vorbisdec ! audioconvert ! vorbisenc ! queue ! mux.audio_0");
                }
#ifdef DEBUG
                if (!Picture::gstLaunch(pipeline, false)) {
#else
                if (!Picture::gstLaunch(pipeline)) {
#endif
                    LOGW(LOG_FORMAT(" - Failed to convert video from MOV to WebM"), __PRETTY_FUNCTION__, __LINE__);
                    fileName.assign(mMovFolder);
                    fileName.append(mFileName); // '.webm'

                    // Delete wrong WebM file (if any)
                    if (boost::filesystem::exists(fileName))
                        boost::filesystem::remove(fileName);

                    mFileName.resize(mFileName.size() - sizeof(WEBM_FILE_EXTENSION) + 1);
                    mFileName.append(MOV_FILE_EXTENSION); // '.mov'
                }
                else { // OK: MOV file converted into WebM

                    fileName.assign(mMovFolder);
                    fileName.append(mFileName); // '.webm'
                    fileName.resize(fileName.size() - sizeof(WEBM_FILE_EXTENSION) + 1);
                    fileName.append(MOV_FILE_EXTENSION); // '.mov'

                    // Delete MOV file
                    boost::filesystem::remove(fileName);
                }
            }
            //else // Server Android (nothing to do)

            //
            if (aborted(__PRETTY_FUNCTION__, __LINE__, proc))
                break;
            LOGI(LOG_LEVEL_VIDEO, 0, LOG_FORMAT(" - Add WebM/MOV video into media album"), __PRETTY_FUNCTION__, __LINE__);
            fileName.assign(mMovFolder);
            fileName.append(mFileName); // WebM/MOV

            std::string videoTitle(VIDEO_TITLE);
            videoTitle.append(Share::extractDate(mFileName));
            Storage::getInstance()->saveMedia(fileName, (fileName.at(fileName.size() - 5) == '.')? WEBM_MIME_TYPE:MOV_MIME_TYPE,
                                              videoTitle);
#else // iOS
#ifdef DEBUG
            if ((!Picture::gstLaunch(pipeline, false)) &&
#else
            if ((!Picture::gstLaunch(pipeline)) &&
#endif
                    ([[NSFileManager defaultManager] fileExistsAtPath:[NSString stringWithUTF8String:fileName.c_str()]]))
                [[NSFileManager defaultManager] removeItemAtPath:[NSString stringWithUTF8String:fileName.c_str()]
                                                                  error:nil]; // Remove wrong OGG file
#endif
            //break;
        }
        case PROC_EXTRACT: { // Extract video & sound to be displayed

            //
            LOGI(LOG_LEVEL_VIDEO, 0, LOG_FORMAT(" - Extract JPEG files to display video"), __PRETTY_FUNCTION__, __LINE__);
            Picture texPic;
            texPic.setFolder(&mPicFolder);

            for (short i = 0; i < mPicCount; ++i) {
                if (aborted(__PRETTY_FUNCTION__, __LINE__, proc))
                    break;

                boost::this_thread::sleep(boost::posix_time::milliseconds(20));
                texPic.extract(mLandscape, i);
            }

            //
            if (aborted(__PRETTY_FUNCTION__, __LINE__, proc))
                break;
            LOGI(LOG_LEVEL_VIDEO, 0, LOG_FORMAT(" - Load OGG file into the player"), __PRETTY_FUNCTION__, __LINE__);
            assert(Player::getInstance()->getIndex(SOUND_ID_FILM) == SOUND_IDX_INVALID);

            std::string oggFile(mPicFolder);
            oggFile.append(MCAM_SUB_FOLDER);
            oggFile.append(MCAM_MIC_FILENAME);
            oggFile.append(OGG_FILE_EXTENSION);
#ifdef __ANDROID__
            if (boost::filesystem::exists(oggFile))
#else
            if ([[NSFileManager defaultManager] fileExistsAtPath:[NSString stringWithUTF8String:oggFile.c_str()]])
#endif
                loadOGG(oggFile); // Load OGG file into player

            mPicIdx = 0;
            mStatus = 1; // Ok
            break;
        }
    }

#ifdef __ANDROID__
    detachThreadJVM(LOG_LEVEL_CONNEXION); // If needed (e.g 'saveMedia' function call)
    if ((proc == PROC_SAVE) && (mStatus < 0))
        purge();
#endif
    LOGI(LOG_LEVEL_VIDEO, 0, LOG_FORMAT(" - Finished (sta:%d; file:%s)"), __PRETTY_FUNCTION__, __LINE__, mStatus,
            mFileName.c_str());
}
void Video::startProcessThread(Video* movie, unsigned char proc) {

    LOGV(LOG_LEVEL_VIDEO, 0, LOG_FORMAT(" - m:%x; p:%d"), __PRETTY_FUNCTION__, __LINE__, movie, proc);
    movie->processThreadRunning(proc);
}

void Video::play() {

    LOGV(LOG_LEVEL_VIDEO, 0, LOG_FORMAT(" - (t:%d)"), __PRETTY_FUNCTION__, __LINE__,
         Player::getInstance()->getIndex(SOUND_ID_FILM));

    unsigned char track = Player::getInstance()->getIndex(SOUND_ID_FILM);
    if (track != SOUND_IDX_INVALID)
        Player::getInstance()->play(track);

    mPlaying = true;
}
void Video::mute() {

    LOGV(LOG_LEVEL_VIDEO, 0, LOG_FORMAT(" - (t:%d)"), __PRETTY_FUNCTION__, __LINE__,
         Player::getInstance()->getIndex(SOUND_ID_FILM));

    unsigned char track = Player::getInstance()->getIndex(SOUND_ID_FILM);
    if ((track != SOUND_IDX_INVALID) && (Player::getInstance()->getStatus(track) == AL_PLAYING))
        Player::getInstance()->pause(track);
}

void Video::update(const Game* game) {

    if ((!mTexGen) && (mPlaying) && (mPicIdx == mPicCount)) {

        --mPicIdx;
        generate(); // Hoping the texture has been successfully generated!
        ++mPicIdx;
        // ...pause/resume when video has been displayed but still playing sound
    }
    else if ((!mTexGen) && (!generate()) && (++mPicIdx == mPicCount))
        mPicIdx = 0;
    if (!mPlaying)
        return;

    Player* player = Player::getInstance();
    unsigned char track = player->getIndex(SOUND_ID_FILM);
    if ((track != SOUND_IDX_INVALID) && (player->isRunning()) && (player->getStatus(track) == AL_PAUSED))
        player->play(track);

    if (mPicIdx == mPicCount) {

        assert(track != SOUND_IDX_INVALID);
        if ((player->isRunning()) && (player->getStatus(track) != AL_PLAYING)) {

            mPicIdx = 0;
            mPlaying = false; // Finish to play video & sound
            generate(); // Load first frame
        }
        return;
    }
    clock_t now = clock();
    if (!mDelay)
        mDelay = now;

    if (((now - mDelay) / game->mTickPerSecond) > (1.f / mFPS)) {

        mDelay = now;
        if (++mPicIdx == mPicCount) {
            if ((track == SOUND_IDX_INVALID) || (player->getStatus(track) != AL_PLAYING)) {

                mPicIdx = 0;
                mPlaying = false;
                generate();
            }
            //else // Still playing sound
        }
        else
            generate();
    }
}
void Video::render() const {
    if (mTexGen)
        mFilm.render(false);
}
