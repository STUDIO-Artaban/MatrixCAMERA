#ifndef CLIENTMGR_H_
#define CLIENTMGR_H_

#include "Global.h"

#include <libeng/Log/Log.h>
#include <libeng/Features/Internet/Socket.h>
#include <boost/thread.hpp>
#include <time.h>

#define MAX_PACKET_SIZE             (65536 - 1) // == USHRT_MAX

using namespace eng;

//////
class ClientMgr {

private:
    int mSecurity; // [0;65535]
    //unsigned int mRcvCount;
    char mRcvBuffer[MAX_PACKET_SIZE + 1];

    unsigned short mRcvLength;

    Socket* mSocket;
    unsigned char mRank; // == Frame No for client socket
    unsigned char mTryCount; // Download try count
    short mPacketCount; // Current download/upload packet count

    time_t mTimeOut;
    bool mEOF; // End Of File when 'Socket::receive' returns 0 (with 'mRcvLength' == 0 means no data received yet)
#ifdef __ANDROID__
    bool mAndroid; // Server/Client OS
#endif

    volatile bool mAbort;
    boost::mutex mMutex;
    boost::thread* mThread;

    void receiveThreadRunning();
    static void startReceiveThread(ClientMgr* mgr);

public:
    enum {

        RCV_REPLY_NONE = 0, // Wait & Receive request for client socket
        RCV_REPLY_ERROR,

        RCV_REPLY_VERIFY,
        RCV_REPLY_KEEPALIVE,
        RCV_REPLY_ORIENTATION,
        RCV_REPLY_READY,
        RCV_REPLY_GO,

        RCV_REPLY_GET,
        RCV_REPLY_DOWNLOAD, // ...
        RCV_REPLY_UPLOAD, // ...from server point of view

        RCV_REPLY_RESERVED = 0xff // Reserved (see 'Connexion::isAllStatus' method)
    };

private:
    unsigned char mStatus;

public:
    ClientMgr(Socket* socket, unsigned char rank);
    virtual ~ClientMgr();

    inline unsigned char getStatus() const { return mStatus; }
    inline time_t getTimeOut() const { return mTimeOut; }
    inline unsigned char getRank() const { return mRank; }
    inline unsigned char getTryCount() const { return mTryCount; }
    inline int getSecurity() const { return mSecurity; }
    //inline unsigned int getRcvCount() const { return mRcvCount; }
    inline unsigned short getRcvLength() const { return mRcvLength; }
    inline const char* getRcvBuffer() const { return mRcvBuffer; }
    inline short getPacketCount() const { return mPacketCount; }

    inline bool isEOF() const { return mEOF; }
#ifdef __ANDROID__
    inline bool isAndroid() const { return mAndroid; }
#endif

    inline void setStatus(unsigned char status) { mStatus = status; }
    inline void setSecurity(int security) { mSecurity = security; }
    inline void setTimeOut(time_t now) { mTimeOut = now; }
    inline void setRank(unsigned char rank) { mRank = rank; }
    inline void setTryCount(unsigned char tryCount) {

        mTryCount = tryCount;

        // BUG: 'read' socket returns EWOULDBLOCK with pending datas
        // -> Reset 'mEOF' flag only
        if (mTryCount)
            mEOF = false;
    }
    //inline void setRcvCount(unsigned int count) { mRcvCount = count; }
    inline void setPacketCount(short count) { mPacketCount = count; }
#ifdef __ANDROID__
    inline void setOS(bool android) { mAndroid = android; }
#endif

    //////
    inline void lock() { mMutex.lock(); }
    inline void unlock() { mMutex.unlock(); }

    inline void reset() {

        LOGV(LOG_LEVEL_CONNEXION, 0, LOG_FORMAT(), __PRETTY_FUNCTION__, __LINE__);
        mEOF = false;
        mRcvLength = 0;
        mTimeOut = time(NULL);
        mStatus = RCV_REPLY_NONE;
    }
};

typedef std::vector<ClientMgr*> ClientList;

#endif // CLIENTMGR_H_
