/*
 * Copyright (C) 2017 Metrological Group B.V.
 * Copyright (C) 2017 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if PLATFORM(QT)

#include "GraphicsContext.h"

#include "TransparencyLayer.h"
#include <QPainter>
#include <QStack>

namespace WebCore {

class GraphicsContextPlatformPrivate {
    WTF_MAKE_NONCOPYABLE(GraphicsContextPlatformPrivate); WTF_MAKE_FAST_ALLOCATED;
public:
    GraphicsContextPlatformPrivate(QPainter*, const QColor& initialSolidColor);
    ~GraphicsContextPlatformPrivate();

    inline QPainter* p() const
    {
        if (layers.isEmpty())
            return painter;
        return &layers.top()->painter;
    }

    bool antiAliasingForRectsAndLines;

    QStack<TransparencyLayer*> layers;
    // Counting real layers. Required by isInTransparencyLayer() calls
    // For example, layers with valid alphaMask are not real layers
    int layerCount;

    // reuse this brush for solid color (to prevent expensive QBrush construction)
    QBrush solidColor;

    bool initialSmoothPixmapTransformHint;

    QRectF clipBoundingRect() const
    {
        return p()->clipBoundingRect();
    }

    void takeOwnershipOfPlatformContext() { platformContextIsOwned = true; }

private:
    QPainter* painter;
    bool platformContextIsOwned;
};

class WEBCORE_EXPORT GraphicsContextQt final : public GraphicsContext {
public:
    GraphicsContextQt(QPainter* painter);
    ~GraphicsContextQt();

    bool hasPlatformContext() const final;
    GraphicsContextQt* platformContext() const final;

    void save(GraphicsContextState::Purpose = GraphicsContextState::Purpose::SaveRestore) final;
    void restore(GraphicsContextState::Purpose = GraphicsContextState::Purpose::SaveRestore) final;

    void didUpdateState(GraphicsContextState&) final;

    void drawRect(const FloatRect&, float borderThickness = 1) final;
    void drawLine(const FloatPoint&, const FloatPoint&) final;
    void drawEllipse(const FloatRect&) final;

    void fillPath(const Path&) final;
    void strokePath(const Path&) final;

    using GraphicsContext::fillRect;
    void fillRect(const FloatRect&, RequiresClipToRect = RequiresClipToRect::Yes) final;
    void fillRect(const FloatRect&, const Color&) final;
    void fillRect(const FloatRect&, Gradient&) final;
    void fillRect(const FloatRect&, Gradient&, const AffineTransform&, RequiresClipToRect = RequiresClipToRect::Yes) final;

    void fillRoundedRectImpl(const FloatRoundedRect&, const Color&) final;
    void fillRectWithRoundedHole(const FloatRect&, const FloatRoundedRect& roundedHoleRect, const Color&) final;
    void clearRect(const FloatRect&) final;
    void strokeRect(const FloatRect&, float lineWidth) final;

    void setLineCap(LineCap) final;
    void setLineDash(const DashArray&, float dashOffset) final;
    void setLineJoin(LineJoin) final;
    void setMiterLimit(float) final;

    void drawNativeImageInternal(NativeImage&, const FloatRect& destRect, const FloatRect& srcRect, ImagePaintingOptions = { }) final;
    void drawPattern(NativeImage&, const FloatRect& destRect, const FloatRect& tileRect, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing, ImagePaintingOptions = { }) final;

    void clip(const FloatRect&) final;
    void clipOut(const FloatRect&) final;

    void clipOut(const Path&) final;
    void resetClip() final;

    void clipPath(const Path&, WindRule = WindRule::EvenOdd) final;
    void clipToImageBuffer(ImageBuffer& buffer, const FloatRect& destRect) final;

    void drawGlyphs(const Font&, std::span<const GlyphBufferGlyph>, std::span<const GlyphBufferAdvance>, const FloatPoint&, FontSmoothingMode) final;

    void drawLinesForText(const FloatPoint&, float thickness, const DashArray& widths, bool printing, bool doubleLines, StrokeStyle) final;
    void drawLineForText(const FloatRect&, bool printing, bool doubleLines = false, StrokeStyle = StrokeStyle::SolidStroke);
    void drawDotsForDocumentMarker(const FloatRect&, DocumentMarkerLineStyle) final;

    void drawFocusRing(const Vector<FloatRect>&, float outlineOffset, float outlineWidth, const Color&) final;
    void drawFocusRing(const Path&, float outlineWidth, const Color&) final;

    void setURLForRect(const URL& url, const FloatRect& rect) final;
    RenderingMode renderingMode() const final;

    using GraphicsContext::scale;
    void scale(const FloatSize&) final;
    void rotate(float angleInRadians) final;
    void translate(float x, float y) final;

    void concatCTM(const AffineTransform&) final;
    void setCTM(const AffineTransform&) override;
    IntRect clipBounds() const final;

    AffineTransform getCTM(IncludeDeviceScale = PossiblyIncludeDeviceScale) const override;

    void beginTransparencyLayer(float) final;
    void endTransparencyLayer() final;

    QPainter* painter() const;

    void pushTransparencyLayerInternal(const QRect&, qreal, const QImage&);

    void takeOwnershipOfPlatformContext();
private:
    void popTransparencyLayerInternal();

    GraphicsContextPlatformPrivate* m_data { nullptr };
};

}

#endif
