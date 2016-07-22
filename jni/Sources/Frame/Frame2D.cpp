#include "Frame2D.h"

#include <libeng/Tools/Tools.h>

#define DIGIT_COUNT(d)          static_cast<unsigned char>((d < 10)? 1:((d < 100)? 2:3))

#define LAND_SCALE_RATIO        (3.f / 7.f)
#define PORT_SCALE_RATIO        (2.f / 3.f)

Frame2D::Frame2D() : mDigitCount(0) {

    LOGV(LOG_LEVEL_FRAME2D, 0, LOG_FORMAT(), __PRETTY_FUNCTION__, __LINE__);
    mSharp = NULL;
    mFrame = NULL;
}
Frame2D::~Frame2D() {

    LOGV(LOG_LEVEL_FRAME2D, 0, LOG_FORMAT(), __PRETTY_FUNCTION__, __LINE__);
    if (mSharp) delete mSharp;
    destroy();
}

void Frame2D::start(const Game2D* game) { // '#1' in landscape orientation

    LOGV(LOG_LEVEL_FRAME2D, 0, LOG_FORMAT(" - g:%x"), __PRETTY_FUNCTION__, __LINE__, game);
    mSharp = new Element2D;
    mSharp->initialize(game);
    mSharp->start(TEXTURE_ID_FONT);

    static const float sharpCoord[8] = { (28 * FONT_WIDTH) / FONT_TEX_WIDTH, 0.f, (28 * FONT_WIDTH) / FONT_TEX_WIDTH,
            FONT_HEIGHT / FONT_TEX_HEIGHT, (29 * FONT_WIDTH) / FONT_TEX_WIDTH, FONT_HEIGHT / FONT_TEX_HEIGHT,
            (29 * FONT_WIDTH) / FONT_TEX_WIDTH, 0.f };
    mSharp->setTexCoords(sharpCoord);

    mSharp->setVertices((game->getScreen()->width >> 1) - (FONT_WIDTH >> 1), (game->getScreen()->height >> 1) +
            (FONT_HEIGHT >> 1), (game->getScreen()->width >> 1) + (FONT_WIDTH >> 1), (game->getScreen()->height >> 1) -
            (FONT_HEIGHT >> 1));

    mDigitCount = 1;
    mFrame = new Element2D*[1];
    mFrame[0] = new Element2D;
    mFrame[0]->initialize(game);
    mFrame[0]->start(TEXTURE_ID_FONT);

    static const float numCoord[8] = { (1 * FONT_WIDTH) / FONT_TEX_WIDTH, (2 * FONT_HEIGHT) / FONT_TEX_HEIGHT,
            (1 * FONT_WIDTH) / FONT_TEX_WIDTH, (3 * FONT_HEIGHT) / FONT_TEX_HEIGHT, (2 * FONT_WIDTH) / FONT_TEX_WIDTH,
            (3 * FONT_HEIGHT) / FONT_TEX_HEIGHT, (2 * FONT_WIDTH) / FONT_TEX_WIDTH, (2 * FONT_HEIGHT) / FONT_TEX_HEIGHT };
    mFrame[0]->setTexCoords(numCoord);
    mFrame[0]->setVertices((game->getScreen()->width >> 1) - (FONT_WIDTH >> 1), (game->getScreen()->height >> 1) +
            (FONT_HEIGHT >> 1), (game->getScreen()->width >> 1) + (FONT_WIDTH >> 1), (game->getScreen()->height >> 1) -
            (FONT_HEIGHT >> 1));
    //
    float scale = game->getScreen()->width * LAND_SCALE_RATIO / (FONT_WIDTH << 1); // << 1 -> 2 letters (#1)
    mSharp->scale(scale, scale);
    mFrame[0]->scale(scale, scale);

    float trans = (FONT_WIDTH * scale) / game->getScreen()->height;
    mSharp->translate(-trans, 0.f);
    mFrame[0]->translate(trans, 0.f);
}

void Frame2D::destroy() {

    LOGV(LOG_LEVEL_FRAME2D, 0, LOG_FORMAT(), __PRETTY_FUNCTION__, __LINE__);
    for (unsigned char i = 0; i < mDigitCount; ++i)
        delete mFrame[i];

    if (mDigitCount)
        delete [] mFrame;
}

void Frame2D::setFrameNo(const Game2D* game, unsigned char frameNo, bool land) {

    LOGV(LOG_LEVEL_FRAME2D, 0, LOG_FORMAT(" - g:%x; f:%d; l:%s"), __PRETTY_FUNCTION__, __LINE__, game, frameNo,
            (land)? "true":"false");
    if (DIGIT_COUNT(frameNo) != mDigitCount) {

        destroy();
        mDigitCount = DIGIT_COUNT(frameNo);

        mFrame = new Element2D*[mDigitCount];
        for (unsigned char i = 0; i < mDigitCount; ++i) {

            mFrame[i] = new Element2D;
            mFrame[i]->initialize(game);
            mFrame[i]->start(TEXTURE_ID_FONT);
            mFrame[i]->setVertices((game->getScreen()->width >> 1) - (FONT_WIDTH >> 1), (game->getScreen()->height >> 1) +
                    (FONT_HEIGHT >> 1), (game->getScreen()->width >> 1) + (FONT_WIDTH >> 1), (game->getScreen()->height >> 1) -
                    (FONT_HEIGHT >> 1));
        }
        setOrientation(game, land);
    }
    for (short i = static_cast<short>(mDigitCount - 1), no = 0, modulo = 10; i != LIBENG_NO_DATA; --i) {

        no = (frameNo % modulo) - no;
        if (modulo > 10) no /= modulo / 10;
        modulo *= 10;

        float numCoord[8];
        numCoord[0] = (no * FONT_WIDTH) / FONT_TEX_WIDTH;
        numCoord[1] = (FONT_HEIGHT << 1) / FONT_TEX_HEIGHT;
        numCoord[2] = numCoord[0];
        numCoord[3] = (3 * FONT_HEIGHT) / FONT_TEX_HEIGHT;
        numCoord[4] = ((no + 1) * FONT_WIDTH) / FONT_TEX_WIDTH;
        numCoord[5] = numCoord[3];
        numCoord[6] = numCoord[4];
        numCoord[7] = numCoord[1];

        mFrame[i]->setTexCoords(numCoord);
    }
}
void Frame2D::setOrientation(const Game2D* game, bool land) {

    LOGV(LOG_LEVEL_FRAME2D, 0, LOG_FORMAT(" - g:%x; l:%s"), __PRETTY_FUNCTION__, __LINE__, game, (land)? "true":"false");
    mSharp->reset();

    float trans, scale;
    if (!land) {

        scale = game->getScreen()->height * PORT_SCALE_RATIO / (FONT_WIDTH * (mDigitCount + 1));
        trans = FONT_WIDTH * mDigitCount * scale / game->getScreen()->height;
#ifdef __ANDROID__
        mSharp->rotate(2.f * PI_F);
#else
        mSharp->rotate(PI_F / 2.f);
#endif
        mSharp->translate(0.f, -trans);
    }
    else {

        scale = game->getScreen()->width * LAND_SCALE_RATIO / (FONT_WIDTH * (mDigitCount + 1));
        trans = FONT_WIDTH * mDigitCount * scale / game->getScreen()->height;
        mSharp->rotate(0.f);
        mSharp->translate(-trans, 0.f);
    }

    mSharp->scale(scale, scale);
    for (short i = static_cast<short>(mDigitCount - 1); i != LIBENG_NO_DATA; --i) {

        mFrame[i]->reset();
        mFrame[i]->scale(scale, scale);
        if (!land) {
#ifdef __ANDROID__
            mFrame[i]->rotate(2.f * PI_F);
#else
            mFrame[i]->rotate(PI_F / 2.f);
#endif
            mFrame[i]->translate(0.f, trans);
        }
        else {
            mFrame[i]->rotate(0.f);
            mFrame[i]->translate(trans, 0.f);
        }
        trans -= (FONT_WIDTH << 1) * scale / game->getScreen()->height;
    }
}

void Frame2D::resume() {

    LOGV(LOG_LEVEL_FRAME2D, 0, LOG_FORMAT(), __PRETTY_FUNCTION__, __LINE__);
    mSharp->resume(TEXTURE_ID_FONT);
    for (unsigned char i = 0; i < mDigitCount; ++i)
        mFrame[i]->resume(TEXTURE_ID_FONT);
}
void Frame2D::render() const {

    mSharp->render(true);
    for (unsigned char i = 0; i < mDigitCount; ++i)
        mFrame[i]->render(true);
}
