#pragma once

#include "camera.h"
#include "ui.h"
#include "texture.h"

#include <array>
#include <cstddef>
#include <optional>
#include <string_view>
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

// Читаемое описание кода ошибки для журналов и сообщений пользователю.
const char* renderErrorMessage(RenderError error);

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
    // Размеры — клиентские пиксели. Очищает весь буфер и сбрасывает ошибку. Вложенный кадр отклоняется.
    void beginFrame(int width, int height, Color clear);
    // Переключает активный вид внутри кадра; отдельного завершения вида нет.
    // Мировые виды задаются до экранных. Смена вида сбрасывает отсечение.
    void setWorldView(const CameraView& view);
    void setUiView(const UiView& view);
    // Отсечение в клиентских пикселях, начало сверху слева. Пересекается с viewport
    // и окном. Нулевой размер скрывает всё; nullopt снимает отсечение.
    void setClip(const std::optional<Viewport>& clip);
    // После первой ошибки остальные команды пропускаются. Завершение закрывает
    // начатый кадр и возвращает первую ошибку; следующий beginFrame начинает новый.
    // Уже принятые команды выполняются; показ кадра выполняет цикл приложения.
    bool endFrame(RenderError& error);

    // Настройки копируются в команду и не влияют на следующие фигуры.
    // Внутри вида — стабильная сортировка по (layer, order). Более поздний вид
    // рисуется поверх предыдущего независимо от ключей сортировки.
    void drawPoint(Vector2 point, const DrawOptions& options = {});
    void drawPoint(Vector2 point, Color color);
    void drawLine(Vector2 start, Vector2 end, const DrawOptions& options = {});
    void drawLine(Vector2 start, Vector2 end, Color color);
    void drawRectangle(const Rect& bounds, const DrawOptions& options = {}, ShapeFill fill = ShapeFill::Filled);
    void drawRectangle(const Rect& bounds, Color color, ShapeFill fill = ShapeFill::Filled);
    // Нулевые размеры допустимы; segments >= 3 задаёт аппроксимацию окружности.
    void drawCircle(const Circle& circle, const DrawOptions& options = {},
        ShapeFill fill = ShapeFill::Filled, unsigned int segments = 48);
    void drawCircle(const Circle& circle, Color color,
        ShapeFill fill = ShapeFill::Filled, unsigned int segments = 48);
    // radii задают неотрицательные полуоси в локальных координатах; segments >= 3.
    void drawEllipse(Vector2 center, Vector2 radii, const DrawOptions& options = {},
        ShapeFill fill = ShapeFill::Filled, unsigned int segments = 48);
    void drawEllipse(Vector2 center, Vector2 radii, Color color,
        ShapeFill fill = ShapeFill::Filled, unsigned int segments = 48);
    // Простой многоугольник без отверстий и самопересечений, >= 3 вершин.
    // Любой обход; соседние повторы, замыкающая вершина и коллинеарность допустимы.
    void drawPolygon(const std::vector<Vector2>& vertices, const DrawOptions& options = {},
        ShapeFill fill = ShapeFill::Filled);
    void drawPolygon(const std::vector<Vector2>& vertices, Color color,
        ShapeFill fill = ShapeFill::Filled);
    void drawGradientRectangle(const Rect& bounds, Color top, Color bottom, const DrawOptions& options = {});
    void drawTexture(const Texture& texture, const Rect& bounds, const DrawOptions& options = {},
        const TextureSampling& sampling = {});
    // Встроенный шрифт 3x5: ASCII A-Z, 0-9 и знаки - + /. Остальные байты дают пробел.
    // pixel — размер пикселя глифа в локальных координатах, конечный и положительный.
    void drawDebugText(std::string_view message, Vector2 position, const DrawOptions& options = {}, float pixel = 2);
    void drawDebugText(std::string_view message, Vector2 position, Color color, float pixel = 2);

private:
    bool queueRectangle(const Rect& rectangle, ShapeFill fill, const DrawOptions& options, RenderError& error);

    bool ready();

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

    bool selectView(const Viewport& area, const Affine2& transform, Stage next, RenderError& error);
    bool prepareDraw(const DrawOptions& options, RenderError& error) const;
    bool submit(Primitive primitive, const Vector2* vertices, std::size_t count,
        const DrawOptions& options, RenderError& error);
    void execute(Primitive primitive, const Vector2* vertices, std::size_t count, const DrawOptions& options,
        const std::optional<std::array<Color, 2>>& gradient) const;
    void flush();
    void executeTexture(const Command& command) const;
    void applyClip(const std::optional<Viewport>& area) const;

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
    RenderError firstError = RenderError::None;
};
}
