/*
 * RFC 3489 client
 */
#include <jni.h>
#include <android/log.h>
#include <time.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <string>

#define LOGD(...) \
  ((void)__android_log_print(ANDROID_LOG_DEBUG, "stun-client-rfc3489::", __VA_ARGS__))

#define LOGE(...) \
  ((void)__android_log_print(ANDROID_LOG_ERROR, "stun-client-rfc3489::", __VA_ARGS__))

#define BUFLEN 512

/*
 *
 */
struct stun_attribute {
    u_short type;
    u_short length;
};

/*
 * The Message Types can take on the following values:
 *
 * 0x0001  :  Binding Request
 * 0x0101  :  Binding Response
 * 0x0111  :  Binding Error Response
 * 0x0002  :  Shared Secret Request
 * 0x0102  :  Shared Secret Response
 * 0x0112  :  Shared Secret Error Response
 */
#define BINDING_REQUEST 0x0001
#define BINDING_RESPONSE 0x0101
#define BINDING_ERROR 0x0111
#define SHARED_SECRET_REQUEST 0x0002
#define SHARED_SECRET_RESPONSE 0x0102
#define SHARED_SECRET_ERROR_RESPONSE 0x0112

/*
 * All STUN messages consist of a 20 byte header:
 *
 *  0                   1                   2                   3
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |      STUN Message Type        |         Message Length        |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *                          Transaction ID
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *                                                                 |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 * The message length is the count, in bytes, of the size of the
 * message, not including the 20 byte header.
 *
 * The transaction ID is a 128 bit identifier.  It also serves as salt
 * to randomize the request and the response.  All responses carry the
 * same identifier as the request they correspond to.
 */
struct stun_header {
    u_short type;
    u_short length;
    u_long transaction_id[4];
};

struct stun_message {
    struct stun_header *header;
    struct stun_attribute *attribute;
};

void generate_transaction_id(u_long *transaction_id) {
    srand((unsigned) time(NULL));

    for (int i = 0; i < 4; i++) {
        transaction_id[i] = htonl((unsigned) random());
    }
}

void prepare_message(struct stun_header *header) {
    header->type = htons(header->type);
    header->length = htons(header->length);

    generate_transaction_id(header->transaction_id);
}

class STUN_Message {};

void printMessage(struct stun_header *header) {
    char* typeString;

    switch (header->type) {
        case BINDING_REQUEST:
            typeString = "Binding Request";
            break;
        case BINDING_RESPONSE:
            typeString = "Binding Response";
            break;
    }

    LOGD("Message Type: %s (0x%.4x)", typeString, header->type);
    LOGD("Message Length: %d (0x%.4x)", header->length, header->length);
    LOGD("Message Transaction ID: %.4x%.4x%.4x%.4x", header->transaction_id[0], header->transaction_id[1], header->transaction_id[2], header->transaction_id[3]);
}

extern "C" JNIEXPORT jstring JNICALL
Java_name_peterbukhal_android_stun_client_MainActivity_stunRequest(JNIEnv *env, jobject thiz) {
    struct stun_message rq_message;
    struct stun_header rq_header;

    rq_header.type = BINDING_REQUEST;
    rq_header.length = 0x0000;

    generate_transaction_id(rq_header.transaction_id);

    LOGD("Request header size: %d bytes", sizeof(struct stun_header));

    //------------
    struct sockaddr_in si_other;
    int s, slen = sizeof(si_other);
    char response[BUFLEN];

    if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
    {
        return env->NewStringUTF("Socket failed");
    }

    memset((char *) &si_other, 0, sizeof(si_other));

    si_other.sin_family = AF_INET;
    si_other.sin_port = htons(19302);

    if (inet_aton("74.125.22.127", &si_other.sin_addr) == 0)
    {
        LOGE("inet_aton() failed");

        return env->NewStringUTF("inet_aton() error");
    }

    LOGD("STUN Request ->");
    printMessage(&rq_header);
    prepare_message(&rq_header);

    //send the request
    if (sendto(s, &rq_header, sizeof(stun_header), 0, (struct sockaddr *) &si_other, slen) == -1)
    {
        LOGE("sendto() error");

        return env->NewStringUTF("sendto() error");
    }

    memset(response, '\0', BUFLEN);

    //receive response
    if (recvfrom(s, response, BUFLEN, 0, (struct sockaddr *) &si_other, &slen) == -1)
    {
        LOGE("recvfrom() error");

        return env->NewStringUTF("recvfrom() error");
    }

    struct stun_header *rs_header = (struct stun_header *) response;

    rs_header->type = ntohs(rs_header->type);
    rs_header->length = ntohs(rs_header->length);

    LOGD("STUN Response ->");
    printMessage(rs_header);

    return env->NewStringUTF("success");
}