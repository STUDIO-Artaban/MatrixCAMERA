#ifndef MATRIXLEVEL_H_
#define MATRIXLEVEL_H_

#include "Global.h"

#include <libeng/Game/Level.h>
#include <libeng/Graphic/Object/2D/Element2D.h>
#include <libeng/Graphic/Text/2D/Text2D.h>
#include <libeng/Features/Camera/Camera.h>
#include <libeng/Features/Mic/Mic.h>
#include <libeng/Tools/Tools.h>
#if !defined(PAID_VERSION) && !defined(DEMO_VERSION)
#include <libeng/Advertising/Advertising.h>
#endif

#ifdef __ANDROID__
#include "Level/PanelCoords.h"
#include "Frame/Frame2D.h"
#include "Frame/Count2D.h"
#include "Wifi/SearchIP.h"
#include "Wifi/Connexion.h"
#include "Share/Share.h"

#else
#include "PanelCoords.h"
#include "Frame2D.h"
#include "Count2D.h"
#include "SearchIP.h"
#include "Connexion.h"
#include "Share.h"

#endif
#include <time.h>

static const float g_landTexCoord[8] = { LAND_X0 / PANEL_TEX_WIDTH, LAND_Y0 / PANEL_TEX_HEIGHT,
        LAND_X0 / PANEL_TEX_WIDTH, LAND_Y2 / PANEL_TEX_HEIGHT, LAND_X2 / PANEL_TEX_WIDTH,
        LAND_Y2 / PANEL_TEX_HEIGHT, LAND_X2 / PANEL_TEX_WIDTH, LAND_Y0 / PANEL_TEX_HEIGHT };
static const float g_portTexCoord[8] = { PORT_X0 / PANEL_TEX_WIDTH, PORT_Y0 / PANEL_TEX_HEIGHT,
        PORT_X0 / PANEL_TEX_WIDTH, PORT_Y2 / PANEL_TEX_HEIGHT, PORT_X2 / PANEL_TEX_WIDTH,
        PORT_Y2 / PANEL_TEX_HEIGHT, PORT_X2 / PANEL_TEX_WIDTH, PORT_Y0 / PANEL_TEX_HEIGHT };

using namespace eng;

//////
class MatrixLevel : public Level {

private:
    Player* mPlayer;
#ifdef __ANDROID__
    mutable Camera* mCamera;
#else
    Camera* mCamera;
#endif
    SearchIP* mSearch;
    Connexion* mConnexion;
    Video* mVideo;
    Share* mShare;

    const Game* mApp;
#ifndef PAID_VERSION
    const unsigned char* mFontBuffer;
#endif

#if !defined(PAID_VERSION) && !defined(DEMO_VERSION)
    Advertising* mAdvertising;
    bool mAdLoaded;
    void adDisplay(bool delay);
#endif
    std::string mRecMicFile;

    enum {

        REC_MIC_NONE = 0,
        REC_MIC_STOPPED,
        REC_MIC_STARTED
    };
    unsigned char mMicRecording;
    bool mMovRecording;

    float mRecBound;
    clock_t mRecCounter;
    time_t mRecElapsed;

public:
    enum {

        MCAM_NONE = 1, // != 0
        MCAM_WAIT,
        MCAM_TIMEOUT,
        MCAM_ORIENTATION,
        MCAM_FRAMENO, // For client only
        MCAM_READY,
        MCAM_GO,

        MCAM_INTERRUPT,
        MCAM_DOWNLOAD,
        MCAM_NO_DISPLAY,
        MCAM_DISPLAY,

        MCAM_PAUSED = 0xff // Or screen locked
    };
    unsigned char mStatus; // Main status
    bool mLandscape; // Orientation flag

    inline bool isRecReady() const { return (!mMovRecording); }
    inline Video* getVideo() { return mVideo; }

#ifndef PAID_VERSION
    inline const unsigned char* getLogo() const { return mFontBuffer; }
#endif

private:
    unsigned char mFrameNo; // #0: Not defined; #1: Server; #[2;245]: Client
    bool mGO;

    Element2D* mOrientation;
    TouchArea mOrientationArea;
    inline void direct(bool land) {

        LOGV(LOG_LEVEL_MATRIXLEVEL, 0, LOG_FORMAT(" - l:%s"), __PRETTY_FUNCTION__, __LINE__, (land)? "true":"false");
        mLandscape = land;
        if (mLandscape) {

            mOrientation->setTexCoords((mFrameNo != 1)? g_landTexCoord:g_portTexCoord);
            mOrientation->rotate(0.f);
        }
        else {

            mOrientation->setTexCoords((mFrameNo != 1)? g_portTexCoord:g_landTexCoord);
#ifdef __ANDROID__
            mOrientation->rotate(2.f * PI_F);
#else
            mOrientation->rotate(PI_F / 2.f);
#endif
        }
    };

    Back2D* mHideCam;
    Static2D* mLogo;
    Text2D* mChoice;

    Static2D* mBack1;
    Static2D* mBackN;
    TouchArea mServerArea;
    TouchArea mClientArea;

    Static2D* mBackCam;
    Back2D* mAlphaCam;
    Static2D* mFilm;
    Static2D* mMark;

    inline void checkWait() {

        if (!mConnexion)
            return; // Server/Client not defined yet

        if (1 == mFrameNo) { // Server

            if ((mStatus == MCAM_ORIENTATION) || (mStatus == MCAM_READY) || (mStatus == MCAM_GO))
                return; // Wait clients ready (nothing to do)

            switch (mConnexion->getStatus()) {

                case Connexion::CONN_NEW:
                case Connexion::CONN_VERIFY:
                case Connexion::CONN_ORIENTATION:
                    mStatus = MCAM_WAIT;
                    break;

                case Connexion::CONN_TIMEOUT:
                    mStatus = MCAM_TIMEOUT;
                    break;
            }
        }
        else // Client
        if (mConnexion->getStatus() == Connexion::CONN_TIMEOUT)
            mStatus = MCAM_TIMEOUT;
    };
    inline float getScreenScale(const Game* game) {

        short filmH = static_cast<short>((game->getScreen()->width >> 1) * SCREEN_SCALE_RATIO *
                CAM_HEIGHT / CAM_WIDTH);
        if (filmH > (game->getScreen()->height >> 1))
            filmH = game->getScreen()->height >> 1;

        return (filmH / static_cast<float>(WAIT_SIZE));
    };

    Frame2D* mFrame;
    Count2D* mCount;

    Element2D* mWait;
    void positionWait(const Game* game, bool server);

    Element2D* mPress;
    Element2D* mPlay;
    TouchArea mPressArea;

    inline void ready() {

        LOGV(LOG_LEVEL_MATRIXLEVEL, 0, LOG_FORMAT(), __PRETTY_FUNCTION__, __LINE__);
        mPress->setRed(MCAM_RED_COLOR / MAX_COLOR);
        mPress->setGreen(MCAM_GREEN_COLOR / MAX_COLOR);
        mPress->setBlue(MCAM_BLUE_COLOR / MAX_COLOR);

        mMark->setRed(MCAM_RED_COLOR / MAX_COLOR);
        mMark->setGreen(MCAM_GREEN_COLOR / MAX_COLOR);
        mMark->setBlue(MCAM_BLUE_COLOR / MAX_COLOR);
        if (1 == mFrameNo) { // Server

            mWait->setRed(MCAM_RED_COLOR / MAX_COLOR);
            mWait->setGreen(MCAM_GREEN_COLOR / MAX_COLOR);
            mWait->setBlue(MCAM_BLUE_COLOR / MAX_COLOR);

            mCount->setColor(MCAM_RED_COLOR / MAX_COLOR, MCAM_GREEN_COLOR / MAX_COLOR, MCAM_BLUE_COLOR / MAX_COLOR);
        }
        mStatus = MCAM_GO;
    };
    inline void done() {

        LOGV(LOG_LEVEL_MATRIXLEVEL, 0, LOG_FORMAT(), __PRETTY_FUNCTION__, __LINE__);
        mPress->setRed(1.f);
        mPress->setGreen(1.f);
        mPress->setBlue(1.f);

        mMark->setRed(1.f);
        mMark->setGreen(1.f);
        mMark->setBlue(1.f);
        if (1 == mFrameNo) { // Server

            mWait->setRed(1.f);
            mWait->setGreen(1.f);
            mWait->setBlue(1.f);

            mCount->setColor(1.f, 1.f, 1.f);
        }
    };

    void interrupt();
    void restart(const Game* game);
    void refresh(const Game* game);

public:
    MatrixLevel(Game* game);
    virtual ~MatrixLevel();

    inline bool isPaused() const { return (!mLoadStep); };
#ifdef __ANDROID__
    inline bool isScreenLocked() const { return mApp->isScreenLocked(); }
#endif

    //////
    inline void pause() {

        LOGV(LOG_LEVEL_MATRIXLEVEL, 0, LOG_FORMAT(), __PRETTY_FUNCTION__, __LINE__);
        Level::pause();

        if (mMovRecording) // Can be true for server only
            interrupt();

        if (mVideo) mVideo->pause();
#ifndef PAID_VERSION
        if (mFontBuffer != NULL) {

            delete [] mFontBuffer;
            mFontBuffer = NULL;
        }
#endif
        if (mHideCam) mHideCam->pause();
        if (mLogo) mLogo->pause();
        if (mBack1) mBack1->pause();
        if (mBack1) mBackN->pause();
        if (mChoice) mChoice->pause();
        if (mBackCam) mBackCam->pause();
        if (mAlphaCam) mAlphaCam->pause();
        if (mFilm) mFilm->pause();
        if (mMark) mMark->pause();
        if (mFrame) mFrame->pause();
        if (mCount) mCount->pause();
        if (mWait) mWait->pause();
        if (mOrientation) mOrientation->pause();
        if (mPress) mPress->pause();
        if (mPlay) mPlay->pause();
        if (mShare) mShare->pause();
    };
#ifdef __ANDROID__
    inline void lockScreen() {

        if (mMovRecording) // Can be true for server only
            interrupt();
    };
#endif

    void wait(float millis);

protected:
    bool loaded(const Game* game);
public:
    bool update(const Game* game);

    void renderLoad() const;
    void renderUpdate() const;

};

#endif // MATRIXLEVEL_H_
