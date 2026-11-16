#include "CanvasStore.hpp"

#include <QtCore/QLoggingCategory>
#include <QtCore/QRectF>

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <numbers>
#include <ranges>
#include <string_view>
#include <variant>

Q_STATIC_LOGGING_CATEGORY(lcCanvas, "app.canvas")

namespace App::Canvas {

namespace {

    constexpr std::size_t MaxHistory = 100;
    // Real shapes score above 0.9, scribbles and open curves below 0.8.
    constexpr double SnapScore = 0.85;

    struct NamedShape
    {
        std::string_view name;
        Shape shape;
    };
    constexpr std::array Shapes{
        NamedShape{ .name = "circle",    .shape = Shape::Circle    },
        NamedShape{ .name = "rectangle", .shape = Shape::Rectangle },
        NamedShape{ .name = "triangle",  .shape = Shape::Triangle  },
    };

    namespace Actions {
        struct AddStroke
        {
            StrokePtr stroke;
        };
        struct ReplaceLastStroke
        {
            StrokePtr stroke;
        };
        struct Clear
        {
        };
        struct SetPage
        {
            Page page;
        };
    } // namespace Actions

    using Action = std::variant<Actions::AddStroke, Actions::ReplaceLastStroke, Actions::Clear,
        Actions::SetPage>;

    template <typename... Fs>
    struct Overloaded : Fs...
    {
        using Fs::operator()...;
    };

    [[nodiscard]] Sketch reduce(Sketch sketch, const Action &action)
    {
        std::visit(
            Overloaded{
                [&](const Actions::AddStroke &add) { sketch.strokes.push_back(add.stroke); },
                [&](const Actions::ReplaceLastStroke &replace) {
                    if (!sketch.strokes.empty())
                        sketch.strokes.back() = replace.stroke;
                },
                [&](Actions::Clear) { sketch.strokes.clear(); },
                [&](const Actions::SetPage &set) { sketch.page = set.page; },
            },
            action);
        return sketch;
    }

    QRectF bounds(std::span<const Shapes::Point> points)
    {
        const auto xBounds = std::ranges::minmax(points | std::views::transform(&Shapes::Point::x));
        const auto yBounds = std::ranges::minmax(points | std::views::transform(&Shapes::Point::y));
        return { QPointF(xBounds.min, yBounds.min), QPointF(xBounds.max, yBounds.max) };
    }

    // The clean outline of a recognized shape. Drawn into a unit square, it is also the template.
    std::vector<Shapes::Point> idealShape(std::string_view name, const QRectF &box)
    {
        const auto at = [&](double x, double y) {
            return Shapes::Point{ .x = box.left() + (x * box.width()),
                .y = box.top() + (y * box.height()) };
        };
        if (name == "rectangle")
            return { at(0, 0), at(1, 0), at(1, 1), at(0, 1), at(0, 0) };
        if (name == "triangle")
            return { at(0, 1), at(0.5, 0), at(1, 1), at(0, 1) };
        std::vector<Shapes::Point> circle;
        for (int i = 0; i <= 64; ++i) {
            const double angle = 2.0 * std::numbers::pi * i / 64.0;
            circle.push_back(at(0.5 + (0.5 * std::cos(angle)), 0.5 + (0.5 * std::sin(angle))));
        }
        return circle;
    }

    // The templates are polygons with only a few points; resampling spreads them out.
    Shapes::Recognizer basicShapes()
    {
        Shapes::Recognizer recognizer;
        for (const auto &[name, shape] : Shapes) {
            auto outline = idealShape(name, QRectF(0, 0, 1, 1));
            recognizer.addTemplate(std::string(name), outline);
            // People draw both clockwise and counterclockwise.
            std::ranges::reverse(outline);
            recognizer.addTemplate(std::string(name), outline);
        }
        return recognizer;
    }

} // namespace

CanvasStore::CanvasStore(QObject *parent)
    : QObject(parent), m_recognizer(basicShapes())
{
}

const Sketch &CanvasStore::sketch() const
{
    return m_history.at(m_current);
}

void CanvasStore::load(Sketch sketch)
{
    m_history = { std::move(sketch) };
    m_current = 0;
    Q_EMIT changed();
}

bool CanvasStore::canUndo() const
{
    return m_current > 0;
}

bool CanvasStore::canRedo() const
{
    return m_current + 1 < m_history.size();
}

bool CanvasStore::isEmpty() const
{
    return sketch().empty();
}

int CanvasStore::strokeCount() const
{
    return static_cast<int>(sketch().strokes.size());
}

int CanvasStore::pointCount() const
{
    std::size_t count = 0;
    for (const auto &stroke : sketch().strokes)
        count += stroke->points.size();
    return static_cast<int>(count);
}

int CanvasStore::pageWidth() const
{
    return sketch().page.width;
}

int CanvasStore::pageHeight() const
{
    return sketch().page.height;
}

Background CanvasStore::background() const
{
    return sketch().page.background;
}

int CanvasStore::spacing() const
{
    return sketch().page.spacing;
}

int CanvasStore::thickness() const
{
    return sketch().page.thickness;
}

bool CanvasStore::snapShapes() const
{
    return m_snapShapes;
}

void CanvasStore::setSnapShapes(bool snap)
{
    if (snap == m_snapShapes)
        return;
    m_snapShapes = snap;
    qCDebug(lcCanvas) << "Shape snapping" << (snap ? "enabled" : "disabled");
    Q_EMIT snapShapesChanged();
}

void CanvasStore::addStroke(const QList<QPointF> &points, const QColor &color, double width)
{
    if (points.isEmpty())
        return;
    const auto toPoint = [](QPointF p) {
        return Shapes::Point{ .x = p.x(), .y = p.y() };
    };
    auto stroke = std::make_shared<const Stroke>(Stroke{
        .points = points | std::views::transform(toPoint) | std::ranges::to<std::vector>(),
        .color = color.name().toStdString(),
        .width = std::clamp(width, 1.0, 36.0),
    });
    dispatch(reduce(sketch(), Actions::AddStroke{ stroke }));

    if (!m_snapShapes)
        return;
    const auto match = m_recognizer.recognize(stroke->points);
    if (!match || match->score < SnapScore)
        return;
    qCDebug(lcCanvas) << "Snapping to" << match->name << "with score" << match->score;
    dispatch(reduce(sketch(),
        Actions::ReplaceLastStroke{ std::make_shared<const Stroke>(Stroke{
            .points = idealShape(match->name, bounds(stroke->points)),
            .color = stroke->color,
            .width = stroke->width,
        }) }));
    Q_EMIT shapeSnapped(std::ranges::find(Shapes, match->name, &NamedShape::name)->shape);
}

void CanvasStore::clear()
{
    dispatch(reduce(sketch(), Actions::Clear{}));
}

void CanvasStore::setPageSize(int width, int height)
{
    if (width <= 0 || height <= 0)
        return;
    auto page = sketch().page;
    page.width = width;
    page.height = height;
    dispatch(reduce(sketch(), Actions::SetPage{ page }));
}

void CanvasStore::setBackground(Background background)
{
    if (background < Background::Plain || background > Background::Ruled)
        return;
    auto page = sketch().page;
    page.background = background;
    dispatch(reduce(sketch(), Actions::SetPage{ page }));
}

void CanvasStore::setPattern(int spacing, int thickness)
{
    auto page = sketch().page;
    page.spacing = std::clamp(spacing, 8, 128);
    page.thickness = std::clamp(thickness, 1, 8);
    dispatch(reduce(sketch(), Actions::SetPage{ page }));
}

void CanvasStore::undo()
{
    if (canUndo()) {
        moveTo(m_current - 1);
        qCDebug(lcCanvas) << "Undo, history position" << m_current;
        Q_EMIT historyMoved(false);
    }
}

void CanvasStore::redo()
{
    if (canRedo()) {
        moveTo(m_current + 1);
        qCDebug(lcCanvas) << "Redo, history position" << m_current;
        Q_EMIT historyMoved(true);
    }
}

void CanvasStore::dispatch(Sketch next)
{
    if (next == sketch())
        return;
    // A new edit makes the undone steps unreachable.
    m_history.resize(m_current + 1);
    m_history.push_back(std::move(next));
    if (m_history.size() > MaxHistory)
        m_history.erase(m_history.begin());
    moveTo(m_history.size() - 1);
}

void CanvasStore::moveTo(std::size_t index)
{
    m_current = index;
    Q_EMIT changed();
    Q_EMIT edited();
}

} // namespace App::Canvas
