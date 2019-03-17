
#pragma once

#include "pch.h"

namespace detail
{
    template<typename T> T LexicalCast(std::string textValue);
    template<> inline int64_t LexicalCast<int64_t>(std::string textValue) { return std::atoll(textValue.c_str()); }
    template<> inline int LexicalCast<int>(std::string textValue) { return std::atoi(textValue.c_str()); }
    template<> inline double LexicalCast<double>(std::string textValue) { return std::stod(textValue); }
    template<> inline std::string LexicalCast<std::string>(std::string textValue) { return textValue; }

    template<> inline SDL_Color LexicalCast<SDL_Color>(std::string textValue) {
        throw std::logic_error{"Not implemented"};
    }

    class ConfigBase
    {
        struct PropertyBase
        {
            std::string name;
            std::string description;

            PropertyBase(std::string name_, std::string description_) :
                name{name_},
                description{description_}
            {}

            virtual ~PropertyBase() = default;
            virtual void Value(std::string valueText) = 0;
        };

        template<typename T>
        struct PropertyTyped : public PropertyBase
        {
            T& field;

            PropertyTyped(T& field_, std::string name_, std::string description_) :
                field{field_},
                PropertyBase{name_, description_}
            {}

            virtual ~PropertyTyped() override = default;
            virtual void Value(std::string valueText) override { field = LexicalCast<T>(std::move(valueText)); }
        };

        std::map<std::string, std::unique_ptr<PropertyBase>> m_properties;

    protected:
        ConfigBase() = default;

        template<typename T>
        void RegisterProperty(T& field, std::string name, std::string description)
        {
            auto insertion = m_properties.insert(std::make_pair(name, std::make_unique<PropertyTyped<T>>(field, name, std::move(description))));
            if (!insertion.second)
                throw std::runtime_error{"Config property '" + name + "' already registered."};
            //auto& prop = *insertion.first->second;
        }

    public:
        const auto& Properties() { return m_properties; }
    };
}

class Config : public detail::ConfigBase
{
public:
    int MinTimeScaleLabelWidthPx = 192;
    int64_t MinCameraWidthNs = 1000;
    SDL_Color BackgroundColor { 26, 22, 22, 255 };
    SDL_Color TopBottomBarColor { 43, 43, 47, 255 };
    SDL_Color RulerLine1Color { 180, 240, 210, 128 };
    SDL_Color RulerLine2Color { 180, 240, 210, 255 };
    SDL_Color WorkItemBlockBorderColor { 16, 6, 6, 255 };
    SDL_Color WorkItemBackgroundColor_Slow {103, 51, 45, 255};
    SDL_Color WorkItemBackgroundColor_Mid {150, 120, 50, 255};
    SDL_Color WorkItemBackgroundColor_Fast {51, 103, 45, 255};
    SDL_Color WorkItemText1Color { 180, 240, 210, 255 };
    SDL_Color WorkItemText2Color { 130, 240, 175, 255 };
    SDL_Color WorkerBannerBackgroundColor { 128, 28, 28, 64 };
    SDL_Color WorkerBannerTextColor { 175, 125, 125, 255 };
    SDL_Color MouseMarkerColor { 180, 240, 210, 135 };
    double MouseZoomSpeed = 0.75;
    std::string FontFilePath = "assets/fonts/Ubuntu_Mono/UbuntuMono-Regular.ttf";

    Config()
    {
        RegisterProperty(MinTimeScaleLabelWidthPx, "ui.timescale.min-label-distance", "Minimal distance of time labels (in pixels) on the time scale.");
        RegisterProperty(MinCameraWidthNs, "ui.timescale.min-view-timespan", "Minimal time span (in nanoseconds) to which camera can zoom in.");
        RegisterProperty(BackgroundColor, "ui.background-color", "Background color.");
        RegisterProperty(TopBottomBarColor, "ui.top-bottom-bar-color", "Top/bottom bars background color.");
        RegisterProperty(RulerLine1Color, "ui.ruler.line1-color", "Time scale horizontal ruler line color.");
        RegisterProperty(RulerLine2Color, "ui.ruler.line2-color", "Time scale vertical ruler lines color.");
        RegisterProperty(WorkItemBlockBorderColor, "ui.workitem.border-color", "Work item block border color.");
        RegisterProperty(WorkItemBackgroundColor_Slow, "ui.workitem.background-color:slow", "Slowest work item background color.");
        RegisterProperty(WorkItemBackgroundColor_Mid, "ui.workitem.background-color:mid", "Average or median work item background color.");
        RegisterProperty(WorkItemBackgroundColor_Fast, "ui.workitem.background-color:fast", "Fastes work item background border color.");
        RegisterProperty(WorkItemText1Color, "ui.workitem.text1-color", "Work item routine name caption color.");
        RegisterProperty(WorkItemText2Color, "ui.workitem.text2-color", "Work item duration color.");
        RegisterProperty(WorkerBannerBackgroundColor, "ui.worker.background-color", "Worker banner background color.");
        RegisterProperty(WorkerBannerTextColor, "ui.worker.text-color", "Worker banner caption color.");
        RegisterProperty(MouseMarkerColor, "ui.mouse.marker-color", "Mouse marker color (and its time point).");
        RegisterProperty(MouseZoomSpeed, "ui.mouse.zoom-speed", "Mouse zoom speed.");
        RegisterProperty(FontFilePath, "ui.font-file", "File path to the font asset (.ttf).");
    }
};
