#include "Share.h"
#include "Global.h"

#include <libeng/Game/2D/Game2D.h>
#include <libeng/Social/Session.h>
#include <libeng/Features/Camera/Camera.h>

#ifdef __ANDROID__
#include "Level/PanelCoords.h"
#include "Level/MatrixLevel.h"
#else
#include "OpenGLES/ES2/gl.h"
#include "PanelCoords.h"
#include "MatrixLevel.h"
#endif

#include <string>
#include <time.h>

#ifndef __ANDROID__
#define GOOGLE_REQ_TIMEOUT      (7 * 60) // 7 seconds
#define VIDEO_NAME              "Matrix CAMERA - Video"
#define VIDEO_DESCRIPTION       "Created on "
#endif
#define TWEET_MESSAGE           "Here is a new #MatrixCAMERA video - Created on "

#define EMPTY_BIRTHDAY          L",,, ,," // Will be replace by "___ __" characters (see font texture)
#define FONT_SCALE_RATIO        (1.8f * 680.f)
#define TEXT_Y_LAG_POS          -0.08f
#define TEXT_X_LAG_POS          0.05f

static const wchar_t* g_Month[12] = { L"Jan", L"Feb", L"Mar", L"Apr", L"May", L"Jun", L"Jul", L"Aug", L"Sep",
        L"Oct", L"Nov", L"Dec" };

//////
Share::Share() : mStatus(PANEL_DISPLAYED), mFacebook(false), mYouTube(false), mGoogle(false), mTwitter(false),
        mPicture(false), mNetworkPic(false), mNetworkID(Network::NONE), mURL(NULL), mResumed(false) {

#ifdef DEBUG
    LOGV(LOG_LEVEL_SHARE, 0, LOG_FORMAT(), __PRETTY_FUNCTION__, __LINE__);
    mLog = NULL;
#endif
    std::memset(&mFacebookArea, 0, sizeof(TouchArea));
    std::memset(&mYouTubeArea, 0, sizeof(TouchArea));
    std::memset(&mGoogleArea, 0, sizeof(TouchArea));
    std::memset(&mTwitterArea, 0, sizeof(TouchArea));

    std::memset(&mShareArea, 0, sizeof(TouchArea));
    std::memset(&mLogoutArea, 0, sizeof(TouchArea));
    std::memset(&mCloseArea, 0, sizeof(TouchArea));

    mSocial = Social::getInstance(true);

    mDispPic[Network::FACEBOOK] = false;
    mDispPic[Network::TWITTER] = false;
    mDispPic[Network::GOOGLE] = false;
}
Share::~Share() {

    LOGV(LOG_LEVEL_SHARE, 0, LOG_FORMAT(), __PRETTY_FUNCTION__, __LINE__);
    Social::freeInstance();
    if (mURL)
        delete mURL;
}

void Share::start(const Game* game, float top) {

#ifdef DEBUG
    LOGV(LOG_LEVEL_SHARE, 0, LOG_FORMAT(" - g:%x; t:%f"), __PRETTY_FUNCTION__, __LINE__, game, top);
    mLog = game->getLog();
#endif
    mShare.initialize(game2DVia(game));
    mFacebook.initialize(game2DVia(game));
    mYouTube.initialize(game2DVia(game));
    mGoogle.initialize(game2DVia(game));
    mTwitter.initialize(game2DVia(game));
    mBackShare.initialize(game2DVia(game));
    mBackLogout.initialize(game2DVia(game));
    mBackClose.initialize(game2DVia(game));
    mLogoutText.initialize(game2DVia(game));
    mCloseText.initialize(game2DVia(game));
    mShareText.initialize(game2DVia(game));
    mPicture.initialize(game2DVia(game));
    mNetworkPic.initialize(game2DVia(game));
    mInfo.initialize(game2DVia(game));
    mGender.initialize(game2DVia(game));
    mBirthday.initialize(game2DVia(game));

    Textures::getInstance()->genTexture(Textures::getInstance()->loadTexture(Facebook::TEXTURE_ID));
    Textures::getInstance()->genTexture(Textures::getInstance()->loadTexture(Twitter::TEXTURE_ID));
    Textures::getInstance()->genTexture(Textures::getInstance()->loadTexture(Google::TEXTURE_ID));

    //
    mShare.start(TEXTURE_ID_PANEL);

    static const float texShare[8] = { SHARE_X0 / PANEL_TEX_WIDTH, SHARE_Y0 / PANEL_TEX_HEIGHT, SHARE_X0 / PANEL_TEX_WIDTH,
            SHARE_Y2 / PANEL_TEX_HEIGHT, SHARE_X2 / PANEL_TEX_WIDTH, SHARE_Y2 / PANEL_TEX_HEIGHT, SHARE_X2 / PANEL_TEX_WIDTH,
            SHARE_Y0 / PANEL_TEX_HEIGHT };
    mShare.setTexCoords(texShare);
    mShare.setVertices((game->getScreen()->width >> 1) - (SHARE_SIZE >> 1), (game->getScreen()->height >> 1) +
            (SHARE_SIZE >> 1), (game->getScreen()->width >> 1) + (SHARE_SIZE >> 1), (game->getScreen()->height >> 1) -
            (SHARE_SIZE >> 1));

    float scale = game->getScreen()->width * CMD_SCALE_RATIO / SHARE_SIZE;
    mShare.scale(scale, scale);

    float transX = 6.f * game->getScreen()->right / 7.f; // According SCREEN_SCALE_RATIO definition
    float transY = top - (SHARE_SIZE * scale / game->getScreen()->height);
    mShare.translate(transX, transY);

    //
    assert(Textures::getInstance()->getIndex(TEXTURE_ID_FONT) == 2);
    mFacebook.start(2);

    static const float texFacebook[8] = { FACEBOOK_X0 / FONT_TEX_WIDTH, FACEBOOK_Y0 / FONT_TEX_HEIGHT, FACEBOOK_X0 / FONT_TEX_WIDTH,
            FACEBOOK_Y2 / FONT_TEX_HEIGHT, FACEBOOK_X2 / FONT_TEX_WIDTH, FACEBOOK_Y2 / FONT_TEX_HEIGHT, FACEBOOK_X2 / FONT_TEX_WIDTH,
            FACEBOOK_Y0 / FONT_TEX_HEIGHT };
    mFacebook.setTexCoords(texFacebook);

    short panel = 7 * game->getScreen()->height / 10;
    short size = panel / 3;
    mFacebookArea.left = (game->getScreen()->width >> 1) - (panel >> 2) - (size >> 1);
    mFacebookArea.top = (game->getScreen()->height >> 1) + (panel >> 2) + (size >> 1);
    mFacebookArea.right = mFacebookArea.left + size;
    mFacebookArea.bottom = mFacebookArea.top - size;

    mFacebook.setVertices(mFacebookArea.left, mFacebookArea.top, mFacebookArea.right, mFacebookArea.bottom);

    //
    assert(Textures::getInstance()->getIndex(TEXTURE_ID_FONT) == 2);
    mYouTube.start(2);

    static const float texYouTube[8] = { YOUTUBE_X0 / FONT_TEX_WIDTH, YOUTUBE_Y0 / FONT_TEX_HEIGHT, YOUTUBE_X0 / FONT_TEX_WIDTH,
            YOUTUBE_Y2 / FONT_TEX_HEIGHT, YOUTUBE_X2 / FONT_TEX_WIDTH, YOUTUBE_Y2 / FONT_TEX_HEIGHT, YOUTUBE_X2 / FONT_TEX_WIDTH,
            YOUTUBE_Y0 / FONT_TEX_HEIGHT };
    mYouTube.setTexCoords(texYouTube);

    mYouTubeArea.left = (game->getScreen()->width >> 1) + (panel >> 2) - (size >> 1);
    mYouTubeArea.top = mFacebookArea.top;
    mYouTubeArea.right = mYouTubeArea.left + size;
    mYouTubeArea.bottom = mFacebookArea.bottom;

    mYouTube.setVertices(mYouTubeArea.left, mYouTubeArea.top, mYouTubeArea.right, mYouTubeArea.bottom);

    //
    assert(Textures::getInstance()->getIndex(TEXTURE_ID_FONT) == 2);
    mGoogle.start(2);

    static const float texGoogle[8] = { GOOGLE_X0 / FONT_TEX_WIDTH, GOOGLE_Y0 / FONT_TEX_HEIGHT, GOOGLE_X0 / FONT_TEX_WIDTH,
            GOOGLE_Y2 / FONT_TEX_HEIGHT, GOOGLE_X2 / FONT_TEX_WIDTH, GOOGLE_Y2 / FONT_TEX_HEIGHT, GOOGLE_X2 / FONT_TEX_WIDTH,
            GOOGLE_Y0 / FONT_TEX_HEIGHT };
    mGoogle.setTexCoords(texGoogle);

    mGoogleArea.left = mFacebookArea.left;
    mGoogleArea.top = (game->getScreen()->height >> 1) - (panel >> 2) + (size >> 1);
    mGoogleArea.right = mFacebookArea.right;
    mGoogleArea.bottom = mGoogleArea.top - size;

    mGoogle.setVertices(mGoogleArea.left, mGoogleArea.top, mGoogleArea.right, mGoogleArea.bottom);

    //
    assert(Textures::getInstance()->getIndex(TEXTURE_ID_FONT) == 2);
    mTwitter.start(2);

    static const float texTwitter[8] = { TWITTER_X0 / FONT_TEX_WIDTH, TWITTER_Y0 / FONT_TEX_HEIGHT, TWITTER_X0 / FONT_TEX_WIDTH,
            TWITTER_Y2 / FONT_TEX_HEIGHT, TWITTER_X2 / FONT_TEX_WIDTH, TWITTER_Y2 / FONT_TEX_HEIGHT, TWITTER_X2 / FONT_TEX_WIDTH,
            TWITTER_Y0 / FONT_TEX_HEIGHT };
    mTwitter.setTexCoords(texTwitter);

    mTwitterArea.left = mYouTubeArea.left;
    mTwitterArea.top = mGoogleArea.top;
    mTwitterArea.right = mYouTubeArea.right;
    mTwitterArea.bottom = mGoogleArea.bottom;

    mTwitter.setVertices(mTwitterArea.left, mTwitterArea.top, mTwitterArea.right, mTwitterArea.bottom);

    //
    mBackLogout.start(TEXTURE_ID_PANEL);

    static const float texPanel[8] = { PANEL_X0 / PANEL_TEX_WIDTH, PANEL_Y0 / PANEL_TEX_HEIGHT, PANEL_X0 / PANEL_TEX_WIDTH,
            PANEL_Y2 / PANEL_TEX_HEIGHT, PANEL_X2 / PANEL_TEX_WIDTH, PANEL_Y2 / PANEL_TEX_HEIGHT, PANEL_X2 / PANEL_TEX_WIDTH,
            PANEL_Y0 / PANEL_TEX_HEIGHT };
    mBackLogout.setTexCoords(texPanel);
    //mBackLogout.setRed(1.f);
    mBackLogout.setGreen(0.f);
    mBackLogout.setBlue(0.f);

    size = game->getScreen()->height / 3; // Width
    mLogoutArea.left = (game->getScreen()->width >> 1) - (size >> 1);
    mLogoutArea.top = (game->getScreen()->height - panel) >> 1;
    mLogoutArea.right = mLogoutArea.left + size;
    mLogoutArea.bottom = 0;

    mBackLogout.setVertices(mLogoutArea.left, mLogoutArea.top, mLogoutArea.right, mLogoutArea.bottom);

    //
    mCloseText.start(L"Close");
    mCloseText.setColor(0.f, 0.f, 0.f);

    float fontScale = game->getScreen()->width / FONT_SCALE_RATIO;
    mCloseText.scale(fontScale, fontScale);

    float yTrans = TEXT_Y_LAG_POS - panel / static_cast<float>(game->getScreen()->height);
    float xTrans = -5.f * FONT_WIDTH * fontScale / game->getScreen()->height;
    mCloseText.position(xTrans, yTrans);

    //
    mBackShare.start(TEXTURE_ID_PANEL);
    mBackShare.setTexCoords(texPanel);
    mBackShare.setRed(MCAM_RED_COLOR / MAX_COLOR);
    mBackShare.setGreen(MCAM_GREEN_COLOR / MAX_COLOR);
    mBackShare.setBlue(MCAM_BLUE_COLOR / MAX_COLOR);

    mShareArea.right = mLogoutArea.left;
    mShareArea.top = mLogoutArea.top;
    mShareArea.left = mShareArea.right - size;
    mShareArea.bottom = 0;

    mBackShare.setVertices(mShareArea.left, mShareArea.top, mShareArea.right, mShareArea.bottom);

    //
    mBackClose.start(TEXTURE_ID_PANEL);
    mBackClose.setTexCoords(texPanel);

    mCloseArea.left = mLogoutArea.right;
    mCloseArea.top = mLogoutArea.top;
    mCloseArea.right = mCloseArea.left + size;
    mCloseArea.bottom = 0;

    mBackClose.setVertices(mCloseArea.left, mCloseArea.top, mCloseArea.right, mCloseArea.bottom);

    //
    mShareText.start(L"Share");
    mShareText.setColor(0.f, 0.f, 0.f);
    mShareText.scale(fontScale, fontScale);
    mShareText.position(xTrans - (2.f / 3.f), yTrans);

    //
    // Initialize 'mPicture' using Facebook network as default
    mPicture.start(Textures::getInstance()->getIndex(Facebook::TEXTURE_ID));
    mPicture.setTexCoords(FULL_TEXCOORD_BUFFER);

    panel = (game->getScreen()->height - panel) >> 1;
    size = game->getScreen()->height + (panel << 1);
    mPicture.setVertices((game->getScreen()->width >> 1) - (game->getScreen()->height >> 1) - panel,
            game->getScreen()->height - panel, (game->getScreen()->width >> 1) - (game->getScreen()->height >> 1) -
            panel + (size >> 2), game->getScreen()->height - panel - (size >> 2));

    //
    assert(Textures::getInstance()->getIndex(TEXTURE_ID_FONT) == 2);
    mNetworkPic.start(2);
    mNetworkID = Network::FACEBOOK; // Using Facebook network as default
    setNetwork();
    mNetworkID = Network::NONE;

    mNetworkPic.setVertices((game->getScreen()->width >> 1) + (size >> 1) - (size >> 3), game->getScreen()->height -
            panel - static_cast<short>(8 * FONT_HEIGHT * fontScale), (game->getScreen()->width >> 1) + (size >> 1),
            game->getScreen()->height - panel - static_cast<short>(8 * FONT_HEIGHT * fontScale) - (size >> 3));

    //
    mLogoutText.start(L"Logout");
    mLogoutText.scale(fontScale, fontScale);
    mLogoutText.position(-6.f * FONT_WIDTH * fontScale / game->getScreen()->height, yTrans);

    //
    mInfo.start(L"Birthday:\nGender:\nExternal profil URL:");
    mInfo.setColor(1.f, 1.f, 0.f);
    mInfo.scale(fontScale, fontScale);

    yTrans = (((game->getScreen()->height >> 1) - panel) << 1) / static_cast<float>(game->getScreen()->height);
    xTrans = TEXT_X_LAG_POS - ((size >> 1) / static_cast<float>(game->getScreen()->height));
    mInfo.position(xTrans, yTrans);

    //
    mBirthday.start(EMPTY_BIRTHDAY);
    mBirthday.scale(fontScale, fontScale);
    mBirthday.position(xTrans + (9.f * (FONT_WIDTH << 1) * fontScale / game->getScreen()->height), yTrans);
    // -> 'Birthday:' contains 9 characters

    //
    mGender.start(L","); // Will be replace by '_' character (see font texture)
    mGender.scale(fontScale, fontScale);
    mGender.position(xTrans + (7.f * (FONT_WIDTH << 1) * fontScale / game->getScreen()->height), yTrans -
            ((FONT_HEIGHT << 1) * fontScale / game->getScreen()->height));
    // -> 'Gender:' contains 7 characters

#ifndef __ANDROID__
    std::swap<short>(mFacebookArea.top, mFacebookArea.bottom);
    std::swap<short>(mGoogleArea.top, mGoogleArea.bottom);
    std::swap<short>(mYouTubeArea.top, mYouTubeArea.bottom);
    std::swap<short>(mTwitterArea.top, mTwitterArea.bottom);

    std::swap<short>(mFacebookArea.top, mGoogleArea.top);
    std::swap<short>(mFacebookArea.bottom, mGoogleArea.bottom);
    std::swap<short>(mYouTubeArea.top, mTwitterArea.top);
    std::swap<short>(mYouTubeArea.bottom, mTwitterArea.bottom);

    mLogoutArea.top = game->getScreen()->height - mLogoutArea.top;
    mLogoutArea.bottom = game->getScreen()->height;
    mShareArea.top = game->getScreen()->height - mShareArea.top;
    mShareArea.bottom = game->getScreen()->height - mShareArea.bottom;
    mCloseArea.top = game->getScreen()->height - mCloseArea.top;
    mCloseArea.bottom = game->getScreen()->height - mCloseArea.bottom;
#endif
}
void Share::resume() {

    LOGV(LOG_LEVEL_SHARE, 0, LOG_FORMAT(), __PRETTY_FUNCTION__, __LINE__);
    Textures::getInstance()->genTexture(Textures::getInstance()->loadTexture(Facebook::TEXTURE_ID));
    Textures::getInstance()->genTexture(Textures::getInstance()->loadTexture(Twitter::TEXTURE_ID));
    Textures::getInstance()->genTexture(Textures::getInstance()->loadTexture(Google::TEXTURE_ID));

    mShare.resume(TEXTURE_ID_PANEL);

    assert(Textures::getInstance()->getIndex(TEXTURE_ID_FONT) == 2);
    mFacebook.resume(2);
    mYouTube.resume(2);
    mGoogle.resume(2);
    mTwitter.resume(2);

    mShareText.resume();
    mCloseText.resume();
    mLogoutText.resume();
    mBackShare.resume(TEXTURE_ID_PANEL);
    mBackLogout.resume(TEXTURE_ID_PANEL);
    mBackClose.resume(TEXTURE_ID_PANEL);

    mInfo.resume();
    switch (mNetworkID) {

        case Network::NONE:
        case Network::FACEBOOK:
            mPicture.resume(Textures::getInstance()->getIndex(Facebook::TEXTURE_ID));
            break;

        case Network::TWITTER: mPicture.resume(Textures::getInstance()->getIndex(Twitter::TEXTURE_ID)); break;
        case Network::GOOGLE: mPicture.resume(Textures::getInstance()->getIndex(Google::TEXTURE_ID)); break;
    }
    mResumed = true;
    if ((mNetworkID != Network::NONE) && (mSocial->_getSession(mNetworkID)) &&
            (mSocial->_getSession(mNetworkID)->getUserPic()) && (mDispPic[mNetworkID]))
        setPicture();

    assert(Textures::getInstance()->getIndex(TEXTURE_ID_FONT) == 2);
    mNetworkPic.resume(2);
    mGender.resume();
    mBirthday.resume();
    if (mURL)
        mURL->resume();
}

void Share::setNetwork() {

    LOGV(LOG_LEVEL_SHARE, 0, LOG_FORMAT(), __PRETTY_FUNCTION__, __LINE__);
    assert(mNetworkID != Network::NONE);

#ifdef DEBUG
    mPicture.pause(); // Reset texture index to avoid assert when 'resume' method is called
#endif
    switch (mNetworkID) {

        case Network::FACEBOOK: {
            mPicture.resume(Textures::getInstance()->getIndex(Facebook::TEXTURE_ID));
            mPicture.setTexCoords(FULL_TEXCOORD_BUFFER);
            static const float texCoords[8] = { FACEBOOK_X0 / FONT_TEX_WIDTH,
                    FACEBOOK_Y0 / FONT_TEX_HEIGHT, FACEBOOK_X0 / FONT_TEX_WIDTH,
                    FACEBOOK_Y2 / FONT_TEX_HEIGHT, FACEBOOK_X2 / FONT_TEX_WIDTH,
                    FACEBOOK_Y2 / FONT_TEX_HEIGHT, FACEBOOK_X2 / FONT_TEX_WIDTH,
                    FACEBOOK_Y0 / FONT_TEX_HEIGHT };
            mNetworkPic.setTexCoords(texCoords);
            break;
        }
        case Network::TWITTER: {
            mPicture.resume(Textures::getInstance()->getIndex(Twitter::TEXTURE_ID));
            mPicture.setTexCoords(FULL_TEXCOORD_BUFFER);
            static const float texCoords[8] = { TWITTER_X0 / FONT_TEX_WIDTH,
                    TWITTER_Y0 / FONT_TEX_HEIGHT, TWITTER_X0 / FONT_TEX_WIDTH,
                    TWITTER_Y2 / FONT_TEX_HEIGHT, TWITTER_X2 / FONT_TEX_WIDTH,
                    TWITTER_Y2 / FONT_TEX_HEIGHT, TWITTER_X2 / FONT_TEX_WIDTH,
                    TWITTER_Y0 / FONT_TEX_HEIGHT };
            mNetworkPic.setTexCoords(texCoords);
            break;
        }
        case Network::GOOGLE: {
            mPicture.resume(Textures::getInstance()->getIndex(Google::TEXTURE_ID));
            mPicture.setTexCoords(FULL_TEXCOORD_BUFFER);
            static const float texCoords[8] = { GOOGLE_X0 / FONT_TEX_WIDTH,
                    GOOGLE_Y0 / FONT_TEX_HEIGHT, GOOGLE_X0 / FONT_TEX_WIDTH,
                    GOOGLE_Y2 / FONT_TEX_HEIGHT, GOOGLE_X2 / FONT_TEX_WIDTH,
                    GOOGLE_Y2 / FONT_TEX_HEIGHT, GOOGLE_X2 / FONT_TEX_WIDTH,
                    GOOGLE_Y0 / FONT_TEX_HEIGHT };
            mNetworkPic.setTexCoords(texCoords);
            break;
        }
    }
}
void Share::setPicture() {

    assert(mNetworkID != Network::NONE);

    Textures* textures = Textures::getInstance();
    unsigned char textureIdx = textures->addTexPic(mNetworkID);
    if (textureIdx != TEXTURE_IDX_INVALID) {

        float texCoords[8] = {0};
        texCoords[3] = static_cast<float>(mSocial->_getSession(mNetworkID)->getPicHeight()) /
                (*textures)[textureIdx]->height;
        texCoords[4] = static_cast<float>(mSocial->_getSession(mNetworkID)->getPicWidth()) /
                (*textures)[textureIdx]->width;
        texCoords[5] = texCoords[3];
        texCoords[6] = texCoords[4];
        mPicture.setTexCoords(texCoords);

        mDispPic[mNetworkID] = true;
    }
    //else // Picture buffer has changed (== NULL)
}
bool Share::select() {

    LOGV(LOG_LEVEL_SHARE, 0, LOG_FORMAT(), __PRETTY_FUNCTION__, __LINE__);
    if (!mSocial->_getSession(mNetworkID))
        mSocial->addSession(mNetworkID, false);

    assert(mSocial->_getSession(mNetworkID));
    if (!mSocial->_getSession(mNetworkID)->isOpened()) {

#ifdef __ANDROID__
        if (mNetworkID == Network::TWITTER)
            Camera::getInstance()->stop();
#endif
        return mSocial->request(mNetworkID, Session::REQUEST_LOGIN, NULL);
    }
    else {

        bool idEmpty = false;
        switch (mNetworkID) {

            case Network::FACEBOOK: idEmpty = mFacebookURL.empty(); break;
            case Network::TWITTER: idEmpty = mTwitterURL.empty(); break;
            case Network::GOOGLE: idEmpty = mGoogleURL.empty(); break;
        }
        if (idEmpty)
            return mSocial->request(mNetworkID, Session::REQUEST_INFO, NULL);

        mStatus = SHARE_DISPLAYING; // Display share dialog
    }
    return false;
}
void Share::displayInfo(const Game* game) {

    LOGV(LOG_LEVEL_SHARE, 0, LOG_FORMAT(" - g:%x (n:%d; s:%x; u:%x)"), __PRETTY_FUNCTION__, __LINE__, game, mNetworkID,
            mSocial->_getSession(mNetworkID), mURL);
    assert(mNetworkID != Network::NONE);
    assert(mSocial->_getSession(mNetworkID));
    assert(!mURL);

    // Gender
    switch (mSocial->_getSession(mNetworkID)->getUserGender()) {
        case GENDER_MALE: if (mGender.getText() != L"M") mGender.update(L"M"); break;
        case GENDER_FEMALE: if (mGender.getText() != L"F") mGender.update(L"F"); break;
    }
    mGender.setAlpha(1.f);

    // Birthday
    std::string userBirthday(mSocial->_getSession(mNetworkID)->getUserBirthday());
    if (userBirthday.length() > 9) { // MM/dd/yyyy

        std::wstring birthday(g_Month[strToNum<short>(userBirthday.substr(0, 2)) - 1]);
        birthday += L' ';
        birthday.append(numToWStr<unsigned char>(strToNum<short>(userBirthday.substr(3, 2))));

        if (mBirthday.getText() != birthday)
            mBirthday.update(birthday);
    }
    else if (mBirthday.getText() != EMPTY_BIRTHDAY)
        mBirthday.update(EMPTY_BIRTHDAY);
    mBirthday.setAlpha(1.f);

    // URL
    std::wstring url;
    switch (mNetworkID) {

        case Network::FACEBOOK: url.assign(mFacebookURL.begin(), mFacebookURL.end()); break;
        case Network::TWITTER: url.assign(mTwitterURL.begin(), mTwitterURL.end()); break;
        case Network::GOOGLE: url.assign(mGoogleURL.begin(), mGoogleURL.end()); break;
    }
    std::replace(url.begin(), url.end(), L'*', L'à');
    std::replace(url.begin(), url.end(), L'#', L'â');
    std::replace(url.begin(), url.end(), L'^', L'€');
    std::replace(url.begin(), url.end(), L'}', L'>');
    std::replace(url.begin(), url.end(), L']', L'}');
    std::replace(url.begin(), url.end(), L'{', L'<');
    std::replace(url.begin(), url.end(), L'[', L'{');
    std::replace(url.begin(), url.end(), L'~', L'\'');
    std::replace(url.begin(), url.end(), L'§', L'"');
    std::replace(url.begin(), url.end(), L'_', L',');

#ifdef DEBUG
    if (url.length() > 255) {
        LOGE(LOG_FORMAT(" - External profile URL too long"), __PRETTY_FUNCTION__, __LINE__);
        assert(NULL);
    }
#endif
    for (unsigned char i = (static_cast<unsigned char>(url.length()) / 20); i; --i)
        url.insert(20 * i, 1, L'\n'); // "External profil URL:" contains 20 letters
    if (url.at(url.size() - 1) == L'\n')
        url.resize(url.size() - 1);

    mURL = new Text2D;
    mURL->initialize(game2DVia(game));
    mURL->start(url);

    float scale = game->getScreen()->width / FONT_SCALE_RATIO;
    mURL->scale(scale, scale);

    short panel = (game->getScreen()->height - (7 * game->getScreen()->height / 10)) >> 1;
    short size = game->getScreen()->height + (panel << 1);
    mURL->position(TEXT_X_LAG_POS - ((size >> 1) / static_cast<float>(game->getScreen()->height)),
            (((game->getScreen()->height >> 1) - panel) << 1) / static_cast<float>(game->getScreen()->height) -
            (3.f * (FONT_HEIGHT << 1) * scale / game->getScreen()->height));
}

std::string Share::extractDate(const std::string &file) {

    LOGV(LOG_LEVEL_SHARE, 0, LOG_FORMAT(" - f:%s"), __PRETTY_FUNCTION__, __LINE__, file.c_str());
    assert((file.size() == 26) || (file.size() == 25)); // From '/MCAM_20150129_140830.webm' (26 characters)...

    std::string date(file.c_str() + 10); // '0129_140830.webm'
    date.insert(2, 1, '/'); // '01/29_140830.webm'
    date.insert(5, 1, '/'); // '01/29/_140830.webm'
    date.insert(6, file.substr(6, 4)); // '01/29/2015_140830.webm'
    date.replace(10, 1, 1, ' '); // '01/29/2015 140830.webm'
    date.insert(13, 1, ':'); // '01/29/2015 14:0830.webm'
    date.insert(16, 1, ':'); // '01/29/2015 14:08:30.webm'
    date.resize(date.size() - ((file.at(file.size() - 5) == '.')? 5:4)); // '.webm' contains 5 characters

    // ...to '01/29/2015 14:08:30'
    return date;
}

void Share::update(const Game* game, const std::string* folder, const std::string* file) {

#ifdef DEBUG
    LOGV(LOG_LEVEL_SHARE, (*mLog % 100), LOG_FORMAT(" - g:%x; f:%s; f:%s"), __PRETTY_FUNCTION__, __LINE__, game,
            folder->c_str(), file->c_str());
#endif
    static bool wait = false;
#ifndef __ANDROID__
    static short signInCount = 0;
#endif

    switch (mStatus) {
        case NETWORKS_DISPLAYING: {

            //mBackLogout.setRed(1.f);
            mBackLogout.setGreen(1.f);
            mBackLogout.setBlue(1.f);

            short panel = 7 * game->getScreen()->height / 10;
            float fontScale = game->getScreen()->width / FONT_SCALE_RATIO;
            mCloseText.position(-5.f * FONT_WIDTH * fontScale / game->getScreen()->height, TEXT_Y_LAG_POS -
                    panel / static_cast<float>(game->getScreen()->height));
            if (mURL) {

                delete mURL;
                mURL = NULL;
            }
            mStatus = NETWORKS_DISPLAYED;
            break;
        }
        case NETWORKS_DISPLAYED: {

            if (wait) {

                switch (mSocial->_getSession(mNetworkID)->getResult()) {
                    case Request::RESULT_SUCCEEDED: {

                        LOGI(LOG_LEVEL_SHARE, 0, LOG_FORMAT(" - RESULT_SUCCEEDED"), __PRETTY_FUNCTION__, __LINE__);
                        switch (mSocial->_getSession(mNetworkID)->getRequest()) {

                            case Session::REQUEST_LOGIN: {
#ifndef __ANDROID__
                                signInCount = 0;
#endif
                                wait = mSocial->request(mNetworkID, Session::REQUEST_INFO, NULL);
                                if (!wait) mSocial->_getSession(mNetworkID)->close();
                                break;
                            }
                            case Session::REQUEST_INFO: {
                                switch (mNetworkID) {
                                    case Network::FACEBOOK: {

                                        mFacebookURL = LIBENG_FACEBOOK_URL;
                                        mFacebookURL.append(mSocial->_getSession(Network::FACEBOOK)->getUserID());
                                        break;
                                    }
                                    case Network::TWITTER: {

                                        mTwitterURL = LIBENG_TWITTER_URL;
                                        mTwitterURL.append(mSocial->_getSession(Network::TWITTER)->getUserID());
                                        break;
                                    }
                                    case Network::GOOGLE: {

                                        mGoogleURL = LIBENG_GOOGLE_URL;
                                        mGoogleURL.append(mSocial->_getSession(Network::GOOGLE)->getUserID());
                                        break;
                                    }
                                }
                                mStatus = SHARE_DISPLAYING; // Display share dialog
                                //break;
                            }
                            case Session::REQUEST_SHARE_VIDEO: {
                                wait = false;
                                break;
                            }
                        }
                        break;
                    }
                    case Request::RESULT_EXPIRED: {

                        LOGI(LOG_LEVEL_SHARE, 0, LOG_FORMAT(" - RESULT_EXPIRED"), __PRETTY_FUNCTION__, __LINE__);
                        mSocial->_getSession(mNetworkID)->close();
                        mDispPic[mNetworkID] = false;
                        switch (mNetworkID) {

                            case Network::FACEBOOK: mFacebookURL.clear(); break;
                            case Network::TWITTER: mTwitterURL.clear(); break;
                            case Network::GOOGLE: mGoogleURL.clear(); break;
                        }
                        wait = mSocial->request(mNetworkID, Session::REQUEST_LOGIN, NULL);
                        break;
                    }
                    case Request::RESULT_WAITING: { // Wait

                        // Time out control for Goolge+ login request on iOS only coz there is no way to catch
                        // a cancel event when no Google+ application is installed (login managed in Safari)
                        // and user goes back to this application without logged
#ifndef __ANDROID__
                        if ((mNetworkID == Network::GOOGLE) &&
                            ((mSocial->_getSession(mNetworkID)->getRequest() == Session::REQUEST_LOGIN) ||
                             (mSocial->_getSession(mNetworkID)->getRequest() == Session::REQUEST_SHARE_VIDEO)) &&
                            (++signInCount > GOOGLE_REQ_TIMEOUT)) {

                            signInCount = 0;
                            mNetworkID = Network::NONE;
                            mStatus = PANEL_DISPLAYED;
                            wait = false;
                            return;
                        }
#endif
                        break;
                    }
                    default: {
                    //case Request::RESULT_NONE: // Request sent error (!wait)
                    //case Request::RESULT_CANCELED:
                    //case Request::RESULT_FAILED: {

                        LOGI(LOG_LEVEL_SHARE, 0, LOG_FORMAT(" - RESULT_CANCELED/FAILED"), __PRETTY_FUNCTION__, __LINE__);
                        if ((mSocial->_getSession(mNetworkID)->getRequest() == Session::REQUEST_INFO) &&
                            (mSocial->_getSession(mNetworkID)->getResult() == Request::RESULT_FAILED))
                            mSocial->_getSession(mNetworkID)->close();

                        mNetworkID = Network::NONE;
                        wait = false;
#ifndef __ANDROID__
                        signInCount = 0;
#endif
                        break;
                    }
                }
                break; // Stop touch management
            }

            // Touch...
            unsigned char touchCount = game->mTouchCount;
            while (touchCount--) {

                if (game->mTouchData[touchCount].Type != TouchInput::TOUCH_UP)
                    continue;

                // Check close
#ifdef LIBENG_PORT_AS_LAND
                if ((game->mTouchData[touchCount].Y > mLogoutArea.left) &&
                        (game->mTouchData[touchCount].Y < mLogoutArea.right) &&
                        (game->mTouchData[touchCount].X < mLogoutArea.top) &&
                        (game->mTouchData[touchCount].X > mLogoutArea.bottom)) {
#else
                if ((game->mTouchData[touchCount].X > mLogoutArea.left) &&
                        (game->mTouchData[touchCount].X < mLogoutArea.right) &&
                        (game->mTouchData[touchCount].Y > mLogoutArea.top) &&
                        (game->mTouchData[touchCount].Y < mLogoutArea.bottom)) {
#endif
                    mNetworkID = Network::NONE;
                    mStatus = PANEL_DISPLAYED;
                    break;
                }

                // Check Facebook
#ifdef LIBENG_PORT_AS_LAND
                if ((game->mTouchData[touchCount].Y > mFacebookArea.left) &&
                        (game->mTouchData[touchCount].Y < mFacebookArea.right) &&
                        (game->mTouchData[touchCount].X < mFacebookArea.top) &&
                        (game->mTouchData[touchCount].X > mFacebookArea.bottom)) {
#else
                if ((game->mTouchData[touchCount].X > mFacebookArea.left) &&
                        (game->mTouchData[touchCount].X < mFacebookArea.right) &&
                        (game->mTouchData[touchCount].Y > mFacebookArea.top) &&
                        (game->mTouchData[touchCount].Y < mFacebookArea.bottom)) {
#endif
                    LOGI(LOG_LEVEL_SHARE, 0, LOG_FORMAT(" - Facebook"), __PRETTY_FUNCTION__, __LINE__);
                    mNetworkID = Network::FACEBOOK;
                    wait = select();
                    break;
                }

                // Check YouTube
#ifdef LIBENG_PORT_AS_LAND
                if ((game->mTouchData[touchCount].Y > mYouTubeArea.left) &&
                        (game->mTouchData[touchCount].Y < mYouTubeArea.right) &&
                        (game->mTouchData[touchCount].X < mYouTubeArea.top) &&
                        (game->mTouchData[touchCount].X > mYouTubeArea.bottom)) {
#else
                if ((game->mTouchData[touchCount].X > mYouTubeArea.left) &&
                        (game->mTouchData[touchCount].X < mYouTubeArea.right) &&
                        (game->mTouchData[touchCount].Y > mYouTubeArea.top) &&
                        (game->mTouchData[touchCount].Y < mYouTubeArea.bottom)) {
#endif
                    LOGI(LOG_LEVEL_SHARE, 0, LOG_FORMAT(" - YouTube"), __PRETTY_FUNCTION__, __LINE__);
#ifdef __ANDROID__
                    alertMessage(LOG_LEVEL_SHARE, "ERROR: Not implemented yet!");
#else
                    alertMessage(LOG_LEVEL_SHARE, 2.5, "ERROR: Not implemented yet!");
#endif
                    break;
                }

                // Check Google
#ifdef LIBENG_PORT_AS_LAND
                if ((game->mTouchData[touchCount].Y > mGoogleArea.left) &&
                        (game->mTouchData[touchCount].Y < mGoogleArea.right) &&
                        (game->mTouchData[touchCount].X < mGoogleArea.top) &&
                        (game->mTouchData[touchCount].X > mGoogleArea.bottom)) {
#else
                if ((game->mTouchData[touchCount].X > mGoogleArea.left) &&
                        (game->mTouchData[touchCount].X < mGoogleArea.right) &&
                        (game->mTouchData[touchCount].Y > mGoogleArea.top) &&
                        (game->mTouchData[touchCount].Y < mGoogleArea.bottom)) {
#endif
                    LOGI(LOG_LEVEL_SHARE, 0, LOG_FORMAT(" - Google"), __PRETTY_FUNCTION__, __LINE__);
                    mNetworkID = Network::GOOGLE;
                    wait = select();
                    break;
                }

                // Check Twitter
#ifdef LIBENG_PORT_AS_LAND
                if ((game->mTouchData[touchCount].Y > mTwitterArea.left) &&
                        (game->mTouchData[touchCount].Y < mTwitterArea.right) &&
                        (game->mTouchData[touchCount].X < mTwitterArea.top) &&
                        (game->mTouchData[touchCount].X > mTwitterArea.bottom)) {
#else
                if ((game->mTouchData[touchCount].X > mTwitterArea.left) &&
                        (game->mTouchData[touchCount].X < mTwitterArea.right) &&
                        (game->mTouchData[touchCount].Y > mTwitterArea.top) &&
                        (game->mTouchData[touchCount].Y < mTwitterArea.bottom)) {
#endif
                    LOGI(LOG_LEVEL_SHARE, 0, LOG_FORMAT(" - Twitter"), __PRETTY_FUNCTION__, __LINE__);
                    mNetworkID = Network::TWITTER;
                    wait = select();
                    break;
                }
            }
            break;
        }
        case SHARE_DISPLAYING: {

            //mBackLogout.setRed(1.f);
            mBackLogout.setGreen(0.f);
            mBackLogout.setBlue(0.f);

            short panel = 7 * game->getScreen()->height / 10;
            float fontScale = game->getScreen()->width / FONT_SCALE_RATIO;
            mCloseText.position((2.f / 3.f) - (5.f * FONT_WIDTH * fontScale / game->getScreen()->height),
                    TEXT_Y_LAG_POS - panel / static_cast<float>(game->getScreen()->height));
            //
            setNetwork();
#ifdef DEBUG
            LOGI(LOG_LEVEL_SHARE, 0, LOG_FORMAT(), __PRETTY_FUNCTION__, __LINE__);
            assert(mSocial->_getSession(mNetworkID));
            switch (mNetworkID) {

                case Network::FACEBOOK: assert(!mFacebookURL.empty()); break;
                case Network::TWITTER: assert(!mTwitterURL.empty()); break;
                case Network::GOOGLE: assert(!mGoogleURL.empty()); break;
            }
#endif
            mSocial->setAdReqInfo(mNetworkID);

            mStatus = SHARE_DISPLAYED;
            if ((!mSocial->_getSession(mNetworkID)->getUserPic()) &&
                    ((mSocial->_getSession(mNetworkID)->getRequest() != Session::REQUEST_PICTURE) ||
                     (mSocial->_getSession(mNetworkID)->getResult() != Request::RESULT_WAITING)))
                mSocial->request(mNetworkID, Session::REQUEST_PICTURE, NULL);
            displayInfo(game);
            break;
        }
        case SHARE_DISPLAYED: {

            if ((mSocial->_getSession(mNetworkID)->getUserPic()) && ((!mDispPic[mNetworkID]) || (mResumed))) {

                setPicture();
                mResumed = false;
            }
            unsigned char touchCount = game->mTouchCount;
            while (touchCount--) {

                if (game->mTouchData[touchCount].Type != TouchInput::TOUCH_UP)
                    continue;

                // Check close
#ifdef LIBENG_PORT_AS_LAND
                if ((game->mTouchData[touchCount].Y > mCloseArea.left) &&
                        (game->mTouchData[touchCount].Y < mCloseArea.right) &&
                        (game->mTouchData[touchCount].X < mCloseArea.top) &&
                        (game->mTouchData[touchCount].X > mCloseArea.bottom)) {
#else
                if ((game->mTouchData[touchCount].X > mCloseArea.left) &&
                        (game->mTouchData[touchCount].X < mCloseArea.right) &&
                        (game->mTouchData[touchCount].Y > mCloseArea.top) &&
                        (game->mTouchData[touchCount].Y < mCloseArea.bottom)) {
#endif
                    mDispPic[mNetworkID] = false;
                    mNetworkID = Network::NONE;
                    mStatus = NETWORKS_DISPLAYING;
                    break;
                }

                // Check logout
#ifdef LIBENG_PORT_AS_LAND
                if ((game->mTouchData[touchCount].Y > mLogoutArea.left) &&
                        (game->mTouchData[touchCount].Y < mLogoutArea.right) &&
                        (game->mTouchData[touchCount].X < mLogoutArea.top) &&
                        (game->mTouchData[touchCount].X > mLogoutArea.bottom)) {
#else
                if ((game->mTouchData[touchCount].X > mLogoutArea.left) &&
                        (game->mTouchData[touchCount].X < mLogoutArea.right) &&
                        (game->mTouchData[touchCount].Y > mLogoutArea.top) &&
                        (game->mTouchData[touchCount].Y < mLogoutArea.bottom)) {
#endif
                    LOGI(LOG_LEVEL_SHARE, 0, LOG_FORMAT(" - Logout"), __PRETTY_FUNCTION__, __LINE__);
                    mSocial->_getSession(mNetworkID)->close();
                    mDispPic[mNetworkID] = false;
                    switch (mNetworkID) {

                        case Network::FACEBOOK: mFacebookURL.clear(); break;
                        case Network::TWITTER: mTwitterURL.clear(); break;
                        case Network::GOOGLE: mGoogleURL.clear(); break;
                    }
                    mNetworkID = Network::NONE;
                    mStatus = NETWORKS_DISPLAYING;
                    break;
                }

                // Check share
#ifdef LIBENG_PORT_AS_LAND
                if ((game->mTouchData[touchCount].Y > mShareArea.left) &&
                        (game->mTouchData[touchCount].Y < mShareArea.right) &&
                        (game->mTouchData[touchCount].X < mShareArea.top) &&
                        (game->mTouchData[touchCount].X > mShareArea.bottom)) {
#else
                if ((game->mTouchData[touchCount].X > mShareArea.left) &&
                        (game->mTouchData[touchCount].X < mShareArea.right) &&
                        (game->mTouchData[touchCount].Y > mShareArea.top) &&
                        (game->mTouchData[touchCount].Y < mShareArea.bottom)) {
#endif
                    LOGI(LOG_LEVEL_SHARE, 0, LOG_FORMAT(" - Share"), __PRETTY_FUNCTION__, __LINE__);
                    ShareData* link = NULL;
                    switch (mNetworkID) {

                        case Network::FACEBOOK: {
                            link = new Facebook::LinkData;
#ifdef __ANDROID__
                            static_cast<Facebook::LinkData*>(link)->name.assign(*folder);
                            static_cast<Facebook::LinkData*>(link)->name.append(*file);
#else
                            static_cast<Facebook::LinkData*>(link)->link.assign(*folder);
                            static_cast<Facebook::LinkData*>(link)->link.append(*file);
                            static_cast<Facebook::LinkData*>(link)->name.assign(VIDEO_NAME);
                            static_cast<Facebook::LinkData*>(link)->caption.assign(MOV_MIME_TYPE);
                            static_cast<Facebook::LinkData*>(link)->description.assign(VIDEO_DESCRIPTION);
                            static_cast<Facebook::LinkData*>(link)->description.append(extractDate(*file));
                            LOGI(LOG_LEVEL_SHARE, 0, LOG_FORMAT(" - Video description: %s"), __PRETTY_FUNCTION__, __LINE__,
                                 static_cast<const Facebook::LinkData*>(link)->description.c_str());
#endif
                            break;
                        }
                        case Network::GOOGLE: {
                            link = new Google::LinkData;
                            static_cast<Google::LinkData*>(link)->url.assign(*folder);
                            static_cast<Google::LinkData*>(link)->url.append(*file);
#ifdef __ANDROID__
                            static_cast<Google::LinkData*>(link)->type.assign((file->at(file->size() - 5) == '.')? // '.webm'
                                    WEBM_MIME_TYPE:MOV_MIME_TYPE);
                            static_cast<Google::LinkData*>(link)->title.assign(VIDEO_TITLE);
                            static_cast<Google::LinkData*>(link)->title.append(extractDate(*file));
                            LOGI(LOG_LEVEL_SHARE, 0, LOG_FORMAT(" - Video title: %s"), __PRETTY_FUNCTION__, __LINE__,
                                    static_cast<const Google::LinkData*>(link)->title.c_str());
#endif
                            break;
                        }
                        case Network::TWITTER:

                            link = new Twitter::LinkData;
                            static_cast<Twitter::LinkData*>(link)->tweet = TWEET_MESSAGE;
                            static_cast<Twitter::LinkData*>(link)->tweet.append(extractDate(*file));
                            static_cast<Twitter::LinkData*>(link)->media.assign(*folder);
                            static_cast<Twitter::LinkData*>(link)->media.append(*file);
                            break;
                    }
                    wait = mSocial->request(mNetworkID, Session::REQUEST_SHARE_VIDEO, link);
                    delete link;
                    mDispPic[mNetworkID] = false;
                    mStatus = NETWORKS_DISPLAYING;
                    break;
                }
            }
            break;
        }
    }
}
void Share::render() const {

#ifdef DEBUG
    LOGV(LOG_LEVEL_SHARE, (*mLog % 100), LOG_FORMAT(), __PRETTY_FUNCTION__, __LINE__);
#endif
    switch (mStatus) {
        case PANEL_DISPLAYED: {

            mShare.render(true);
            break;
        }
        case NETWORKS_DISPLAYING:
        case NETWORKS_DISPLAYED: {

            mFacebook.render(true);
            mYouTube.render(false);
            mGoogle.render(false);
            mTwitter.render(false);

            mBackLogout.render(false);
            mCloseText.render(true);
            break;
        }
        case SHARE_DISPLAYING:
        case SHARE_DISPLAYED: {

            glDisable(GL_BLEND);
            mPicture.render(true);
            glEnable(GL_BLEND);

            mNetworkPic.render(false);
            mBackShare.render(false);
            mBackLogout.render(false);
            mBackClose.render(false);

            mInfo.render(true);
            mGender.render(true);
            mBirthday.render(true);
            if (mURL)
                mURL->render(true);

            mShareText.render(true);
            mLogoutText.render(true);
            mCloseText.render(true);
            break;
        }
    }
}
