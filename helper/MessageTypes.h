#ifndef MSGTYPES
#define MSGTYPES

enum class MsgTypes
{
    ServerAccept,
    ServerDeny,
    ServerPing,
    MessageAll,
    ServerMessage,
    ServerACK,
    ClientTextMessage,
    ClientUpdateGS,
};

#endif