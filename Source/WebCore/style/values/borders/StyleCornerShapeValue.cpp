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

#include "config.h"
#include "StyleCornerShapeValue.h"

#include "CSSFunctionValue.h"
#include "CSSPrimitiveValue.h"
#include "CSSValuePool.h"
#include "StylePrimitiveNumericTypes+Blending.h"

namespace WebCore {
namespace Style {

// MARK: - Conversion

Ref<CSSValue> toCSSValue(const CornerShapeValue& cornerShapeValue, const RenderStyle&)
{
    if (cornerShapeValue == CornerShapeValue::round())
        return CSSPrimitiveValue::create(CSSValueRound);
    if (cornerShapeValue == CornerShapeValue::scoop())
        return CSSPrimitiveValue::create(CSSValueScoop);
    if (cornerShapeValue == CornerShapeValue::bevel())
        return CSSPrimitiveValue::create(CSSValueBevel);
    if (cornerShapeValue == CornerShapeValue::notch())
        return CSSPrimitiveValue::create(CSSValueNotch);
    if (cornerShapeValue == CornerShapeValue::straight())
        return CSSPrimitiveValue::create(CSSValueStraight);
    if (cornerShapeValue == CornerShapeValue::squircle())
        return CSSPrimitiveValue::create(CSSValueSquircle);
    return CSSFunctionValue::create(cornerShapeValue.superellipse.name, CSSPrimitiveValue::create(cornerShapeValue.superellipse->value));
}

// MARK: - Blending

// https://drafts.csswg.org/css-borders-4/#corner-shape-interpolation

static Number<CSS::Nonnegative> convertExponentToInterpolationValue(const CornerShapeValue& cornerShape)
{
    auto exponent = cornerShape.superellipse->value;

    // 1. If exponent is 0, return 0.
    if (exponent == 0.0)
        return 0.0;

    // 2. If exponent is ∞, return 1.
    if (exponent == std::numeric_limits<double>::infinity())
        return 1.0;

    // 3. Return 1/(2^(1/exponent)).
    return 1.0 / std::pow(2.0, 1.0 / exponent);
}

static CornerShapeValue convertInterpolationValueToExponent(Number<CSS::Nonnegative> interpolationValue)
{
    // 1. If interpolationValue is 0, return 0.
    if (interpolationValue.value == 0.0)
        return { SuperellipseFunction { 0.0 } };

    // 2. If interpolationValue is 1, return ∞.
    if (interpolationValue.value == 1.0)
        return { SuperellipseFunction { std::numeric_limits<double>::infinity() } };

    // 3. Return ln(0.5)/ln(interpolationValue).
    return { SuperellipseFunction { std::log(0.5) / std::log(interpolationValue.value) } };
}

auto Blending<CornerShapeValue>::blend(const CornerShapeValue& a, const CornerShapeValue& b, const BlendingContext& context) -> CornerShapeValue
{
    auto aInterpolationValue = convertExponentToInterpolationValue(a);
    auto bInterpolationValue = convertExponentToInterpolationValue(b);

    auto interpolatedValue = Style::blend(aInterpolationValue, bInterpolationValue, context);

    return convertInterpolationValueToExponent(interpolatedValue);
}

} // namespace Style
} // namespace WebCore
