// Copyright (C) 2021-2023 Apple Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
// BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
// THE POSSIBILITY OF SUCH DAMAGE.

// FIXME: Declare SPI symbols for GroupActivity so that URLActivity is available in the public build
#if USE_APPLE_INTERNAL_SDK && ENABLE_MEDIA_SESSION_COORDINATOR && HAVE_GROUP_ACTIVITIES

import AVFoundation
import Combine
import Foundation
@_spi(Safari) import GroupActivities
#if compiler(>=6.0)
internal import WebKit_Internal
#else
@_implementationOnly import WebKit_Internal
#endif

fileprivate extension WKGroupSessionState {
    init(_ state: GroupSession<URLActivity>.State) {
        switch state {
        case .waiting:
            self = .waiting
        case .joined:
            self = .joined
        case .invalidated:
            self = .invalidated
        @unknown default:
            // Unanticipated state value.
            assertionFailure()
            self = .invalidated
        }
    }
}

@_objcImplementation extension WKURLActivity {
    // Used to workaround the fact that `@_objcImplementation` does not support stored properties whose size can change
    // due to Library Evolution. Do not use this property directly.
    @nonobjc private var _urlActivity: Any

    @nonobjc final private var urlActivity: URLActivity {
        _urlActivity as! URLActivity
    }

    var fallbackURL: URL? { urlActivity.webpageURL }

    @nonobjc fileprivate convenience init(activity: URLActivity) {
        self.init()
        self._urlActivity = activity as Any
    }

#if compiler(<6.0)
    @objc deinit { }
#endif
}

@_objcImplementation extension WKGroupSession {
    @nonobjc private var groupSession: GroupSession<URLActivity>
    @nonobjc private var _activity: WKURLActivity
    @nonobjc private var cancellables: Set<AnyCancellable> = []

    @nonobjc fileprivate convenience init(groupSession: GroupSession<URLActivity>) {
        self.init()

        self.groupSession = groupSession
        self._activity = .init(activity: groupSession.activity)

        self.groupSession.$activity
            .sink { [unowned self] in self.activityChanged(activity: $0) }
            .store(in: &cancellables)
        self.groupSession.$state
            .sink { [unowned self] in self.stateChanged(state: $0) }
            .store(in: &cancellables)
    }

    var activity: WKURLActivity { _activity }
    var uuid: UUID { groupSession.id }

    var state: WKGroupSessionState {
        WKGroupSessionState(groupSession.state)
    }

    var newActivityCallback: ((WKURLActivity) -> Void)?
    var stateChangedCallback: ((WKGroupSessionState) -> Void)?

    func join() {
        groupSession.join()
    }

    func leave() {
        groupSession.leave()
    }

    @objc(coordinateWithCoordinator:)
    func coordinate(with coordinator: AVPlaybackCoordinator) {
        coordinator.coordinateWithSession(groupSession)
    }

    @nonobjc private final func activityChanged(activity: URLActivity) {
        _activity = .init(activity: groupSession.activity)

        guard let callback = newActivityCallback else {
            return
        }

        callback(_activity)
    }

    @nonobjc private final func stateChanged(state: GroupSession<URLActivity>.State) {
        guard let callback = stateChangedCallback else {
            return
        }

        callback(.init(state))
    }

#if compiler(<6.0)
    @objc deinit { }
#endif
}

@_objcImplementation extension WKGroupSessionObserver {
    @nonobjc private var incomingSessionsTask: Task<Void, Never>?

    var newSessionCallback: ((WKGroupSession) -> Void)?

    override init() {
        super.init()

        incomingSessionsTask = Task { [weak self] in
            for await newSession in URLActivity.sessions() {
                self?.receivedSession(newSession)
            }
        }
    }

    @nonobjc final private func receivedSession(_ session: GroupSession<URLActivity>) {
        guard let callback = newSessionCallback else {
            return
        }

        callback(.init(groupSession: session))
    }

#if compiler(<6.0)
    @objc deinit { }
#endif
}

#endif // USE_APPLE_INTERNAL_SDK && ENABLE_MEDIA_SESSION_COORDINATOR && HAVE_GROUP_ACTIVITIES
