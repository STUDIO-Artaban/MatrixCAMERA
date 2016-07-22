#include "Connexion.h"

#include <libeng/Features/Camera/Camera.h>

#ifdef __ANDROID__
#include "Level/MatrixLevel.h"
#else
#include "MatrixLevel.h"
#endif

#define KEEPALIVE_INTERVAL          15 // In second
#define KEEPALIVE_TIMEOUT           2 // ... + KEEPALIVE_INTERVAL (also used to wait CMD_UPLOAD request)
#define READY_TIMEOUT               50 // In milliseconds (while !mAbort loop count)

#define MAX_IP_IDX                  0xff
#define MAX_MESSAGE_SIZE            (MAX_PACKET_SIZE - SECURITY_LEN - CHECKSUM_LEN)
#define MAX_TRY_COUNT               12 // + 1 tries

#define ORIENTATION_LAND            '1'
#define ORIENTATION_PORT            '0'
#define DOWNLOAD_FROM_BEGIN         ORIENTATION_LAND
#define DOWNLOAD_NEXT_PART          ORIENTATION_PORT
#ifdef __ANDROID__
#define OS_ANDROID                  ORIENTATION_LAND
#endif
#define OS_IOS                      ORIENTATION_PORT

// Commands
#define CMD_VERIFY                  "VERIF_MCAM#"
#define CMD_KEEPALIVE               "KEEPALIVE_MCAM#"
//#define CMD_SYNCHRO               Send 'time_t' & Reply received 'time_t' to check delay
#define CMD_GO                      "1"
#define CMD_REPLY_GO                "DONE_MCAM#"
#define CMD_DOWNLOAD                "DOWNLOAD_PIC_MCAM#"
#define CMD_UPLOAD                  "UPLOAD_MCAM#"

#define VERIFY_LEN                  ((sizeof(CMD_VERIFY) - 1) + 3 + SECURITY_LEN + CHECKSUM_LEN) // + 3 -> Frame rank + Orientation + Server OS
#define VERIFY_REPLY_LEN            ((sizeof(MCAM_VERSION) - 1) + VERIFY_LEN - 1) // + 3 (above) - 1 -> + 2 -> Main status + Client OS
#define KEEPALIVE_LEN               ((sizeof(CMD_KEEPALIVE) - 1) + SECURITY_LEN + CHECKSUM_LEN)
#define ORIENTATION_REPLY_LEN       (ORIENTATION_LEN - 1)
#define GO_REPLY_LEN                ((sizeof(CMD_REPLY_GO) - 1) + SECURITY_LEN + CHECKSUM_LEN)
#define GET_LEN                     ((sizeof(CMD_GET) - 1) + SECURITY_LEN + CHECKSUM_LEN)
#define GET_REPLY_LEN               (GET_LEN + 3) // + 3 -> DIGIT_COUNT(CAM_WIDTH * CAM_HEIGHT * 4) > Picture size (JPEG)
#define DOWNLOAD_LEN                ((sizeof(CMD_DOWNLOAD) - 1) + 1 + SECURITY_LEN + CHECKSUM_LEN) // + 1 -> '1' from begin / '0' next part
#define UPLOAD_LEN                  ((sizeof(CMD_UPLOAD) - 1) + 5 + SECURITY_LEN + CHECKSUM_LEN) // + 5 -> Video file size (4) + FPS (1)
#define UPLOAD_REPLY_LEN            ((sizeof(CMD_UPLOAD) - 1) + 2 + SECURITY_LEN + CHECKSUM_LEN) // + 2 -> Packet requested by the client [0;n]

// Indexes
#define VERIFY_FRAMENO_IDX          (sizeof(CMD_VERIFY) - 1)
#define VERIFY_ORIENTATION_IDX      (VERIFY_FRAMENO_IDX + 1)
#define VERIFY_OS_IDX               (VERIFY_ORIENTATION_IDX + 1)
#define ORIENTATION_IDX             (sizeof(CMD_ORIENTATION) - 1)
#define DOWNLOAD_FROM_IDX           (sizeof(CMD_DOWNLOAD) - 1)
#define UPLOAD_SIZE_IDX             (sizeof(CMD_UPLOAD) - 1)
#define UPLOAD_FPS_IDX              ((sizeof(CMD_UPLOAD) - 1) + 4)

#define REPLY_STATUS_IDX            (sizeof(CMD_VERIFY) - 1 + sizeof(MCAM_VERSION) - 1)
#ifdef __ANDROID__
#define REPLY_OS_IDX                (REPLY_STATUS_IDX + 1)
#endif

//////
Connexion::Connexion(bool server, Level* caller, bool connected) : mServer(server), mCaller(caller), mAbort(true),
        mThread(NULL), mStatus(CONN_OPEN), mTimeOutIdx(0), mVideo(static_cast<MatrixLevel*>(caller)->getVideo()),
        mPicSize(0), mCurClient(NULL), mSearch(NULL), mConnected(connected), mCloseAll(false) {

    LOGV(LOG_LEVEL_CONNEXION, 0, LOG_FORMAT(" - s:%s"), __PRETTY_FUNCTION__, __LINE__, (server)? "true":"false");
    mSocket = new Socket(server);
}
Connexion::~Connexion() {

    LOGV(LOG_LEVEL_CONNEXION, 0, LOG_FORMAT(), __PRETTY_FUNCTION__, __LINE__);
    if (mThread) {

        mAbort = true;
        mThread->join();
        delete mThread;
    }
    if (mCurClient) delete mCurClient;
    for (ClientList::iterator iter = mClients.begin(); iter != mClients.end(); ++iter)
        delete (*iter);
    mClients.clear();
#ifdef __ANDROID__
    for (Video::FrameList::iterator iter = mFrames.begin(); iter != mFrames.end(); ++iter)
        delete (*iter);
#endif
    mFrames.clear();

    delete mSocket;
}

bool Connexion::open() {

    LOGV(LOG_LEVEL_CONNEXION, 0, LOG_FORMAT(), __PRETTY_FUNCTION__, __LINE__);
    assert(mStatus == CONN_OPEN);

    if ((mServer) && (!mSocket->open()))
        return false;

    mStatus = (mServer)? CONN_START:CONN_CONNEXION;
    return true;
}
bool Connexion::start(int port) {

    LOGV(LOG_LEVEL_CONNEXION, 0, LOG_FORMAT(" - p:%d"), __PRETTY_FUNCTION__, __LINE__, port);
    assert(mServer);
    assert(mStatus == CONN_START);

    if (!mSocket->start(port))
        return false;

    mStatus = CONN_WAIT;
    mAbort = false;
    mThread = new boost::thread(Connexion::startConnexionThread, this);
    return true;
}
void Connexion::connect(SearchIP* search) {

    assert(!mServer);
    if (mSearch)
        return;

    LOGV(LOG_LEVEL_CONNEXION, 0, LOG_FORMAT(" - s:%x"), __PRETTY_FUNCTION__, __LINE__, search);
    assert(mStatus == CONN_CONNEXION);
    assert(!mCurClient);
    mSearch = search;

    mAbort = false;
    mThread = new boost::thread(Connexion::startConnexionThread, this);
}

bool Connexion::isConnected() {

    assert(!mServer);
    assert(mSearch);
    if (mStatus != CONN_CONNEXION)
        return true; // Already connected

    if (!mSearch->isRunning()) {

        static unsigned char ip = MAX_IP_IDX;
        if (MAX_IP_IDX == ip) {
            if (!mSearch->getCount()) {

                mSearch->start(mConnected);
                return false;
            }
            ip = mSearch->getCount();
        }
        static int port = INITIAL_PORT_NO;
        if (mSocket->connexion(mSearch->getIP(--ip), port)) {

            LOGI(LOG_LEVEL_CONNEXION, 0, LOG_FORMAT(" - Connected: %s:%d"), __PRETTY_FUNCTION__, __LINE__,
                    mSearch->getIP(ip).c_str(), port);
            port = INITIAL_PORT_NO;
            ip = MAX_IP_IDX;

            mCurClient = new ClientMgr(mSocket, 0);
            mStatus = CONN_VERIFY;
            return true;
        }
        if (!ip) {
            ip = mSearch->getCount();
            if (++port > MAX_PORT_NO) {

                port = INITIAL_PORT_NO;
                ip = MAX_IP_IDX;
                mSearch->start(mConnected);
            }
        }
    }
    return false;
}

void Connexion::addCheckSum(const char* cmd, size_t len, int security) {

    LOGV(LOG_LEVEL_CONNEXION, 0, LOG_FORMAT(" - c:%x; l:%d; s:%d"), __PRETTY_FUNCTION__, __LINE__, cmd, len, security);
    assert((len - SECURITY_LEN - CHECKSUM_LEN) <= MAX_MESSAGE_SIZE);
    assert(security > 0);

    len -= SECURITY_LEN + CHECKSUM_LEN;
    std::memcpy(mSendBuffer, cmd, len);

    mSendBuffer[len] = static_cast<char>(security >> 8);
    mSendBuffer[len + 1] = static_cast<char>(security);

    unsigned int checkSum = 0;
    for (unsigned short i = 0; i < (len + 2); ++i)
        checkSum += static_cast<unsigned char>(mSendBuffer[i]);

    mSendBuffer[len + 2] = static_cast<char>(checkSum >> 16);
    mSendBuffer[len + 3] = static_cast<char>(checkSum >> 8);
    mSendBuffer[len + 4] = static_cast<char>(checkSum);
}
bool Connexion::verifyCheckSum(const ClientMgr* mgr) {

    LOGV(LOG_LEVEL_CONNEXION, 0, LOG_FORMAT(" - m:%x"), __PRETTY_FUNCTION__, __LINE__, mgr);
    assert(mgr->getRcvLength() > (SECURITY_LEN + CHECKSUM_LEN)); // Always check this B4 calling this

    unsigned int checkSum = 0;
    for (unsigned short i = 0; i < (mgr->getRcvLength() - CHECKSUM_LEN); ++i)
        checkSum += static_cast<unsigned char>(mgr->getRcvBuffer()[i]);

    unsigned int checkComp = static_cast<unsigned char>(mgr->getRcvBuffer()[mgr->getRcvLength() - 3]) << 16;
    checkComp |= static_cast<unsigned char>(mgr->getRcvBuffer()[mgr->getRcvLength() - 2]) << 8;
    checkComp |= static_cast<unsigned char>(mgr->getRcvBuffer()[mgr->getRcvLength() - 1]);

    LOGI(LOG_LEVEL_CONNEXION, 0, LOG_FORMAT(" - Checksum %u == Received %u"), __PRETTY_FUNCTION__, __LINE__, checkSum,
            checkComp);
    return (checkSum == checkComp);
}
void Connexion::assignSecurity(ClientMgr* mgr) {

    LOGV(LOG_LEVEL_CONNEXION, 0, LOG_FORMAT(" - m:%x"), __PRETTY_FUNCTION__, __LINE__, mgr);
    assert(mgr->getRcvLength() > (SECURITY_LEN + CHECKSUM_LEN)); // Always check this B4 calling this
    //assert(!mServer);

    int security = static_cast<unsigned char>(mgr->getRcvBuffer()[mgr->getRcvLength() - 5]) << 8;
    security |= static_cast<unsigned char>(mgr->getRcvBuffer()[mgr->getRcvLength() - 4]);

    //LOGI(LOG_LEVEL_CONNEXION, 0, LOG_FORMAT(" - Reply security %d (cnt:%d)"), __PRETTY_FUNCTION__, __LINE__, security,
    //        mgr->getRcvCount());
    LOGI(LOG_LEVEL_CONNEXION, 0, LOG_FORMAT(" - Reply security %d"), __PRETTY_FUNCTION__, __LINE__, security);
    mgr->setSecurity(security);
}
bool Connexion::verifySecurity(const ClientMgr* mgr) {

    LOGV(LOG_LEVEL_CONNEXION, 0, LOG_FORMAT(" - m:%x"), __PRETTY_FUNCTION__, __LINE__, mgr);
    assert(mgr->getRcvLength() > (SECURITY_LEN + CHECKSUM_LEN)); // Always check this B4 calling this

    int security = static_cast<unsigned char>(mgr->getRcvBuffer()[mgr->getRcvLength() - 5]) << 8;
    security |= static_cast<unsigned char>(mgr->getRcvBuffer()[mgr->getRcvLength() - 4]);

    //LOGI(LOG_LEVEL_CONNEXION, 0, LOG_FORMAT(" - Received %d == Security %d"), __PRETTY_FUNCTION__, __LINE__, security,
    //        ((mgr->getRcvCount()) % 5)? (mgr->getSecurity() / 2):(mgr->getSecurity() / 3));
    LOGI(LOG_LEVEL_CONNEXION, 0, LOG_FORMAT(" - Received %d == Security %d"), __PRETTY_FUNCTION__, __LINE__, security,
            mgr->getSecurity());
    //return (mgr->getRcvCount() % 5)? (security == (mgr->getSecurity() / 2)):(security == (mgr->getSecurity() / 3));
    return (security == mgr->getSecurity());
}

bool Connexion::isExpectedReply(short rlen, const char* cmd, size_t clen, unsigned char client) {

    LOGV(LOG_LEVEL_CONNEXION, 0, LOG_FORMAT(" - rl:%d; c:%s; cl:%d; c:%d"), __PRETTY_FUNCTION__, __LINE__, rlen, cmd,
            clen, client);
    assert(mServer);

    if ((mClients[client]->getRcvLength() != rlen) ||
            (std::memcmp(mClients[client]->getRcvBuffer(), cmd, clen)) ||
            (!verifyCheckSum(mClients[client])) || (!verifySecurity(mClients[client]))) {

        LOGE(LOG_FORMAT(" - Wrong %s message replied (cli:%d)"), __PRETTY_FUNCTION__, __LINE__, cmd, client);
        timeOut(client); // Error
        return false;
    }
    return true;
}

bool Connexion::launch(unsigned char status, const char* cmd, size_t len) {

    LOGV(LOG_LEVEL_CONNEXION, 0, LOG_FORMAT(" - s:%d; c:%s; l:%d"), __PRETTY_FUNCTION__, __LINE__, status, cmd, len);
    assert(mServer);

    if ((!isAllStatus(ClientMgr::RCV_REPLY_NONE)) || (!mMutex.try_lock()))
        return false;

    assert(!mClients.empty());
    mStatus = status;
    for (unsigned char i = 0; i < static_cast<unsigned char>(mClients.size()); ++i) {

        int security = (std::rand() % 65536); // [0;65535]
        addCheckSum(cmd, len, security);
        if (mSocket->send(mSendBuffer, len, i) != static_cast<int>(len)) {

            LOGE(LOG_FORMAT(" - Failed to send command (cli:%d; cns:%d"), __PRETTY_FUNCTION__, __LINE__, i, mStatus);
            timeOut(i);
            break;
        }
        mClients[i]->setSecurity(security);
        mClients[i]->setTimeOut(time(NULL));

        unsigned char reply = ClientMgr::RCV_REPLY_ERROR;
        switch (status) {

            case CONN_ORIENTATION: reply = ClientMgr::RCV_REPLY_ORIENTATION; break;
            case CONN_READY: reply = ClientMgr::RCV_REPLY_READY; break;
#ifdef DEBUG
            default: {
                LOGE(LOG_FORMAT(" - Unknown command"), __PRETTY_FUNCTION__, __LINE__);
                assert(NULL);
                break;
            }
#endif
        }
        mClients[i]->setStatus(reply);
    }
    mMutex.unlock();
    return true;
}
void Connexion::go() {

    LOGV(LOG_LEVEL_CONNEXION, 0, LOG_FORMAT(), __PRETTY_FUNCTION__, __LINE__);
    assert(mServer);

    for (unsigned char i = 0; i < static_cast<unsigned char>(mClients.size()); ++i) {

        if (mClients[i]->getStatus() != ClientMgr::RCV_REPLY_NONE)
            break; // Can be in error status

        mSocket->send(CMD_GO, 1, i); // If failed client will not receive CMD_GO but an unexpected close connexion (MCAM_DELAY_TIMEOUT)
        mClients[i]->setTimeOut(time(NULL));
        mClients[i]->setStatus(ClientMgr::RCV_REPLY_GO);
    }
    mStatus = CONN_DOWNLOAD;
}

bool Connexion::replyUpload(short packet) {

    LOGV(LOG_LEVEL_CONNEXION, 0, LOG_FORMAT(" - p:%d"), __PRETTY_FUNCTION__, __LINE__, packet);
    assert(!mServer);

    char reply[UPLOAD_REPLY_LEN] = {0};
    std::memcpy(reply, CMD_UPLOAD, sizeof(CMD_UPLOAD) - 1);

    // Add packet index requested [0;n]
    reply[UPLOAD_SIZE_IDX] = static_cast<char>(packet >> 8);
    reply[UPLOAD_SIZE_IDX + 1] = static_cast<char>(packet);

    return send(reply, UPLOAD_REPLY_LEN);
}
bool Connexion::send(const char* data, size_t len, unsigned char client, unsigned char reply) {

#ifdef DEBUG
    char log[128] = {0};
    std::memcpy(log, data, (len > 127)? 127:len);
    //LOGV(LOG_LEVEL_CONNEXION, 0, LOG_FORMAT(" - d:%s; l:%d; c:%d; r:%d (s:%s; cnt:%d)"), __PRETTY_FUNCTION__, __LINE__,
    //        log, len, client, reply, (mServer)? "true":"false", (mServer)? mClients[client]->getRcvCount():
    //        mCurClient->getRcvCount());
    LOGV(LOG_LEVEL_CONNEXION, 0, LOG_FORMAT(" - d:%s; l:%d; c:%d; r:%d (s:%s)"), __PRETTY_FUNCTION__, __LINE__, log, len,
            client, reply, (mServer)? "true":"false");
    assert(len <= MAX_PACKET_SIZE);
#endif
    if (mServer) { ////// Server

        int security = (std::rand() % 65536); // [0;65535]
        addCheckSum(data, len, security);
        if (mSocket->send(mSendBuffer, len, client) != len) {

            LOGE(LOG_FORMAT(" - Failed to send data (cli:%d; cls:%d"), __PRETTY_FUNCTION__, __LINE__, client, reply);
            timeOut(client); // Error
            return false;
        }
        mClients[client]->setSecurity(security);
        mClients[client]->setTimeOut(time(NULL));
        mClients[client]->setStatus(reply);
    }
    else { ////// Client

        //int security = (mCurClient->getRcvCount() % 5)? (mCurClient->getSecurity() / 2):(mCurClient->getSecurity() / 3);
        int security = mCurClient->getSecurity();
        addCheckSum(data, len, security);
        if (mSocket->send(mSendBuffer, len, 0) != len) {

            LOGE(LOG_FORMAT(" - Failed to send data: %s"), __PRETTY_FUNCTION__, __LINE__, data);
            mStatus = CONN_TIMEOUT; // Error
            return false;
        }
        mCurClient->setTimeOut(time(NULL));
    }
    return true;
}
unsigned char Connexion::receive(time_t now, time_t timeout) {

    if (mServer) { ////// Server

        for (unsigned char i = 0; i < static_cast<unsigned char>(mClients.size()); ++i) {
            if ((mClients[i]->getStatus() != ClientMgr::RCV_REPLY_NONE) &&
                    (mClients[i]->getStatus() != ClientMgr::RCV_REPLY_ERROR)) {

                if (difftime(now, mClients[i]->getTimeOut()) > MCAM_DELAY_TIMEOUT) {

                    LOGW(LOG_FORMAT(" - Timed out (cli:%d)"), __PRETTY_FUNCTION__, __LINE__, i);
                    timeOut(i);
                    break;
                }
                if (!mClients[i]->isEOF())
                    continue;
            }
            switch (mClients[i]->getStatus()) {
                case ClientMgr::RCV_REPLY_NONE: {
                    if ((mStatus == CONN_DOWNLOAD) || (mStatus == CONN_UPLOAD))
                        break; // No keep alive for those status

                    if (difftime(now, mClients[i]->getTimeOut()) > KEEPALIVE_INTERVAL) {

                        LOGI(LOG_LEVEL_CONNEXION, 0, LOG_FORMAT(" - Request keep alive (cli:%d)"), __PRETTY_FUNCTION__,
                                __LINE__, i);
                        if (!send(CMD_KEEPALIVE, KEEPALIVE_LEN, i, ClientMgr::RCV_REPLY_KEEPALIVE))
                            break;
                    }
                    break;
                }
                case ClientMgr::RCV_REPLY_KEEPALIVE: {

                    if (!isExpectedReply(KEEPALIVE_LEN, CMD_KEEPALIVE, sizeof(CMD_KEEPALIVE) - 1, i)) break;
                    mClients[i]->reset();
                    break;
                }
                case ClientMgr::RCV_REPLY_VERIFY: {

                    if (!isExpectedReply(VERIFY_REPLY_LEN, CMD_VERIFY, sizeof(CMD_VERIFY) - 1, i)) break;
                    if (std::memcmp(mClients[i]->getRcvBuffer() + sizeof(CMD_VERIFY),
                            MCAM_VERSION, sizeof(MCAM_VERSION) - 1) > 0) { // Check application version compatibility

                        LOGE(LOG_FORMAT(" - Wrong client %d version (>%s)"), __PRETTY_FUNCTION__, __LINE__, i, MCAM_VERSION);
                        timeOut(i); // Wrong application version (current version < client version)
                        break;
                    }

                    // Check client status (main status)
                    switch (static_cast<unsigned char>(mClients[i]->getRcvBuffer()[REPLY_STATUS_IDX])) {

                        case MatrixLevel::MCAM_NONE:
                        case MatrixLevel::MCAM_WAIT:
                            break;

                        case MatrixLevel::MCAM_PAUSED:
                            break;
                    }
#ifdef __ANDROID__
                    LOGI(LOG_LEVEL_CONNEXION, 0, LOG_FORMAT(" - Client OS: %s (cli:%d)"), __PRETTY_FUNCTION__, __LINE__,
                            (mClients[i]->getRcvBuffer()[REPLY_OS_IDX] == OS_ANDROID)? "Android":"iOS", i);
                    mClients[i]->setOS(mClients[i]->getRcvBuffer()[REPLY_OS_IDX] == OS_ANDROID); // Client OS
#endif
                    mClients[i]->reset();

                    if (!isAnyStatus(ClientMgr::RCV_REPLY_VERIFY))
                        mStatus = CONN_WAIT;
                    //else // Still client(s) in verify status
                    break;
                }
                case ClientMgr::RCV_REPLY_ORIENTATION: {

                    if (!isExpectedReply(ORIENTATION_REPLY_LEN, CMD_ORIENTATION, sizeof(CMD_ORIENTATION) - 1, i)) break;
                    mClients[i]->reset();

                    if (!isAnyStatus(ClientMgr::RCV_REPLY_ORIENTATION))
                        mStatus = CONN_WAIT;
                    //else // Still client(s) in orientation status
                    break;
                }
                case ClientMgr::RCV_REPLY_READY: {

                    if (!isExpectedReply(READY_LEN, CMD_READY, sizeof(CMD_READY) - 1, i)) break;
                    mClients[i]->reset();

                    if (!isAnyStatus(ClientMgr::RCV_REPLY_READY))
                        mStatus = CONN_GO;
                    //else // Still client(s) in ready status
                    break;
                }

                // No time out/error management from here!
                case ClientMgr::RCV_REPLY_GO: {

                    assert(mStatus == CONN_DOWNLOAD);
                    if (!isExpectedReply(GO_REPLY_LEN, CMD_REPLY_GO, sizeof(CMD_REPLY_GO) - 1, i)) break;
                    mClients[i]->reset();

                    // Send CMD_GET
                    send(CMD_GET, GET_LEN, i, ClientMgr::RCV_REPLY_GET);
                    break;
                }
                case ClientMgr::RCV_REPLY_GET: {

                    assert(mStatus == CONN_DOWNLOAD);
                    if (!isExpectedReply(GET_REPLY_LEN, CMD_GET, sizeof(CMD_GET) - 1, i)) break;
                    mClients[i]->reset();

                    // Check if ready to download frame picture (check picture size)
                    mPicSize = extractPicSize(i);
                    if (!mPicSize) {
                        send(CMD_GET, GET_LEN, i, ClientMgr::RCV_REPLY_GET); // Send CMD_GET again
                        break;
                    }
                    mClients[i]->setTryCount(0);
                    mVideo->add(new Picture(mPicSize), i);

                    // Send CMD_DOWNLOAD
                    std::string cmd(CMD_DOWNLOAD);
                    cmd += DOWNLOAD_FROM_BEGIN;
                    send(cmd.c_str(), DOWNLOAD_LEN, i, ClientMgr::RCV_REPLY_DOWNLOAD);
                    break;
                }
                case ClientMgr::RCV_REPLY_DOWNLOAD: {

                    assert(mStatus == CONN_DOWNLOAD);
                    assert(mVideo->get(i));

                    char step = DOWNLOAD_NEXT_PART; // Download next part of the picture buffer
                    signed char res = mVideo->get(i)->fill(mClients[i]);
                    if (res < 0) { // Error

                        //LOGW(LOG_FORMAT(" - Retry to download (%d/%d)"), __PRETTY_FUNCTION__, __LINE__,
                        //        mClients[i]->getTryCount(), MAX_TRY_COUNT);

                        LOGW(LOG_FORMAT(" - Unexpected/Partial data received (%d/%d)"), __PRETTY_FUNCTION__, __LINE__,
                                mClients[i]->getTryCount(), MAX_TRY_COUNT);
                        // BUG: 'read' socket returns EWOULDBLOCK with pending datas
                        // -> Retry to received the expected data count
                        if (mClients[i]->getTryCount() > MAX_TRY_COUNT) {
                            mClients[i]->setStatus(ClientMgr::RCV_REPLY_ERROR);
                            break; // Cancel it
                        }
                        mClients[i]->setTryCount(mClients[i]->getTryCount() + 1);
                        // See 'setTryCount' method definition where only EOF flag is reseted
                        break;

                        //mVideo->get(i)->retry(mPicSize);
                        //step = DOWNLOAD_FROM_BEGIN; // Restart download from begining
                    }
                    mClients[i]->setTryCount(0); // BUG: 'read' socket returns EWOULDBLOCK with pending datas
                    mClients[i]->reset();

                    if (!res) {
#ifndef PAID_VERSION
                        mVideo->get(i)->save(NULL, static_cast<const MatrixLevel*>(mCaller)->mLandscape, i);
#else
                        mVideo->get(i)->save(static_cast<const MatrixLevel*>(mCaller)->mLandscape, i);
#endif
                        if (mVideo->get(i)->isError())
                            mClients[i]->setStatus(ClientMgr::RCV_REPLY_ERROR);
                        else // Client status is still at ClientMgr::RCV_REPLY_NONE
                            mClients[i]->setPacketCount(0);
                    }
                    else {

                        // Send CMD_DOWNLOAD again
                        std::string cmd(CMD_DOWNLOAD);
                        cmd += step;
                        send(cmd.c_str(), DOWNLOAD_LEN, i, ClientMgr::RCV_REPLY_DOWNLOAD);
                    }
                    break;
                }
                case ClientMgr::RCV_REPLY_UPLOAD: {

                    assert(mStatus == CONN_UPLOAD);
#ifdef __ANDROID__
                    mVideo->select(mClients[i]->isAndroid());
#endif
                    assert(mVideo->getBuffer()); // Always true even for iOS clients (see 'mVideo->open()' call below)
                                                 // -> Coz no upload reply if no MOV file (see client side)
                    if (!isExpectedReply(UPLOAD_REPLY_LEN, CMD_UPLOAD, sizeof(CMD_UPLOAD) - 1, i)) break;
                    mClients[i]->reset();

                    short last = (mVideo->getSize() / MAX_MESSAGE_SIZE) + 1; // Last packet available + 1

                    // Get packet requested [0;n]
                    short packet = static_cast<unsigned char>(mClients[i]->getRcvBuffer()[UPLOAD_SIZE_IDX]) << 8;
                    packet |= static_cast<unsigned char>(mClients[i]->getRcvBuffer()[UPLOAD_SIZE_IDX + 1]);
                    if ((packet < 0) || (packet > last)) { // Error

                        LOGE(LOG_FORMAT(" - Wrong packet request received (cli:%d): %d (size:%d; last:%d)"),
                                __PRETTY_FUNCTION__, __LINE__, i, packet, mVideo->getSize(), last);
                        mClients[i]->setStatus(ClientMgr::RCV_REPLY_ERROR);
                        break;
                    }
                    if (packet == last) // Check end of packet requested
                        break; // Upload done! Client status at RCV_REPLY_NONE

                    // Send video buffer packet requested
                    LOGI(LOG_LEVEL_CONNEXION, 0, LOG_FORMAT(" - Send packet %d (cli:%d)"), __PRETTY_FUNCTION__, __LINE__,
                            packet, i);
                    size_t len = mVideo->getSize() - (packet * MAX_MESSAGE_SIZE);
                    send(mVideo->getBuffer() + (packet * MAX_MESSAGE_SIZE), (len > MAX_MESSAGE_SIZE)?
                            MAX_PACKET_SIZE:(len + SECURITY_LEN + CHECKSUM_LEN), i, ClientMgr::RCV_REPLY_UPLOAD);
                    break;
                }
                case ClientMgr::RCV_REPLY_ERROR: {

                    timeOut(i); // No status update if >= CONN_GO (nothing is done here)
                    break;
                }
            }
            if (CONN_TIMEOUT == mStatus) // Time out/Error
                break;
        }
        static bool recConverted = false;
        switch (mStatus) {

            case CONN_DOWNLOAD: {

                // Check no more client available after Go! pressed - Time out/Error management after CONN_GO here!
                if (isAllStatus(ClientMgr::RCV_REPLY_ERROR)) {

                    LOGW(LOG_FORMAT(" - No more valid client"), __PRETTY_FUNCTION__, __LINE__);
                    mTimeOutIdx = 0;
                    mStatus = CONN_TIMEOUT;
                    static_cast<MatrixLevel*>(mCaller)->mStatus = MatrixLevel::MCAM_INTERRUPT;
                    return ClientMgr::RCV_REPLY_NONE; // ...unused with server
                }

                // From here ClientMgr::RCV_REPLY_NONE client means download frame done!
                if (isAllStatus(ClientMgr::RCV_REPLY_NONE, ClientMgr::RCV_REPLY_ERROR)) {

                    if (!static_cast<const MatrixLevel*>(mCaller)->isRecReady())
                        break; // Wait to finish recording (after bullet time effect)

                    // Check if no bullet time effect to save
                    if (!getCountStatus(ClientMgr::RCV_REPLY_NONE)) {

                        reset();
                        static_cast<MatrixLevel*>(mCaller)->mStatus = MatrixLevel::MCAM_WAIT;
                        return ClientMgr::RCV_REPLY_NONE; // ...unused with server
                    }
                    duplicate(); // Store client list
                    recConverted = false;
                    mStatus = CONN_WAIT_UPLOAD; // Manage Keepalive
                    break;
                }
                break;
            }
            case CONN_WAIT_UPLOAD: {

                if ((!recConverted) && (!mVideo->getRecorder()->isConverted()))
                    break; // Record frame conversion from BIN to JPEG in progress

                // Prepare video creation
                if ((!recConverted) && (!mVideo->save(&mFrames, static_cast<const MatrixLevel*>(mCaller)->mLandscape))) {

                    reset();
                    static_cast<MatrixLevel*>(mCaller)->mStatus = MatrixLevel::MCAM_WAIT;
                    return ClientMgr::RCV_REPLY_NONE; // ...unused with server
                }
                recConverted = true;

                // Check video creation processus terminated (WebM &| MOV exists) & All clients are ready (Not in Keepalive processus)
                if ((mVideo->getStatus()) && (isAllStatus(ClientMgr::RCV_REPLY_NONE, ClientMgr::RCV_REPLY_ERROR))) {

                    if (mVideo->getStatus() < 0) { // Error (no video created)
                        mVideo->clear();
                        static_cast<MatrixLevel*>(mCaller)->mStatus = MatrixLevel::MCAM_TIMEOUT; // CONN_WAIT | CONN_TIMEOUT
                    }
                    else {

                        mVideo->extract(); // Prepare to display
                        mStatus = CONN_UPLOAD;

                        // Open video file to be uploaded to clients (WebM &| MOV)
                        if (mVideo->open()) {

                            char upload[UPLOAD_LEN + 1] = {0};
                            std::memcpy(upload, CMD_UPLOAD, sizeof(CMD_UPLOAD));
#ifdef __ANDROID__
                            // Send CMD_UPLOAD only to clients at RCV_REPLY_NONE status
                            for (unsigned char i = 0; i < static_cast<unsigned char>(mClients.size()); ++i) {
                                if (mClients[i]->getStatus() != ClientMgr::RCV_REPLY_NONE)
                                    continue;

                                mVideo->select(mClients[i]->isAndroid());

                                // Set video size according client OS:
                                // * Android -> WebM file size
                                // * iOS -> MOV file size
                                upload[UPLOAD_SIZE_IDX] = static_cast<char>(mVideo->getSize() >> 24);
                                upload[UPLOAD_SIZE_IDX + 1] = static_cast<char>(mVideo->getSize() >> 16);
                                upload[UPLOAD_SIZE_IDX + 2] = static_cast<char>(mVideo->getSize() >> 8);
                                upload[UPLOAD_SIZE_IDX + 3] = static_cast<char>(mVideo->getSize());

                                upload[UPLOAD_FPS_IDX] = static_cast<char>(mVideo->getFPS()); // Add FPS

                                send(upload, UPLOAD_LEN, i, ClientMgr::RCV_REPLY_UPLOAD);
                            }
#else
                            // Add video size (MOV file size)
                            upload[UPLOAD_SIZE_IDX] = static_cast<char>(mVideo->getSize() >> 24);
                            upload[UPLOAD_SIZE_IDX + 1] = static_cast<char>(mVideo->getSize() >> 16);
                            upload[UPLOAD_SIZE_IDX + 2] = static_cast<char>(mVideo->getSize() >> 8);
                            upload[UPLOAD_SIZE_IDX + 3] = static_cast<char>(mVideo->getSize());

                            upload[UPLOAD_FPS_IDX] = static_cast<char>(mVideo->getFPS()); // Add FPS

                            // Send CMD_UPLOAD only to clients at RCV_REPLY_NONE status
                            for (unsigned char i = 0; i < static_cast<unsigned char>(mClients.size()); ++i)
                                if (mClients[i]->getStatus() == ClientMgr::RCV_REPLY_NONE)
                                    send(upload, UPLOAD_LEN, i, ClientMgr::RCV_REPLY_UPLOAD);
#endif
                        }
                        //else // Clients still have RCV_REPLY_NONE status (but with CONN_UPLOAD so see below)
                    }
                }
                break;
            }
            case CONN_UPLOAD: {

                // From here ClientMgr::RCV_REPLY_NONE client means upload video done! / Failed to open video!
                if (isAllStatus(ClientMgr::RCV_REPLY_NONE, ClientMgr::RCV_REPLY_ERROR)) {

                    if (mVideo->getStatus()) { // Processus terminated (all JPEG picture are in RGBA buffers...

                        reset();
                        if (mVideo->getStatus() < 0) {
                            mVideo->clear();
                            static_cast<MatrixLevel*>(mCaller)->mStatus = MatrixLevel::MCAM_TIMEOUT; // CONN_WAIT | CONN_TIMEOUT
                        }
                        else // ...or 2 pictures at least)
                            static_cast<MatrixLevel*>(mCaller)->mStatus = MatrixLevel::MCAM_DISPLAY;
                    }
                    return ClientMgr::RCV_REPLY_NONE; // ...unused with server
                }

                // Upload in progress - Check processus terminated (all JPEG picture are in RGBA buffers...
                if (mVideo->getStatus() < 0) {
                    mVideo->clear();
                    static_cast<MatrixLevel*>(mCaller)->mStatus = MatrixLevel::MCAM_NO_DISPLAY; // Wait finish to upload
                }
                else if (mVideo->getStatus() > 0) // ...or 2 pictures at least)
                    static_cast<MatrixLevel*>(mCaller)->mStatus = MatrixLevel::MCAM_DISPLAY;
                break;
            }
        }
    }
    else { ////// Client

        if ((mStatus == CONN_UPLOAD) && (mVideo->isFilled())) { // Wait ready/error to display video
            if (!mVideo->getStatus()) // Processing...
                return ClientMgr::RCV_REPLY_NONE;

            mStatus = CONN_WAIT;
            static_cast<MatrixLevel*>(mCaller)->mStatus = (mVideo->getStatus() < 0)?
                    MatrixLevel::MCAM_WAIT:MatrixLevel::MCAM_DISPLAY;
        }

        // Check error...
        if (mCurClient->getStatus() == ClientMgr::RCV_REPLY_ERROR) {

            mStatus = CONN_TIMEOUT; // Error (during receive)
            return ClientMgr::RCV_REPLY_ERROR;
        }
        if (difftime(now, mCurClient->getTimeOut()) > timeout) { // ...or time out

            LOGW(LOG_FORMAT(" - Timed out"), __PRETTY_FUNCTION__, __LINE__);
            mStatus = CONN_TIMEOUT; // Time out
            return ClientMgr::RCV_REPLY_ERROR;
        }
        if (mCurClient->isEOF()) {

            if (mStatus == CONN_UPLOAD) // Upload in progress (packet received)
                return ClientMgr::RCV_REPLY_UPLOAD;

            switch (mCurClient->getRcvLength()) {
                case VERIFY_LEN: {
                    if ((std::memcmp(mCurClient->getRcvBuffer(), CMD_VERIFY, sizeof(CMD_VERIFY) - 1)) ||
                            (!verifyCheckSum(mCurClient))) break;
                    return ClientMgr::RCV_REPLY_VERIFY;
                }
                case KEEPALIVE_LEN: {
                    if ((std::memcmp(mCurClient->getRcvBuffer(), CMD_KEEPALIVE, sizeof(CMD_KEEPALIVE) - 1)) ||
                            (!verifyCheckSum(mCurClient))) break;
                    return ClientMgr::RCV_REPLY_KEEPALIVE;
                }
                case ORIENTATION_LEN: {
                    if ((std::memcmp(mCurClient->getRcvBuffer(), CMD_ORIENTATION, sizeof(CMD_ORIENTATION) - 1)) ||
                            (!verifyCheckSum(mCurClient))) break;
                    return ClientMgr::RCV_REPLY_ORIENTATION;
                }
                case READY_LEN: {
                    if ((std::memcmp(mCurClient->getRcvBuffer(), CMD_READY, sizeof(CMD_READY) - 1)) ||
                            (!verifyCheckSum(mCurClient))) break;
                    return ClientMgr::RCV_REPLY_READY;
                }
                case GET_LEN: {
                    if ((std::memcmp(mCurClient->getRcvBuffer(), CMD_GET, sizeof(CMD_GET) - 1)) ||
                            (!verifyCheckSum(mCurClient))) break;
                    return ClientMgr::RCV_REPLY_GET;
                }
                case DOWNLOAD_LEN: {
                    if ((std::memcmp(mCurClient->getRcvBuffer(), CMD_DOWNLOAD, sizeof(CMD_DOWNLOAD) - 1)) ||
                            (!verifyCheckSum(mCurClient))) break;
                    return ClientMgr::RCV_REPLY_DOWNLOAD;
                }
                case UPLOAD_LEN: {
                    if ((std::memcmp(mCurClient->getRcvBuffer(), CMD_UPLOAD, sizeof(CMD_UPLOAD) - 1)) ||
                            (!verifyCheckSum(mCurClient))) break;
                    return ClientMgr::RCV_REPLY_UPLOAD;
                }
            }
            LOGE(LOG_FORMAT(" - Wrong request message received (len: %d)"), __PRETTY_FUNCTION__, __LINE__,
                    mCurClient->getRcvLength());
            mStatus = CONN_TIMEOUT; // Error (default length)
            return ClientMgr::RCV_REPLY_ERROR;
        }
    }
    return ClientMgr::RCV_REPLY_NONE; // ...only used by client socket
}

void Connexion::connexionThreadRunning() {

    LOGV(LOG_LEVEL_CONNEXION, 0, LOG_FORMAT(" - Begin"), __PRETTY_FUNCTION__, __LINE__);
    while (!mAbort) {

        if (mServer) { ////// Server

            boost::this_thread::sleep(boost::posix_time::milliseconds(150));
            if (mCloseAll)
                mStatus = CONN_CLOSE;

            switch (mStatus) {

                case CONN_WAIT:
                case CONN_NEW:
                case CONN_ORIENTATION:
                case CONN_READY:
                case CONN_DOWNLOAD:
                case CONN_WAIT_UPLOAD:
                case CONN_UPLOAD: { // Receive keep alive & command replies

                    mMutex.lock();
                    receive(time(NULL));
                    if ((CONN_TIMEOUT == mStatus) || (mStatus != CONN_WAIT)) { // Time out/Error | No wait client

                        mMutex.unlock();
                        break;
                    }
                    // Check new client(s)
                    while (mSocket->getClientCount() != getClientCount()) {

                        assert(mSocket->getClientCount() > getClientCount());

                        int security = (std::rand() % 65536); // [0;65535]
                        std::string verify(CMD_VERIFY);
                        verify += static_cast<char>(getClientCount() + 2); // Frame rank
                        verify += (static_cast<const MatrixLevel*>(mCaller)->mLandscape)?
                                ORIENTATION_LAND:ORIENTATION_PORT; // Orientation
#ifdef __ANDROID__
                        verify += OS_ANDROID; // Server OS
#else
                        verify += OS_IOS; // ...
#endif

                        addCheckSum(verify.c_str(), VERIFY_LEN, security);
                        if (mSocket->send(mSendBuffer, VERIFY_LEN, getClientCount()) != VERIFY_LEN) {

                            LOGE(LOG_FORMAT(" - Failed to send verify request (cli:%d)"), __PRETTY_FUNCTION__, __LINE__,
                                    getClientCount());
                            mSocket->closeClient(getClientCount());
                            continue;
                        }
                        ClientMgr* client = new ClientMgr(mSocket, getClientCount());
                        client->setSecurity(security);
                        client->setTimeOut(time(NULL));
                        client->setStatus(ClientMgr::RCV_REPLY_VERIFY);

                        mClients.push_back(client);
                        mStatus = CONN_NEW;
                    }
                    mMutex.unlock();
                    break;
                }
                case CONN_TIMEOUT: {

                    LOGW(LOG_FORMAT(" - Time out/Error (cli:%d)"), __PRETTY_FUNCTION__, __LINE__, mTimeOutIdx);

                    mMutex.lock();
                    ClientList::iterator iter = (mClients.begin() + mTimeOutIdx);
                    delete (*iter);
                    mClients.erase(iter);

                    // Change client rank
                    for (unsigned char i = mTimeOutIdx; i < static_cast<unsigned char>(mClients.size()); ++i)
                        mClients[i]->lock();
                    for (unsigned char i = mTimeOutIdx; i < static_cast<unsigned char>(mClients.size()); ++i)
                        mClients[i]->setRank(i);

                    mSocket->closeClient(mTimeOutIdx);
                    for (unsigned char i = mTimeOutIdx; i < static_cast<unsigned char>(mClients.size()); ++i)
                        mClients[i]->unlock();

                    // Inform all remaining client of their new frame rank (if any)
                    mStatus = (!mClients.empty())? CONN_VERIFY:CONN_WAIT;
                    mMutex.unlock();
                    break;
                }
                case CONN_VERIFY: {

                    mMutex.lock();
                    receive(time(NULL));
                    if (CONN_TIMEOUT == mStatus) {

                        mMutex.unlock();
                        break;
                    }
                    mStatus = CONN_VERIFY;
                    if (isAllStatus(ClientMgr::RCV_REPLY_NONE)) {
                        for (unsigned char i = 0; i < static_cast<unsigned char>(mClients.size()); ++i) {

                            std::string verify(CMD_VERIFY);
                            verify += static_cast<char>(i + 2); // Frame rank
                            verify += (static_cast<const MatrixLevel*>(mCaller)->mLandscape)?
                                    ORIENTATION_LAND:ORIENTATION_PORT; // Orientation
#ifdef __ANDROID__
                            verify += OS_ANDROID; // Server OS
#else
                            verify += OS_IOS; // ...
#endif
                            if (!send(verify.c_str(), VERIFY_LEN, i, ClientMgr::RCV_REPLY_VERIFY))
                                break;
                        }
                        if (mStatus != CONN_TIMEOUT)
                            mStatus = CONN_NEW; // Receive verif response(s) + No wait client
                    }
                    mMutex.unlock();
                    break;
                }
                case CONN_CLOSE: {

                    LOGW(LOG_FORMAT(" - Close all connexion requested"), __PRETTY_FUNCTION__, __LINE__);

                    mMutex.lock();
                    for (ClientList::iterator iter = mClients.begin(); iter != mClients.end(); ++iter)
                        delete (*iter);
                    for (unsigned char i = 0; i < static_cast<unsigned char>(mClients.size()); ++i)
                        mSocket->closeClient(0);

                    mClients.clear();
                    mMutex.unlock();

                    mCloseAll = false;
                    mStatus = CONN_WAIT;
                    break;
                }
                case CONN_GO: {

                    // Wait user send Go! ...no more keep alive but keep client time out up to date
                    time_t now = time(NULL);
                    for (unsigned char i = 0; i < static_cast<unsigned char>(mClients.size()); ++i)
                        mClients[i]->setTimeOut(now);
                    break;
                }
            }
        }
        else { ////// Client

            if (mStatus == CONN_READY)
                boost::this_thread::sleep(boost::posix_time::milliseconds(1)); // Wait Go!
            else
                boost::this_thread::sleep(boost::posix_time::milliseconds(200));
            if (!isConnected())
                continue;

            switch (mStatus) {

                case CONN_VERIFY: // ...only used at connexion
                case CONN_WAIT:
                case CONN_DOWNLOAD:
                case CONN_UPLOAD: { // Wait VERIFY | KEEPALIVE | ORIENTATION | READY | GET | DOWNLOAD | UPLOAD request messages

                    time_t now = time(NULL);
                    unsigned char status = receive(now, (CONN_VERIFY == mStatus)?
                            MCAM_DELAY_TIMEOUT:(KEEPALIVE_INTERVAL + KEEPALIVE_TIMEOUT));
                    if ((status != ClientMgr::RCV_REPLY_NONE) && (status != ClientMgr::RCV_REPLY_ERROR)) {

                        assignSecurity(mCurClient);
                        if ((status != ClientMgr::RCV_REPLY_READY) && (mStatus != CONN_UPLOAD))
                            mCurClient->reset();
                        //else
                        // RCV_REPLY_READY: Keep EOF to avoid receiving CMD_GO through the 'ClientMgr::receiveThreadRunning' method
                        // CONN_UPLOAD: Keep 'mRcvLength' in order to know video packet size received (see 'RCV_REPLY_UPLOAD' management)
                    }
                    switch (status) {

                        case ClientMgr::RCV_REPLY_VERIFY: {

                            // Assign frame No & orientation
                            mCurClient->setRank(static_cast<unsigned char>(mCurClient->getRcvBuffer()[VERIFY_FRAMENO_IDX]));
                            static_cast<MatrixLevel*>(mCaller)->mLandscape =
                                    (mCurClient->getRcvBuffer()[VERIFY_ORIENTATION_IDX] == ORIENTATION_LAND)? true:false;
                            if (static_cast<const MatrixLevel*>(mCaller)->mStatus != MatrixLevel::MCAM_DISPLAY)
                                static_cast<MatrixLevel*>(mCaller)->mStatus = MatrixLevel::MCAM_FRAMENO;
#ifdef __ANDROID__
                            LOGI(LOG_LEVEL_CONNEXION, 0, LOG_FORMAT(" - Server OS: %s"), __PRETTY_FUNCTION__, __LINE__,
                                    (mCurClient->getRcvBuffer()[VERIFY_OS_IDX] == OS_ANDROID)? "Android":"iOS");
                            mCurClient->setOS(mCurClient->getRcvBuffer()[VERIFY_OS_IDX] == OS_ANDROID); // ... & Server OS
#endif
                            // Reply to verify request
                            std::string reply(CMD_VERIFY);
                            reply.append(MCAM_VERSION); // Application version
#ifdef __ANDROID__
                            if ((static_cast<const MatrixLevel*>(mCaller)->isPaused()) ||
                                    (static_cast<const MatrixLevel*>(mCaller)->isScreenLocked()))
#else
                            if (static_cast<const MatrixLevel*>(mCaller)->isPaused())
#endif
                                reply += static_cast<char>(MatrixLevel::MCAM_PAUSED); // Paused/Screen locked
                            else
                                reply += static_cast<char>(static_cast<const MatrixLevel*>(mCaller)->mStatus); // Main status (!= 0)
#ifdef __ANDROID__
                            reply += OS_ANDROID; // Client OS
#else
                            reply += OS_IOS; // ...
#endif

                            send(reply.c_str(), VERIFY_REPLY_LEN);
                            if (mStatus != CONN_TIMEOUT)
                                mStatus = CONN_WAIT;
                            break;
                        }
                        case ClientMgr::RCV_REPLY_KEEPALIVE: {

                            // Reply to keep alive request
                            send(CMD_KEEPALIVE, KEEPALIVE_LEN);
                            break;
                        }
                        case ClientMgr::RCV_REPLY_ORIENTATION: {

                            // Change orientation
                            static_cast<MatrixLevel*>(mCaller)->mLandscape =
                                    (mCurClient->getRcvBuffer()[ORIENTATION_IDX] == ORIENTATION_LAND)? true:false;
                            if (static_cast<const MatrixLevel*>(mCaller)->mStatus != MatrixLevel::MCAM_DISPLAY)
                                static_cast<MatrixLevel*>(mCaller)->mStatus = MatrixLevel::MCAM_ORIENTATION;

                            // Reply to orientation request
                            send(CMD_ORIENTATION, ORIENTATION_REPLY_LEN);
                            break;
                        }
                        case ClientMgr::RCV_REPLY_READY: {

                            if (static_cast<const MatrixLevel*>(mCaller)->mStatus == MatrixLevel::MCAM_DISPLAY) {

                                LOGW(LOG_FORMAT(" - Client not ready"), __PRETTY_FUNCTION__, __LINE__);
                                break; // Let's server time out close connexion (no CMD_READY reply send)
                            }
                            // Reply to ready request
                            send(CMD_READY, READY_LEN);

                            mStatus = CONN_READY;
                            static_cast<MatrixLevel*>(mCaller)->mStatus = MatrixLevel::MCAM_READY;
                            break;
                        }
                        case ClientMgr::RCV_REPLY_GET: {

                            assert(mStatus == CONN_DOWNLOAD);
                            if (mVideo->get()->isError())
                                break; // Let's server time out close connexion (no CMD_GET reply send)

                            // Reply to get request
                            char reply[GET_REPLY_LEN] = {0};
                            std::memcpy(reply, CMD_GET, sizeof(CMD_GET) - 1);

                            // Check if ready to send picture (JPEG compressed & opened)
                            if (mVideo->get()->isDone()) {

                                LOGI(LOG_LEVEL_CONNEXION, 0, LOG_FORMAT(" - JPEG created (size:%d)"), __PRETTY_FUNCTION__,
                                        __LINE__, mVideo->get()->getSize());
                                reply[REPLY_PICSIZE_IDX - 1] = static_cast<char>(mVideo->get()->getSize() >> 16);
                                reply[REPLY_PICSIZE_IDX] = static_cast<char>(mVideo->get()->getSize() >> 8);
                                reply[REPLY_PICSIZE_IDX + 1] = static_cast<char>(mVideo->get()->getSize());
                                mCurClient->setPacketCount(0);
                            }
                            //else // Picture size already set at zero
                            send(reply, GET_REPLY_LEN);
                            break;
                        }
                        case ClientMgr::RCV_REPLY_DOWNLOAD: {

                            assert(mStatus == CONN_DOWNLOAD);

                            // Check from where to upload JPEG picture
                            if (mCurClient->getRcvBuffer()[DOWNLOAD_FROM_IDX] == DOWNLOAD_FROM_BEGIN) {
                                LOGW(LOG_FORMAT(" - Start/Restart download picture"), __PRETTY_FUNCTION__, __LINE__);
                                mCurClient->setPacketCount(0);
                            }
#ifdef DEBUG
                            if ((mCurClient->getPacketCount() * MAX_MESSAGE_SIZE) >= mVideo->get()->getSize()) {
                                LOGE(LOG_FORMAT(" - Picture already sent"), __PRETTY_FUNCTION__, __LINE__);
                                assert(NULL);
                            }
#endif
                            // Download JPEG picture
                            size_t len = mVideo->get()->getSize() - (mCurClient->getPacketCount() * MAX_MESSAGE_SIZE);
                            send(mVideo->get()->getBuffer() + (mCurClient->getPacketCount() * MAX_MESSAGE_SIZE),
                                    (len > MAX_MESSAGE_SIZE)? MAX_PACKET_SIZE:(len + SECURITY_LEN + CHECKSUM_LEN));

                            mCurClient->setPacketCount(mCurClient->getPacketCount() + 1); // Next buffer part
                            static_cast<MatrixLevel*>(mCaller)->mStatus = MatrixLevel::MCAM_DOWNLOAD;
                            break;
                        }
                        case ClientMgr::RCV_REPLY_UPLOAD: {

                            switch (mStatus) {
                                case CONN_DOWNLOAD: { // Upload request received

                                    mCurClient->setPacketCount(0);
                                    mCurClient->setTryCount(0);
                                    mVideo->clear();

                                    // Get video file size & FPS
                                    unsigned int size = static_cast<unsigned char>(mCurClient->getRcvBuffer()[UPLOAD_SIZE_IDX]) << 24;
                                    size |= static_cast<unsigned char>(mCurClient->getRcvBuffer()[UPLOAD_SIZE_IDX + 1]) << 16;
                                    size |= static_cast<unsigned char>(mCurClient->getRcvBuffer()[UPLOAD_SIZE_IDX + 2]) << 8;
                                    size |= static_cast<unsigned char>(mCurClient->getRcvBuffer()[UPLOAD_SIZE_IDX + 3]);
                                    if (static_cast<int>(size) <= 0) { // Keep both condition '<' & '=' (can be == 0 - see server side)
                                                                       // -> Coz needed for iOS client with no MOV file at server side
                                        LOGW(LOG_FORMAT(" - Wrong video size received: %d"), __PRETTY_FUNCTION__, __LINE__, size);
                                        break; // Let's server time out close connexion (no CMD_UPLOAD reply send)
                                    }
                                    mVideo->prepare(static_cast<int>(size),
                                            static_cast<unsigned char>(mCurClient->getRcvBuffer()[UPLOAD_FPS_IDX]));

                                    // Reply to upload request
                                    if (!replyUpload(0)) // Packet 0 requested (first one)
                                        break; // CONN_TIMEOUT

                                    mStatus = CONN_UPLOAD;
                                    break;
                                }
                                case CONN_UPLOAD: { // Upload in progress

                                    assert(mVideo->getSize() > 1);
                                    assert(!mVideo->isFilled());

                                    signed char res = mVideo->fill(mCurClient);
                                    // BUG: 'read' socket returns EWOULDBLOCK with pending datas
                                    //mCurClient->reset(); // 'mRcvLength' use finished
                                    if (res < 0) { // Error

                                        //LOGW(LOG_FORMAT(" - Retry to upload (%d/%d)"), __PRETTY_FUNCTION__, __LINE__,
                                        //        mCurClient->getTryCount(), MAX_TRY_COUNT);

                                        LOGW(LOG_FORMAT(" - Unexpected/Partial data received (%d/%d)"), __PRETTY_FUNCTION__,
                                                __LINE__, mCurClient->getTryCount(), MAX_TRY_COUNT);
                                        // BUG: 'read' socket returns EWOULDBLOCK with pending datas
                                        // -> Retry to received the expected data count
                                        if (mCurClient->getTryCount() > MAX_TRY_COUNT) {
                                            mCurClient->reset(); // BUG: 'read' socket returns EWOULDBLOCK with pending datas
                                            //break; // Let's server time out close connexion (no CMD_UPLOAD reply send)
                                        }
                                        else
                                            mCurClient->setTryCount(mCurClient->getTryCount() + 1);
                                        break; // BUG: 'read' socket returns EWOULDBLOCK with pending datas
                                    }
                                    else // Request next packet (if any or not)
                                        mCurClient->setPacketCount(mCurClient->getPacketCount() + 1);

                                    // BUG: 'read' socket returns EWOULDBLOCK with pending datas
                                    mCurClient->setTryCount(0);
                                    mCurClient->reset(); // 'mRcvLength' use finished

                                    // Reply to upload request
                                    replyUpload(mCurClient->getPacketCount());

#ifdef __ANDROID__                  // Check if finished to receive video buffer & Failed to save video
                                    if ((!res) && (!mVideo->store(static_cast<const MatrixLevel*>(mCaller)->mLandscape,
                                            mCurClient->isAndroid()))) {
#else
                                    if ((!res) && (!mVideo->store(static_cast<const MatrixLevel*>(mCaller)->mLandscape))) {
#endif
                                        mStatus = CONN_WAIT;
                                        static_cast<MatrixLevel*>(mCaller)->mStatus = MatrixLevel::MCAM_WAIT;
                                    }
                                    //else if (!res && store) // Let's 'receive' method check 'mVideo' status
                                    break;
                                }
#ifdef DEBUG
                                default: {

                                    LOGF(LOG_FORMAT(" - Unexpected status %d"), __PRETTY_FUNCTION__, __LINE__, mStatus);
                                    assert(NULL);
                                    break;
                                }
#endif
                            }
                            break;
                        }
                        case ClientMgr::RCV_REPLY_NONE:
                        case ClientMgr::RCV_REPLY_ERROR: // Timeout/Unexpected request message received
                            break;
                    }
                    break;
                }
                case CONN_TIMEOUT:
                    break; // ...let's 'MatrixLevel::update' method do the job

                case CONN_READY: {

                    // Receive CMD_GO request here and not in ClientMgr loop!
                    char rcv[2];
                    short rcvLen = static_cast<short>(mSocket->receive(rcv, 1));
                    if (rcvLen < 0) {

                        switch (errno) {
                            case EWOULDBLOCK: // EAGAIN
                                break; // No data to read yet (loop)

                            default: {

                                LOGE(LOG_FORMAT(" - Received CMD_GO error: %d"), __PRETTY_FUNCTION__, __LINE__, rcvLen);
                                mStatus = CONN_TIMEOUT;
                                //assert(NULL); // Error / Unexpected socket close
                                break;
                            }
                        }
                        break; // Loop
                    }
                    if ((!rcvLen) || (rcvLen != 1) || (rcv[0] != CMD_GO[0])) { // Check received CMD_GO

                        LOGW(LOG_FORMAT(" - Unexpected socket close or go command (rcv: %c)"), __PRETTY_FUNCTION__, __LINE__,
                                (rcvLen > 0)? rcv[0]:'?');
                        mStatus = CONN_TIMEOUT;
                        //assert(NULL); // Unexpected socket close
                        break;
                    }
#ifdef __ANDROID__
                    if ((static_cast<const MatrixLevel*>(mCaller)->isPaused()) ||
                            (static_cast<const MatrixLevel*>(mCaller)->isScreenLocked())) {

                        LOGW(LOG_FORMAT(" - Client not ready: (paused:%s; locked:%s)"), __PRETTY_FUNCTION__, __LINE__,
                             (static_cast<const MatrixLevel*>(mCaller)->isPaused())? "true":"false",
                             (static_cast<const MatrixLevel*>(mCaller)->isScreenLocked())? "true":"false");
#else
                    if (static_cast<const MatrixLevel*>(mCaller)->isPaused()) {

                        LOGW(LOG_FORMAT(" - Client not ready: (paused:%s)"), __PRETTY_FUNCTION__, __LINE__,
                             (static_cast<const MatrixLevel*>(mCaller)->isPaused())? "true":"false");
#endif
                        mStatus = CONN_TIMEOUT;
                        break; // Let's 'MatrixLevel::checkWait' method call with 'MCAM_GO' main status proceed the timeout
                    }
                    Camera* camera = Camera::getInstance();
                    camera->freeze();

                    // Apply camera frozen delay
                    boost::this_thread::sleep(boost::posix_time::microseconds(static_cast<unsigned long>(FREEZE_CAMERA_DURATION *
                            1000) + (mCurClient->getRank() * 9000))); // Using rank to avoid back in time during the video montage
                    camera->unfreeze(); // Take picture (ASAP)

                    mVideo->add(new Picture(false));
#ifndef PAID_VERSION
                    mVideo->get()->save(static_cast<const MatrixLevel*>(mCaller)->getLogo(),
                            static_cast<const MatrixLevel*>(mCaller)->mLandscape);
#else
                    mVideo->get()->save(static_cast<const MatrixLevel*>(mCaller)->mLandscape);
#endif

                    mCurClient->reset();
                    //mCurClient->setRcvCount(mCurClient->getRcvCount() + 1);

                    // Reply to go request
                    send(CMD_REPLY_GO, GO_REPLY_LEN);
                    mStatus = CONN_DOWNLOAD;
                    break;
                }
            }
        }
    }
#ifdef __ANDROID__
    detachThreadJVM(LOG_LEVEL_CONNEXION); // If needed ('alertMessage' function call)
#endif
    LOGI(LOG_LEVEL_CONNEXION, 0, LOG_FORMAT(" - Finished"), __PRETTY_FUNCTION__, __LINE__);
}
void Connexion::startConnexionThread(Connexion* conn) {

    LOGV(LOG_LEVEL_CONNEXION, 0, LOG_FORMAT(" - c:%x"), __PRETTY_FUNCTION__, __LINE__, conn);
    conn->connexionThreadRunning();
}
