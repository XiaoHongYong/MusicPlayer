#import <Foundation/Foundation.h>
#import <Cocoa/Cocoa.h>
#import <QuartzCore/QuartzCore.h>
#import <math.h>
#include "../FontInfoEx.h"
#include "GfxFont.h"


CTFontRef createFont(CFStringRef iFamilyName, CTFontSymbolicTraits iTraits, CGFloat iSize, bool isUnderline) {
    assert(iFamilyName != nullptr);

    CTFontDescriptorRef descriptor = nullptr;

    // create a mutable dictionary to hold our attributes.
    CFMutableDictionaryRef attributes = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (attributes == nullptr) {
        return nullptr;
    }

    CFMutableDictionaryRef traits;
    CFNumberRef symTraits;

    // Add a family name to our attributes.
    CFDictionaryAddValue(attributes, kCTFontFamilyNameAttribute, iFamilyName);

    // create the traits dictionary.
    symTraits = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &iTraits);
    assert(symTraits);

    // create a dictionary to hold our traits values.
    traits = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    if (traits != nullptr) {
        // Add the symbolic traits value to the traits dictionary.
        CFDictionaryAddValue(traits, kCTFontSymbolicTrait, symTraits);

        // Add the traits attribute to our attributes.
        CFDictionaryAddValue(attributes, kCTFontTraitsAttribute, traits);
        CFRelease(traits);
    }
    CFRelease(symTraits);

//    int underlie = kCTUnderlineStyleSingle;
//    CFDictionaryAddValue(attributes, kCTUnderlineStyleAttributeName,
//         CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &underlie));

    // create the font descriptor with our attributes and input size.
    descriptor = CTFontDescriptorCreateWithAttributes(attributes);
    assert(descriptor);

    CFRelease(attributes);

    // Return our font descriptor.
    return CTFontCreateWithFontDescriptor(descriptor, iSize, nullptr);
}

CGFloat GetLineHeightForFont(CTFontRef iFont) {
    CGFloat lineHeight = 0.0;

    // Get the ascent from the font, already scaled for the font's size
    lineHeight += CTFontGetAscent(iFont);

    // Get the descent from the font, already scaled for the font's size
    lineHeight += CTFontGetDescent(iFont);

    // Get the leading from the font, already scaled for the font's size
    lineHeight += CTFontGetLeading(iFont);

    return lineHeight;
}

GfxFont::GfxFont() {
    m_font = nullptr;
    m_attributes = nullptr;
}

GfxFont::~GfxFont(void) {
    destroy();
}

bool GfxFont::create(cstr_t faceName, int fontHeight, bool italic, int weight, bool isUnderline) {
    destroy();

    if (faceName == nullptr || isEmptyString(faceName)) {
        faceName = "Helvetica";
    }

    for (int i = 0; i < 2; i++) {
        CFStringRef name = CFStringCreateWithCString(nullptr, faceName, kCFStringEncodingUTF8);
        CTFontSymbolicTraits fontTraits = 0;
        if (weight == FW_BOLD) {
            fontTraits |= kCTFontBoldTrait;
        }
        if (italic) {
            fontTraits |= kCTFontItalicTrait;
        }
        m_font = createFont(name, fontTraits, fontHeight, isUnderline);
        CFRelease(name);
        if (m_font != nullptr) {
            break;
        }
        faceName = "Helvetica";
    }

    if (m_font == nullptr) {
        return false;
    }

    CGColorSpaceRef rgbColorSpace = CGColorSpaceCreateDeviceRGB();
    CGFloat components[] = { 1.0, 1.0, 1.0, 1.0 };
    CGColorRef color = CGColorCreate(rgbColorSpace, components);
    CGColorSpaceRelease(rgbColorSpace);

    // Initialize string, font, and context
    CFStringRef keys[] = { kCTFontAttributeName, kCTForegroundColorAttributeName };
    CFTypeRef values[] = { m_font, color };

    m_attributes = CFDictionaryCreate(kCFAllocatorDefault, (const void**)&keys,
        (const void**)&values, sizeof(keys) / sizeof(keys[0]),
        &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);

    CGFloat lineHeight = GetLineHeightForFont(m_font);
    m_fontHeight = ceil(lineHeight);

    return true;
}

// 在基线位置绘制，返回字符串宽度
int GfxFont::draw(CGContextRef ctx, CFStringRef string, int x, int y) {
    assert(string);

    CFAttributedStringRef attrString = CFAttributedStringCreate(kCFAllocatorDefault, string, m_attributes);
    assert(attrString);

    CTLineRef line = CTLineCreateWithAttributedString(attrString);
    assert(line);

    CGContextSetTextPosition(ctx, x, y);

    CTLineDraw(line, ctx);

    CGPoint ptEnd = CGContextGetTextPosition(ctx);

    CFRelease(line);
    CFRelease(attrString);

    return ptEnd.x - x;
}

int GfxFont::getAscent() const {
    if (m_font == nullptr) {
        return 0;
    }
    return (int)ceil(CTFontGetAscent(m_font));
}

int GfxFont::getDescent() const {
    if (m_font == nullptr) {
        return 0;
    }
    return (int)ceil(CTFontGetDescent(m_font));
}

void GfxFont::destroy() {
    if (m_font != nullptr) {
        CFRelease(m_font);
        m_font = nullptr;
    }

    if (m_attributes) {
        CFRelease(m_attributes);
        m_attributes = nullptr;
    }
}
