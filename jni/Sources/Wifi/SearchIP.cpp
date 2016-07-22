#include "SearchIP.h"

#include <boost/algorithm/string.hpp>
#include <libeng/Tools/Tools.h>

#ifdef __ANDROID__
#define SEARCH_TIMEOUT          250
#else
#define SEARCH_TIMEOUT          350
#endif

//////
SearchIP::SearchIP() : mRunning(false), mThread(NULL) {

    LOGV(LOG_LEVEL_SEARCHIP, 0, LOG_FORMAT(), __PRETTY_FUNCTION__, __LINE__);
}
SearchIP::~SearchIP() {

    LOGV(LOG_LEVEL_SEARCHIP, 0, LOG_FORMAT(), __PRETTY_FUNCTION__, __LINE__);
    if (mRunning) {

        assert(mThread);
#ifdef __ANDROID__
        Internet::abortNetworkIP();
#else
        Internet::getInstance()->abortNetworkIP();
#endif
        mThread->join();
    }
    if (mThread)
        delete mThread;

    mListIP.clear();
}

void SearchIP::start(bool connected) {

    LOGV(LOG_LEVEL_SEARCHIP, 0, LOG_FORMAT(" - (r:%s)"), __PRETTY_FUNCTION__, __LINE__, (mRunning)? "true":"false");
    assert(!mRunning);

    if (mThread)
        delete mThread;
    if (!connected)
        mListIP.clear();

    mRunning = true;
    mThread = new boost::thread(SearchIP::startSearchThread, this);
}

void SearchIP::searchThreadRunning() {

    LOGV(LOG_LEVEL_SEARCHIP, 0, LOG_FORMAT(" - Begin"), __PRETTY_FUNCTION__, __LINE__);
    if (mListIP.empty()) {

#ifdef __ANDROID__ // Search IP of connected devices
        std::string ipList(Internet::getNetworkIP(SEARCH_TIMEOUT));
#else
        std::string ipList(Internet::getInstance()->getNetworkIP(SEARCH_TIMEOUT));
#endif
        if (!ipList.empty())
            boost::split(mListIP, ipList, boost::is_any_of(SEPARATOR_IP_NETWORK_STR));

#ifdef DEBUG
        for (std::vector<std::string>::iterator iter = mListIP.begin(); iter != mListIP.end(); ++iter) {
            LOGI(LOG_LEVEL_SEARCHIP, 0, LOG_FORMAT(" - IP: %s"), __PRETTY_FUNCTION__, __LINE__, (*iter).c_str());
        }
#endif
#ifdef __ANDROID__
        detachThreadJVM(LOG_LEVEL_SEARCHIP);
#endif
    }
    else
        boost::this_thread::sleep(boost::posix_time::milliseconds(MCAM_DELAY_TIMEOUT * 1000)); // Wait

    LOGI(LOG_LEVEL_SEARCHIP, 0, LOG_FORMAT(" - Finish"), __PRETTY_FUNCTION__, __LINE__);
    mRunning = false;
}
void SearchIP::startSearchThread(SearchIP* search) {

    LOGV(LOG_LEVEL_SEARCHIP, 0, LOG_FORMAT(" - s:%x"), __PRETTY_FUNCTION__, __LINE__, search);
    search->searchThreadRunning();
}
