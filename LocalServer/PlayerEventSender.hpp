//
//  PlayerEventSender.hpp
//  MusicPlayer
//

#ifndef PlayerEventSender_hpp
#define PlayerEventSender_hpp

#include "../MPlayerUI/MPEventsDispatcher.h"
#include <memory>


class PlayerEventSender : public IEventHandler {
public:
    PlayerEventSender();

    virtual void onEvent(const IEvent *pEvent);

};

using PlayerEventSenderPtr = std::shared_ptr<PlayerEventSender>;

#endif /* PlayerEventSender_hpp */
