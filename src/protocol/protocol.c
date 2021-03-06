#include "../wheatserver.h"
#include "protocol.h"

struct protocol *spotProtocol(char *ip, int port, int fd)
{
    static int is_init = 0;
    if (!is_init) {
        ProtocolTable[0].initProtocol();
        is_init = 1;
    }
    return &ProtocolTable[0];
}
