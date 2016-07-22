#ifndef SEARCHIP_H_
#define SEARCHIP_H_

#include "Global.h"

#include <libeng/Log/Log.h>
#include <libeng/Features/Internet/Internet.h>

#include <boost/thread.hpp>
#include <string>
#include <vector>

#define MCAM_DELAY_TIMEOUT          3 // ...

using namespace eng;

//////
class SearchIP {

private:
    std::vector<std::string> mListIP;
    bool mRunning;

    boost::thread* mThread;

    void searchThreadRunning();
    static void startSearchThread(SearchIP* search);

public:
    SearchIP();
    virtual ~SearchIP();

    inline bool isRunning() const { return mRunning; }

    inline unsigned char getCount() const { return static_cast<unsigned char>(mListIP.size()); }
    inline std::string getIP(unsigned char idx) const {

        LOGV(LOG_LEVEL_SEARCHIP, 0, LOG_FORMAT(" - i:%d"), __PRETTY_FUNCTION__, __LINE__, idx);
        assert(!mRunning);
        assert(mListIP.size() > idx);

        return mListIP[idx];
    };

    //////
    void start(bool connected);

};

#endif // SEARCHIP_H_
