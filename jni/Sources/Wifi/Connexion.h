#ifndef CONNEXION_H_
#define CONNEXION_H_

#include "Global.h"

#include <libeng/Log/Log.h>
#include <libeng/Features/Internet/Socket.h>
#include <libeng/Game/Level.h>

#ifdef __ANDROID__
#include "Video/Video.h"
#include "Wifi/SearchIP.h"
#else
#include "Video.h"
#include "SearchIP.h"
#endif

#define INITIAL_PORT_NO             1024
#define MAX_PORT_NO                 (INITIAL_PORT_NO + 20)

// Commands
#define CMD_ORIENTATION             "ORIENTATION_MCAM#"
#define CMD_READY                   "READY_MCAM#"
#define CMD_GET                     "GET_MCAM#"

#define ORIENTATION_LEN             ((sizeof(CMD_ORIENTATION) - 1) + 1 + SECURITY_LEN + CHECKSUM_LEN) // + 1 -> Orientation
#define READY_LEN                   ((sizeof(CMD_READY) - 1) + SECURITY_LEN + CHECKSUM_LEN)

// Indexes
#define REPLY_PICSIZE_IDX           sizeof(CMD_GET)

using namespace eng;

//////
class Connexion {

private:
    unsigned char mStatus;
    unsigned char mTimeOutIdx; // Error/Timeout client index
    Level* mCaller;
    Video* mVideo;

    bool mConnected; // Already connected
    SearchIP* mSearch;

    char mSendBuffer[MAX_PACKET_SIZE + 1];

    Socket* mSocket;
    bool mServer;
    unsigned int mPicSize;

    Video::FrameList mFrames;
    inline void duplicate() { // Duplicate client list into frame list
                              // -> Avoid to take count of client changes after this call (store infos in 'mFrames')
#ifdef __ANDROID__
        for (Video::FrameList::iterator iter = mFrames.begin(); iter != mFrames.end(); ++iter)
            delete (*iter);
#endif
        mFrames.clear();
#ifdef __ANDROID__
        for (ClientList::const_iterator iter = mClients.begin(); iter != mClients.end(); ++iter) {

            Video::FrameClient* frame = new Video::FrameClient;
            frame->done = (*iter)->getStatus() == ClientMgr::RCV_REPLY_NONE;
            frame->android = (*iter)->isAndroid();

            mFrames.push_back(frame);
        }
#else
        for (ClientList::const_iterator iter = mClients.begin(); iter != mClients.end(); ++iter)
            mFrames.push_back((*iter)->getStatus() == ClientMgr::RCV_REPLY_NONE);
#endif
    };
    ClientList mClients;
    ClientMgr* mCurClient;
    bool mCloseAll;

    volatile bool mAbort;
    boost::mutex mMutex;
    boost::thread* mThread;

    void connexionThreadRunning();
    static void startConnexionThread(Connexion* conn);

public:
    Connexion(bool server, Level* caller, bool connected = false);
    virtual ~Connexion();

    enum {

        CONN_OPEN = 0,
        CONN_WAIT, // Wait client
        CONN_TIMEOUT, // Time out / Error

        CONN_VERIFY, // Inform client(s) of their new rank after a CONN_TIMEOUT
        CONN_ORIENTATION,
        CONN_READY,

        // Client
        CONN_CONNEXION,

        // Server
        CONN_START,
        CONN_CLOSE, // Close all clients
        CONN_NEW, // New client(s)

        CONN_GO, // >= CONN_GO -> No time out/error management (see 'timeOut' method)
        CONN_DOWNLOAD,
        CONN_WAIT_UPLOAD,
        CONN_UPLOAD

    };
    inline unsigned char getStatus() const { return mStatus; }
    inline unsigned char getClientCount() const {
        return (mServer)? static_cast<unsigned char>(mClients.size()):mCurClient->getRank();
    };
    inline unsigned char getCountStatus(unsigned char status) const {

        assert(mServer);
        unsigned char count = 0;
        for (ClientList::const_iterator iter = mClients.begin(); iter != mClients.end(); ++iter)
            if ((*iter)->getStatus() == status)
                ++count;

        return count;
    };

private:
    bool isConnected();
    inline void timeOut(unsigned char client) {

        assert(mServer);
        mClients[client]->setStatus(ClientMgr::RCV_REPLY_ERROR);
        mTimeOutIdx = client;
        if (mStatus < CONN_GO)
            mStatus = CONN_TIMEOUT;
    };
    inline void reset() {

        LOGV(LOG_LEVEL_CONNEXION, 0, LOG_FORMAT(), __PRETTY_FUNCTION__, __LINE__);
        assert(mServer);

        mTimeOutIdx = 0;
        mStatus = CONN_WAIT; // Back to initial step (wait client)
        for (unsigned char i = 0; i < static_cast<unsigned char>(mClients.size()); ++i)
            if (mClients[i]->getStatus() == ClientMgr::RCV_REPLY_ERROR) {

                mTimeOutIdx = i;
                mStatus = CONN_TIMEOUT;
                break;
            }
    };

    //
    inline bool isAnyStatus(unsigned char status) const {

        assert(mServer);
        for (ClientList::const_iterator iter = mClients.begin(); iter != mClients.end(); ++iter)
            if ((*iter)->getStatus() == status)
                return true;

        return false;
    };
    inline bool isAllStatus(unsigned char status, unsigned char otherwise = ClientMgr::RCV_REPLY_RESERVED) const {

        assert(mServer);
        for (ClientList::const_iterator iter = mClients.begin(); iter != mClients.end(); ++iter)
            if (((*iter)->getStatus() != status) && ((*iter)->getStatus() != otherwise))
                return false;

        LOGI(LOG_LEVEL_CONNEXION, 0, LOG_FORMAT(" - All client are %d status (or %d)"), __PRETTY_FUNCTION__, __LINE__,
                status, otherwise);
        return true;
    };

    void addCheckSum(const char* cmd, size_t len, int security);
    static void assignSecurity(ClientMgr* mgr);

    inline unsigned int extractPicSize(unsigned char client) const {

        LOGV(LOG_LEVEL_CONNEXION, 0, LOG_FORMAT(" - c:%d"), __PRETTY_FUNCTION__, __LINE__, client);
        assert(mServer);

        unsigned int size = static_cast<unsigned char>(mClients[client]->getRcvBuffer()[REPLY_PICSIZE_IDX - 1]) << 16;
        size |= static_cast<unsigned char>(mClients[client]->getRcvBuffer()[REPLY_PICSIZE_IDX]) << 8;
        size |= static_cast<unsigned char>(mClients[client]->getRcvBuffer()[REPLY_PICSIZE_IDX + 1]);

        LOGI(LOG_LEVEL_CONNEXION, 0, LOG_FORMAT(" - Picture size: %d"), __PRETTY_FUNCTION__, __LINE__, size);
        return size;
    };
    bool isExpectedReply(short rlen, const char* cmd, size_t clen, unsigned char client);

    bool replyUpload(short packet);
    bool send(const char* cmd, size_t len, unsigned char client = 0, unsigned char reply = 0);
    unsigned char receive(time_t now, time_t timeout = 0);

public:
    static bool verifyCheckSum(const ClientMgr* mgr);
    static bool verifySecurity(const ClientMgr* mgr);

    //////
    bool open();
    inline void close() {

        LOGV(LOG_LEVEL_CONNEXION, 0, LOG_FORMAT(), __PRETTY_FUNCTION__, __LINE__);
        assert(mServer);

        mCloseAll = true;
    };
    bool start(int port);
    void connect(SearchIP* search);

    bool launch(unsigned char status, const char* cmd, size_t len); // CMD_READY | CMD_ORIENTATION
    void go(); // CMD_GO

};

#endif // CONNEXION_H_
