/*
    Copyright (C) 2010 Robert Hogan <robert@roberthogan.net>
    Copyright (C) 2008,2009,2010 Nokia Corporation and/or its subsidiary(-ies)
    Copyright (C) 2007 Staikos Computing Services Inc.
    Copyright (C) 2007, 2012, 2013 Apple Inc.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "DumpRenderTreeSupportQt.h"

#include "ChromeClientQt.h"
#include "EditorClientQt.h"
#include "FrameLoaderClientQt.h"
#include "NotificationPresenterClientQt.h"
#include "ProgressTrackerClientQt.h"
#include "QWebFrameAdapter.h"
#include "QWebPageAdapter.h"
#include "qwebelement.h"
#include "qwebhistory.h"
#include "qwebhistory_p.h"
#include "qwebscriptworld.h"

#include <JavaScriptCore/APICast.h>
#include <JavaScriptCore/JSObject.h>
#include <JavaScriptCore/JSGlobalObject.h>
#include <WebCore/BackForwardController.h>
#include <WebCore/CommonVM.h>
#include <WebCore/DeprecatedGlobalSettings.h>
#include <WebCore/DocumentLoader.h>
#include <WebCore/DOMWrapperWorld.h>
#include <WebCore/Editor.h>
#include <WebCore/FocusController.h>
#include <WebCore/FrameLoader.h>
#include <WebCore/FrameLoadRequest.h>
#include <WebCore/GraphicsContextQt.h>
#include <WebCore/GCController.h>
#include <WebCore/HTMLInputElement.h>
#include <WebCore/InspectorController.h>
#include <WebCore/JSDOMWindowBase.h>
#include <WebCore/JSDOMWindow.h>
#include <WebCore/JSNode.h>
#include <WebCore/LegacySchemeRegistry.h>
#include <WebCore/MemoryCache.h>
#include <WebCore/NetworkStorageSession.h>
#include <WebCore/NetworkingContext.h>
#include <WebCore/PrintContext.h>
#include <WebCore/RenderTreeAsText.h>
#include <WebCore/ScriptController.h>
#include <WebCore/SecurityPolicy.h>
#include <WebCore/Settings.h>
#include <WebCore/TextIterator.h>
#include <WebCore/ThirdPartyCookiesQt.h>
#include <WebCore/UserStyleSheet.h>
#include <WebCore/WebCoreTestSupport.h>
#include <WebCore/qt_runtime.h>

#if ENABLE(GEOLOCATION)
#include <WebCore/GeolocationClientMock.h>
#include <WebCore/GeolocationController.h>
#endif

//#if ENABLE(VIDEO) && USE(QT_MULTIMEDIA)
//#include <WebCore/HTMLVideoElement.h>
//#include "MediaPlayerPrivateQt.h"
//#endif

#include <QPainter>
#include <wtf/WallTime.h>

using namespace WebCore;

QMap<int, QWebScriptWorld*> m_worldMap;

#if ENABLE(GEOLOCATION)
GeolocationClientMock& toGeolocationClientMock(GeolocationClient& client)
{
    ASSERT(QWebPageAdapter::drtRun);
    return static_cast<GeolocationClientMock&>(client);
}
#endif

#if ENABLE(DEVICE_ORIENTATION)
DeviceOrientationClientMock* toDeviceOrientationClientMock(DeviceOrientationClient* client)
{
    ASSERT(QWebPageAdapter::drtRun);
    return static_cast<DeviceOrientationClientMock*>(client);
}
#endif

QDRTNode::QDRTNode()
    : m_node(0)
{
}

QDRTNode::QDRTNode(WebCore::Node* node)
    : m_node(0)
{
    if (node) {
        m_node = node;
        m_node->ref();
    }
}

QDRTNode::~QDRTNode()
{
    if (m_node)
        m_node->deref();
}

QDRTNode::QDRTNode(const QDRTNode& other)
    :m_node(other.m_node)
{
    if (m_node)
        m_node->ref();
}

QDRTNode& QDRTNode::operator=(const QDRTNode& other)
{
    if (this != &other) {
        Node* otherNode = other.m_node;
        if (otherNode)
            otherNode->ref();
        if (m_node)
            m_node->deref();
        m_node = otherNode;
    }
    return *this;
}

QDRTNode QtDRTNodeRuntime::create(WebCore::Node* node)
{
    return QDRTNode(node);
}

WebCore::Node* QtDRTNodeRuntime::get(const QDRTNode& node)
{
    return node.m_node;
}

static QVariant convertJSValueToNodeVariant(JSC::JSGlobalObject* lexicalGlobalObject, JSC::JSObject* object, int *distance, HashSet<JSObjectRef>*)
{
    JSC::VM& vm = lexicalGlobalObject->vm();
    if (!object || !object->inherits<JSNode>())
        return QVariant();
    return QVariant::fromValue<QDRTNode>(QtDRTNodeRuntime::create(JSNode::toWrapped(vm, object)));
}

static JSC::JSValue convertNodeVariantToJSValue(JSC::JSGlobalObject* lexicalGlobalObject, WebCore::JSDOMGlobalObject* globalObject, const QVariant& variant)
{
    return toJS(lexicalGlobalObject, globalObject, QtDRTNodeRuntime::get(variant.value<QDRTNode>()));
}

void QtDRTNodeRuntime::initialize()
{
    static bool initialized = false;
    if (initialized)
        return;
    initialized = true;
    int id = qRegisterMetaType<QDRTNode>();
    JSC::Bindings::registerCustomType(id, convertJSValueToNodeVariant, convertNodeVariantToJSValue);
}

DumpRenderTreeSupportQt::DumpRenderTreeSupportQt()
{
}

DumpRenderTreeSupportQt::~DumpRenderTreeSupportQt()
{
}

void DumpRenderTreeSupportQt::initialize()
{
    QtDRTNodeRuntime::initialize();
}

void DumpRenderTreeSupportQt::overwritePluginDirectories()
{
    qWarning("Plugins have been removed from WebKit.");
}

void DumpRenderTreeSupportQt::setDumpRenderTreeModeEnabled(bool b)
{
    QWebPageAdapter::drtRun = b;
}

void DumpRenderTreeSupportQt::webPageSetGroupName(QWebPageAdapter *adapter, const QString& groupName)
{
    adapter->page->setGroupName(groupName);
}

QString DumpRenderTreeSupportQt::webPageGroupName(QWebPageAdapter* adapter)
{
    return adapter->page->groupName();
}

void DumpRenderTreeSupportQt::webInspectorExecuteScript(QWebPageAdapter* adapter, const QString& script)
{
    adapter->page->inspectorController().evaluateForTestInFrontend(script);
}

void DumpRenderTreeSupportQt::webInspectorShow(QWebPageAdapter* adapter)
{
    adapter->page->inspectorController().show();
}

void DumpRenderTreeSupportQt::webInspectorClose(QWebPageAdapter* adapter)
{
    // FIXME: Call InspectorFrontendClientQt::closeWindow()
    // adapter->page->inspectorController().close();
}

bool DumpRenderTreeSupportQt::hasDocumentElement(QWebFrameAdapter *adapter)
{
    return adapter->frame->document()->documentElement();
}

void DumpRenderTreeSupportQt::setValueForUser(const QWebElement& element, const QString& value)
{
    WebCore::Element* webElement = element.m_element;
    if (!webElement || !is<HTMLInputElement>(webElement))
        return;
    downcast<HTMLInputElement>(*webElement).setValueForUser(value);
}

void DumpRenderTreeSupportQt::clearFrameName(QWebFrameAdapter *adapter)
{
    Frame* coreFrame = adapter->frame;
    coreFrame->tree().clearName();
}

size_t DumpRenderTreeSupportQt::javaScriptObjectsCount()
{
    return WebCore::commonVM().heap.globalObjectCount();
}

void DumpRenderTreeSupportQt::garbageCollectorCollect()
{
    GCController::singleton().garbageCollectNow();
}

void DumpRenderTreeSupportQt::garbageCollectorCollectOnAlternateThread(bool waitUntilDone)
{
    GCController::singleton().garbageCollectOnAlternateThreadForDebugging(waitUntilDone);
}

void DumpRenderTreeSupportQt::allowListAccessFromOrigin(const QString& sourceOrigin, const QString& destinationProtocol, const QString& destinationHost, bool allowDestinationSubdomains)
{
    SecurityPolicy::addOriginAccessAllowlistEntry(SecurityOrigin::createFromString(sourceOrigin), destinationProtocol, destinationHost, allowDestinationSubdomains);
}

void DumpRenderTreeSupportQt::removeAllowListAccessFromOrigin(const QString& sourceOrigin, const QString& destinationProtocol, const QString& destinationHost, bool allowDestinationSubdomains)
{
    SecurityPolicy::removeOriginAccessAllowlistEntry(SecurityOrigin::createFromString(sourceOrigin), destinationProtocol, destinationHost, allowDestinationSubdomains);
}

void DumpRenderTreeSupportQt::resetOriginAccessAllowLists()
{
    SecurityPolicy::resetOriginAccessAllowlists();
}

void DumpRenderTreeSupportQt::setDomainRelaxationForbiddenForURLScheme(bool forbidden, const QString& scheme)
{
    LegacySchemeRegistry::setDomainRelaxationForbiddenForURLScheme(forbidden, scheme);
}

void DumpRenderTreeSupportQt::setCaretBrowsingEnabled(QWebPageAdapter* adapter, bool value)
{
    adapter->page->settings().setCaretBrowsingEnabled(value);
}

void DumpRenderTreeSupportQt::setAuthorAndUserStylesEnabled(QWebPageAdapter* adapter, bool value)
{
    adapter->page->settings().setAuthorAndUserStylesEnabled(value);
}

void DumpRenderTreeSupportQt::executeCoreCommandByName(QWebPageAdapter* adapter, const QString& name, const QString& value)
{
    RefPtr frame = adapter->page->focusController().focusedOrMainFrame();
    if (frame)
        frame->editor().command(name).execute(value);
}

bool DumpRenderTreeSupportQt::isCommandEnabled(QWebPageAdapter *adapter, const QString& name)
{
    RefPtr frame = adapter->page->focusController().focusedOrMainFrame();
    if (frame)
        return frame->editor().command(name).isEnabled();
    else
        return false;
}

QVariantList DumpRenderTreeSupportQt::selectedRange(QWebPageAdapter *adapter)
{
    RefPtr frame = adapter->page->focusController().focusedOrMainFrame();
    if (!frame)
        return QVariantList();

    QVariantList selectedRange;
    auto range = createLiveRange(frame->selection().selection().toNormalizedRange()).get();

    Element* selectionRoot = frame->selection().selection().rootEditableElement();
    Element* scope = selectionRoot ? selectionRoot : frame->document()->documentElement();

    auto testRange = makeRangeSelectingNodeContents(scope->document());
    testRange.start = { range->startContainer(), range->startOffset() };

    ASSERT(&testRange.startContainer() == scope);
    int startPosition = characterCount(testRange);

    testRange.end = { range->endContainer(), range->endOffset() };
    ASSERT(&testRange.startContainer() == scope);
    int endPosition = characterCount(testRange);

    selectedRange << startPosition << (endPosition - startPosition);

    return selectedRange;

}

QVariantList DumpRenderTreeSupportQt::firstRectForCharacterRange(QWebPageAdapter *adapter, uint64_t location, uint64_t length)
{
    RefPtr frame = adapter->page->focusController().focusedOrMainFrame();
    if (!frame)
        return QVariantList();

    QVariantList rect;

    if ((location + length < location) && (location + length))
        length = 0;

    WebCore::CharacterRange range { location, length };
    auto* element = frame->selection().rootEditableElementOrDocumentElement();

    if (!element)
        return QVariantList();

    auto resolvedRange = resolveCharacterRange(makeRangeSelectingNodeContents(*element), range);
    QRect resultRect = frame->editor().firstRectForRange(resolvedRange);
    rect << resultRect.x() << resultRect.y() << resultRect.width() << resultRect.height();
    return rect;
}

void DumpRenderTreeSupportQt::setWindowsBehaviorAsEditingBehavior(QWebPageAdapter* adapter)
{
    RefPtr corePage = adapter->page;
    if (!corePage)
        return;
    corePage->settings().setEditingBehaviorType(EditingBehaviorType::Windows);
}

void DumpRenderTreeSupportQt::dumpFrameLoader(bool b)
{
    FrameLoaderClientQt::dumpFrameLoaderCallbacks = b;
}

void DumpRenderTreeSupportQt::dumpProgressFinishedCallback(bool b)
{
    ProgressTrackerClientQt::dumpProgressFinishedCallback = b;
}

void DumpRenderTreeSupportQt::dumpUserGestureInFrameLoader(bool b)
{
    FrameLoaderClientQt::dumpUserGestureInFrameLoaderCallbacks = b;
}

void DumpRenderTreeSupportQt::dumpResourceLoadCallbacks(bool b)
{
    FrameLoaderClientQt::dumpResourceLoadCallbacks = b;
}

void DumpRenderTreeSupportQt::dumpResourceLoadCallbacksPath(const QString& path)
{
    FrameLoaderClientQt::dumpResourceLoadCallbacksPath = path;
}

void DumpRenderTreeSupportQt::dumpResourceResponseMIMETypes(bool b)
{
    FrameLoaderClientQt::dumpResourceResponseMIMETypes = b;
}

void DumpRenderTreeSupportQt::dumpWillCacheResponseCallbacks(bool b)
{
    FrameLoaderClientQt::dumpWillCacheResponseCallbacks = b;
}

void DumpRenderTreeSupportQt::setWillSendRequestReturnsNullOnRedirect(bool b)
{
    FrameLoaderClientQt::sendRequestReturnsNullOnRedirect = b;
}

void DumpRenderTreeSupportQt::setWillSendRequestReturnsNull(bool b)
{
    FrameLoaderClientQt::sendRequestReturnsNull = b;
}

void DumpRenderTreeSupportQt::setWillSendRequestClearHeaders(const QStringList& headers)
{
    FrameLoaderClientQt::sendRequestClearHeaders = headers;
}

void DumpRenderTreeSupportQt::setDeferMainResourceDataLoad(bool b)
{
    FrameLoaderClientQt::deferMainResourceDataLoad = b;
}

void DumpRenderTreeSupportQt::setCustomPolicyDelegate(bool enabled, bool permissive)
{
    FrameLoaderClientQt::policyDelegateEnabled = enabled;
    FrameLoaderClientQt::policyDelegatePermissive = permissive;
}

void DumpRenderTreeSupportQt::dumpHistoryCallbacks(bool b)
{
    FrameLoaderClientQt::dumpHistoryCallbacks = b;
}

void DumpRenderTreeSupportQt::dumpVisitedLinksCallbacks(bool b)
{
    ChromeClientQt::dumpVisitedLinksCallbacks = b;
}

void DumpRenderTreeSupportQt::dumpEditingCallbacks(bool b)
{
    EditorClientQt::dumpEditingCallbacks = b;
}

void DumpRenderTreeSupportQt::dumpSetAcceptsEditing(bool b)
{
    EditorClientQt::acceptsEditing = b;
}

void DumpRenderTreeSupportQt::dumpNotification(bool b)
{
#if ENABLE(NOTIFICATIONS)
    NotificationPresenterClientQt::dumpNotification = b;
#endif
}

QString DumpRenderTreeSupportQt::viewportAsText(QWebPageAdapter* adapter, int deviceDPI, const QSize& deviceSize, const QSize& availableSize)
{
    WebCore::ViewportArguments args = adapter->viewportArguments();

    float devicePixelRatio = deviceDPI / WebCore::ViewportArguments::deprecatedTargetDPI;
    WebCore::ViewportAttributes conf = WebCore::computeViewportAttributes(args,
        /* desktop-width    */980,
        /* device-width     */deviceSize.width(),
        /* device-height    */deviceSize.height(),
        devicePixelRatio,
        availableSize);
    WebCore::restrictMinimumScaleFactorToViewportSize(conf, availableSize, devicePixelRatio);
    WebCore::restrictScaleFactorToInitialScaleIfNotUserScalable(conf);

    return QLatin1String("viewport size %1x%2 scale %3 with limits [%4, %5] and userScalable %6\n")
        .arg(QByteArray::number(conf.layoutSize.width()))
        .arg(QByteArray::number(conf.layoutSize.height()))
        .arg(conf.initialScale)
        .arg(conf.minimumScale)
        .arg(conf.maximumScale)
        .arg(conf.userScalable);
}

void DumpRenderTreeSupportQt::scalePageBy(QWebFrameAdapter* adapter, float scalefactor, const QPoint& origin)
{
    WebCore::Frame* coreFrame = adapter->frame;
    if (Page* page = coreFrame->page())
        page->setPageScaleFactor(scalefactor, origin);
}

void DumpRenderTreeSupportQt::setMockDeviceOrientation(QWebPageAdapter* adapter, bool canProvideAlpha, double alpha, bool canProvideBeta, double beta, bool canProvideGamma, double gamma)
{
#if ENABLE(DEVICE_ORIENTATION)
    RefPtr corePage = adapter->page;
    DeviceOrientationClientMock* mockClient = toDeviceOrientationClientMock(DeviceOrientationController::from(corePage.get())->deviceOrientationClient());
    mockClient->setOrientation(DeviceOrientationData::create(canProvideAlpha, alpha, canProvideBeta, beta, canProvideGamma, gamma));
#endif
}

void DumpRenderTreeSupportQt::resetGeolocationMock(QWebPageAdapter* adapter)
{
#if ENABLE(GEOLOCATION)
    RefPtr corePage = adapter->page;
    auto& mockClient = toGeolocationClientMock(GeolocationController::from(corePage.get())->client());
    mockClient.reset();
#endif
}

void DumpRenderTreeSupportQt::setMockGeolocationPermission(QWebPageAdapter* adapter, bool allowed)
{
#if ENABLE(GEOLOCATION)
    RefPtr corePage = adapter->page;
    auto& mockClient = toGeolocationClientMock(GeolocationController::from(corePage.get())->client());
    mockClient.setPermission(allowed);
#endif
}

void DumpRenderTreeSupportQt::setMockGeolocationPosition(QWebPageAdapter* adapter, double latitude, double longitude, double accuracy)
{
#if ENABLE(GEOLOCATION)
    RefPtr corePage = adapter->page;
    auto& mockClient = toGeolocationClientMock(GeolocationController::from(corePage.get())->client());
    mockClient.setPosition(GeolocationPositionData { WallTime::now().secondsSinceEpoch().seconds(), latitude, longitude, accuracy });
#endif
}

void DumpRenderTreeSupportQt::setMockGeolocationPositionUnavailableError(QWebPageAdapter* adapter, const QString& message)
{
#if ENABLE(GEOLOCATION)
    RefPtr corePage = adapter->page;
    auto& mockClient = toGeolocationClientMock(GeolocationController::from(corePage.get())->client());
    mockClient.setPositionUnavailableError(message);
#endif
}

int DumpRenderTreeSupportQt::numberOfPendingGeolocationPermissionRequests(QWebPageAdapter* adapter)
{
#if ENABLE(GEOLOCATION)
    RefPtr corePage = adapter->page;
    auto& mockClient = toGeolocationClientMock(GeolocationController::from(corePage.get())->client());
    return mockClient.numberOfPendingPermissionRequests();
#else
    return -1;
#endif
}

QString DumpRenderTreeSupportQt::historyItemTarget(const QWebHistoryItem& historyItem)
{
    QWebHistoryItem it = historyItem;
    return (String(QWebHistoryItemPrivate::core(&it)->target()));
}

QMap<QString, QWebHistoryItem> DumpRenderTreeSupportQt::getChildHistoryItems(const QWebHistoryItem& historyItem)
{
    QWebHistoryItem it = historyItem;
    HistoryItem* item = QWebHistoryItemPrivate::core(&it);
    const auto& children = item->children();

    unsigned size = children.size();
    QMap<QString, QWebHistoryItem> kids;
    for (unsigned i = 0; i < size; ++i) {
        QWebHistoryItem kid(new QWebHistoryItemPrivate(children[i]->copy()));
        kids.insert(DumpRenderTreeSupportQt::historyItemTarget(kid), kid);
    }
    return kids;
}

bool DumpRenderTreeSupportQt::shouldClose(QWebFrameAdapter *adapter)
{
    WebCore::LocalFrame* coreFrame = adapter->frame;
    return coreFrame->loader().shouldClose();
}

void DumpRenderTreeSupportQt::clearScriptWorlds()
{
    m_worldMap.clear();
}

void DumpRenderTreeSupportQt::evaluateScriptInIsolatedWorld(QWebFrameAdapter *adapter, int worldID, const QString& script)
{
    QWebScriptWorld* scriptWorld;
    if (!worldID) {
        scriptWorld = new QWebScriptWorld();
    } else if (!m_worldMap.contains(worldID)) {
        scriptWorld = new QWebScriptWorld();
        m_worldMap.insert(worldID, scriptWorld);
    } else
        scriptWorld = m_worldMap.value(worldID);

    WebCore::LocalFrame* coreFrame = adapter->frame;

    ScriptController& proxy = coreFrame->script();
    proxy.executeScriptInWorldIgnoringException(*scriptWorld->world(), script, JSC::SourceTaintedOrigin::Untainted, true);
}

void DumpRenderTreeSupportQt::addUserStyleSheet(QWebPageAdapter* adapter, const QString& sourceCode)
{
    // QTFIXME: Fix userContentProvider/userContentController
    RELEASE_ASSERT_NOT_REACHED();

//    auto styleSheet = std::make_unique<UserStyleSheet>(sourceCode, URL(), Vector<String>(), Vector<String>(),
//        WebCore::InjectInAllFrames, UserStyleUserLevel);
//    adapter->page->userContentController()->addUserStyleSheet(mainThreadNormalWorld(), WTFMove(styleSheet),
//        InjectInExistingDocuments);
}

void DumpRenderTreeSupportQt::removeUserStyleSheets(QWebPageAdapter* adapter)
{
    // QTFIXME: Fix userContentProvider/userContentController
    RELEASE_ASSERT_NOT_REACHED();
//    adapter->page->userContentProvider()->removeUserStyleSheets(mainThreadNormalWorld());
}

void DumpRenderTreeSupportQt::simulateDesktopNotificationClick(const QString& title)
{
#if ENABLE(NOTIFICATIONS)
    NotificationPresenterClientQt::notificationPresenter()->notificationClicked(title);
#endif
}

void DumpRenderTreeSupportQt::setDefersLoading(QWebPageAdapter* adapter, bool flag)
{
    RefPtr corePage = adapter->page;
    if (corePage)
        corePage->setDefersLoading(flag);
}

void DumpRenderTreeSupportQt::goBack(QWebPageAdapter* adapter)
{
    RefPtr corePage = adapter->page;
    if (corePage)
        corePage->backForward().goBack();
}

// API Candidate?
QString DumpRenderTreeSupportQt::responseMimeType(QWebFrameAdapter* adapter)
{
    WebCore::LocalFrame* coreFrame = adapter->frame;
    WebCore::DocumentLoader* docLoader = coreFrame->loader().documentLoader();
    return docLoader->responseMIMEType();
}

void DumpRenderTreeSupportQt::clearOpener(QWebFrameAdapter* adapter)
{
    WebCore::LocalFrame* coreFrame = adapter->frame;
    coreFrame->setOpenerForWebKitLegacy(nullptr);
}

void DumpRenderTreeSupportQt::addURLToRedirect(const QString& origin, const QString& destination)
{
    FrameLoaderClientQt::URLsToRedirect[origin] = destination;
}

void DumpRenderTreeSupportQt::setInteractiveFormValidationEnabled(QWebPageAdapter* adapter, bool enable)
{
    RefPtr corePage = adapter->page;
    if (corePage)
        corePage->settings().setInteractiveFormValidationEnabled(enable);
}

QStringList DumpRenderTreeSupportQt::contextMenu(QWebPageAdapter* page)
{
    return page->menuActionsAsText();
}

bool DumpRenderTreeSupportQt::thirdPartyCookiePolicyAllows(QWebPageAdapter *adapter, const QUrl& url, const QUrl& firstPartyUrl)
{
    RefPtr corePage = adapter->page;
    auto* localFrame = dynamicDowncast<LocalFrame>(corePage->mainFrame());
    if (!localFrame)
        return false;
    auto* storageSession = localFrame->loader().networkingContext()->storageSession();
    return thirdPartyCookiePolicyPermits(storageSession, url, firstPartyUrl);
}

void DumpRenderTreeSupportQt::enableMockScrollbars()
{
    DeprecatedGlobalSettings::setMockScrollbarsEnabled(true);
}

QUrl DumpRenderTreeSupportQt::mediaContentUrlByElementId(QWebFrameAdapter* adapter, const QString& elementId)
{
    QUrl res;

//#if ENABLE(VIDEO) && USE(QT_MULTIMEDIA)
//    Frame* coreFrame = adapter->frame;
//    if (!coreFrame)
//        return res;

//    Document* doc = coreFrame->document();
//    if (!doc)
//        return res;

//    Node* coreNode = doc->getElementById(String(elementId));
//    if (!coreNode)
//        return res;

//    HTMLVideoElement* videoElement = downcast<HTMLVideoElement>(coreNode);
//    PlatformMedia platformMedia = videoElement->platformMedia();
//    if (platformMedia.type != PlatformMedia::QtMediaPlayerType)
//        return res;

//    MediaPlayerPrivateQt* mediaPlayerQt = static_cast<MediaPlayerPrivateQt*>(platformMedia.media.qtMediaPlayer);
//    if (mediaPlayerQt && mediaPlayerQt->mediaPlayer())
//        res = mediaPlayerQt->mediaPlayer()->media().canonicalUrl();
//#endif

    return res;
}

// API Candidate?
void DumpRenderTreeSupportQt::setAlternateHtml(QWebFrameAdapter* adapter, const QString& html, const QUrl& baseUrl, const QUrl& failingUrl)
{
    URL kurl(baseUrl);
    WebCore::LocalFrame* coreFrame = adapter->frame;
    WebCore::ResourceRequest request(kurl);
    const QByteArray utf8 = html.toUtf8();
    WTF::RefPtr<WebCore::SharedBuffer> data = WebCore::SharedBuffer::create(std::span { utf8.constData(), static_cast<size_t>(utf8.length()) });
    WebCore::ResourceResponse response(failingUrl, "text/html"_s, data->size(), "utf-8"_s);
    // FIXME: visibility?
    WebCore::SubstituteData substituteData(WTFMove(data), failingUrl, response, SubstituteData::SessionHistoryVisibility::Hidden);
    coreFrame->loader().load(WebCore::FrameLoadRequest(*coreFrame, request, substituteData));
}

void DumpRenderTreeSupportQt::confirmComposition(QWebPageAdapter *adapter, const char* text)
{
    RefPtr frame = adapter->page->focusController().focusedOrMainFrame();
    if (!frame)
        return;

    Editor& editor = frame->editor();
    if (!editor.hasComposition() && !text)
        return;

    if (editor.hasComposition()) {
        if (text)
            editor.confirmComposition(String::fromUTF8(text));
        else
            editor.confirmComposition();
    } else
        editor.insertText(String::fromUTF8(text), 0);
}

void DumpRenderTreeSupportQt::injectInternalsObject(QWebFrameAdapter* adapter)
{
    WebCore::LocalFrame* coreFrame = adapter->frame;
    JSDOMWindow* window = toJSDOMWindow(coreFrame, mainThreadNormalWorldSingleton());
    Q_ASSERT(window);

    JSC::JSGlobalObject* lexicalGlobalObject = window->globalObject();
    Q_ASSERT(lexicalGlobalObject);
    JSC::JSLockHolder lock(lexicalGlobalObject);

    JSContextRef context = toRef(lexicalGlobalObject);
    WebCoreTestSupport::injectInternalsObject(context);
}

void DumpRenderTreeSupportQt::injectInternalsObject(JSContextRef context)
{
    WebCoreTestSupport::injectInternalsObject(context);
}

void DumpRenderTreeSupportQt::resetInternalsObject(QWebFrameAdapter* adapter)
{
    WebCore::LocalFrame* coreFrame = adapter->frame;
    JSDOMWindow* window = toJSDOMWindow(coreFrame, mainThreadNormalWorldSingleton());
    Q_ASSERT(window);

    JSC::JSGlobalObject* lexicalGlobalObject = window->globalObject();
    Q_ASSERT(lexicalGlobalObject);
    JSC::JSLockHolder lock(lexicalGlobalObject);

    JSContextRef context = toRef(lexicalGlobalObject);
    WebCoreTestSupport::resetInternalsObject(context);
}

void DumpRenderTreeSupportQt::resetInternalsObject(JSContextRef context)
{
    WebCoreTestSupport::resetInternalsObject(context);
}

QImage DumpRenderTreeSupportQt::paintPagesWithBoundaries(QWebFrameAdapter* adapter)
{
    LocalFrame* frame = adapter->frame;
    PrintContext printContext(frame);

    QRect rect = frame->view()->frameRect();

    IntRect pageRect(0, 0, rect.width(), rect.height());

    printContext.begin(pageRect.width(), pageRect.height());
    float pageHeight = 0;
    printContext.computePageRects(pageRect, /* headerHeight */ 0, /* footerHeight */ 0, /* userScaleFactor */ 1.0, pageHeight);

    QPainter painter;
    int pageCount = printContext.pageCount();
    // pages * pageHeight and 1px line between each page
    int totalHeight = pageCount * (pageRect.height() + 1) - 1;
    QImage image(pageRect.width(), totalHeight, QImage::Format_ARGB32);
    image.fill(Qt::white);
    painter.begin(&image);

    GraphicsContextQt ctx(&painter);
    for (int i = 0; i < printContext.pageCount(); ++i) {
        printContext.spoolPage(ctx, i, pageRect.width());
        // translate to next page coordinates
        ctx.translate(0, pageRect.height() + 1);

        // if there is a next page, draw a blue line between these two
        if (i + 1 < printContext.pageCount()) {
            ctx.save();
            ctx.setStrokeColor(Color(Color::blue));
            ctx.setFillColor(Color(Color::blue));
            ctx.drawLine(IntPoint(0, -1), IntPoint(pageRect.width(), -1));
            ctx.restore();
        }
    }

    painter.end();
    printContext.end();

    return image;
}

void DumpRenderTreeSupportQt::setTrackRepaintRects(QWebFrameAdapter* adapter, bool enable)
{
    adapter->frame->view()->setTracksRepaints(enable);
}

bool DumpRenderTreeSupportQt::trackRepaintRects(QWebFrameAdapter* adapter)
{
    return adapter->frame->view()->isTrackingRepaints();
}

void DumpRenderTreeSupportQt::getTrackedRepaintRects(QWebFrameAdapter* adapter, QVector<QRectF>& result)
{
    LocalFrame* coreFrame = adapter->frame;
    const Vector<FloatRect>& rects = coreFrame->view()->trackedRepaintRects();
    result.resize(rects.size());
    for (size_t i = 0; i < rects.size(); ++i)
        result.append(rects[i]);
}

void DumpRenderTreeSupportQt::setDisableFontSubpixelAntialiasingForTesting(bool enabled)
{
    WebCore::FontCascade::setDisableFontSubpixelAntialiasingForTesting(enabled);
}

QString DumpRenderTreeSupportQt::frameRenderTreeDump(QWebFrameAdapter* adapter)
{
    if (adapter->frame->view() && adapter->frame->view()->layoutContext().isLayoutPending())
        adapter->frame->view()->layoutContext().layout();

    return externalRepresentation(adapter->frame);
}

void DumpRenderTreeSupportQt::clearNotificationPermissions()
{
#if ENABLE(NOTIFICATIONS)
    WebCore::NotificationPresenterClientQt::notificationPresenter()->clearCachedPermissions();
#endif
}

void DumpRenderTreeSupportQt::resetPageVisibility(QWebPageAdapter* adapter)
{
    adapter->page->setIsVisible(true);
}

void DumpRenderTreeSupportQt::getJSWindowObject(QWebFrameAdapter* adapter, JSContextRef* context, JSObjectRef* object)
{
    JSDOMWindow* window = toJSDOMWindow(adapter->frame, mainThreadNormalWorldSingleton());

    // TODO: fix this
    //*object = toRef(window);
    *context = toRef(window->globalObject());
}
