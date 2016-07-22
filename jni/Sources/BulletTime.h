#ifndef BULLETTIME_H_
#define BULLETTIME_H_

#include <libeng/Game/2D/Game2D.h>
#include <libeng/Intro/Intro.h>

#include "Global.h"

#ifdef __ANDROID__
#include "Level/MatrixLevel.h"
#else
#include "MatrixLevel.h"
#endif

#define FILE_NAME       "BulletTime.backup"

using namespace eng;

//////
class BulletTime : public Game2D {

private:
    BulletTime();
    virtual ~BulletTime();

    MatrixLevel* mLevel;
    WaitConn* mWaitNet;

public:
    static BulletTime* getInstance() {
        if (!mThis)
            mThis = new BulletTime;
        return static_cast<BulletTime*>(mThis);
    }
    static void freeInstance() {
        if (mThis) {
            delete mThis;
            mThis = NULL;
        }
    }

    //////
    void init() { }

    bool start();
    void pause();
#ifdef __ANDROID__
    void lockScreen();
#endif
    void destroy();

    void wait(float millis);
    void update();

    void render() const;

};

#endif // BULLETTIME_H_
