#pragma once

#include <shapes/Recognizer.h>

#include <QtCore/QObject>
#include <QtCore/QtTypes>
#include <QtQmlIntegration/qqmlintegration.h>

#include <memory>
#include <string>
#include <vector>

namespace app::canvas {

Q_NAMESPACE
QML_NAMED_ELEMENT(Canvas)

enum class Background { Plain, Dots, Ruled };
Q_ENUM_NS(Background)

enum class Shape : quint8 { Circle, Rectangle, Triangle };
Q_ENUM_NS(Shape)

struct Stroke
{
    std::vector<shapes::Point> points;
    std::string color;
    double width = 4.0;
};

using StrokePtr = std::shared_ptr<const Stroke>;

struct Page
{
    int width = 1600;
    int height = 1200;
    Background background = Background::Plain;
    int spacing = 32;
    int thickness = 1;

    bool operator==(const Page &) const = default;
};

struct Sketch
{
    std::vector<StrokePtr> strokes;
    Page page;

    [[nodiscard]] bool empty() const { return strokes.empty(); }
    bool operator==(const Sketch &) const = default;
};

} // namespace app::canvas
