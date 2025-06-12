/*
 * Copyright (C) 2006 Zack Rusin <zack@kde.org>
 * Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "EventHandler.h"

#include "Cursor.h"
#include "DataTransfer.h"
#include "Document.h"
#include "EventNames.h"
#include "FloatPoint.h"
#include "FocusController.h"
#include "FrameInlines.h"
#include "LocalFrame.h"
#include "FrameLoader.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "HandleUserInputEventResult.h"
#include "HTMLFrameSetElement.h"
#include "HitTestRequest.h"
#include "HitTestResult.h"
#include "KeyboardEvent.h"
#include "MouseEventWithHitTestResults.h"
#include "NotImplemented.h"
#include "Page.h"
#include "PlatformKeyboardEvent.h"
#include "PlatformWheelEvent.h"
#include "RenderWidget.h"
#include "Scrollbar.h"
#include <QCoreApplication>

namespace WebCore {

bool EventHandler::tabsToAllFormControls(KeyboardEvent* event) const
{
    return !isKeyboardOptionTab(*event);
}

void EventHandler::focusDocumentView()
{
    Page* page = m_frame->page();
    if (!page)
        return;
    page->focusController().setFocusedFrame(m_frame.ptr());
}

bool EventHandler::passWidgetMouseDownEventToWidget(const MouseEventWithHitTestResults&)
{
    notImplemented();
    return false;
}

bool EventHandler::eventActivatedView(const PlatformMouseEvent&) const
{
    // Qt has an activation event which is sent independently
    //   of mouse event so this thing will be a snafu to implement
    //   correctly
    return false;
}

bool EventHandler::passWheelEventToWidget(const PlatformWheelEvent& event, Widget& widget, OptionSet<WheelEventProcessingSteps>)
{
    if (!widget.isLocalFrameView())
        return false;

    auto [result, _] = downcast<LocalFrameView>(widget).frame().eventHandler().handleWheelEvent(event, { });
    return result.wasHandled();
}

HandleUserInputEventResult EventHandler::passMousePressEventToSubframe(MouseEventWithHitTestResults& mev, LocalFrame& subframe)
{
    subframe.eventHandler().handleMousePressEvent(mev.event());
    return true;
}

HandleUserInputEventResult EventHandler::passMouseMoveEventToSubframe(MouseEventWithHitTestResults& mev, LocalFrame& subframe, HitTestResult* hoveredNode)
{
    subframe.eventHandler().handleMouseMoveEvent(mev.event(), hoveredNode);
    return true;
}

HandleUserInputEventResult EventHandler::passMouseReleaseEventToSubframe(MouseEventWithHitTestResults& mev, LocalFrame& subframe)
{
    subframe.eventHandler().handleMouseReleaseEvent(mev.event());
    return true;
}

OptionSet<PlatformEvent::Modifier> EventHandler::accessKeyModifiers()
{
#if OS(DARWIN)
    // On macOS, the ControlModifier value corresponds
    // to the Command keys on the keyboard,
    // and the MetaModifier value corresponds to the Control keys.
    // See http://doc.qt.io/qt-5/qt.html#KeyboardModifier-enum
    if (QCoreApplication::testAttribute(Qt::AA_MacDontSwapCtrlAndMeta)) [[unlikely]]
        return { PlatformEvent::Modifier::ControlKey, PlatformEvent::Modifier::AltKey };
    else
        return { PlatformEvent::Modifier::MetaKey, PlatformEvent::Modifier::AltKey };
#else
    return PlatformEvent::Modifier::AltKey;
#endif
}

}
