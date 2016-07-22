#ifndef SHARE_H_
#define SHARE_H_

#include "Global.h"

#include <libeng/Graphic/Object/2D/Element2D.h>
#include <libeng/Graphic/Text/2D/Text2D.h>
#include <libeng/Tools/Tools.h>

#include <libeng/Social/Social.h>
#include <libeng/Social/Network.h>

#include <string>
#include <map>
#include <assert.h>

#define CMD_SCALE_RATIO         ((1.f / 7.f) * 0.8f)

#define WEBM_MIME_TYPE          "video/webm"
#define MOV_MIME_TYPE           "video/quicktime"
#define VIDEO_TITLE             "Matrix CAMERA - Video: "

// Texture IDs
#define TEXTURE_ID_PANEL        3

using namespace eng;

//////
class Share {

private:
#ifdef DEBUG
    const unsigned int* mLog;
#endif
    Social* mSocial;
    Network::ID mNetworkID;
    bool mResumed;

    // External profile URL
    std::string mFacebookURL;
    std::string mTwitterURL;
    std::string mGoogleURL;

    std::map<Network::ID, bool> mDispPic;

    void setNetwork();
    void setPicture();
    bool select();
    void displayInfo(const Game* game);

    enum {

        PANEL_DISPLAYED = 0,
        NETWORKS_DISPLAYING,
        NETWORKS_DISPLAYED,
        SHARE_DISPLAYING,
        SHARE_DISPLAYED
    };
    unsigned char mStatus;

    Element2D mShare;

    Static2D mFacebook;
    Static2D mYouTube;
    Static2D mGoogle;
    Static2D mTwitter;

    TouchArea mFacebookArea;
    TouchArea mYouTubeArea;
    TouchArea mGoogleArea;
    TouchArea mTwitterArea;

    Text2D mShareText;
    Text2D mLogoutText;
    Text2D mCloseText;
    Static2D mBackShare;
    Static2D mBackLogout;
    Static2D mBackClose;

    Static2D mPicture;
    Static2D mNetworkPic;
    Text2D mInfo;
    Text2D mGender;
    Text2D mBirthday;
    Text2D* mURL;

    TouchArea mShareArea;
    TouchArea mLogoutArea; // Close (when displaying networks)
    TouchArea mCloseArea;

public:
    Share();
    virtual ~Share();

    static std::string extractDate(const std::string &file);

    //
    inline bool isRunning() const { return (mStatus > PANEL_DISPLAYED); }

    inline void setOrientation(bool landscape) {
#ifdef __ANDROID__
        mShare.rotate((landscape)? 0.f:(2.f * PI_F));
#else
        mShare.rotate((landscape)? 0.f:(PI_F / 2.f));
#endif
    }
    inline void run() { mStatus = NETWORKS_DISPLAYING; }

    //////
    void start(const Game* game, float top);
    inline void pause() {

        mShare.pause();

        mFacebook.pause();
        mYouTube.pause();
        mGoogle.pause();
        mTwitter.pause();

        mBackShare.pause();
        mBackLogout.pause();
        mBackClose.pause();
        mShareText.pause();
        mCloseText.pause();
        mLogoutText.pause();

        mPicture.pause();
        mNetworkPic.pause();
        mInfo.pause();
        mGender.pause();
        mBirthday.pause();
        if (mURL)
            mURL->pause();
    }
    void resume();

    void update(const Game* game, const std::string* folder, const std::string* file);
    void render() const;

};

#endif // SHARE_H_
