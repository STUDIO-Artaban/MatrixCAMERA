#include "MatrixLevel.h"

#include <libeng/Game/2D/Game2D.h>
#include <libeng/Storage/Storage.h>
#include <libeng/Tools/Tools.h>
#ifdef __ANDROID__
#include <boost/math/tr1.hpp>

#else
#import <AVFoundation/AVAudioSession.h>
#include <math.h>

#endif

#define MAX_LOAD_STEP               7

#define SERVER_CLIENT_CHOICE        L"â1/ân?" // ...with 'â' replaced by '#'
#define WAIT_ROTATE_VEL             (PI_F / -30.f)
#define SEND_GO_DELAY               7 // In millisecond
#ifdef __ANDROID__
#define OUTPUT_FORMAT_3GP           1
#endif

#define MIN_FREE_SPACE              250000000 // 250 MB (Server)
#define UNSUFFICIENT_FREE_SPACE     "Free space unsufficient (< 250 MB)"

// Texture IDs
#define TEXTURE_ID_APP              2

//////
MatrixLevel::MatrixLevel(Game* game) : Level(game), mLandscape(true), mFrameNo(0), mStatus(MCAM_NONE), mApp(game),
        mMovRecording(false), mRecCounter(0), mRecBound(0.f), mGO(false), mMicRecording(REC_MIC_NONE) {

    LOGV(LOG_LEVEL_MATRIXLEVEL, 0, LOG_FORMAT(" - g:%x"), __PRETTY_FUNCTION__, __LINE__, game);
    std::memset(&mServerArea, 0, sizeof(TouchArea));
    std::memset(&mClientArea, 0, sizeof(TouchArea));
    std::memset(&mOrientationArea, 0, sizeof(TouchArea));
    std::memset(&mPressArea, 0, sizeof(TouchArea));

    Inputs::InputUse = USE_INPUT_TOUCH;

    mCamera = Camera::getInstance();
    mPlayer = Player::getInstance();
#if !defined(PAID_VERSION) && !defined(DEMO_VERSION)
    mAdLoaded = false;
    mAdvertising = Advertising::getInstance();
#endif

    mConnexion = NULL;
    mSearch = NULL;
    mVideo = NULL;
#ifndef PAID_VERSION
    mFontBuffer = NULL;
#endif

    mHideCam = NULL;
    mLogo = NULL;
    mChoice = NULL;
    mBack1 = NULL;
    mBackN = NULL;
    mBackCam = NULL;
    mAlphaCam = NULL;
    mFilm = NULL;
    mMark = NULL;
    mFrame = NULL;
    mCount = NULL;
    mWait = NULL;
    mOrientation = NULL;
    mPress = NULL;
    mPlay = NULL;
    mShare = NULL;
}
MatrixLevel::~MatrixLevel() {

    LOGV(LOG_LEVEL_MATRIXLEVEL, 0, LOG_FORMAT(), __PRETTY_FUNCTION__, __LINE__);
    if (mConnexion) delete mConnexion;
    if (mSearch) delete mSearch;
#ifdef __ANDROID__
    if ((mStatus == MCAM_DISPLAY) && (1 == mFrameNo))
        mVideo->purge();
#endif
    if (mVideo) delete mVideo;

#ifndef PAID_VERSION
    if (mFontBuffer != NULL) {

        delete [] mFontBuffer;
        mFontBuffer = NULL;
    }
#endif
    if (mHideCam) delete mHideCam;
    if (mLogo) delete mLogo;
    if (mBack1) delete mBack1;
    if (mBackN) delete mBackN;
    if (mChoice) delete mChoice;
    if (mBackCam) delete mBackCam;
    if (mAlphaCam) delete mAlphaCam;
    if (mFilm) delete mFilm;
    if (mMark) delete mMark;
    if (mFrame) delete mFrame;
    if (mCount) delete mCount;
    if (mWait) delete mWait;
    if (mOrientation) delete mOrientation;
    if (mPress) delete mPress;
    if (mPlay) delete mPlay;
    if (mShare) delete mShare;
}

bool MatrixLevel::loaded(const Game* game) {

    LOGV(LOG_LEVEL_MATRIXLEVEL, 0, LOG_FORMAT(" - l:%d"), __PRETTY_FUNCTION__, __LINE__, mLoadStep);
    switch (mLoadStep) {
        case 1: {

            if (!mHideCam) {

                mHideCam = new Back2D;
                mHideCam->initialize(game2DVia(game));
                mHideCam->start(0, 0, 0);
            }
            else
                mHideCam->resume(0, 0, 0);

            // Camera
            if (!mCamera->isStarted())
                mCamera->start(CAM_WIDTH, CAM_HEIGHT);
            break;
        }
        case 2: {

            // Search IP list
            if (!mSearch) {

                mSearch = new SearchIP;
                mSearch->start(false);
            }
            if (!mVideo) {

                mVideo = new Video();
                mVideo->initialize(game2DVia(game));
                Picture::removePath(mVideo->getPicFolder());

                mRecMicFile.assign(*mVideo->getPicFolder());
                mRecMicFile.append(MCAM_SUB_FOLDER);
                mRecMicFile.append(RECORD_MIC_FILENAME);
#ifdef __ANDROID__
                mRecMicFile.append(GP3_FILE_EXTENSION);
#else
                mRecMicFile.append(AAC_FILE_EXTENSION);
#endif
            }
            else
                mVideo->resume();

            // Font texture
            Textures::loadTexture(TEXTURE_ID_FONT);
            assert(mTextures->getIndex(TEXTURE_ID_FONT) == 2);
#ifndef PAID_VERSION
            assert(!mFontBuffer);
            mFontBuffer = (*mTextures)[2]->textureBuffer;
            mTextures->genTexture(2, false); // Keep font texture buffer (MCAM logo)
#else
            mTextures->genTexture(2);
#endif

#if !defined(PAID_VERSION) && !defined(DEMO_VERSION)
            // Advertising
            unsigned char adStatus = Advertising::getStatus();
            assert(adStatus != Advertising::STATUS_NONE);

            mAdLoaded = false;
            if ((adStatus == Advertising::STATUS_READY) || (adStatus == Advertising::STATUS_FAILED))
                mAdvertising->load();
            else
                mAdLoaded = (adStatus > Advertising::STATUS_LOADED); // STATUS_DISPLAYING | STATUS_DISPLAYED

            adDisplay(false);
#endif
            break;
        }
        case 3: {

            Textures::loadTexture(TEXTURE_ID_APP);
            assert(mTextures->getIndex(TEXTURE_ID_APP) == 3);
            mTextures->genTexture(3);
            // Always load this texture to keep FILM_TEXTURE_IDX valid in 'Video' module

            if (!mBackCam) {
                if (!mLogo) {

                    mLogo = new Static2D(false);
                    mLogo->initialize(game2DVia(game));
                    mLogo->start(3);
                    mLogo->setTexCoords(FULL_TEXCOORD_BUFFER);

                    short halfH = game->getScreen()->height / 3;
                    mLogo->setVertices((game->getScreen()->width >> 1) - halfH, (game->getScreen()->height >> 1) + halfH,
                            (game->getScreen()->width >> 1) + halfH, (game->getScreen()->height >> 1) - halfH);
                }
                else
                    mLogo->resume(3);
            }
            else if (mLogo)
                mLogo->resume(3);

            if (!mFilm) {

                mFilm = new Static2D(false);
                mFilm->initialize(game2DVia(game));
                mFilm->start(mCamera->getCamTexIdx());
#ifndef __ANDROID__
                mFilm->setBGRA(true);
#endif
                static const float texCoord[8] = { 0.f, 0.f, 0.f, CAM_HEIGHT / CAM_TEX_HEIGHT, CAM_WIDTH / CAM_TEX_WIDTH,
                        CAM_HEIGHT / CAM_TEX_HEIGHT, CAM_WIDTH / CAM_TEX_WIDTH, 0.f };
                mFilm->setTexCoords(texCoord);

                short screenW = (game->getScreen()->width >> 1) * SCREEN_SCALE_RATIO; // Half
                short screenH = screenW * CAM_HEIGHT / CAM_WIDTH;
                if (screenH > (game->getScreen()->height >> 1)) {

                    screenH = game->getScreen()->height >> 1;
                    screenW = screenH * CAM_WIDTH / CAM_HEIGHT;
                }
                mFilm->setVertices((game->getScreen()->width >> 1) - screenW, (game->getScreen()->height >> 1) + screenH,
                        (game->getScreen()->width >> 1) + screenW, (game->getScreen()->height >> 1) - screenH);
            }
            else
                mFilm->resume(mCamera->getCamTexIdx());

#if !defined(PAID_VERSION) && !defined(DEMO_VERSION)
            // Advertising
            adDisplay(false);
#endif
            break;
        }
        case 4: {

            if (!mBackCam) {

                mBackCam = new Static2D(false);
                mBackCam->initialize(game2DVia(game));
                mBackCam->start(mCamera->getCamTexIdx());
#ifndef __ANDROID__
                mBackCam->setBGRA(true);
#endif
                mBackCam->setAlpha(0.f);

                short backW = CAM_WIDTH;
                short backH = (CAM_WIDTH * game->getScreen()->height) / game->getScreen()->width;
                if (backH > CAM_HEIGHT) {
                    backH = CAM_HEIGHT;
                    backW = (CAM_HEIGHT * game->getScreen()->width) / game->getScreen()->height;
                }
                mBackCam->setVertices(0, 0, game->getScreen()->width, game->getScreen()->height);

                float texCoord[8];
                texCoord[0] = ((CAM_WIDTH >> 1) - (backW >> 1)) / CAM_TEX_WIDTH;
                texCoord[1] = ((CAM_HEIGHT >> 1) + (backH >> 1)) / CAM_TEX_HEIGHT;
                texCoord[2] = texCoord[0];
                texCoord[3] = ((CAM_HEIGHT >> 1) - (backH >> 1)) / CAM_TEX_HEIGHT;
                texCoord[4] = ((CAM_WIDTH >> 1) + (backW >> 1)) / CAM_TEX_WIDTH;
                texCoord[5] = texCoord[3];
                texCoord[6] = texCoord[4];
                texCoord[7] = texCoord[1];

                mBackCam->setTexCoords(texCoord);
            }
            else
                mBackCam->resume(mCamera->getCamTexIdx());

            if (!mAlphaCam) {

                mAlphaCam = new Back2D;
                mAlphaCam->initialize(game2DVia(game));
                mAlphaCam->start(0, 0, 0);
                mAlphaCam->setAlpha(0.f);
            }
            else
                mAlphaCam->resume(0, 0, 0);

            if (!mChoice) {

                mChoice = new Text2D;
                mChoice->initialize(game2DVia(game));
                mChoice->start(SERVER_CLIENT_CHOICE); // " #1/#n?" = 7 letters

                float scale = (game->getScreen()->width / static_cast<float>(7 * FONT_WIDTH)) * SCREEN_SCALE_RATIO;
                mChoice->scale(scale, scale);
                mChoice->position((-5.f * FONT_WIDTH * scale) / game->getScreen()->height,
                        (FONT_HEIGHT * scale) / game->getScreen()->height);

                mChoice->setColor(0.f, 0.f, 0.f);
                mChoice->setColor(1.f, 1.f, 1.f, 2);
                mChoice->setColor(1.f, 1.f, 1.f, 5);
            }
            else
                mChoice->resume();

#if !defined(PAID_VERSION) && !defined(DEMO_VERSION)
            // Advertising
            adDisplay(false);
#endif
            break;
        }
        case 5: {

            if (!mBack1) {

                mBack1 = new Static2D;
                mBack1->initialize(game2DVia(game));
                mBack1->start(TEXTURE_ID_FONT);

                static const float texCoord[8] = { BACK_X2 / FONT_TEX_WIDTH, BACK_Y0 / FONT_TEX_HEIGHT,
                        BACK_X0 / FONT_TEX_WIDTH, BACK_Y0 / FONT_TEX_HEIGHT, BACK_X0 / FONT_TEX_WIDTH,
                        BACK_Y2 / FONT_TEX_HEIGHT, BACK_X2 / FONT_TEX_WIDTH, BACK_Y2 / FONT_TEX_HEIGHT };
                mBack1->setTexCoords(texCoord);

                float scale = (game->getScreen()->width / static_cast<float>(7 * FONT_WIDTH)) * SCREEN_SCALE_RATIO;
                short fontW = static_cast<short>(FONT_WIDTH * scale);
                short fontH = static_cast<short>(FONT_HEIGHT * scale);

                mServerArea.left = (game->getScreen()->width >> 1) - ((fontW << 1) + (fontW >> 1));
                mServerArea.top = (game->getScreen()->height >> 1) + fontH;
                mServerArea.right = (game->getScreen()->width >> 1) - (fontW >> 1);
                mServerArea.bottom = (game->getScreen()->height >> 1) - fontH;
#ifndef __ANDROID__
                std::swap<short>(mServerArea.top, mServerArea.bottom);
#endif
                mBack1->setVertices(mServerArea.left, mServerArea.top, mServerArea.right, mServerArea.bottom);
            }
            else
                mBack1->resume(TEXTURE_ID_FONT);

            if (!mBackN) {

                mBackN = new Static2D;
                mBackN->initialize(game2DVia(game));
                mBackN->start(TEXTURE_ID_FONT);

                static const float texCoord[8] = { BACK_X2 / FONT_TEX_WIDTH, BACK_Y0 / FONT_TEX_HEIGHT,
                        BACK_X0 / FONT_TEX_WIDTH, BACK_Y0 / FONT_TEX_HEIGHT, BACK_X0 / FONT_TEX_WIDTH,
                        BACK_Y2 / FONT_TEX_HEIGHT, BACK_X2 / FONT_TEX_WIDTH, BACK_Y2 / FONT_TEX_HEIGHT };
                mBackN->setTexCoords(texCoord);

                float scale = (game->getScreen()->width / static_cast<float>(7 * FONT_WIDTH)) * SCREEN_SCALE_RATIO;
                short fontW = static_cast<short>(FONT_WIDTH * scale);
                short fontH = static_cast<short>(FONT_HEIGHT * scale);

                mClientArea.left = (game->getScreen()->width >> 1) + (fontW >> 1);
                mClientArea.top = (game->getScreen()->height >> 1) + fontH;
                mClientArea.right = mClientArea.left + (fontW << 1);
                mClientArea.bottom = (game->getScreen()->height >> 1) - fontH;
#ifndef __ANDROID__
                std::swap<short>(mClientArea.top, mClientArea.bottom);
#endif
                mBackN->setVertices(mClientArea.left, mClientArea.top, mClientArea.right, mClientArea.bottom);
            }
            else
                mBackN->resume(TEXTURE_ID_FONT);

            if (!mMark) {

                mMark = new Static2D;
                mMark->initialize(game2DVia(game));
                mMark->start(TEXTURE_ID_PANEL);

                static const float texCoord[8] = { SCREEN_X0 / PANEL_TEX_WIDTH, SCREEN_Y0 / PANEL_TEX_HEIGHT,
                        SCREEN_X0 / PANEL_TEX_WIDTH, SCREEN_Y2 / PANEL_TEX_HEIGHT, SCREEN_X2 / PANEL_TEX_WIDTH,
                        SCREEN_Y2 / PANEL_TEX_HEIGHT, SCREEN_X2 / PANEL_TEX_WIDTH, SCREEN_Y0 / PANEL_TEX_HEIGHT };
                mMark->setTexCoords(texCoord);

                short screenW = (game->getScreen()->width >> 1) * SCREEN_SCALE_RATIO; // Half
                short screenH = screenW * CAM_HEIGHT / CAM_WIDTH;
                if (screenH > (game->getScreen()->height >> 1)) {

                    screenH = game->getScreen()->height >> 1;
                    screenW = screenH * CAM_WIDTH / CAM_HEIGHT;
                }
                mMark->setVertices((game->getScreen()->width >> 1) - screenW, (game->getScreen()->height >> 1) + screenH,
                        (game->getScreen()->width >> 1) + screenW, (game->getScreen()->height >> 1) - screenH);
            }
            else
                mMark->resume(TEXTURE_ID_PANEL);

#if !defined(PAID_VERSION) && !defined(DEMO_VERSION)
            // Advertising
            adDisplay(false);
#endif
            break;
        }
        case 6: {

            if (!mFrame) {

                mFrame = new Frame2D;
                mFrame->start(game2DVia(game));
                mFrame->hide();
            }
            else
                mFrame->resume();

            if (!mCount) {

                mCount = new Count2D;
                mCount->start(game2DVia(game), mFilm->getBottom());
                mCount->hide();
            }
            else
                mCount->resume();

            if (!mWait) {

                mWait = new Element2D;
                mWait->initialize(game2DVia(game));
                mWait->start(TEXTURE_ID_PANEL);

                static const float texCoord[8] = { WAIT_X0 / PANEL_TEX_WIDTH, WAIT_Y0 / PANEL_TEX_HEIGHT,
                        WAIT_X0 / PANEL_TEX_WIDTH, WAIT_Y2 / PANEL_TEX_HEIGHT, WAIT_X2 / PANEL_TEX_WIDTH,
                        WAIT_Y2 / PANEL_TEX_HEIGHT, WAIT_X2 / PANEL_TEX_WIDTH, WAIT_Y0 / PANEL_TEX_HEIGHT };
                mWait->setTexCoords(texCoord);
                mWait->setVertices((game->getScreen()->width >> 1) - (WAIT_SIZE >> 1), (game->getScreen()->height >> 1) +
                        (WAIT_SIZE >> 1), (game->getScreen()->width >> 1) + (WAIT_SIZE >> 1), (game->getScreen()->height >> 1) -
                        (WAIT_SIZE >> 1));

                // Initial position & scale as server expect
                positionWait(game, true);
            }
            else
                mWait->resume(TEXTURE_ID_PANEL);

            if (!mOrientation) {

                mOrientation = new Element2D;
                mOrientation->initialize(game2DVia(game));
                mOrientation->start(TEXTURE_ID_PANEL);

                mOrientation->setTexCoords(g_portTexCoord);
                mOrientation->setVertices((game->getScreen()->width >> 1) - (LAND_SIZE >> 1), (game->getScreen()->height >> 1) +
                        (LAND_SIZE >> 1), (game->getScreen()->width >> 1) + (LAND_SIZE >> 1), (game->getScreen()->height >> 1) -
                        (LAND_SIZE >> 1));

                // Position & Scale as server expect
                float scale = game->getScreen()->width * CMD_SCALE_RATIO / LAND_SIZE;
                mOrientation->scale(scale, scale);

                float transX = 6.f * game->getScreen()->right / 7.f; // According SCREEN_SCALE_RATIO definition
                float transY = mFilm->getTop() - (LAND_SIZE * scale / game->getScreen()->height);
                mOrientation->translate(transX, transY);

                short size = static_cast<short>(CMD_SCALE_RATIO * game->getScreen()->width);
                mOrientationArea.left = static_cast<short>((6.5f / 7.f) * game->getScreen()->width) - (size >> 1);
                // According SCREEN_SCALE_RATIO definition

                short screenH = static_cast<short>((game->getScreen()->width >> 1) * SCREEN_SCALE_RATIO *
                        CAM_HEIGHT / CAM_WIDTH);
                if (screenH > (game->getScreen()->height >> 1))
                    screenH = game->getScreen()->height >> 1;

                mOrientationArea.top = (game->getScreen()->height >> 1) + screenH;
                mOrientationArea.right = mOrientationArea.left + size;
                mOrientationArea.bottom = mOrientationArea.top - size;
#ifndef __ANDROID__
                mOrientationArea.top = game->getScreen()->height - mOrientationArea.top;
                mOrientationArea.bottom = game->getScreen()->height - mOrientationArea.bottom;
#endif
            }
            else
                mOrientation->resume(TEXTURE_ID_PANEL);

#if !defined(PAID_VERSION) && !defined(DEMO_VERSION)
            // Advertising
            adDisplay(false);
#endif
            break;
        }
        case MAX_LOAD_STEP: {

            if (!mPress) {

                mPress = new Element2D;
                mPress->initialize(game2DVia(game));
                mPress->start(TEXTURE_ID_PANEL);

                static const float texCoord[8] = { PRESS_X0 / PANEL_TEX_WIDTH, PRESS_Y0 / PANEL_TEX_HEIGHT,
                        PRESS_X0 / PANEL_TEX_WIDTH, PRESS_Y2 / PANEL_TEX_HEIGHT, PRESS_X2 / PANEL_TEX_WIDTH,
                        PRESS_Y2 / PANEL_TEX_HEIGHT, PRESS_X2 / PANEL_TEX_WIDTH, PRESS_Y0 / PANEL_TEX_HEIGHT };
                mPress->setTexCoords(texCoord);
                mPress->setVertices((game->getScreen()->width >> 1) - (PRESS_SIZE >> 1), (game->getScreen()->height >> 1) +
                        (PRESS_SIZE >> 1), (game->getScreen()->width >> 1) + (PRESS_SIZE >> 1), (game->getScreen()->height >> 1) -
                        (PRESS_SIZE >> 1));

                // Position & Scale as server expect (same as 'mWait')
                mPress->scale(mWait->getTransform()[Dynamic2D::SCALE_X], mWait->getTransform()[Dynamic2D::SCALE_Y]);
#ifdef LIBENG_PORT_AS_LAND
                mPress->translate(-mWait->getTransform()[Dynamic2D::TRANS_X], 0.f);
#else // iOS
                mPress->translate(mWait->getTransform()[Dynamic2D::TRANS_X], 0.f);
#endif
                short size = static_cast<short>(CMD_SCALE_RATIO * game->getScreen()->width);
                mPressArea.left = mOrientationArea.left;
                mPressArea.top = (game->getScreen()->height >> 1) + (size >> 1);
                mPressArea.right = mOrientationArea.right;
                mPressArea.bottom = mPressArea.top - size;
#ifndef __ANDROID__
                std::swap<short>(mPressArea.top, mPressArea.bottom);
#endif
            }
            else
                mPress->resume(TEXTURE_ID_PANEL);

            if (!mPlay) {

                mPlay = new Element2D;
                mPlay->initialize(game2DVia(game));
                mPlay->start(TEXTURE_ID_PANEL);

                static const float texCoord[8] = { PLAY_X0 / PANEL_TEX_WIDTH, PLAY_Y0 / PANEL_TEX_HEIGHT,
                        PLAY_X0 / PANEL_TEX_WIDTH, PLAY_Y2 / PANEL_TEX_HEIGHT, PLAY_X2 / PANEL_TEX_WIDTH,
                        PLAY_Y2 / PANEL_TEX_HEIGHT, PLAY_X2 / PANEL_TEX_WIDTH, PLAY_Y0 / PANEL_TEX_HEIGHT };
                mPlay->setTexCoords(texCoord);
                mPlay->setVertices((game->getScreen()->width >> 1) - (PLAY_SIZE >> 1), (game->getScreen()->height >> 1) +
                        (PLAY_SIZE >> 1), (game->getScreen()->width >> 1) + (PLAY_SIZE >> 1),
                        (game->getScreen()->height >> 1) - (PLAY_SIZE >> 1));

                float scale = getScreenScale(game);
                mPlay->scale(scale, scale);
            }
            else
                mPlay->resume(TEXTURE_ID_PANEL);

            if (!mShare) {

                mShare = new Share;
                mShare->start(game, mFilm->getTop());
            }
            else
                mShare->resume();

#if !defined(PAID_VERSION) && !defined(DEMO_VERSION)
            // Advertising
            adDisplay(false);
#endif
            break;
        }
    }
    if (mLoadStep != MAX_LOAD_STEP)
        return false;

    delete mHideCam;
    mHideCam = NULL;

    return true;
}
void MatrixLevel::renderLoad() const {

    assert(mHideCam);
    mHideCam->render(false);

#ifdef __ANDROID__
    if (mCamera->getCamTexIdx() == TEXTURE_IDX_INVALID)
        mCamera->interstitialAdCamResume();
    // BUG: When displaying the the Google+ connection window a lock/unlock screen operation is done except that no change
    //      on surface occurs (no 'EngSurface::surfaceChanged' java method call)
#endif

    if ((mLogo) && (mLogo->getTextureIdx() != TEXTURE_IDX_INVALID))
        mLogo->render(false);
}

void MatrixLevel::positionWait(const Game* game, bool server) {

    static bool client = true;
    if (server) {

        if (!client)
            return; // Already positionned as server expected
        client = false;

        float scale = game->getScreen()->width * CMD_SCALE_RATIO / WAIT_SIZE;
        mWait->scale(scale, scale);

        float transX = 6.f * game->getScreen()->right / 7.f; // According SCREEN_SCALE_RATIO definition
        mWait->translate(transX, 0.f);
    }
    else if (!client) { // Position it as client expected
        client = true;

        mWait->reset();
        float scale = getScreenScale(game);
        mWait->scale(scale, scale);
    }
}

#if !defined(PAID_VERSION) && !defined(DEMO_VERSION)
void MatrixLevel::adDisplay(bool delay) {

    if (!mAdLoaded) {

        static unsigned char counter = 0;
        if ((!delay) || (++counter == (DISPLAY_DELAY << 1))) { // Avoid to call 'Advertising::getStatus' continually

            LOGI(LOG_LEVEL_MATRIXLEVEL, 0, LOG_FORMAT(" - Check to load/display advertising"), __PRETTY_FUNCTION__, __LINE__);
            switch (Advertising::getStatus()) {
                case Advertising::STATUS_FAILED: {

                    mAdvertising->load();
                    break;
                }
                case Advertising::STATUS_LOADED: {

                    mAdLoaded = true;
                    Advertising::display(0);
                    break;
                }
            }
            counter = 0;
        }
    }
}
#endif

void MatrixLevel::interrupt() {

    LOGW(LOG_FORMAT(" - Pause/Lockscreen during recording"), __PRETTY_FUNCTION__, __LINE__);
    //assert(mMovRecording);

    mMovRecording = false;
    if (mMicRecording == REC_MIC_STARTED)
        Mic::stopRecorder();
#if !defined(PAID_VERSION) && !defined(DEMO_VERSION)
    mAdvertising->display(0);
#endif
    mMicRecording = REC_MIC_NONE;
    mVideo->clear();
    mConnexion->close();
    mCount->setCount(game2DVia(mApp), mFilm->getBottom(), 1, mLandscape);
    mStatus = MCAM_WAIT;
    done();
}
void MatrixLevel::restart(const Game* game) {

    LOGV(LOG_LEVEL_MATRIXLEVEL, 0, LOG_FORMAT(" - g:%x"), __PRETTY_FUNCTION__, __LINE__, game);
    assert(mFrameNo != 1); // Client

#if !defined(PAID_VERSION) && !defined(DEMO_VERSION)
    mAdvertising->display(0);
#endif
    done();
    delete mConnexion;
    mConnexion = new Connexion(false, this, true);
    mSearch->start(true);
    mVideo->clear();
    positionWait(game, false);
    mStatus = MCAM_WAIT;
}
void MatrixLevel::refresh(const Game* game) {

    LOGV(LOG_LEVEL_MATRIXLEVEL, 0, LOG_FORMAT(" - g:%x (l:%s)"), __PRETTY_FUNCTION__, __LINE__, game,
            (mLandscape)? "true":"false");
    assert(mFrameNo != 1); // Client

    direct(mLandscape);
#ifdef __ANDROID__
    mPlay->rotate((mLandscape)? 0.f:(2.f * PI_F));
#else
    mPlay->rotate((mLandscape)? 0.f:(PI_F / 2.f));
#endif
    mShare->setOrientation(mLandscape);
    mFrame->setOrientation(game2DVia(game), mLandscape);
    mFrame->setFrameNo(game2DVia(game), mConnexion->getClientCount(), mLandscape);
    mStatus = MCAM_NONE;
}

void MatrixLevel::wait(float millis){

    boost::this_thread::sleep(boost::posix_time::microseconds(static_cast<unsigned long>(millis * 500)));
    if (mStatus < MCAM_READY)
        checkWait();
}
bool MatrixLevel::update(const Game* game) {

#ifdef DEBUG
    LOGV(LOG_LEVEL_MATRIXLEVEL, (*game->getLog() % 100), LOG_FORMAT(" - g:%x (s:%d)"), __PRETTY_FUNCTION__, __LINE__,
            game, mStatus);
#endif
#ifdef __ANDROID__
    // Restart player (needed for lock/unlock operation)
    mPlayer->resume();

    if (mCamera->getCamTexIdx() == TEXTURE_IDX_INVALID)
        mCamera->interstitialAdCamResume();
    // BUG: When displaying some social network login UI a lock/unlock screen operation is done except that no change
    //      on surface occurs (no 'EngSurface::surfaceChanged' java method call)
    // -> Resume the camera manually

#endif
    mCamera->updateTexture(mMovRecording);
    if ((mMovRecording) && (!mCamera->getCamBuffer()))
        return true;

#if !defined(PAID_VERSION) && !defined(DEMO_VERSION)
    // Advertising
    adDisplay(true);
#endif

    // Finish displaying app logo
    if (mLogo) {

        static unsigned char delay = 0;
        static unsigned char status = 0;
        switch (status) {
            case 0: { // Show background

                if (++delay == DISPLAY_DELAY) {

                    mBackCam->setAlpha(1.f);
                    mAlphaCam->setAlpha(0.6f);

                    delay = 0;
                    status = 1;
                }
                else {
                    mBackCam->setAlpha(delay / static_cast<float>(DISPLAY_DELAY));
                    mAlphaCam->setAlpha(mBackCam->getAlpha() * 0.6f);
                }
                break;
            }
            case 1: { // Delay

                if (++delay == (DISPLAY_DELAY << 1)) {
                    delay = DISPLAY_DELAY;
                    status = 2;
                }
                break;
            }
            case 2: { // Hide logo

                if (--delay == 0) {

                    assert(!mFrameNo);
                    delete mLogo;
                    mLogo = NULL;
                }
                else
                    mLogo->setAlpha(delay / static_cast<float>(DISPLAY_DELAY));
                break;
            }
        }
    }

    // Process...
    switch (mStatus) {

        case MCAM_NONE:
        case MCAM_GO: {

            checkWait();
            if (!mFrameNo)
                break;

            if (mFrameNo != 1) { // Client...
                if (mConnexion->getStatus() > Connexion::CONN_GO) { // ...after CONN_GO

#if !defined(PAID_VERSION) && !defined(DEMO_VERSION)
                    mAdvertising->display(0);
#endif
                    done();
                    positionWait(game, true); // Position wait symbol as server expect
                    mStatus = MCAM_WAIT;
                }
            }
            else if (mStatus == MCAM_GO) { // Server with MCAM_GO status

                if (!mMovRecording)
                    break;

                clock_t now = clock();
                if (((now - mRecCounter) / game->mTickPerSecond) > mRecBound) {

                    mRecCounter = now;
                    mRecBound = 1.f / MAX_VIDEO_FPS;
                    time_t elapsed = mVideo->getRecorder()->add(mCamera->getCamBuffer(), true);
                    if ((!elapsed) || ((elapsed - mRecElapsed) > RECORD_DURATION_BEFORE) || (mGO)) {

                        ////// Go!
                        mConnexion->go();

                        mRecBound = 0.f;
                        mRecCounter = clock();
                        mRecElapsed = time(NULL);
                        mCount->setCount(game2DVia(game), mFilm->getBottom(), RECORD_DURATION_AFTER, mLandscape);
                        if (mStatus == MCAM_GO) // Means still client(s) available (can be MCAM_WAIT for all client in pause/lockscreen)
                            mStatus = MCAM_DOWNLOAD;
                    }
                    else {

                        mCount->setCount(game2DVia(game), mFilm->getBottom(),
                                RECORD_DURATION_BEFORE - (elapsed - mRecElapsed), mLandscape);

                        if (mMicRecording == REC_MIC_STOPPED) {
#ifdef __ANDROID__
                            mMicRecording = REC_MIC_STARTED;
                            Mic::startRecorder();
#else
                            mMicRecording = REC_MIC_NONE;
                            if (Mic::startRecorder())
                                mMicRecording = REC_MIC_STARTED;
#endif
                        }
                    }
                }
            }
            break;
        }
        case MCAM_FRAMENO: {

            assert(mFrameNo != 1); // Client
            refresh(game);
            break;
        }
        case MCAM_READY:
        case MCAM_ORIENTATION: {

            if (mFrameNo != 1) { // Client
                if (mStatus == MCAM_READY) {

                    ready();
#if !defined(PAID_VERSION) && !defined(DEMO_VERSION)
                    mAdvertising->hide(0);
#endif
                }
                else // MCAM_ORIENTATION
                    refresh(game);
                break;
            }

            // Server
            if (mStatus == MCAM_ORIENTATION) {

                std::string cmd(CMD_ORIENTATION);
                cmd += (mLandscape)? '1':'0';
                if (mConnexion->launch(Connexion::CONN_ORIENTATION, cmd.c_str(), ORIENTATION_LEN))
                    mStatus = MCAM_WAIT; // All clients were ready to receive CMD_ORIENTATION command
            }
            else // MCAM_READY
            if (mConnexion->launch(Connexion::CONN_READY, CMD_READY, READY_LEN))
                mStatus = MCAM_WAIT; // All clients were ready to receive CMD_READY command

            //break; // MCAM_WAIT
        }
        case MCAM_TIMEOUT: {
            if (mStatus == MCAM_TIMEOUT) {

                if (1 == mFrameNo) { // Server
                    if (mConnexion->getStatus() == Connexion::CONN_WAIT)
                        mStatus = MCAM_WAIT;
                }
                else // Client
                    restart(game);
            }
            //break; // MCAM_WAIT
        }
        case MCAM_NO_DISPLAY: {
            if (mStatus == MCAM_NO_DISPLAY) {
                assert(1 == mFrameNo); // Server

                // Wait finish to upload video
                if (mConnexion->getStatus() != Connexion::CONN_UPLOAD) {

                    mCount->setCount(game2DVia(game), mFilm->getBottom(), 1, mLandscape);
                    mStatus = MCAM_WAIT;
                }
            }
            //break; // MCAM_WAIT
        }
        case MCAM_DISPLAY: {
            if (mStatus == MCAM_DISPLAY) {

                mShare->update(game, mVideo->getMovFolder(), mVideo->getFileName());
                if (!mShare->isRunning())
                    mVideo->update(game); // Update frame if playing (+ generate video texture if not already done)
                mPlay->setAlpha((mVideo->isPlaying())? 0.f:1.f);

                if (mFrameNo != 1) // Client
                    break; // No more wait

                assert(mConnexion);
                if (mConnexion->getStatus() != Connexion::CONN_UPLOAD)
                    break; // Stop displaying 'mWait' element (display 'mPress')
            }
            //break; // MCAM_WAIT
        }
        case MCAM_DOWNLOAD: {
            if (mStatus == MCAM_DOWNLOAD) {

                if ((1 == mFrameNo) && (mMovRecording)) { // Server (only)

                    clock_t now = clock();
                    if (((now - mRecCounter) / game->mTickPerSecond) > mRecBound) {

                        mRecCounter = now;
                        mRecBound = 1.f / MAX_VIDEO_FPS;
                        time_t elapsed = mVideo->getRecorder()->add(mCamera->getCamBuffer(), false);
                        if ((!elapsed) || ((elapsed - mRecElapsed) > RECORD_DURATION_AFTER)) {

                            if (mMicRecording == REC_MIC_STARTED)
                                Mic::stopRecorder();
                            mMicRecording = REC_MIC_NONE;
#ifndef PAID_VERSION
                            mVideo->getRecorder()->start(mFontBuffer, mLandscape);
#ifndef DEMO_VERSION
                            mAdvertising->display(0);
#endif
#else
                            mVideo->getRecorder()->start(mLandscape);
#endif
                            mMovRecording = false;
                            done(); // Finished
                        }
                        else
                            mCount->setCount(game2DVia(game), mFilm->getBottom(),
                                    RECORD_DURATION_AFTER - (elapsed - mRecElapsed), mLandscape);
                    }
                }
            }
            //break; // MCAM_WAIT
        }
        case MCAM_INTERRUPT: {
            if (mStatus == MCAM_INTERRUPT)
                interrupt();

            //break; // MCAM_WAIT
        }
        case MCAM_WAIT: {

#ifdef LIBENG_PORT_AS_LAND
            static float angle = PI_F / -2.f;
            static float round = (PI_F * -5.f) / 2.f;
#else
            static float angle = 0.f;
            static float round = PI_F * -2.f;
#endif
            angle += WAIT_ROTATE_VEL;
            if (angle < round)
#ifdef LIBENG_PORT_AS_LAND
                angle = PI_F / -2.f;
#else
                angle = 0.f;
#endif
            mWait->rotate(angle);

            assert(mConnexion);
            switch (mConnexion->getStatus()) {
                case Connexion::CONN_OPEN: {

                    static unsigned char delay = (DISPLAY_DELAY - 1);
                    if (++delay == DISPLAY_DELAY) {
                        mConnexion->open();
                        delay = 0;
                    }
                    break;
                }
                case Connexion::CONN_START: {

                    static int port = INITIAL_PORT_NO;

                    assert(1 == mFrameNo); // Server
                    if (!mConnexion->start(port)) {

                        if (++port > MAX_PORT_NO)
                            port = INITIAL_PORT_NO;
                    }
                    break;
                }
                case Connexion::CONN_CONNEXION: {

                    assert(mFrameNo != 1); // Client
                    mConnexion->connect(mSearch);
                    break;
                }
                case Connexion::CONN_WAIT: {

                    static unsigned char delay = 0;
                    if (++delay == DISPLAY_DELAY) {

                        delay = 0;
                        if (1 == mFrameNo) { // Server
                            if (!mConnexion->getClientCount()) {

                                mCount->setCount(game2DVia(game), mFilm->getBottom(), 1, mLandscape);
                                break;
                            }
                            LOGI(LOG_LEVEL_MATRIXLEVEL, 0, LOG_FORMAT(" - Client count: %d"), __PRETTY_FUNCTION__, __LINE__,
                                    mConnexion->getClientCount() + 1);
                            mCount->setCount(game2DVia(game), mFilm->getBottom(), mConnexion->getClientCount() + 1, mLandscape);
                        }
                        else // Client
                            positionWait(game, false);

                        mStatus = MCAM_NONE;
                        checkWait();
                    }
                    break;
                }
                case Connexion::CONN_GO: {

                    assert(1 == mFrameNo); // Server
                    ready();

#if !defined(PAID_VERSION) && !defined(DEMO_VERSION)
                    mAdvertising->hide(0);
#endif
                    mMicRecording = REC_MIC_STOPPED;
                    mMovRecording = true;

                    Picture::createPath(mVideo->getPicFolder());
#ifdef __ANDROID__
                    if (!Mic::initRecorder(mRecMicFile, OUTPUT_FORMAT_3GP)) {

                        mMicRecording = REC_MIC_NONE;
                        LOGW(LOG_FORMAT(" - Failed to initialize mic recorder"), __PRETTY_FUNCTION__, __LINE__);
                        assert(NULL);
                    }
#else
                    Mic::initRecorder(mRecMicFile, kAudioFormatMPEG4AAC, 44100.f, 1);
#endif
                    mRecBound = 0.f;
                    mRecCounter = clock();
                    mCount->setCount(game2DVia(game), mFilm->getBottom(), RECORD_DURATION_BEFORE, mLandscape);
                    mRecElapsed = time(NULL);
                    mVideo->getRecorder()->clear();
                    break;
                }
                case Connexion::CONN_DOWNLOAD: {

                    if (mMovRecording)
                        break; // Can be true for server only

                    if (1 == mFrameNo) // Server
                        mCount->setCount(game2DVia(game), mFilm->getBottom(), mVideo->getCount() - 1, mLandscape);
                        // -> Display the number of frame in the bullet time process

                    break;
                }
                case Connexion::CONN_TIMEOUT: {

                    if ((mFrameNo != 1) && (mStatus == MCAM_DOWNLOAD)) // Client B4 a CMD_UPLOAD command
                        restart(game);
                    break;
                }
            }
            break;
        }
    }

    // Touch...
    if ((mFrameNo > 1) && (mStatus != MCAM_DISPLAY))
        return true; // No need to manage touch for client (B4 displaying video)

    unsigned char touchCount = game->mTouchCount;
    while (touchCount--) {

        if (game->mTouchData[touchCount].Type == TouchInput::TOUCH_UP) {

            // Choose Server/Client (#1/#n)
            if ((!mFrameNo) && (!mLogo)) {

#ifdef LIBENG_PORT_AS_LAND
                if ((game->mTouchData[touchCount].Y > mServerArea.left) && (game->mTouchData[touchCount].Y < mServerArea.right) &&
                        (game->mTouchData[touchCount].X < mServerArea.top) && (game->mTouchData[touchCount].X > mServerArea.bottom)) {
#else
                if ((game->mTouchData[touchCount].X > mServerArea.left) && (game->mTouchData[touchCount].X < mServerArea.right) &&
                        (game->mTouchData[touchCount].Y > mServerArea.top) && (game->mTouchData[touchCount].Y < mServerArea.bottom)) {
#endif
                    LOGI(LOG_LEVEL_MATRIXLEVEL, 0, LOG_FORMAT(" - Server"), __PRETTY_FUNCTION__, __LINE__);
                    assert(!mConnexion);

                    float space = Storage::getFreeSpace(*mVideo->getPicFolder());
                    if (space < MIN_FREE_SPACE) {

                        LOGW(LOG_FORMAT(" - Unsufficient free space: %f < %d"), __PRETTY_FUNCTION__, __LINE__, space, MIN_FREE_SPACE);
#ifdef __ANDROID__
                        alertMessage(LOG_LEVEL_MATRIXLEVEL, UNSUFFICIENT_FREE_SPACE);
#else
                        alertMessage(LOG_LEVEL_MATRIXLEVEL, 3.0, UNSUFFICIENT_FREE_SPACE);
#endif
                        break;
                    }
#ifdef DEBUG
                    else {
                        LOGI(LOG_LEVEL_MATRIXLEVEL, 0, LOG_FORMAT(" - Free space available: %f"), __PRETTY_FUNCTION__, __LINE__, space);
                    }
#endif
                    mConnexion = new Connexion(true, this);
                    mFrameNo = 1;
                    mFrame->show();
                    mCount->show();
                    mStatus = MCAM_WAIT;
                    break;
                }
#ifdef LIBENG_PORT_AS_LAND
                else if ((game->mTouchData[touchCount].Y > mClientArea.left) && (game->mTouchData[touchCount].Y < mClientArea.right) &&
                        (game->mTouchData[touchCount].X < mClientArea.top) && (game->mTouchData[touchCount].X > mClientArea.bottom)) {
#else
                else if ((game->mTouchData[touchCount].X > mClientArea.left) && (game->mTouchData[touchCount].X < mClientArea.right) &&
                        (game->mTouchData[touchCount].Y > mClientArea.top) && (game->mTouchData[touchCount].Y < mClientArea.bottom)) {
#endif
                    LOGI(LOG_LEVEL_MATRIXLEVEL, 0, LOG_FORMAT(" - Client"), __PRETTY_FUNCTION__, __LINE__);
                    assert(!mConnexion);

                    mConnexion = new Connexion(false, this);
                    mFrameNo = 2;
                    mFrame->show();
                    mOrientation->resetY();
                    positionWait(game, false);

                    mStatus = MCAM_WAIT;
                    break;
                }
                continue;
            }

            if ((1 == mFrameNo) && (mStatus != MCAM_DISPLAY)) { ////// Server (B4 displaying video)

                if ((MCAM_ORIENTATION == mStatus) || (MCAM_WAIT == mStatus))
                    continue;

                // Ready/Go ?
#ifdef LIBENG_PORT_AS_LAND
                if ((game->mTouchData[touchCount].Y > mPressArea.left) && (game->mTouchData[touchCount].Y < mPressArea.right) &&
                        (game->mTouchData[touchCount].X < mPressArea.top) && (game->mTouchData[touchCount].X > mPressArea.bottom)) {
#else
                if ((game->mTouchData[touchCount].X > mPressArea.left) && (game->mTouchData[touchCount].X < mPressArea.right) &&
                        (game->mTouchData[touchCount].Y > mPressArea.top) && (game->mTouchData[touchCount].Y < mPressArea.bottom)) {
#endif
                    LOGI(LOG_LEVEL_MATRIXLEVEL, 0, LOG_FORMAT(" - Ready/Go"), __PRETTY_FUNCTION__, __LINE__);
                    if (MCAM_NONE == mStatus) {

                        mGO = false;
                        mVideo->clear();
                        mStatus = MCAM_READY; // Ready
                    }
                    else
                        mGO = true; // Go!

                    break;
                }

                // Orientation ?
#ifdef LIBENG_PORT_AS_LAND
                if ((game->mTouchData[touchCount].Y > mOrientationArea.left) && (game->mTouchData[touchCount].Y < mOrientationArea.right) &&
                        (game->mTouchData[touchCount].X < mOrientationArea.top) && (game->mTouchData[touchCount].X > mOrientationArea.bottom) &&
                        (mStatus == MCAM_NONE)) {
#else
                if ((game->mTouchData[touchCount].X > mOrientationArea.left) && (game->mTouchData[touchCount].X < mOrientationArea.right) &&
                        (game->mTouchData[touchCount].Y > mOrientationArea.top) && (game->mTouchData[touchCount].Y < mOrientationArea.bottom) &&
                        (mStatus == MCAM_NONE)) {
#endif
                    LOGI(LOG_LEVEL_MATRIXLEVEL, 0, LOG_FORMAT(" - Change orientation"), __PRETTY_FUNCTION__, __LINE__);
                    direct(!mLandscape);
                    mFrame->setOrientation(game2DVia(game), mLandscape);
                    mCount->setOrientation(game2DVia(game), mFilm->getBottom(), mLandscape);
                    mShare->setOrientation(mLandscape);
#ifdef __ANDROID__
                    mPlay->rotate((mLandscape)? 0.f:(2.f * PI_F));
#else
                    mPlay->rotate((mLandscape)? 0.f:(PI_F / 2.f));
#endif
                    mStatus = MCAM_ORIENTATION;
                    break;
                }
            }

            // Both: Server/Client (displaying video only)
            if ((mStatus != MCAM_DISPLAY) || (mShare->isRunning()))
                continue;

            // Play video ?
#ifdef LIBENG_PORT_AS_LAND
            if (boost::math::tr1::hypotf((game->getScreen()->width >> 1) - game->mTouchData[touchCount].Y,
                    (game->getScreen()->height >> 1) - game->mTouchData[touchCount].X) < ((PLAY_SIZE >> 1) * getScreenScale(game))) {
#else
            if (hypotf((game->getScreen()->width >> 1) - game->mTouchData[touchCount].X,
                    (game->getScreen()->height >> 1) - game->mTouchData[touchCount].Y) < ((PLAY_SIZE >> 1) * getScreenScale(game))) {
#endif
                if (mVideo->isPlaying())
                    continue;

                LOGI(LOG_LEVEL_MATRIXLEVEL, 0, LOG_FORMAT(" - Play video"), __PRETTY_FUNCTION__, __LINE__);
                mVideo->play();
                break;
            }

            // Share ?
#ifdef LIBENG_PORT_AS_LAND
            if ((game->mTouchData[touchCount].Y > mOrientationArea.left) && (game->mTouchData[touchCount].Y < mOrientationArea.right) &&
                    (game->mTouchData[touchCount].X < mOrientationArea.top) && (game->mTouchData[touchCount].X > mOrientationArea.bottom)) {
#else
            if ((game->mTouchData[touchCount].X > mOrientationArea.left) && (game->mTouchData[touchCount].X < mOrientationArea.right) &&
                    (game->mTouchData[touchCount].Y > mOrientationArea.top) && (game->mTouchData[touchCount].Y < mOrientationArea.bottom)) {
#endif
                LOGI(LOG_LEVEL_MATRIXLEVEL, 0, LOG_FORMAT(" - Share"), __PRETTY_FUNCTION__, __LINE__);
                mShare->run();
                if (mVideo->isPlaying())
                    mVideo->mute();
                break;
            }

            // Back to common processus ?
#ifdef LIBENG_PORT_AS_LAND
            if ((game->mTouchData[touchCount].Y > mPressArea.left) && (game->mTouchData[touchCount].Y < mPressArea.right) &&
                    (game->mTouchData[touchCount].X < mPressArea.top) && (game->mTouchData[touchCount].X > mPressArea.bottom)) {
#else
            if ((game->mTouchData[touchCount].X > mPressArea.left) && (game->mTouchData[touchCount].X < mPressArea.right) &&
                    (game->mTouchData[touchCount].Y > mPressArea.top) && (game->mTouchData[touchCount].Y < mPressArea.bottom)) {
#endif
                LOGI(LOG_LEVEL_MATRIXLEVEL, 0, LOG_FORMAT(" - First step"), __PRETTY_FUNCTION__, __LINE__);
#ifdef __ANDROID__
                mVideo->free(1 == mFrameNo);
#else
                mVideo->free();
#endif
                if (mFrameNo != 1) { // Client

                    refresh(game);
                    positionWait(game, false);
                }
                else // Server
                    mCount->setCount(game2DVia(game), mFilm->getBottom(), 1, mLandscape);

                mStatus = MCAM_WAIT;
                break;
            }
        }
    }
    return true;
}
void MatrixLevel::renderUpdate() const {

    mBackCam->render(false);
    mAlphaCam->render(false);
    if (mLogo)
        mLogo->render(false);

    else if (!mFrameNo) {

        mBack1->render(false);
        mBackN->render(false);
        mChoice->render(false);
    }
    else switch (mStatus) {

        case MCAM_NONE:
        case MCAM_ORIENTATION:
        case MCAM_FRAMENO: {

            if ((mFrameNo != 1) || (MCAM_ORIENTATION != mStatus)) {

                mFilm->render(false);
                mMark->render(false);
                mOrientation->render(true);

                mFrame->render();
                if (1 == mFrameNo) { // Server

                    mPress->render(true);
                    mCount->render();
                }
                break;
            }
            //break; // MCAM_ORIENTATION status for server
        }
        case MCAM_WAIT:
        case MCAM_TIMEOUT:
        case MCAM_READY:
        case MCAM_INTERRUPT:
        case MCAM_DOWNLOAD:
        case MCAM_NO_DISPLAY: {

            mFilm->render(false);
            mMark->render(false);

            if (1 == mFrameNo) { // Server

                if ((mStatus != MCAM_DOWNLOAD) && (mStatus != MCAM_NO_DISPLAY))
                    mFrame->render();
                mCount->render();
                if (!mMovRecording)
                    mWait->render(true);
            }
            else // Client
                mWait->render(true);

            break;
        }
        case MCAM_GO: {

            mFilm->render(false);
            mMark->render(false);
            if (1 == mFrameNo) { // Server

                mPress->render(true);
                mCount->render();
            }
            break;
        }
        case MCAM_DISPLAY: {

            if (!mShare->isRunning()) {

                mVideo->render();
                mPlay->render(true);
                if (1 == mFrameNo) { // Server

                    if (mConnexion->getStatus() == Connexion::CONN_UPLOAD)
                        mWait->render(true);
                    else
                        mPress->render(true);
                }
                else // Client
                    mPress->render(true);
            }
            mShare->render();
            break;
        }
    }
}
