#ifndef VIDEO_H_
#define VIDEO_H_

#include "Global.h"

#include <libeng/Log/Log.h>
#include <libeng/Graphic/Object/2D/Static2D.h>
#include <libeng/Game/2D/Game2D.h>

#include <boost/thread.hpp>
#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <time.h>

#ifdef __ANDROID__
#include "Video/Picture.h"
#else
#include "Picture.h"
#endif

#define SCREEN_SCALE_RATIO          (5.f / 7.f)

#define MAX_VIDEO_FPS               16
#define MIN_VIDEO_FPS               5
#define MCAM_FPS_FACTOR(fps)        ((fps > 13)? 4:((fps > 8)? 3:2))

#define FREEZE_CAMERA_DURATION      700 // In milliseconds

#define RECORD_DURATION_BEFORE      7 // Seconds
#define RECORD_DURATION_AFTER       3 // ...
#define RECORD_MIC_FILENAME         "/micFile"

using namespace eng;

class Video;
class Recorder {

    friend class Video;

private:
    enum {

        STATUS_PROGRESS = 0,
        STATUS_DONE,
        STATUS_ERROR
    };
    typedef struct {

        time_t elapsed;
        short index;
        unsigned char status;

    } RecFrame;

    std::vector<RecFrame*> mBefore;
    std::vector<RecFrame*> mAfter;

    inline short getDoneCount(bool before) const {

        short res = 0;
        const std::vector<RecFrame*>* vec = (before)? &mBefore:&mAfter;
        for (std::vector<RecFrame*>::const_iterator iter = vec->begin(); iter != vec->end(); ++iter)
            if ((*iter)->status == STATUS_DONE)
                ++res;

        return res;
    };
    inline unsigned char getFPS() const {

        time_t last = 0, first = 0;
        for (std::vector<RecFrame*>::const_iterator iter = mBefore.begin(); iter != mBefore.end(); ++iter) {
            if ((*iter)->status != STATUS_DONE)
                continue;

            if (!first)
                first = (*iter)->elapsed;
            last = (*iter)->elapsed;
        }
        time_t duration = last - first;
        last = first = 0;
        for (std::vector<RecFrame*>::const_iterator iter = mAfter.begin(); iter != mAfter.end(); ++iter) {
            if ((*iter)->status != STATUS_DONE)
                continue;

            if (!first)
                first = (*iter)->elapsed;
            last = (*iter)->elapsed;
        }
        float recFPS = static_cast<float>(duration + last - first) / (getDoneCount(true) + getDoneCount(false));
        return (recFPS < (1.f / MAX_VIDEO_FPS))? MAX_VIDEO_FPS:((recFPS > (1.f / MIN_VIDEO_FPS))?
                MIN_VIDEO_FPS:static_cast<unsigned char>(1.f / recFPS));
    };

#ifndef PAID_VERSION
    const unsigned char* mLogo;
#endif
    bool mLandscape;
    const std::string* mFolder; // Picture folder

    volatile bool mAbort;
    boost::mutex mMutex;
    boost::thread* mThread;

    void processThreadRunning();
    static void startProcessThread(Recorder* recorder);

public:
    Recorder(const std::string* folder);
    virtual ~Recorder();

    //
    time_t add(const unsigned char* rgba, bool before);
#ifndef PAID_VERSION
    void start(const unsigned char* logo, bool landscape);
#else
    void start(bool landscape);
#endif
    void clear();

    inline bool isConverted() const { // From BIN to JPEG files

        for (std::vector<RecFrame*>::const_iterator iter = mBefore.begin(); iter != mBefore.end(); ++iter)
            if (!(*iter)->status) // STATUS_PROGRESS
                return false;
        for (std::vector<RecFrame*>::const_iterator iter = mAfter.begin(); iter != mAfter.end(); ++iter)
            if (!(*iter)->status)
                return false;

        return true;
    };

};

//////
class Video {

private:
    std::string mPicFolder;
    std::string mMovFolder;
    Static2D mFilm;

    clock_t mDelay;

    Recorder* mRecorder;
    unsigned char mFPS;
    inline void copyFile(std::string* src, std::string* dst, size_t size, short srcIdx, short dstIdx) const {

        src->resize(size);
        dst->assign(*src);
        src->append(numToStr<short>(srcIdx));
        src->append(JPEG_FILE_EXTENSION);
        dst->append(numToStr<short>(dstIdx));
        dst->append(JPEG_FILE_EXTENSION);

        std::ifstream copySrc(src->c_str(), std::ios::binary);
        std::ofstream copyDst(dst->c_str(), std::ios::binary);

        copyDst << copySrc.rdbuf(); // Copy JPEG file

        copySrc.close();
        copyDst.close();
    };

    typedef struct {

        unsigned char client; // Client index
        Picture* picture;

    } Frame;
    std::vector<Frame*> mPictures;

    short mPicIdx; // Video texture index displaying
    short mPicCount;
    std::string mFileName; // Video file name (prefixed with '/')

#ifdef __ANDROID__
    bool miOS;

    char* mBufferWEBM;
    char* mBufferMOV;

    int mBufferLenWEBM;
    int mBufferLenMOV;
#endif
    void mergeWAV();
    unsigned char loadOGG(const std::string &file);

    char* mBuffer;
    int mBufferLen;
    int mRcvLen;

    bool mPlaying;
    bool mTexGen;
    char* mTexBuffer;
    bool generate();

    bool mLandscape;
    unsigned char mClientCount; // Bullet time frame count

public:
    Video();
    virtual ~Video();

    inline const std::string* getPicFolder() const { return &mPicFolder; }
    inline const std::string* getMovFolder() const { return &mMovFolder; }
    inline unsigned char getCount() const {

        unsigned char count = 1; // Server frame
        for (std::vector<Frame*>::const_iterator iter = mPictures.begin(); iter != mPictures.end(); ++iter)
            if ((*iter)->picture->isDone())
                ++count;

        return count;
    };

    //
    void initialize(const Game2D* game);
    inline void pause() { mFilm.pause(); }
    void resume();

    void play();
    void mute();
    inline bool isPlaying() const { return mPlaying; }

#ifdef __ANDROID__
    void purge(); // Delete MOV video file (if any)
    void free(bool server); // Delete video textures & clear
#else
    void free(); // ...
#endif

    void update(const Game* game);
    void render() const;

private:
    signed char mStatus;

    volatile bool mAbort;
    boost::thread* mThread;
    void start(unsigned char process);

    enum {

        PROC_SAVE = 0,
        PROC_STORE,
        PROC_EXTRACT
    };
    inline bool aborted(const char* function, int line, unsigned char proc) {

        if (mAbort) {

            LOGW(LOG_FORMAT(" - Process %d aborted"), function, line, proc);
            mStatus = LIBENG_NO_DATA;
            return true;
        }
        return false;
    };
    void processThreadRunning(unsigned char proc);
    static void startProcessThread(Video* movie, unsigned char proc);

public:
    inline signed char getStatus() const { return mStatus; } // WARNING - 1: Done; 0: Processing; -1: Error
    inline const char* getBuffer() const { return mBuffer; }
    inline int getSize() const { return mBufferLen; }
    inline unsigned char getFPS() const { return mFPS; }

    inline const std::string* getFileName() const { return &mFileName; }
    inline Recorder* getRecorder() { return mRecorder; }

    //////
    void add(Picture* picture, unsigned char client = 0);
    inline Picture* get(unsigned char client = 0) {

        for (std::vector<Frame*>::const_iterator iter = mPictures.begin(); iter != mPictures.end(); ++iter)
            if ((*iter)->client == client)
                return (*iter)->picture;

        return NULL;
    };
    void clear(bool remove = true); // Remove path

    void prepare(int size, unsigned char fps); // Prepare to fill WebM buffer (method below)
    signed char fill(const ClientMgr* mgr);
    inline bool isFilled() const { return (mRcvLen == mBufferLen); }
#ifdef __ANDROID__
    bool store(bool landscape, bool android); // Server OS
    void select(bool android); // WebM/MOV buffer & size selection (client OS)
#else
    bool store(bool landscape);
#endif

#ifdef __ANDROID__
    typedef struct {

        bool done; // Status
        bool android; // Client OS

    } FrameClient;
    typedef std::vector<FrameClient*> FrameList;
#else
    typedef std::vector<bool> FrameList;
#endif

    bool save(const FrameList* clients, bool landscape);
    void extract();
    bool open();

};

#endif // VIDEO_H_
