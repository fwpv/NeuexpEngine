#pragma once

#include "camera.h"
#include "ui.h"

#include <optional>
#include <vector>

namespace Engine
{
enum class RenderError
{
    None,
    InvalidState,
    InvalidFrameSize,
    InvalidView,
    InvalidClip,
    InvalidGeometry,
    InvalidOptions
};

enum class ShapeFill
{
    Outline,
    Filled
};

struct DrawOptions
{
    Color color;
    // Локальные координаты объекта переводятся в координаты активного мира или UI.
    Affine2 transform;
    // Толщина линий и размер точек в клиентских пикселях, независимо от масштаба вида.
    // Поддерживаемый диапазон и округление определяются драйвером OpenGL.
    float lineWidth = 1;
    float pointSize = 1;
};

// Рисует в текущий графический контекст в потоке приложения. Контекст должен оставаться
// действующим до endFrame(). Одновременно кадром управляет только один рендерер;
// прямые изменения состояния OpenGL внутри кадра не поддерживаются.
// GPU-ресурсы не создаются; конструктор и деструктор не обращаются к контексту.
class Renderer2D
{
public:
    Renderer2D() = default;
    Renderer2D(const Renderer2D&) = delete;
    Renderer2D& operator=(const Renderer2D&) = delete;

    // Размеры — фактические клиентские пиксели. Очищается весь цветовой буфер,
    // включая поля; глубина отключена. Вложенные кадры отклоняются.
    bool beginFrame(int width, int height, Color clearColor, RenderError& error);
    // Несколько мировых видов допустимы до первого beginUi(). Команды выполняются
    // сразу, в порядке вызовов, с обычным альфа-смешиванием без записи глубины.
    bool beginWorld(const CameraView& view, RenderError& error);
    // Один или несколько экранных видов рисуются после мира, поверх него.
    // Виды должны быть получены из Camera2::createView / UiCanvas::createView.
    bool beginUi(const UiView& view, RenderError& error);
    // Дополнительное отсечение в клиентских пикселях: начало сверху слева,
    // правая и нижняя границы исключены. Пересекается с viewport и окном;
    // нулевой размер отсекает всё. nullopt снимает дополнительное отсечение.
    // Каждый beginWorld/beginUi сбрасывает дополнительное отсечение.
    bool setClip(const std::optional<Viewport>& clip, RenderError& error);
    // Завершает кадр без показа: present() вызывается игровым циклом отдельно.
    bool endFrame(RenderError& error);

    // Рисование требует активного вида. Ошибка не завершает кадр и не рисует фигуру.
    // Все координаты и коэффициенты преобразований конечны; компоненты цвета — [0, 1].
    bool drawPoint(Vector2 point, const DrawOptions& options, RenderError& error);
    bool drawLine(Vector2 start, Vector2 end, const DrawOptions& options, RenderError& error);
    bool drawRectangle(const Rect& rectangle, ShapeFill fill, const DrawOptions& options, RenderError& error);
    // Нулевые размеры прямоугольника/радиус окружности допустимы, заливка не рисуется.
    // segments >= 3; аппроксимация окружности задаётся в локальных координатах.
    bool drawCircle(const Circle& circle, ShapeFill fill, const DrawOptions& options,
        RenderError& error, unsigned int segments = 48);
    // Простой многоугольник без отверстий и самопересечений, любой обход, >= 3 вершин.
    // Замыкающая копия первой вершины, соседние повторы и коллинеарные вершины допустимы.
    // Вогнутая заливка выполняется триангуляцией; вырожденный контур отклоняется.
    bool drawPolygon(const std::vector<Vector2>& vertices, ShapeFill fill,
        const DrawOptions& options, RenderError& error);

private:
    enum class Stage
    {
        Idle,
        Frame,
        World,
        Ui
    };

    Stage stage = Stage::Idle;
    int frameWidth = 0;
    int frameHeight = 0;
    Viewport viewport;
    Affine2 viewTransform;
    std::optional<Viewport> clip;

    bool beginView(const Viewport& area, const Affine2& transform, Stage next, RenderError& error);
    bool prepareDraw(const DrawOptions& options, RenderError& error) const;
    void applyClip() const;
};
}
