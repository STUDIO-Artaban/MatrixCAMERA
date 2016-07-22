#ifndef FRAME2D_H_
#define FRAME2D_H_

#include "Global.h"

#include <libeng/Game/2D/Game2D.h>
#include <libeng/Graphic/Object/2D/Element2D.h>

using namespace eng;

//////
class Frame2D {

private:
    Element2D* mSharp;
    Element2D** mFrame;

    unsigned char mDigitCount;
    void destroy();

public:
    Frame2D();
    virtual ~Frame2D();

    inline void hide() {

        LOGV(LOG_LEVEL_FRAME2D, 0, LOG_FORMAT(), __PRETTY_FUNCTION__, __LINE__);
        mSharp->setAlpha(0.f);
        for (unsigned char i = 0; i < mDigitCount; ++i)
            mFrame[i]->setAlpha(0.f);
    };
    inline void show() {

        LOGV(LOG_LEVEL_FRAME2D, 0, LOG_FORMAT(), __PRETTY_FUNCTION__, __LINE__);
        mSharp->setAlpha(1.f);
        for (unsigned char i = 0; i < mDigitCount; ++i)
            mFrame[i]->setAlpha(1.f);
    };

    //////
    void start(const Game2D* game);

    void setFrameNo(const Game2D* game, unsigned char frameNo, bool land); // WARNING: Do not change current orientation
    void setOrientation(const Game2D* game, bool land);

    inline void pause() {

        LOGV(LOG_LEVEL_FRAME2D, 0, LOG_FORMAT(), __PRETTY_FUNCTION__, __LINE__);
        mSharp->pause();
        for (unsigned char i = 0; i < mDigitCount; ++i)
            mFrame[i]->pause();
    };
    void resume();
    void render() const;

};

#endif // FRAME2D_H_
