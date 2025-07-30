/*
    Copyright (C) 2009 Nokia Corporation and/or its subsidiary(-ies)
    Copyright (C) 2017 Konstantin Tokarev <annulen@yandex.ru>

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

#include "config.h"
#include "BlobUrlConversion.h"

#include "BlobData.h"
#include "BlobRegistryImpl.h"

#include <QUrl>
#include <wtf/text/Base64.h>

namespace WebCore {

static bool appendBlobResolved(Vector<char>& out, const URL& url, const BlobRegistryImpl& blobRegistry, QString& contentType)
{
    RefPtr<BlobData> blobData = blobRegistry.blobDataFromURL(url);
    if (!blobData)
        return false;

    contentType = blobData->contentType();

    BlobDataItemList::const_iterator it = blobData->items().begin();
    const BlobDataItemList::const_iterator itend = blobData->items().end();
    for (; it != itend; ++it) {
        const BlobDataItem& blobItem = *it;
        if (blobItem.type() == BlobDataItem::Type::Data) {
            ASSERT(blobItem.data());
            if (!out.tryAppend(blobItem.data()->span().subspan(blobItem.offset(), blobItem.length())))
                return false;
        } else if (blobItem.type() == BlobDataItem::Type::File) {
            // File types are not allowed here, so just ignore it.
            RELEASE_ASSERT_WITH_MESSAGE(false, "File types are not allowed here");
        } else
            ASSERT_NOT_REACHED();
    }
    return true;
}

static QUrl resolveBlobUrl(const URL& url, const BlobRegistryImpl& blobRegistry)
{
    RefPtr<BlobData> blobData = blobRegistry.blobDataFromURL(url);
    if (!blobData)
        return QUrl();

    Vector<char> data;
    QString contentType;
    if (!appendBlobResolved(data, url, blobRegistry, contentType)) {
        qWarning("Failed to convert blob data to base64: cannot allocate memory for continuous blob data");
        return QUrl();
    }

    // QByteArray::{from,to}Base64 are prone to integer overflow, this is the maximum size that can be safe
    size_t maxBase64Size = std::numeric_limits<int>::max() / 3 - 1;

    std::span<unsigned char> base64;
    std::span<const std::byte> bytes { reinterpret_cast<const std::byte*>(data.data()), static_cast<size_t>(data.size()) };
    WTF::base64Encode(bytes, base64, Base64EncodeOption::URL);
    if (base64.empty() || base64.size() > maxBase64Size) {
        qWarning("Failed to convert blob data to base64: data is too large");
        return QUrl();
    }

    QString dataUri(QStringLiteral("data:"));
    dataUri.append(contentType);
    dataUri.append(QStringLiteral(";base64,"));
    dataUri.reserve(dataUri.size() + base64.size());
    dataUri.append(QLatin1String(reinterpret_cast<const char*>(base64.data()), base64.size()));
    return QUrl(dataUri);
}

QUrl convertBlobToDataUrl(const QUrl& url, const BlobRegistryImpl& blobRegistry)
{
    QT_TRY {
        return resolveBlobUrl(url, blobRegistry);
    } QT_CATCH(const std::bad_alloc &) {
        qWarning("Failed to convert blob data to base64: not enough memory");
    }
    return QUrl();
}

} // namespace WebCore
