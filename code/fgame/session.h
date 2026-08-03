// session.h: Process-lifetime script variables.

#pragma once

#include "../corepp/listener.h"

class Session : public Listener
{
public:
    CLASS_PROTOTYPE(Session);

    void PrepareForScriptReset();
    void Restore();
    void Save();

private:
    bool restored = false;
};

extern Session session;
