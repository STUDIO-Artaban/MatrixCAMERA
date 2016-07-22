#include "Count2D.h"

#include <libeng/Tools/Tools.h>

#define LAND_SCALE_RATIO        ((1.f / 7.f) * 0.8f)
#define PORT_SCALE_RATIO        ((1.f / 7.f) * 0.65f)

Count2D::Count2D() : mDigitCount(0) {

    LOGV(LOG_LEVEL_COUNT2D, 0, LOG_FORMAT(), __PRETTY_FUNCTION__, __LINE__);
    mDivide = NULL;
    mCount = NULL;
}
Count2D::~Count2D() {

    LOGV(LOG_LEVEL_COUNT2D, 0, LOG_FORMAT(), __PRETTY_FUNCTION__, __LINE__);
    if (mDivide) delete mDivide;
    destroy();
}

void Count2D::start(const Game2D* game, float bottom) { // '/1' in landscape orientation

    LOGV(LOG_LEVEL_COUNT2D, 0, LOG_FORMAT(" - g:%x"), __PRETTY_FUNCTION__, __LINE__, game);
    mDivide = new Element2D;
    mDivide->initialize(game);
    mDivide->start(TEXTURE_ID_FONT);

    static const float divCoord[8] = { (21 * FONT_WIDTH) / FONT_TEX_WIDTH, (2 * FONT_HEIGHT) / FONT_TEX_HEIGHT,
            (21 * FONT_WIDTH) / FONT_TEX_WIDTH, (3 * FONT_HEIGHT) / FONT_TEX_HEIGHT, (22 * FONT_WIDTH) / FONT_TEX_WIDTH,
            (3 * FONT_HEIGHT) / FONT_TEX_HEIGHT, (22 * FONT_WIDTH) / FONT_TEX_WIDTH, (2 * FONT_HEIGHT) / FONT_TEX_HEIGHT };
    mDivide->setTexCoords(divCoord);
    mDivide->setVertices((game->getScreen()->width >> 1) - (FONT_WIDTH >> 1), (game->getScreen()->height >> 1) +
            (FONT_HEIGHT >> 1), (game->getScreen()->width >> 1) + (FONT_WIDTH >> 1), (game->getScreen()->height >> 1) -
            (FONT_HEIGHT >> 1));

    mDigitCount = 1;
    mCount = new Element2D*[1];
    mCount[0] = new Element2D;
    mCount[0]->initialize(game);
    mCount[0]->start(TEXTURE_ID_FONT);

    static const float cntCoord[8] = { (1 * FONT_WIDTH) / FONT_TEX_WIDTH, (2 * FONT_HEIGHT) / FONT_TEX_HEIGHT,
            (1 * FONT_WIDTH) / FONT_TEX_WIDTH, (3 * FONT_HEIGHT) / FONT_TEX_HEIGHT, (2 * FONT_WIDTH) / FONT_TEX_WIDTH,
            (3 * FONT_HEIGHT) / FONT_TEX_HEIGHT, (2 * FONT_WIDTH) / FONT_TEX_WIDTH, (2 * FONT_HEIGHT) / FONT_TEX_HEIGHT };
    mCount[0]->setTexCoords(cntCoord);
    mCount[0]->setVertices((game->getScreen()->width >> 1) - (FONT_WIDTH >> 1), (game->getScreen()->height >> 1) +
            (FONT_HEIGHT >> 1), (game->getScreen()->width >> 1) + (FONT_WIDTH >> 1), (game->getScreen()->height >> 1) -
            (FONT_HEIGHT >> 1));
    //
    float scale = game->getScreen()->width * LAND_SCALE_RATIO / (FONT_WIDTH << 1); // << 1 -> 2 letters (/1)
    mDivide->scale(scale, scale);
    mCount[0]->scale(scale, scale);

    float transX = (FONT_WIDTH * scale) / game->getScreen()->height;
    mDivide->translate(-transX, 0.f);
    mCount[0]->translate(transX, 0.f);

    transX = 6.f * game->getScreen()->right / 7.f; // Using SCREEN_SCALE_RATIO definition
    float transY = (FONT_HEIGHT * scale) / game->getScreen()->height;
    mDivide->translate(transX, transY + bottom);
    mCount[0]->translate(transX, transY + bottom);
}

void Count2D::destroy() {

    LOGV(LOG_LEVEL_COUNT2D, 0, LOG_FORMAT(), __PRETTY_FUNCTION__, __LINE__);
    for (unsigned char i = 0; i < mDigitCount; ++i)
        delete mCount[i];

    if (mDigitCount)
        delete [] mCount;
}

void Count2D::setCount(const Game2D* game, float bottom, unsigned char count, bool land) {

#ifdef DEBUG
    LOGV(LOG_LEVEL_COUNT2D, (*game->getLog() % 100), LOG_FORMAT(" - g:%x; b:%f; c:%d; l:%s"), __PRETTY_FUNCTION__, __LINE__,
            game, bottom, count, (land)? "true":"false");
#endif
    if (DIGIT_COUNT(count) != mDigitCount) {

        destroy();
        mDigitCount = DIGIT_COUNT(count);

        mCount = new Element2D*[mDigitCount];
        for (unsigned char i = 0; i < mDigitCount; ++i) {

            mCount[i] = new Element2D;
            mCount[i]->initialize(game);
            mCount[i]->start(TEXTURE_ID_FONT);
            mCount[i]->setVertices((game->getScreen()->width >> 1) - (FONT_WIDTH >> 1), (game->getScreen()->height >> 1) +
                    (FONT_HEIGHT >> 1), (game->getScreen()->width >> 1) + (FONT_WIDTH >> 1), (game->getScreen()->height >> 1) -
                    (FONT_HEIGHT >> 1));
        }
        setOrientation(game, bottom, land);
    }
    for (short i = static_cast<short>(mDigitCount - 1), no = 0, modulo = 10; i != LIBENG_NO_DATA; --i) {

        no = (count % modulo) - no;
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

        mCount[i]->setTexCoords(numCoord);
    }
}
void Count2D::setOrientation(const Game2D* game, float bottom, bool land) {

    LOGV(LOG_LEVEL_COUNT2D, 0, LOG_FORMAT(" - g:%x; b:%f; l:%s"), __PRETTY_FUNCTION__, __LINE__, game, bottom,
            (land)? "true":"false");
    float transX = 6.f * game->getScreen()->right / 7.f; // Using SCREEN_SCALE_RATIO definition
    float scale;
    if (!land)
        scale = game->getScreen()->width * PORT_SCALE_RATIO / (FONT_WIDTH << 1);
    else
        scale = game->getScreen()->width * LAND_SCALE_RATIO / (FONT_WIDTH * (mDigitCount + 1));
    float pos = FONT_WIDTH * mDigitCount * scale / game->getScreen()->height;
    float digitW = (FONT_WIDTH << 1) * scale / game->getScreen()->height;
    float transY;
    if (!land)
        transY = digitW + bottom;
    else
        transY = ((FONT_HEIGHT * scale) / game->getScreen()->height) + bottom;

    mDivide->reset();
    mDivide->scale(scale, scale);
    mDivide->translate(transX, transY);
    if (!land) {

#ifdef __ANDROID__
        mDivide->rotate(2.f * PI_F);
#else
        mDivide->rotate(PI_F / 2.f);
#endif
        if (mDigitCount > 1) pos -= (digitW / 2.f) * (mDigitCount - 1);
        mDivide->translate(0.f, -pos);
        if (mDigitCount > 1) pos += digitW * (mDigitCount - 1);
    }
    else {

        mDivide->rotate(0.f);
        mDivide->translate(-pos, 0.f);
    }
    for (short i = static_cast<short>(mDigitCount - 1); i != LIBENG_NO_DATA; --i) {

        mCount[i]->reset();
        mCount[i]->scale(scale, scale);
        mCount[i]->translate(transX, transY);
        if (!land) {

#ifdef __ANDROID__
            mCount[i]->rotate(2.f * PI_F);
#else
            mCount[i]->rotate(PI_F / 2.f);
#endif
            mCount[i]->translate(0.f, pos);
        }
        else {

            mCount[i]->rotate(0.f);
            mCount[i]->translate(pos, 0.f);
        }
        pos -= digitW;
    }
}

void Count2D::resume() {

    LOGV(LOG_LEVEL_COUNT2D, 0, LOG_FORMAT(), __PRETTY_FUNCTION__, __LINE__);
    mDivide->resume(TEXTURE_ID_FONT);
    for (unsigned char i = 0; i < mDigitCount; ++i)
        mCount[i]->resume(TEXTURE_ID_FONT);
}
void Count2D::render() const {

    mDivide->render(true);
    for (unsigned char i = 0; i < mDigitCount; ++i)
        mCount[i]->render(true);
}
