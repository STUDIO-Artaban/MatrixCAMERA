#ifndef COUNT2D_H_
#define COUNT2D_H_

#include "Global.h"

#include <libeng/Game/2D/Game2D.h>
#include <libeng/Graphic/Object/2D/Element2D.h>

using namespace eng;

//////
class Count2D {

private:
    Element2D* mDivide;
    Element2D** mCount;

    unsigned char mDigitCount;
    void destroy();

public:
    Count2D();
    virtual ~Count2D();

    inline void hide() {

        LOGV(LOG_LEVEL_COUNT2D, 0, LOG_FORMAT(), __PRETTY_FUNCTION__, __LINE__);
        mDivide->setAlpha(0.f);
        for (unsigned char i = 0; i < mDigitCount; ++i)
            mCount[i]->setAlpha(0.f);
    };
    inline void show() {

        LOGV(LOG_LEVEL_COUNT2D, 0, LOG_FORMAT(), __PRETTY_FUNCTION__, __LINE__);
        mDivide->setAlpha(1.f);
        for (unsigned char i = 0; i < mDigitCount; ++i)
            mCount[i]->setAlpha(1.f);
    };

    inline void setColor(float red, float green, float blue) {

        LOGV(LOG_LEVEL_COUNT2D, 0, LOG_FORMAT(" - r:%f; g:%f; b:%f"), __PRETTY_FUNCTION__, __LINE__, red, green, blue);
        mDivide->setRed(red);
        mDivide->setGreen(green);
        mDivide->setBlue(blue);

        for (unsigned char i = 0; i < mDigitCount; ++i) {

            mCount[i]->setRed(red);
            mCount[i]->setGreen(green);
            mCount[i]->setBlue(blue);
        }
    };

    //////
    void start(const Game2D* game, float bottom); // 'bottom': Bottom of the film

    void setCount(const Game2D* game, float bottom, unsigned char count, bool land); // WARNING: Do not change current orientation
    void setOrientation(const Game2D* game, float bottom, bool land);

    inline void pause() {

        LOGV(LOG_LEVEL_COUNT2D, 0, LOG_FORMAT(), __PRETTY_FUNCTION__, __LINE__);
        mDivide->pause();
        for (unsigned char i = 0; i < mDigitCount; ++i)
            mCount[i]->pause();
    };
    void resume();
    void render() const;

};

#endif // COUNT2D_H_
