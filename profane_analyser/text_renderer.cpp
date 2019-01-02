
#include "pch.h"
#include "text_renderer.h"

constexpr char MonospaceFontAssetName[] = "assets/fonts/Ubuntu_Mono/UbuntuMono-Regular.ttf";

std::string FormatDuration(std::chrono::nanoseconds duration, int8_t significantDigits)
{
    return FormatDuration(duration.count(), significantDigits);
}

std::string FormatDuration(int64_t durationNs, int8_t significantDigits)
{
    bool negative = durationNs < 0;
    if (negative) durationNs = -durationNs;

    int8_t divisorLog10;
    const char* suffix;

    if (durationNs >= 316'200'000)
    {
        divisorLog10 = 9;
        suffix = " s";
    }
    else if (durationNs >= 316'200)
    {
        divisorLog10 = 6;
        suffix = " ms";
    }
    else if (durationNs >= 316)
    {
        divisorLog10 = 3;
        suffix = u8" \u00B5s";
    }
    else
    {
        divisorLog10 = 0;
        suffix = " ns";
    }

    char text[32];
    int8_t digitIdx = 0;
    int8_t lastCharIdx = 24;
    int8_t prependCharIdx = lastCharIdx;
    int8_t dotIdx = -1;

    while (durationNs > 0)
    {
        auto div_res = std::div(durationNs, 10LL);

        text[prependCharIdx--] = static_cast<char>(div_res.rem) + '0';
        ++digitIdx;

        if (digitIdx == divisorLog10)
        {
            dotIdx = prependCharIdx--;
            text[dotIdx] = '.';
        }

        durationNs = div_res.quot;
    }

    if (dotIdx == -1 && digitIdx < divisorLog10)
    {
        while (digitIdx < divisorLog10)
        {
            text[prependCharIdx--] = '0';
            ++digitIdx;
        }
        dotIdx = prependCharIdx--;
        text[dotIdx] = '.';
    }

    if (prependCharIdx + 1 == dotIdx || prependCharIdx == lastCharIdx)
    {
        text[prependCharIdx--] = '0';
        ++digitIdx;
    }

    if (dotIdx != -1)
    {
        significantDigits -= dotIdx - prependCharIdx - 1;

        if (significantDigits > 0)
            lastCharIdx = std::min(lastCharIdx, static_cast<int8_t>(dotIdx + significantDigits));
        else
            lastCharIdx = dotIdx;

        while (text[lastCharIdx] == '0')
            --lastCharIdx;

        if (text[lastCharIdx] == '.')
            --lastCharIdx;
    }

    if (negative)
        text[prependCharIdx--] = '-';

    assert(lastCharIdx > prependCharIdx);
    const auto suffixLength = std::strlen(suffix);
    const auto textLength = lastCharIdx - prependCharIdx + static_cast<int8_t>(suffixLength);

    std::strncpy(&text[lastCharIdx + 1], suffix, suffixLength);

    return std::string(&text[prependCharIdx + 1], textLength);
}

std::string FormatTimePoint(int64_t timeNs)
{
    bool negative = timeNs < 0;
    if (negative) timeNs = -timeNs;

    constexpr const char* const suffixes[] = {"n", u8"\u00B5 ", "m ", "s "};

    char text[32];
    int8_t digitIdx = 0;
    int8_t lastCharIdx = 31;
    int8_t prependCharIdx = lastCharIdx;
    bool nonZeroEncountered = false;

    while (timeNs > 0)
    {
        auto div_res = std::div(timeNs, 10LL);

        if (digitIdx % 3 == 0)
        {
            const auto suffixIndex = static_cast<int8_t>(digitIdx / 3);

            if (suffixIndex < static_cast<int8_t>(sizeof(suffixes) / sizeof(suffixes[0])))
            {
                const auto* const suffix = suffixes[suffixIndex];
                const auto suffixLength = std::strlen(suffix);

                if (!nonZeroEncountered)
                {
                    lastCharIdx = prependCharIdx;
                }

                prependCharIdx -= static_cast<int8_t>(suffixLength);
                std::strncpy(&text[prependCharIdx + 1], suffix, suffixLength);
            }
        }

        nonZeroEncountered |= (div_res.rem != 0);

        text[prependCharIdx--] = static_cast<char>(div_res.rem) + '0';
        ++digitIdx;

        timeNs = div_res.quot;
    }

    if (digitIdx == 0)
        text[prependCharIdx--] = '0';

    if (negative)
        text[prependCharIdx--] = '-';

    return std::string(&text[prependCharIdx + 1], lastCharIdx - prependCharIdx);
}

TextRenderer::TextRenderer(SDL_Renderer* renderer) :
    m_renderer{renderer}
{
    assert(m_renderer);

    m_font = TTF_OpenFont(MonospaceFontAssetName, 16);
}

SDL_Texture* TextRenderer::PrepareText(const std::string& text)
{
    assert(!text.empty());
    auto finding = m_inscriptions.find(text);
    if (finding == std::end(m_inscriptions))
    {
        SDL_Surface* surface = TTF_RenderUTF8_Solid(m_font, text.c_str(), SDL_Color{255, 255, 255, 255});
        SDL_Texture* texture = SDL_CreateTextureFromSurface(m_renderer, surface);
        SDL_FreeSurface(surface);

        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);

        auto insertion = m_inscriptions.insert(std::make_pair(std::string{text}, Inscription{decltype(Inscription::texture){texture, &SDL_DestroyTexture}}));
        finding = insertion.first;
    }

    finding->second.renderedFrameIdx = m_frameIdx;

    return finding->second.texture.get();
}

SDL_Rect TextRenderer::RenderText(int x, int y, SDL_Texture* texture, SDL_Color color)
{
    int width, height;
    SDL_QueryTexture(texture, nullptr, nullptr, &width, &height);

    SDL_SetTextureColorMod(texture, color.r, color.g, color.b);
    SDL_SetTextureAlphaMod(texture, color.a);

    SDL_Rect dstRect { x, y, width, height };
    SDL_RenderCopy(m_renderer, texture, nullptr, &dstRect);

    return dstRect;
}

SDL_Rect TextRenderer::RenderText(int x, int y, const std::string& text, SDL_Color color)
{
    return RenderText(x, y, PrepareText(text), color);
}

void TextRenderer::OnUpdate()
{
    if ((++m_frameIdx % FramesPerCollect) == 0)
    {
        Collect();
    }
}

void TextRenderer::Collect()
{
    std::vector<const char*> textsToRemove;

    for (const auto& inscriptionKV : m_inscriptions)
    {
        if (m_frameIdx - inscriptionKV.second.renderedFrameIdx >= FramesToRetire)
        {
            textsToRemove.push_back(inscriptionKV.first.c_str());
        }
    }

    for (const char* text : textsToRemove)
    {
        m_inscriptions.erase(text);
    }
}
