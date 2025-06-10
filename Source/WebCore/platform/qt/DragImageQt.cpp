/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
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
#include "DragImage.h"

#include "Image.h"
#include <QImage>

namespace WebCore {

IntSize dragImageSize(DragImageRef image)
{
//    if (!image)
//        return IntSize();

    return image.size();
}

void deleteDragImage(DragImageRef image)
{
//    delete image;
}

DragImageRef scaleDragImage(DragImageRef image, FloatSize scale)
{
    if (image.isNull())
        return QImage();

    int scaledWidth = image.width() * scale.width();
    int scaledHeight = image.height() * scale.height();

    return image.scaled(scaledWidth, scaledHeight);
}

DragImageRef dissolveDragImageToFraction(DragImageRef image, float)
{
    return image;
}

DragImageRef createDragImageFromImage(Image* image, ImageOrientation, GraphicsClient*, float)
{
    if (!image/* || !image->currentNativeImage()*/)
        return QImage();

    return image->currentNativeImage()->platformImage();
//    return new QImage(*image->currentNativeImage());
}

DragImageRef createDragImageIconForCachedImageFilename(const String&)
{
    return QImage();
}

DragImageRef createDragImageForLink(Element&, URL&, const String&, TextIndicatorData&, float)
{
    return QImage();
}

DragImageRef createDragImageForColor(const Color&, const FloatRect&, float, Path&)
{
    return QImage();
}

}
