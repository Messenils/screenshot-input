#pragma once
#include "pch.h"

enum ProtoMessageFilterIDs : unsigned int {
    RawInputFilterID = 0,
    MouseMoveFilterID,
    MouseActivateFilterID,
    WindowActivateFilterID,
    WindowActivateAppFilterID,
    MouseWheelFilterID,
    MouseButtonFilterID,
    KeyboardButtonFilterID
};