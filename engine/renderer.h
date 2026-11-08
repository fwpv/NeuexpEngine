#pragma once

#include "camera.h"
#include "font.h"
#include "ui.h"
#include "texture.h"
#include "sprite.h"
#include "geometry.h"
#include "math2d.h"
#include "resources.h"
#include "transform.h"

#include <array>
#include <cstddef>
#include <optional>
#include <string_view>
#include <vector>
#include <memory>
#include <string>

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
    InvalidTexture,
    Graphics
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

// Координаты, размеры, цвета и коэффициенты преобразований должны быть конечными.
// Это предусловие вызывающего кода; диагностика NaN/Inf в обычном рисовании не гарантируется.
// Ключ сортировки и числа повторений проверяются из-за требований алгоритмов.
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
    // Исходные размеры и пиксели сохраняются. Для OpenGL 1.1 GPU-изображение
    // дополняется до степени двойки; UV и границы фильтрации учитывают дополнение.
    bool createTexture(const Image& image, Texture& result, ResourceError& error, bool keepPixels = false);
    // Кеш по каноническому пути внутри этого рендерера, независимо от параметров рисования.
    // Сохранение CPU-пикселей запрашивается при первой загрузке. Для изменения
    // этого выбора нужно освободить все ссылки и вызвать collectUnusedTextures().
    bool loadTexture(const ResourceFiles& files, const std::string& name, Texture& result,
        ResourceError& error, bool keepPixels = false);
    // PNG используется как альфа-маска: RGB нормализуется в белый в памяти.
    bool loadFont(const ResourceFiles& files, const std::string& name, BitmapFont& result, ResourceError& error);
    // Позиция задаёт начало логической области; options.transform применяется после переноса.
    // Раскладка рисуется одной командой с подготовленным массивом геометрии.
    void drawText(const TextLayout& text, Vector2 position, const DrawOptions& options = {},
        TextureFilter filter = TextureFilter::Linear);
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
    // Ошибки OpenGL, накопленные к завершению кадра, возвращаются как Graphics,
    // если ранее не обнаружена ошибка аргументов или порядка вызовов.
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
    // Пустые и вырожденные контуры отклоняются, в том числе при рисовании обводки.
    void drawPolygon(const Polygon& polygon, const DrawOptions& options = {},
        ShapeFill fill = ShapeFill::Filled);
    void drawPolygon(const Polygon& polygon, Color color, ShapeFill fill = ShapeFill::Filled);
    void drawPolygon(const Quad& polygon, const DrawOptions& options = {},
        ShapeFill fill = ShapeFill::Filled);
    void drawPolygon(const Quad& polygon, Color color, ShapeFill fill = ShapeFill::Filled);
    void drawGradientRectangle(const Rect& bounds, Color top, Color bottom, const DrawOptions& options = {});
    void drawTexture(const Texture& texture, const Rect& bounds, const DrawOptions& options = {},
        const TextureSampling& sampling = {});
    // Невидимый спрайт пропускается. Преобразование options применяется после transform
    // спрайта; цвета перемножаются. Слой, порядок и отсечение работают как у остальных команд.
    void drawSprite(const Sprite& sprite, const DrawOptions& options = {});
    // Встроенный UTF-8 шрифт: латиница 3x5, русский алфавит 5x7, цифры и - + / . : %.
    // Строчные буквы рисуются прописными, неизвестные символы дают пробел; поддержан LF.
    // pixel — размер пикселя глифа в локальных координатах, конечный и положительный.
    static Size2 measureDebugText(std::string_view message, float pixel = 2);
    void drawDebugText(std::string_view message, Vector2 position, const DrawOptions& options = {}, float pixel = 2);
    void drawDebugText(std::string_view message, Vector2 position, Color color, float pixel = 2);

private:
    bool queueRectangle(const Rect& rectangle, ShapeFill fill, const DrawOptions& options);
    void queuePolygon(const Vector2* vertices, std::size_t count, const DrawOptions& options, ShapeFill fill);

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
        std::optional<Rect> textureRect;
        std::optional<TextLayout> text;
    };

    bool selectView(const Viewport& area, const Affine2& transform, Stage next);
    bool prepareDraw(const DrawOptions& options);
    bool submit(Primitive primitive, const Vector2* vertices, std::size_t count,
        const DrawOptions& options);
    void execute(Primitive primitive, const Vector2* vertices, std::size_t count, const DrawOptions& options,
        const std::optional<std::array<Color, 2>>& gradient) const;
    void flush();
    void executeText(const Command& command) const;
    void executeTexture(const Command& command) const;
    void queueTexture(const Texture& texture, const Rect& bounds, const DrawOptions& options,
        const TextureSampling& sampling, const std::optional<Rect>& textureRect);
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
