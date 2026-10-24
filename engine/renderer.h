#pragma once

#include "camera.h"
#include "ui.h"
#include "texture.h"

#include <array>
#include <cstddef>
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
    InvalidOptions,
    NoCurrentContext,
    InvalidTexture
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
    // Сначала рисуются меньшие слои, затем меньшие значения order внутри слоя.
    // order может задаваться координатой Y для сортировки объектов по положению.
    int layer = 0;
    float order = 0;
};

// Рисует в текущий графический контекст в потоке приложения. Контекст должен оставаться
// действующим от initialize() до shutdown(). Одновременно кадром управляет только один рендерер;
// прямые изменения состояния OpenGL внутри кадра не поддерживаются.
// Конструктор и деструктор не обращаются к контексту.
class Renderer2D
{
public:
    Renderer2D() = default;
    Renderer2D(const Renderer2D&) = delete;
    Renderer2D& operator=(const Renderer2D&) = delete;

    // Инициализация при текущем контексте; повторная инициализация отклоняется.
    bool initialize(RenderError& error);
    bool isInitialized() const;
    // Отменяет незавершённый кадр и освобождает очередь команд. Повторный вызов безопасен.
    // Удаляет все текстуры при текущем контексте. Обязателен до уничтожения контекста;
    // деструктор освобождает только память и не заменяет shutdown().
    void shutdown();
    // Лимит суммарного объёма GPU-текстур; задаётся до initialize().
    bool setTextureBudget(std::size_t bytes, ResourceError& error);
    // Исходные размеры сохраняются. Для OpenGL 1.1 стороны GPU-изображения
    // округляются вверх до степени двойки с масштабированием ближайшим соседом.
    bool createTexture(const Image& image, Texture& result, ResourceError& error, bool keepPixels = false);
    // Кеш по каноническому пути внутри этого рендерера, независимо от параметров рисования.
    // Сохранение CPU-пикселей запрашивается при первой загрузке. Для изменения
    // этого выбора нужно освободить все ссылки и вызвать collectUnusedTextures().
    bool loadTexture(const ResourceFiles& files, const std::string& name, Texture& result,
        ResourceError& error, bool keepPixels = false);
    // Удаляет ресурсы без пользовательских ссылок и команд; также вызывается в beginFrame().
    void collectUnusedTextures();
    TextureStats textureStats() const;
    // Размеры — фактические клиентские пиксели. Очищается весь цветовой буфер,
    // включая поля. Вложенные кадры отклоняются.
    bool beginFrame(int width, int height, Color clearColor, RenderError& error);
    // Команды копируются в очередь активного вида. При смене вида/endFrame()
    // выполняется стабильная сортировка по (layer, order), общая для непрозрачных
    // и полупрозрачных фигур. При одинаковых ключах сохраняется порядок команд.
    // Сортировка не пересекает границы видов: более поздний вид рисуется поверх
    // предыдущего. Несколько мировых видов допустимы до первого beginUi().
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
    // Рисует целую текстуру; использует общие преобразования, сортировку и отсечение.
    bool drawTexture(const Texture& texture, const Rect& rectangle, const DrawOptions& options,
        RenderError& error, const TextureSampling& sampling = {});
    bool drawLine(Vector2 start, Vector2 end, const DrawOptions& options, RenderError& error);
    bool drawRectangle(const Rect& rectangle, ShapeFill fill, const DrawOptions& options, RenderError& error);
    // Вертикальный градиент в локальных координатах. Цвета умножаются на options.color.
    bool drawGradientRectangle(const Rect& rectangle, Color top, Color bottom,
        const DrawOptions& options, RenderError& error);
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

    enum class Primitive
    {
        Points,
        Lines,
        LineLoop,
        Quads,
        TriangleFan,
        Triangles
    };

    struct Command
    {
        Primitive primitive;
        std::vector<Vector2> vertices;
        DrawOptions options;
        std::optional<Viewport> clip;
        std::optional<std::array<Color, 2>> gradient;
        Texture texture;
        TextureSampling sampling;
    };

    bool initialized = false;
    Stage stage = Stage::Idle;
    int frameWidth = 0;
    int frameHeight = 0;
    Viewport viewport;
    Affine2 viewTransform;
    std::optional<Viewport> clip;
    std::vector<Command> commands;
    std::vector<std::shared_ptr<Detail::TextureData>> textures;
    std::size_t textureBudget = 256 * 1024 * 1024;

    bool beginView(const Viewport& area, const Affine2& transform, Stage next, RenderError& error);
    bool prepareDraw(const DrawOptions& options, RenderError& error) const;
    bool submit(Primitive primitive, const Vector2* vertices, std::size_t count,
        const DrawOptions& options, RenderError& error);
    void execute(Primitive primitive, const Vector2* vertices, std::size_t count, const DrawOptions& options,
        const std::optional<std::array<Color, 2>>& gradient) const;
    void flush();
    void executeTexture(const Command& command) const;
    void applyClip(const std::optional<Viewport>& area) const;
};
}
