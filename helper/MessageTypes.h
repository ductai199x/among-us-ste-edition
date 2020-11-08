#ifndef MSGTYPES
#define MSGTYPES

enum class MsgTypes
{
    ServerAccept,
    ServerDeny,
    ServerPing,
    ClientAccept,
    ClientDeny,
    ClientPing,
    MessageAll,
    ServerMessage,
    ServerACK,
    ClientTextMessage,
    ClientUpdateGS,
    ServerUpdateGS,
};

#endif