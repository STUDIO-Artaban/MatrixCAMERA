#include "BulletTime.h"
#include "Global.h"

#include <libeng/Log/Log.h>
#include <libeng/Storage/Storage.h>
#include <libeng/Features/Internet/Internet.h>
#if !defined(PAID_VERSION) && !defined(DEMO_VERSION)
#include <libeng/Advertising/Advertising.h>
#endif

#include <boost/thread.hpp>


//////
#ifdef DEBUG
BulletTime::BulletTime() : Game2D(1) {
#else
BulletTime::BulletTime() : Game2D(0) {
#endif
    LOGV(LOG_LEVEL_BULLETTIME, 0, LOG_FORMAT(), __PRETTY_FUNCTION__, __LINE__);
    if (!mGameLevel)
#if !defined(PAID_VERSION) && !defined(DEMO_VERSION)
        mGameIntro = new Intro(Intro::LANG_EN, true);
#else
        mGameIntro = new Intro(Intro::LANG_EN, false);
#endif

    mFonts->addFont(0, FONT_WIDTH, FONT_HEIGHT, static_cast<short>(FONT_TEX_WIDTH), static_cast<short>(FONT_TEX_HEIGHT));
    
    mLevel = NULL;
    mWaitNet = NULL;
}
BulletTime::~BulletTime() { LOGV(LOG_LEVEL_BULLETTIME, 0, LOG_FORMAT(), __PRETTY_FUNCTION__, __LINE__); }

bool BulletTime::start() {

    LOGV(LOG_LEVEL_BULLETTIME, 0, LOG_FORMAT(), __PRETTY_FUNCTION__, __LINE__);
    if (Game2D::start())
        return true;

    else if (mGameLevel) {

        if (!(mGameLevel & 0x01))
            --mGameLevel;

#if !defined(PAID_VERSION) && !defined(DEMO_VERSION)
        if (Advertising::getStatus() == Advertising::STATUS_DISPLAYED)
            Advertising::hide(0);

        // Check if still online under a Wifi connection
#ifdef __ANDROID__
        if ((Internet::isConnected() != Internet::CONNECTION_WIFI) || (!Internet::isOnline(2000))) {
#else
        if ((Internet::isConnected() != Internet::CONNECTION_WIFI) || (!Internet::isOnline())) {
#endif
#else // PAID_VERSION | DEMO_VERSION

        // Check if still connected to a Wifi
        if (Internet::isConnected() != Internet::CONNECTION_WIFI) {
#endif
            assert(!mWaitNet);
            mGameLevel = 0;

            mWaitNet = new WaitConn(true);
            mWaitNet->initialize(this);
            mWaitNet->start(mScreen);
        }
    }
    else if (mWaitNet) {

#if !defined(PAID_VERSION) && !defined(DEMO_VERSION)
#ifdef __ANDROID__
        // Check if still not online under a Wifi connection
        if ((Internet::isConnected() != Internet::CONNECTION_WIFI) || (!Internet::isOnline(2000)))
#else
        if ((Internet::isConnected() != Internet::CONNECTION_WIFI) || (!Internet::isOnline()))
#endif
#else
        // Check if still connected to a Wifi
        if (Internet::isConnected() != Internet::CONNECTION_WIFI)
#endif
            mWaitNet->resume();

        else {

            delete mWaitNet;
            mWaitNet = NULL;

            ++mGameLevel;
        }
    }
    return false;
}
void BulletTime::pause() {

    LOGV(LOG_LEVEL_BULLETTIME, 0, LOG_FORMAT(), __PRETTY_FUNCTION__, __LINE__);
    Game2D::pause();

    if (mLevel) mLevel->pause();
    if (mWaitNet) mWaitNet->pause();
}
#ifdef __ANDROID__
void BulletTime::lockScreen() {

    LOGV(LOG_LEVEL_BULLETTIME, 0, LOG_FORMAT(), __PRETTY_FUNCTION__, __LINE__);
    Game::lockScreen();

    if (mLevel) mLevel->lockScreen();
}
#endif
void BulletTime::destroy() {

    LOGV(LOG_LEVEL_BULLETTIME, 0, LOG_FORMAT(), __PRETTY_FUNCTION__, __LINE__);
    Game2D::destroy();

    if (mWaitNet) delete mWaitNet;
    if (mLevel) delete mLevel;
}

void BulletTime::wait(float millis) {

#ifdef DEBUG
    LOGV(LOG_LEVEL_BULLETTIME, (mLog % 100), LOG_FORMAT(" - Delay: %f milliseconds"), __PRETTY_FUNCTION__, __LINE__, millis);
#endif
    if (mLevel)
        mLevel->wait(millis);
    else
        boost::this_thread::sleep(boost::posix_time::microseconds(static_cast<unsigned long>(millis * 500)));
}
void BulletTime::update() {

#ifdef DEBUG
    LOGV(LOG_LEVEL_BULLETTIME, (mLog % 100), LOG_FORMAT(" - (g:%d)"), __PRETTY_FUNCTION__, __LINE__, mGameLevel);
#endif
    switch (mGameLevel) {
        case 0: {

            if (!mWaitNet) // Introduction
                updateIntro();

            else { // Wait Internet | Wifi

                static unsigned char wait = 0;
                if (wait < (LIBENG_WAITCONN_DELAY << 2)) {

                    ++wait;
                    break;
                }
                mWaitNet->update();
            }
            break;
        }
        case 1: {

            if (!mLevel) {

                if (Internet::isConnected() != Internet::CONNECTION_WIFI) { // Check Wifi connection

                    LOGW(LOG_FORMAT(" - No Wifi connection"), __PRETTY_FUNCTION__, __LINE__);
                    mGameLevel = 0;

                    mWaitNet = new WaitConn(true);
                    mWaitNet->initialize(this);
                    mWaitNet->start(mScreen);
                    break;
                }
                mLevel = new MatrixLevel(this);
                mLevel->initialize();
            }
            if (!mLevel->load(this))
                break;

            ++mGameLevel;
            //break;
        }
        case 2: {

            mLevel->update(this);
            break;
        }

#ifdef DEBUG
        default: {

            LOGW(LOG_FORMAT(" - Unknown game level to update: %d"), __PRETTY_FUNCTION__, __LINE__, mGameLevel);
            assert(NULL);
            break;
        }
#endif
    }
}

void BulletTime::render() const {
    switch (mGameLevel) {

        case 0: {

            if (!mWaitNet)
                renderIntro(); // Introduction
            else
                mWaitNet->render(); // Wait Internet
            break;
        }
        case 1: mLevel->renderLoad(); break;
        case 2: mLevel->renderUpdate(); break;

#ifdef DEBUG
        default: {

            LOGW(LOG_FORMAT(" - Unknown game level to render: %d"), __PRETTY_FUNCTION__, __LINE__, mGameLevel);
            assert(NULL);
            break;
        }
#endif
    }
}
