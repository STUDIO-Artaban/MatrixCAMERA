#include "ClientMgr.h"

//////
ClientMgr::ClientMgr(Socket* socket, unsigned char rank) : mSocket(socket), mRank(rank), mTimeOut(0), mSecurity(0),
    mStatus(RCV_REPLY_NONE), mRcvLength(0), mEOF(false), mTryCount(0), mPacketCount(0) { // , mRcvCount(0) {

    LOGV(LOG_LEVEL_CONNEXION, 0, LOG_FORMAT(" - s:%x; r:%d"), __PRETTY_FUNCTION__, __LINE__, socket, rank);
    if (!socket->isServer()) {

        mTimeOut = time(NULL);
        mStatus = RCV_REPLY_VERIFY;
    }
#ifdef __ANDROID__
    mAndroid = true;
#endif
    mAbort = false;
    mThread = new boost::thread(ClientMgr::startReceiveThread, this);
}
ClientMgr::~ClientMgr() {

    LOGV(LOG_LEVEL_CONNEXION, 0, LOG_FORMAT(), __PRETTY_FUNCTION__, __LINE__);
    mAbort = true;
    mThread->join();
    delete mThread;
}

void ClientMgr::receiveThreadRunning() {

    LOGV(LOG_LEVEL_CONNEXION, 0, LOG_FORMAT(" - Begin"), __PRETTY_FUNCTION__, __LINE__);
    while (!mAbort) {

        boost::this_thread::sleep(boost::posix_time::milliseconds(75));
        if ((mStatus > RCV_REPLY_ERROR) || ((!mSocket->isServer()) && (mStatus == RCV_REPLY_NONE))) {

            if (mEOF)
                continue;

            char rcv[MAX_PACKET_SIZE + 1];
            mMutex.lock();
            int rcvLen = static_cast<int>(mSocket->receive(rcv, MAX_PACKET_SIZE, mRank));
            mMutex.unlock();
            if (rcvLen < 0) {

                switch (errno) {
                    case EWOULDBLOCK: { // EAGAIN
                        if (mRcvLength) {

                            LOGI(LOG_LEVEL_CONNEXION, 0, LOG_FORMAT(" - End of data: %d bytes (cli:%d; cls:%d)"),
                                    __PRETTY_FUNCTION__, __LINE__, mRcvLength, (mSocket->isServer())? mRank:LIBENG_NO_DATA,
                                    mStatus);
                            //++mRcvCount;
                            mEOF = true;
                        }
                        break; // No/No more data to read
                    }
                    default: {

                        LOGE(LOG_FORMAT(" - Received data error: %d (cli:%d; cls:%d)"), __PRETTY_FUNCTION__, __LINE__,
                                rcvLen, (mSocket->isServer())? mRank:LIBENG_NO_DATA, mStatus);
                        mStatus = RCV_REPLY_ERROR;
                        //assert(NULL); // Error / Unexpected socket close
                        break;
                    }
                }
                if (mStatus == RCV_REPLY_ERROR)
                    break;

                continue;
            }
            if (!rcvLen) {

                LOGW(LOG_FORMAT(" - Unexpected socket close (cli:%d; cls:%d)"), __PRETTY_FUNCTION__, __LINE__,
                        (mSocket->isServer())? mRank:LIBENG_NO_DATA, mStatus);
                mStatus = RCV_REPLY_ERROR;
                break;
            }
#ifdef DEBUG
            LOGI(LOG_LEVEL_CONNEXION, 0, LOG_FORMAT(" - Data received: %d/%d bytes (cli:%d; cls:%d)"), __PRETTY_FUNCTION__,
                        __LINE__, rcvLen, mRcvLength, (mSocket->isServer())? mRank:LIBENG_NO_DATA, mStatus);
#endif
            std::memcpy(mRcvBuffer + mRcvLength, rcv, rcvLen);
            mRcvLength += rcvLen;
            mTimeOut = time(NULL);
        }
    }
    LOGI(LOG_LEVEL_CONNEXION, 0, LOG_FORMAT(" - Finished"), __PRETTY_FUNCTION__, __LINE__);
}
void ClientMgr::startReceiveThread(ClientMgr* mgr) {

    LOGV(LOG_LEVEL_CONNEXION, 0, LOG_FORMAT(), __PRETTY_FUNCTION__, __LINE__);
    mgr->receiveThreadRunning();
}
