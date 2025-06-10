/*
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "BoxExtents.h"
#include "CSSPrimitiveNumericUnits.h"
#include "Length.h"
#include "StyleValueTypes.h"

namespace WebCore {

class CSSValue;
class LayoutRect;
class LayoutUnit;
class RenderStyle;

namespace Style {

class BuilderState;
struct ExtractorState;

// <'scroll-margin-*'> = <length>
// https://drafts.csswg.org/css-scroll-snap-1/#margin-longhands-physical
struct ScrollMarginEdge {
    ScrollMarginEdge(WebCore::Length&& value)
        : m_value { WTFMove(value) }
    {
        RELEASE_ASSERT(m_value.isFixed());
    }

    ScrollMarginEdge(CSS::ValueLiteral<CSS::LengthUnit::Px> pixels)
        : m_value { pixels.value, WebCore::LengthType::Fixed }
    {
    }

    LayoutUnit evaluate(LayoutUnit referenceLength) const;
    float evaluate(float referenceLength) const;

    Ref<CSSValue> toCSS(ExtractorState&) const;

    bool isZero() const { return m_value.isZero(); }

    template<typename F> decltype(auto) switchOn(F&& functor) const
    {
        return functor(Style::Length<> { m_value.value() });
    }

    bool operator==(const ScrollMarginEdge&) const = default;

private:
    friend WTF::TextStream& operator<<(WTF::TextStream&, const ScrollMarginEdge&);

    WebCore::Length m_value;
};

// <'scroll-margin'> = <length>{1,4}
// https://drafts.csswg.org/css-scroll-snap-1/#propdef-scroll-margin
struct ScrollMargin : SpaceSeparatedRectEdges<ScrollMarginEdge> {
    using Wrapped = SpaceSeparatedRectEdges<ScrollMarginEdge>;
    using Wrapped::Wrapped;
    using Wrapped::operator=;

    template<size_t I> friend const auto& get(const ScrollMargin& self)
    {
        return get<I>(static_cast<const Wrapped&>(self));
    }

    bool operator==(const ScrollMargin&) const = default;
};

// MARK: - Conversion

ScrollMarginEdge scrollMarginEdgeFromCSSValue(const CSSValue&, BuilderState&);

// MARK: - Evaluation

template<> struct Evaluation<ScrollMarginEdge> {
    template<typename T> auto operator()(const ScrollMarginEdge& edge, T referenceLength) -> T
    {
        return edge.evaluate(referenceLength);
    }
};

// MARK: - Extent

LayoutBoxExtent extentForRect(const ScrollMargin&, const LayoutRect&);

// MARK: - Logging

WTF::TextStream& operator<<(WTF::TextStream&, const ScrollMarginEdge&);

} // namespace Style
} // namespace WebCore

template<> inline constexpr auto WebCore::TreatAsVariantLike<WebCore::Style::ScrollMarginEdge> = true;

DEFINE_TUPLE_LIKE_CONFORMANCE(WebCore::Style::ScrollMargin, 4)
