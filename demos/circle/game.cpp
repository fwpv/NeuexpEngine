#include "platform.h"
#include "game.h"

#if defined(_WIN32)
#include <windows.h>
#endif

#include <GL/gl.h>
#include <array>
#include <chrono>
#include <cmath>
#include <string>
#include <thread>
#include <vector>

namespace Game
{
// Координата на игровом поле. Экранные координаты считаются в пикселях.
struct Point
{
    float x;
    float y;
};

struct Color
{
    // Компоненты цвета OpenGL задаются числами от 0 до 1.
    float red;
    float green;
    float blue;
    float alpha;
};

// Цвета сгруппированы по назначению, чтобы визуальные решения не терялись в коде отрисовки.
namespace Palette
{
// Цвета хранятся в диапазоне 0..1: последние компоненты — альфа-прозрачность.
const Color PortalGlow = { 0.1f, 0.6f, 1.0f, 0.15f };
const Color PortalOuterRing = { 0.25f, 0.8f, 1.0f, 0.75f };
const Color PortalInnerRing = { 0.75f, 0.3f, 1.0f, 0.8f };
const Color PortalCore = { 0.01f, 0.02f, 0.06f, 1.0f };
const Color Star = { 0.6f, 0.75f, 1.0f, 1.0f };
const Color Nebula = { 0.18f, 0.17f, 0.55f, 0.06f };
const Color PlanetAtmosphere = { 0.15f, 0.65f, 1.0f, 0.16f };
const Color PlanetSurface = { 0.08f, 0.28f, 0.58f, 1.0f };
const Color PlanetBand = { 0.35f, 0.72f, 1.0f, 0.2f };
const Color PlanetRing = { 0.3f, 0.8f, 1.0f, 0.55f };
const Color PlanetRingHighlight = { 0.8f, 0.92f, 1.0f, 0.7f };
const Color StationOuterRing = { 0.12f, 0.8f, 1.0f, 0.28f };
const Color StationBody = { 0.06f, 0.15f, 0.28f, 1.0f };
const Color StationRing = { 0.15f, 0.85f, 1.0f, 1.0f };
const Color StationReactor = { 0.28f, 0.95f, 1.0f, 1.0f };
const Color StationModule = { 0.1f, 0.3f, 0.43f, 1.0f };
const Color StationModuleRing = { 0.25f, 0.82f, 1.0f, 1.0f };
const Color StationModuleLight = { 1.0f, 0.66f, 0.23f, 1.0f };
const Color BeaconGlow = { 1.0f, 0.48f, 0.18f, 0.3f };
const Color Beacon = { 1.0f, 0.6f, 0.22f, 1.0f };
const Color DroneGlow = { 1.0f, 0.12f, 0.18f, 0.2f };
const Color DroneBody = { 0.48f, 0.08f, 0.18f, 1.0f };
const Color DroneRing = { 1.0f, 0.25f, 0.3f, 1.0f };
const Color DroneCore = { 1.0f, 0.82f, 0.75f, 1.0f };
const Color BoltGlow = { 0.2f, 0.9f, 1.0f, 0.3f };
const Color Bolt = { 0.72f, 1.0f, 1.0f, 1.0f };
const Color ShuttleFlame = { 1.0f, 0.3f, 0.05f, 1.0f };
const Color ShuttleHull = { 0.14f, 0.56f, 0.78f, 1.0f };
const Color ShuttleWindow = { 0.05f, 0.15f, 0.26f, 1.0f };
const Color ShuttleShield = { 0.3f, 0.9f, 1.0f, 0.75f };
const Color HudFrame = { 0.15f, 0.72f, 0.92f, 0.55f };
const Color Radar = { 0.2f, 0.65f, 0.82f, 0.7f };
const Color RadarSweep = { 0.2f, 0.8f, 0.95f, 0.4f };
const Color ShuttleBlip = { 0.3f, 0.95f, 1.0f, 1.0f };
const Color BeaconBlip = Beacon;
const Color ShieldCell = { 0.14f, 0.82f, 0.95f, 0.9f };
const Color EnergyCell = { 1.0f, 0.58f, 0.2f, 0.9f };
const Color Score = { 1.0f, 0.32f, 0.28f, 0.95f };
const Color Target = { 1.0f, 0.55f, 0.2f, 1.0f };
const Color BackgroundTop = { 0.025f, 0.04f, 0.12f, 1.0f };
const Color BackgroundBottom = { 0.005f, 0.009f, 0.028f, 1.0f };
const Color Clear = { 0.004f, 0.008f, 0.025f, 1.0f };
}

// Раскладка интерфейса рассчитана на ReferenceScreenWidth x ReferenceScreenHeight.
// При другом размере окна drawHud равномерно масштабирует её.
namespace HudLayout
{
// Координаты и размеры элементов HUD в опорном разрешении 800x600.
const float FrameLeft = 22.0f;
const float FrameRight = 778.0f;
const float FrameTop = 22.0f;
const float FrameBottom = 522.0f;
const float RadarCenterX = 720.0f;
const float RadarCenterY = 470.0f;
const float RadarOuterRadiusX = 44.0f;
const float RadarOuterRadiusY = 22.0f;
const float RadarInnerRadiusX = 28.0f;
const float RadarInnerRadiusY = 14.0f;
const float RadarSweepRadiusX = 42.0f;
const float RadarSweepRadiusY = 21.0f;
const float RadarMapScale = 0.2f;
const float BlipRadius = 2.5f;
const float RadarBeaconRadiusX = 36.0f;
const float RadarBeaconRadiusY = 18.0f;
const int BarCellCount = 12;
const int ShieldCellCount = 10;
const int EnergyCellCount = 8;
const float BarStartX = 35.0f;
const float BarShieldY = 546.0f;
const float BarEnergyY = 568.0f;
const float BarCellSpacing = 13.0f;
const float BarCellSize = 9.0f;
const int ScoreLightCount = 8;
const float ScoreStartX = 694.0f;
const float ScoreY = 65.0f;
const float ScoreSpacing = 12.0f;
const float ScoreLightRadius = 3.5f;
const float TargetRadius = 38.0f;
const float TargetMarkInner = 33.0f;
const float TargetMarkOuter = 48.0f;
const int FineCircleSides = 8;
}

// Вершины шаттла заданы относительно его центра; поэтому всю модель можно
// перемещать и поворачивать одной матрицей OpenGL.
namespace ShuttleModel
{
// Контур корпуса, иллюминатора и пламени в локальных координатах шаттла.
const Point FlameLeft = { -7.0f, 13.0f };
const Point FlameRight = { 7.0f, 13.0f };
const std::array<Point, 8> Hull = {{
    { 0.0f, -20.0f }, { 7.0f, -5.0f }, { 17.0f, 9.0f }, { 8.0f, 7.0f },
    { 0.0f, 13.0f }, { -8.0f, 7.0f }, { -17.0f, 9.0f }, { -7.0f, -5.0f }
}};
const std::array<Point, 4> Window = {{
    { -3.0f, 1.0f }, { 3.0f, 1.0f }, { 5.0f, 7.0f }, { -5.0f, 7.0f }
}};
}

void setColor(const Color& color)
{
    // glColor4f задаёт текущий цвет RGBA. Следующие вершины используют его,
    // пока цвет не будет изменён новым вызовом.
    glColor4f(color.red, color.green, color.blue, color.alpha);
}

void setColor(const Color& color, float alpha)
{
    // Этот вариант сохраняет RGB из палитры, но подменяет альфа-канал.
    glColor4f(color.red, color.green, color.blue, alpha);
}

// Общие параметры геометрии и системы координат игрового поля.
const int PortalCount = 2;
const float Pi = 3.14159265f;
const double RadiansPerFullTurn = 2.0 * Pi;
const int DefaultDiscSides = 32;
const int DefaultRingSides = 48;
const int OctagonSides = 8;
const int DodecagonSides = 12;
const float DegreesPerHalfTurn = 180.0f;
const float ReferenceScreenWidth = 800.0f;
const float ReferenceScreenHeight = 600.0f;
const float HudFrameCornerLength = 50.0f;
const float InactiveHudAlpha = 0.18f;
const float InactiveTargetAlpha = 0.45f;
const float DefaultLineWidth = 1.0f;
const float NearDepth = -1.0f;
const float FarDepth = 1.0f;

// Общие настройки шаттла.
struct ShuttleConfig
{
    // Управление и границы игрового поля.
    float headingOffset;
    float movementSpeed;
    float diagonalSpeedFactor;
    float leftLimit;
    float rightLimit;
    float topLimit;
    float bottomLimit;

    // Параметры стрельбы.
    float shotSpeed;
    float shotSpawnOffset;
    float shotLifetime;
    float shotCooldown;

    // Модель, защитный контур и пламя.
    float scale;
    float shieldRadius;
    float flameBase;
    float flamePulse;
    float flameSpeed;

    ShuttleConfig()
        : headingOffset(90.0f)
        , movementSpeed(205.0f)
        , diagonalSpeedFactor(0.7071f)
        , leftLimit(38.0f)
        , rightLimit(762.0f)
        , topLimit(76.0f)
        , bottomLimit(505.0f)
        , shotSpeed(470.0f)
        , shotSpawnOffset(20.0f)
        , shotLifetime(2.0f)
        , shotCooldown(0.24f)
        , scale(1.0f)
        , shieldRadius(22.0f)
        , flameBase(13.0f)
        , flamePulse(5.0f)
        , flameSpeed(25.0f)
    {
    }
};

const ShuttleConfig ShuttleSettings;

// Изменяемое состояние.
struct Shuttle
{
    Point position;
    float heading;
    bool thrusting;
    float nextShotTime;

    Shuttle()
        : position{170.0f, 390.0f}
        , heading(90.0f)
        , thrusting(false)
        , nextShotTime(0.0f)
    {
    }
};

// Общие настройки снарядов.
struct BoltConfig
{
    float glowRadius;
    float trailLength;
    float lineWidth;
    int glowSides;

    BoltConfig()
        : glowRadius(8.0f)
        , trailLength(0.025f)
        , lineWidth(3.0f)
        , glowSides(12)
    {
    }
};

const BoltConfig BoltSettings;

// Изменяемое состояние.
struct Bolt
{
    Point position;
    Point velocity;
    float secondsLeft;

    Bolt()
        : position{0.0f, 0.0f}
        , velocity{0.0f, 0.0f}
        , secondsLeft(0.0f)
    {
    }
};

// Общие настройки дронов.
struct DroneConfig
{
    Point orbitCenter;
    float orbitRadiusX;
    float orbitRadiusY;
    float orbitSpeed;
    float hitRadius;
    float respawnDelay;

    // Размеры визуальных слоёв.
    float glowRadius;
    float bodyRadius;
    float outlineRadius;
    float coreRadius;

    DroneConfig()
        : orbitCenter{655.0f, 340.0f}
        , orbitRadiusX(150.0f)
        , orbitRadiusY(47.0f)
        , orbitSpeed(0.42f)
        , hitRadius(17.0f)
        , respawnDelay(4.0f)
        , glowRadius(19.0f)
        , bodyRadius(11.0f)
        , outlineRadius(13.0f)
        , coreRadius(3.0f)
    {
    }
};

const DroneConfig DroneSettings;

// Изменяемое состояние.
struct Drone
{
    float phase;
    float respawnTime;
    bool active;

    explicit Drone(float startPhase = 0.0f)
        : phase(startPhase)
        , respawnTime(0.0f)
        , active(true)
    {
    }
};

// Общие настройки порталов.
struct PortalConfig
{
    float triggerRadius;
    float cooldown;

    // Пульсация и размеры визуальных слоёв.
    float pulseSpeed;
    float pulseSize;
    float openBrightness;
    float closedBrightness;
    float glowRadiusX;
    float glowRadiusY;
    float outerRadiusX;
    float outerRadiusY;
    float innerRadiusX;
    float innerRadiusY;
    float coreRadiusX;
    float coreRadiusY;
    float pulseBase;

    PortalConfig()
        : triggerRadius(28.0f)
        , cooldown(1.0f)
        , pulseSpeed(3.0f)
        , pulseSize(0.12f)
        , openBrightness(1.0f)
        , closedBrightness(0.3f)
        , glowRadiusX(48.0f)
        , glowRadiusY(38.0f)
        , outerRadiusX(34.0f)
        , outerRadiusY(28.0f)
        , innerRadiusX(25.0f)
        , innerRadiusY(20.0f)
        , coreRadiusX(14.0f)
        , coreRadiusY(12.0f)
        , pulseBase(1.0f)
    {
    }
};

const PortalConfig PortalSettings;

// Положение и направление перехода конкретного портала.
struct Portal
{
    Point position;
    Point destination;

    Portal(Point entrance, Point exit)
        : position(entrance)
        , destination(exit)
    {
    }
};

struct Planet
{
    Point position;
    float atmosphereRadius;
    float radius;

    // Размеры колец и параметры полос на поверхности.
    float innerRingRadiusX;
    float innerRingRadiusY;
    float outerRingRadiusX;
    float outerRingRadiusY;
    int bandCount;
    float bandSpacing;
    float bandHalfWidth;
    float bandPulseAlpha;
    float bandLineWidth;
    float innerRingLineWidth;
    float outerRingLineWidth;

    Planet()
        : position{655.0f, 340.0f}
        , atmosphereRadius(145.0f)
        , radius(126.0f)
        , innerRingRadiusX(175.0f)
        , innerRingRadiusY(49.0f)
        , outerRingRadiusX(191.0f)
        , outerRingRadiusY(55.0f)
        , bandCount(7)
        , bandSpacing(27.0f)
        , bandHalfWidth(115.0f)
        , bandPulseAlpha(0.08f)
        , bandLineWidth(1.0f)
        , innerRingLineWidth(6.0f)
        , outerRingLineWidth(1.0f)
    {
    }
};

const Planet PlanetSettings;

struct Station
{
    Point position;
    float lockRadius;

    // Размеры корпуса и реактора.
    float outerRadiusX;
    float outerRadiusY;
    float reactorRadius;
    float reactorRingRadius;

    // Модули размещаются сеткой; размеры и шаги можно менять независимо.
    int moduleCount;
    int moduleColumns;
    float moduleSpacingX;
    float moduleSpacingY;
    float moduleRadiusX;
    float moduleRadiusY;
    float moduleLightRadius;

    // Маяк движется по эллиптической орбите вокруг станции.
    float beaconSpeed;
    float beaconOrbitX;
    float beaconOrbitY;
    float beaconGlowRadius;
    float beaconRadius;

    // Свечение центрального реактора меняется со временем.
    float reactorGlowBase;
    float reactorGlowRange;
    float reactorGlowSpeed;
    float reactorGlowRadius;
    int reactorSides;
    int moduleSides;

    Station()
        : position{418.0f, 292.0f}
        , lockRadius(72.0f)
        , outerRadiusX(102.0f)
        , outerRadiusY(42.0f)
        , reactorRadius(32.0f)
        , reactorRingRadius(33.0f)
        , moduleCount(4)
        , moduleColumns(2)
        , moduleSpacingX(70.0f)
        , moduleSpacingY(27.0f)
        , moduleRadiusX(18.0f)
        , moduleRadiusY(9.0f)
        , moduleLightRadius(3.0f)
        , beaconSpeed(0.9f)
        , beaconOrbitX(105.0f)
        , beaconOrbitY(42.0f)
        , beaconGlowRadius(14.0f)
        , beaconRadius(6.0f)
        , reactorGlowBase(0.7f)
        , reactorGlowRange(0.3f)
        , reactorGlowSpeed(2.0f)
        , reactorGlowRadius(14.0f)
        , reactorSides(12)
        , moduleSides(8)
    {
    }
};

const Station StationSettings;

struct StarField
{
    // Количество точек, их размер и диапазон яркости.
    unsigned int count;
    float pointSize;
    float glowBase;
    float glowRange;

    StarField()
        : count(130u)
        , pointSize(2.0f)
        , glowBase(0.65f)
        , glowRange(0.35f)
    {
    }
};

const StarField StarFieldSettings;

struct Nebula
{
    // Положение облаков и амплитуда их колебаний.
    int cloudCount;
    float startX;
    float startY;
    float spacing;
    float sway;
    float swaySpeed;
    float radiusX;
    float radiusY;

    Nebula()
        : cloudCount(6)
        , startX(80.0f)
        , startY(290.0f)
        , spacing(110.0f)
        , sway(30.0f)
        , swaySpeed(0.1f)
        , radiusX(150.0f)
        , radiusY(64.0f)
    {
    }
};

const Nebula NebulaSettings;

// Общие настройки игрового мира.
struct WorldConfig
{
    float width;
    float height;
    float maxFrameTime;
    float portalCycleDuration;
    float portalOpenTime;
    float portalCloseTime;
    int startingDroneCount;
    float dronePhaseSpacing;

    WorldConfig()
        : width(800.0f)
        , height(600.0f)
        , maxFrameTime(0.08f)
        , portalCycleDuration(24.0f)
        , portalOpenTime(7.0f)
        , portalCloseTime(14.0f)
        , startingDroneCount(3)
        , dronePhaseSpacing(2.094f)
    {
    }
};

const WorldConfig WorldSettings;

// Изменяемое состояние.
struct World
{
    float nextPortalTime;
    int score;

    World()
        : nextPortalTime(0.0f)
        , score(0)
    {
    }
};

struct State
{
    // Состояние игрового мира и всех находящихся в нём сущностей.
    World world;
    Shuttle shuttle;
    std::vector<Drone> drones;
    std::vector<Bolt> bolts;
    // Порталы соединены попарно; их расположение не меняется во время игры.
    const std::array<Portal, PortalCount> portals;

    State()
        : portals{{
            Portal(Point{138.0f, 184.0f}, Point{738.0f, 432.0f}),
            Portal(Point{690.0f, 432.0f}, Point{90.0f, 184.0f})
        }}
    {

        // Фаза разносит дроны по орбите, чтобы они не летели одной группой.
        float phase = 0.0f;
        for (int drone = 0; drone < WorldSettings.startingDroneCount; ++drone) {
            drones.emplace_back(phase);
            phase += WorldSettings.dronePhaseSpacing;
        }
    }
};

Point dronePosition(const Drone& drone, float time)
{
    // Синус и косинус задают эллиптическую траекторию; phase сдвигает дроны друг относительно друга.
    const float angle = time * DroneSettings.orbitSpeed + drone.phase;
    const Point position = { DroneSettings.orbitCenter.x + std::cos(angle) * DroneSettings.orbitRadiusX,
                            DroneSettings.orbitCenter.y + std::sin(angle) * DroneSettings.orbitRadiusY };
    return position;
}

// Для простоты учебного примера геометрия передаётся старым непосредственным
// способом OpenGL: glBegin выбирает примитив, glVertex задаёт вершины, glEnd его завершает.
// glVertex2d принимает double: целочисленные счётчики точно переводятся в этот тип
// при расчёте углов, без промежуточного округления до float.
void disc(float x, float y, float radiusX, float radiusY, int sides = DefaultDiscSides)
{
    // GL_TRIANGLE_FAN строит заполненный круг веером треугольников от центра к контуру.
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(x, y);
    for (int side = 0; side <= sides; ++side) {
        const double angle = RadiansPerFullTurn * side / sides;
        glVertex2d(x + std::cos(angle) * radiusX, y + std::sin(angle) * radiusY);
    }
    glEnd();
}

void ring(float x, float y, float radiusX, float radiusY, int sides = DefaultRingSides)
{
    // GL_LINE_LOOP соединяет вершины по кругу и автоматически замыкает последнюю с первой.
    glBegin(GL_LINE_LOOP);
    for (int side = 0; side < sides; ++side) {
        const double angle = RadiansPerFullTurn * side / sides;
        glVertex2d(x + std::cos(angle) * radiusX, y + std::sin(angle) * radiusY);
    }
    glEnd();
}

void rectangle(float x, float y, float width, float height)
{
    // Четыре вершины GL_QUADS образуют прямоугольник.
    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + width, y);
    glVertex2f(x + width, y + height);
    glVertex2f(x, y + height);
    glEnd();
}

void drawPortal(const Portal& portal, float time, bool open)
{
    // Закрытый портал остаётся видимым, но его свечение становится тусклее.
    const float brightness = open ? PortalSettings.openBrightness : PortalSettings.closedBrightness;
    const float pulse = PortalSettings.pulseBase + PortalSettings.pulseSize * std::sin(time * PortalSettings.pulseSpeed);

    // Слои идут от мягкого свечения к яркому центру; каждый следующий слой
    // перекрывает предыдущий благодаря включённому альфа-смешиванию.
    setColor(Palette::PortalGlow, Palette::PortalGlow.alpha * brightness);
    disc(portal.position.x, portal.position.y, PortalSettings.glowRadiusX * pulse, PortalSettings.glowRadiusY * pulse);
    setColor(Palette::PortalOuterRing, Palette::PortalOuterRing.alpha * brightness);
    ring(portal.position.x, portal.position.y, PortalSettings.outerRadiusX, PortalSettings.outerRadiusY);
    setColor(Palette::PortalInnerRing, Palette::PortalInnerRing.alpha * brightness);
    ring(portal.position.x, portal.position.y, PortalSettings.innerRadiusX, PortalSettings.innerRadiusY);
    setColor(Palette::PortalCore);
    disc(portal.position.x, portal.position.y, PortalSettings.coreRadiusX, PortalSettings.coreRadiusY);
}

void drawStars(float time, const StarField& stars)
{
    const unsigned int SeedMultiplier = 7919u;
    const unsigned int SeedOffset = 17u;
    const unsigned int XMultiplier = 37u;
    const unsigned int XOffset = 91u;
    const unsigned int YMultiplier = 53u;
    const unsigned int YOffset = 29u;
    const unsigned int screenWidth = static_cast<unsigned int>(WorldSettings.width);
    const unsigned int screenHeight = static_cast<unsigned int>(WorldSettings.height);

    // Фиксированные множители дают псевдослучайное, но повторяемое расположение звёзд
    // без отдельного массива координат.
    // GL_POINTS рисует каждую вершину как отдельную точку заданного размера.
    glPointSize(stars.pointSize);
    glBegin(GL_POINTS);
    for (unsigned int star = 0; star < stars.count; ++star) {
        // Простая арифметика даёт каждой звезде повторяемую координату.
        const unsigned int seed = star * SeedMultiplier + SeedOffset;
        const double x = (seed * XMultiplier + XOffset) % screenWidth;
        const double y = (seed * YMultiplier + YOffset) % screenHeight;
        const float glow = stars.glowBase + stars.glowRange * std::sin(time + static_cast<float>(star));
        setColor(Palette::Star, glow);
        glVertex2d(x, y);
    }
    glEnd();
}

void drawNebula(float time, const Nebula& nebula)
{
    // Несколько прозрачных овалов накладываются друг на друга и изображают туманность.
    float x = nebula.startX;
    float phase = time * nebula.swaySpeed;
    for (int cloud = 0; cloud < nebula.cloudCount; ++cloud) {
        // Синус смещает облака по вертикали с разной фазой, сохраняя шаг по X.
        const float y = nebula.startY + nebula.sway * std::sin(phase);
        setColor(Palette::Nebula);
        disc(x, y, nebula.radiusX, nebula.radiusY);
        x += nebula.spacing;
        phase += 1.0f;
    }
}

void drawPlanet(float time, const Planet& planet)
{
    // Атмосфера рисуется первой и больше поверхности, поэтому видна как внешний ореол.
    setColor(Palette::PlanetAtmosphere);
    disc(planet.position.x, planet.position.y, planet.atmosphereRadius, planet.atmosphereRadius);
    setColor(Palette::PlanetSurface);
    disc(planet.position.x, planet.position.y, planet.radius, planet.radius);

    // Полосы движутся и меняют прозрачность, создавая простую анимацию атмосферы.
    const int firstBand = -planet.bandCount / 2;
    const float firstBandOffset = static_cast<float>(firstBand);
    float bandY = planet.position.y + firstBandOffset * planet.bandSpacing;
    float bandPhase = time + firstBandOffset;
    // Устанавливаем толщину линий атмосферы до построения полос.
    glLineWidth(planet.bandLineWidth);
    for (int band = 0; band < planet.bandCount; ++band) {
        setColor(Palette::PlanetBand,
                 Palette::PlanetBand.alpha + planet.bandPulseAlpha * std::sin(bandPhase));
        // GL_LINES трактует каждую пару вершин как отдельный отрезок.
        glBegin(GL_LINES);
        glVertex2f(planet.position.x - planet.bandHalfWidth, bandY);
        glVertex2f(planet.position.x + planet.bandHalfWidth, bandY);
        glEnd();
        bandY += planet.bandSpacing;
        bandPhase += 1.0f;
    }

    // glLineWidth меняет толщину последующих линий, поэтому кольцам задаются свои значения.
    glLineWidth(planet.innerRingLineWidth);
    setColor(Palette::PlanetRing);
    ring(planet.position.x, planet.position.y, planet.innerRingRadiusX, planet.innerRingRadiusY);
    glLineWidth(planet.outerRingLineWidth);
    setColor(Palette::PlanetRingHighlight);
    ring(planet.position.x, planet.position.y, planet.outerRingRadiusX, planet.outerRingRadiusY);
}

void drawStation(float time, const Station& station)
{
    const float x = station.position.x;
    const float y = station.position.y;
    // Синус плавно меняет яркость реактора между базовым значением и максимумом.
    const float glow = station.reactorGlowBase +
        station.reactorGlowRange * std::sin(time * station.reactorGlowSpeed);
    // Корпус, реактор, модули и орбитальный маяк рисуются отдельными слоями.
    setColor(Palette::StationOuterRing);
    ring(x, y, station.outerRadiusX, station.outerRadiusY);
    setColor(Palette::StationBody);
    disc(x, y, station.reactorRadius, station.reactorRadius);
    setColor(Palette::StationRing);
    ring(x, y, station.reactorRingRadius, station.reactorRingRadius);
    setColor(Palette::StationReactor, glow);
    disc(x, y, station.reactorGlowRadius, station.reactorGlowRadius, station.reactorSides);

    // Центральная часть рисуется отдельно от модулей, чтобы параметры каждой части
    // станции можно было настраивать независимо.
    // Модули располагаются по строкам и столбцам вокруг реактора.
    const int moduleRows = station.moduleCount / station.moduleColumns;
    const float firstModuleX = x - static_cast<float>(station.moduleColumns - 1) * 0.5f * station.moduleSpacingX;
    float moduleY = y - static_cast<float>(moduleRows - 1) * 0.5f * station.moduleSpacingY;
    int module = 0;
    while (module < station.moduleCount) {
        float moduleX = firstModuleX;
        for (int column = 0; column < station.moduleColumns && module < station.moduleCount; ++column) {
            setColor(Palette::StationModule);
            disc(moduleX, moduleY, station.moduleRadiusX, station.moduleRadiusY, station.moduleSides);
            setColor(Palette::StationModuleRing);
            ring(moduleX, moduleY, station.moduleRadiusX, station.moduleRadiusY, station.moduleSides);
            setColor(Palette::StationModuleLight);
            disc(moduleX, moduleY, station.moduleLightRadius, station.moduleLightRadius, station.moduleSides);
            moduleX += station.moduleSpacingX;
            ++module;
        }
        moduleY += station.moduleSpacingY;
    }

    const float orbit = time * station.beaconSpeed;
    const float beaconX = x + std::cos(orbit) * station.beaconOrbitX;
    const float beaconY = y + std::sin(orbit) * station.beaconOrbitY;
    setColor(Palette::BeaconGlow);
    disc(beaconX, beaconY, station.beaconGlowRadius, station.beaconGlowRadius);
    setColor(Palette::Beacon);
    disc(beaconX, beaconY, station.beaconRadius, station.beaconRadius, DodecagonSides);
}

void drawDrone(const Drone& drone, float time)
{
    // Уничтоженный дрон остаётся в состоянии ожидания и временно не рисуется.
    if (!drone.active) {
        return;
    }

    const Point position = dronePosition(drone, time);
    setColor(Palette::DroneGlow);
    disc(position.x, position.y, DroneSettings.glowRadius, DroneSettings.glowRadius);
    setColor(Palette::DroneBody);
    disc(position.x, position.y, DroneSettings.bodyRadius, DroneSettings.bodyRadius, OctagonSides);
    setColor(Palette::DroneRing);
    ring(position.x, position.y, DroneSettings.outlineRadius, DroneSettings.outlineRadius, OctagonSides);
    setColor(Palette::DroneCore);
    disc(position.x, position.y, DroneSettings.coreRadius, DroneSettings.coreRadius, OctagonSides);
}

void drawBolts(const std::vector<Bolt>& bolts)
{
    // Все снаряды используют общие настройки; после цикла восстанавливаем толщину линии.
    for (const Bolt& bolt : bolts) {
        glLineWidth(BoltSettings.lineWidth);
        setColor(Palette::BoltGlow);
        ring(bolt.position.x, bolt.position.y, BoltSettings.glowRadius, BoltSettings.glowRadius, BoltSettings.glowSides);
        setColor(Palette::Bolt);
        // Отрезок начинается позади снаряда в направлении, обратном его скорости.
        glBegin(GL_LINES);
        glVertex2f(bolt.position.x - bolt.velocity.x * BoltSettings.trailLength,
                   bolt.position.y - bolt.velocity.y * BoltSettings.trailLength);
        glVertex2f(bolt.position.x, bolt.position.y);
        glEnd();
    }
    glLineWidth(DefaultLineWidth);
}

void drawShuttle(const Shuttle& shuttle, float time)
{
    // glPushMatrix сохраняет текущую матрицу модели перед преобразованиями шаттла.
    glPushMatrix();

    // OpenGL умножает текущую матрицу на каждое преобразование справа налево:
    // вершина сначала масштабируется, затем поворачивается и переносится к позиции шаттла.
    glTranslatef(shuttle.position.x, shuttle.position.y, 0.0f);
    glRotatef(shuttle.heading, 0.0f, 0.0f, 1.0f);
    glScalef(ShuttleSettings.scale, ShuttleSettings.scale, 1.0f);

    if (shuttle.thrusting) {
        const float flame = ShuttleSettings.flameBase + ShuttleSettings.flamePulse * std::sin(time * ShuttleSettings.flameSpeed);
        setColor(Palette::ShuttleFlame);
        // Три вершины GL_TRIANGLES образуют отдельный треугольник пламени.
        glBegin(GL_TRIANGLES);
        glVertex2f(ShuttleModel::FlameLeft.x, ShuttleModel::FlameLeft.y);
        glVertex2f(0.0f, ShuttleModel::FlameLeft.y + flame);
        glVertex2f(ShuttleModel::FlameRight.x, ShuttleModel::FlameRight.y);
        glEnd();
    }

    setColor(Palette::ShuttleHull);
    // Вершины модели передаются в OpenGL по очереди и образуют корпус шаттла.
    glBegin(GL_POLYGON);
    for (const Point& vertex : ShuttleModel::Hull) {
        glVertex2f(vertex.x, vertex.y);
    }
    glEnd();

    setColor(Palette::ShuttleWindow);
    // Иллюминатор задан четырьмя вершинами и рисуется как четырёхугольник.
    glBegin(GL_QUADS);
    for (const Point& vertex : ShuttleModel::Window) {
        glVertex2f(vertex.x, vertex.y);
    }
    glEnd();
    setColor(Palette::ShuttleShield);
    ring(0.0f, 0.0f, ShuttleSettings.shieldRadius, ShuttleSettings.shieldRadius);

    // Возвращаем предыдущую матрицу, чтобы последующая отрисовка была в координатах мира.
    glPopMatrix();
}

void drawHudFrame()
{
    // GL_LINES получает четыре пары точек: верх/низ рамки и короткие угловые метки.
    setColor(Palette::HudFrame);
    glBegin(GL_LINES);
    glVertex2f(HudLayout::FrameLeft, HudLayout::FrameTop);
    glVertex2f(HudLayout::FrameRight, HudLayout::FrameTop);
    glVertex2f(HudLayout::FrameLeft, HudLayout::FrameBottom);
    glVertex2f(HudLayout::FrameRight, HudLayout::FrameBottom);
    glVertex2f(HudLayout::FrameLeft, HudLayout::FrameTop);
    glVertex2f(HudLayout::FrameLeft, HudFrameCornerLength);
    glVertex2f(HudLayout::FrameRight, HudLayout::FrameTop);
    glVertex2f(HudLayout::FrameRight, HudFrameCornerLength);
    glEnd();
}

void drawRadar(const State& state, float time)
{
    // Два овала образуют шкалу радара, а луч вращается вместе со временем игры.
    setColor(Palette::Radar);
    ring(HudLayout::RadarCenterX, HudLayout::RadarCenterY,
         HudLayout::RadarOuterRadiusX, HudLayout::RadarOuterRadiusY);
    ring(HudLayout::RadarCenterX, HudLayout::RadarCenterY,
         HudLayout::RadarInnerRadiusX, HudLayout::RadarInnerRadiusY);
    setColor(Palette::RadarSweep);
    // Два конца отрезка задают луч; его конец движется по эллипсу радара.
    glBegin(GL_LINES);
    glVertex2f(HudLayout::RadarCenterX, HudLayout::RadarCenterY);
    glVertex2f(HudLayout::RadarCenterX + HudLayout::RadarSweepRadiusX * std::cos(time),
               HudLayout::RadarCenterY + HudLayout::RadarSweepRadiusY * std::sin(time));
    glEnd();

    // Положение шаттла показывается относительно станции, а не края всего уровня.
    setColor(Palette::ShuttleBlip);
    const float shuttleBlipX = HudLayout::RadarCenterX +
        (state.shuttle.position.x - StationSettings.position.x) * HudLayout::RadarMapScale;
    const float shuttleBlipY = HudLayout::RadarCenterY +
        (state.shuttle.position.y - StationSettings.position.y) * HudLayout::RadarMapScale;
    disc(shuttleBlipX, shuttleBlipY,
         HudLayout::BlipRadius, HudLayout::BlipRadius, HudLayout::FineCircleSides);
    setColor(Palette::BeaconBlip);
    const float beaconOrbit = time * StationSettings.beaconSpeed;
    const float beaconX = HudLayout::RadarCenterX +
        std::cos(beaconOrbit) * HudLayout::RadarBeaconRadiusX;
    const float beaconY = HudLayout::RadarCenterY +
        std::sin(beaconOrbit) * HudLayout::RadarBeaconRadiusY;
    disc(beaconX, beaconY,
         HudLayout::BlipRadius, HudLayout::BlipRadius, HudLayout::FineCircleSides);
}

void drawStatusBars()
{
    // Все ячейки видны постоянно; альфа показывает, заполнена ли конкретная ячейка.
    float cellX = HudLayout::BarStartX;
    for (int cell = 0; cell < HudLayout::BarCellCount; ++cell) {
        const float shieldAlpha = cell < HudLayout::ShieldCellCount
            ? Palette::ShieldCell.alpha
            : InactiveHudAlpha;
        setColor(Palette::ShieldCell, shieldAlpha);
        rectangle(cellX,
                  HudLayout::BarShieldY, HudLayout::BarCellSize, HudLayout::BarCellSize);
        const float energyAlpha = cell < HudLayout::EnergyCellCount
            ? Palette::EnergyCell.alpha
            : InactiveHudAlpha;
        setColor(Palette::EnergyCell, energyAlpha);
        rectangle(cellX,
                  HudLayout::BarEnergyY, HudLayout::BarCellSize, HudLayout::BarCellSize);
        cellX += HudLayout::BarCellSpacing;
    }
}

void drawScore(const State& state)
{
    // Первые score огоньков активны, остальные остаются тусклыми.
    float lightX = HudLayout::ScoreStartX;
    for (int point = 0; point < HudLayout::ScoreLightCount; ++point) {
        const float alpha = point < state.world.score
            ? Palette::Score.alpha
            : InactiveHudAlpha;
        setColor(Palette::Score, alpha);
        disc(lightX,
             HudLayout::ScoreY, HudLayout::ScoreLightRadius, HudLayout::ScoreLightRadius,
             HudLayout::FineCircleSides);
        lightX += HudLayout::ScoreSpacing;
    }
}

void drawTarget(const State& state, bool locked)
{
    // При захвате цели угловые метки дополняют круговой индикатор.
    setColor(Palette::Target, locked ? Palette::Target.alpha : InactiveTargetAlpha);
    ring(state.shuttle.position.x, state.shuttle.position.y,
         HudLayout::TargetRadius, HudLayout::TargetRadius);
    if (locked) {
        // Четыре коротких отрезка направлены к центру при захваченной цели.
        glBegin(GL_LINES);
        const float x = state.shuttle.position.x;
        const float y = state.shuttle.position.y;
        glVertex2f(x - HudLayout::TargetMarkOuter, y);
        glVertex2f(x - HudLayout::TargetMarkInner, y);
        glVertex2f(x + HudLayout::TargetMarkInner, y);
        glVertex2f(x + HudLayout::TargetMarkOuter, y);
        glVertex2f(x, y - HudLayout::TargetMarkOuter);
        glVertex2f(x, y - HudLayout::TargetMarkInner);
        glVertex2f(x, y + HudLayout::TargetMarkInner);
        glVertex2f(x, y + HudLayout::TargetMarkOuter);
        glEnd();
    }
}

void drawHud(const State& state, float time, bool locked)
{
    const float horizontalScale = WorldSettings.width / ReferenceScreenWidth;
    const float verticalScale = WorldSettings.height / ReferenceScreenHeight;
    // Сохраняем матрицу мира и масштабируем только HUD из опорного разрешения.
    glPushMatrix();
    glScalef(horizontalScale, verticalScale, 1.0f);

    drawHudFrame();
    drawRadar(state, time);
    drawStatusBars();
    drawScore(state);
    drawTarget(state, locked);

    // Не переносим масштаб HUD на игровые объекты, которые будут рисоваться дальше.
    glPopMatrix();
}

void drawScene(const State& state, float time, bool portalsOpen, bool locked)
{
    // Четыре вершины заполняют весь экран фоном; цвет меняется между верхом и низом.
    glBegin(GL_QUADS);
    setColor(Palette::BackgroundTop);
    glVertex2f(0.0f, 0.0f);
    glVertex2f(WorldSettings.width, 0.0f);
    setColor(Palette::BackgroundBottom);
    glVertex2f(WorldSettings.width, WorldSettings.height);
    glVertex2f(0.0f, WorldSettings.height);
    glEnd();

    // Порядок важен: дальние фоновые объекты рисуются раньше ближайших.
    drawNebula(time, NebulaSettings);
    drawStars(time, StarFieldSettings);
    drawPlanet(time, PlanetSettings);
    drawStation(time, StationSettings);
    float portalTime = time;
    for (const Portal& portal : state.portals) {
        drawPortal(portal, portalTime, portalsOpen);
        portalTime += 1.0f;
    }

    for (const Drone& drone : state.drones) {
        drawDrone(drone, time);
    }

    drawBolts(state.bolts);
    drawShuttle(state.shuttle, time);
    drawHud(state, time, locked);
}

// Снимок игровых действий на текущий кадр; не содержит системных кодов клавиш.
struct InputState
{
    bool up;
    bool down;
    bool left;
    bool right;
    bool fire;
};

InputState readInput()
{
    return {
        Platform::isKeyDown(Platform::Key::Up),
        Platform::isKeyDown(Platform::Key::Down),
        Platform::isKeyDown(Platform::Key::Left),
        Platform::isKeyDown(Platform::Key::Right),
        Platform::isKeyDown(Platform::Key::Fire)
    };
}

void updateShuttle(Shuttle& shuttle, const InputState& input, float deltaTime)
{
    // Направление движения сначала собирается из независимых нажатий клавиш.
    Point movement = { 0.0f, 0.0f };
    if (input.up) {
        movement.y -= 1.0f;
    }
    if (input.down) {
        movement.y += 1.0f;
    }
    if (input.left) {
        movement.x -= 1.0f;
    }
    if (input.right) {
        movement.x += 1.0f;
    }

    shuttle.thrusting = movement.x != 0.0f || movement.y != 0.0f;
    if (shuttle.thrusting) {
        float speed = ShuttleSettings.movementSpeed;

        // При движении по диагонали уменьшаем скорость, чтобы длина вектора
        // не была больше скорости движения по одной оси.
        if (movement.x != 0.0f && movement.y != 0.0f) {
            speed *= ShuttleSettings.diagonalSpeedFactor;
        }
        shuttle.position.x += movement.x * speed * deltaTime;
        shuttle.position.y += movement.y * speed * deltaTime;

        // atan2 возвращает угол в радианах; переводим его в градусы для OpenGL.
        shuttle.heading = std::atan2(movement.y, movement.x) *
            DegreesPerHalfTurn / Pi + ShuttleSettings.headingOffset;
    }

    // Ограничиваем позицию после перемещения, чтобы шаттл не покидал игровую область.
    if (shuttle.position.x < ShuttleSettings.leftLimit) {
        shuttle.position.x = ShuttleSettings.leftLimit;
    }
    if (shuttle.position.x > ShuttleSettings.rightLimit) {
        shuttle.position.x = ShuttleSettings.rightLimit;
    }
    if (shuttle.position.y < ShuttleSettings.topLimit) {
        shuttle.position.y = ShuttleSettings.topLimit;
    }
    if (shuttle.position.y > ShuttleSettings.bottomLimit) {
        shuttle.position.y = ShuttleSettings.bottomLimit;
    }
}

void fireBolt(Shuttle& shuttle, std::vector<Bolt>& bolts, const InputState& input, float time)
{
    // Выстрел возможен только при удержании Fire и после окончания перезарядки.
    if (!input.fire || time < shuttle.nextShotTime) {
        return;
    }

    // Угол шаттла задан в градусах, а sin/cos принимают радианы.
    const float angle = (shuttle.heading - ShuttleSettings.headingOffset) *
        Pi / DegreesPerHalfTurn;
    const Point direction = { std::cos(angle), std::sin(angle) };
    Bolt bolt;
    bolt.position = Point{ shuttle.position.x + direction.x * ShuttleSettings.shotSpawnOffset,
                           shuttle.position.y + direction.y * ShuttleSettings.shotSpawnOffset };
    bolt.velocity = Point{ direction.x * ShuttleSettings.shotSpeed, direction.y * ShuttleSettings.shotSpeed };
    bolt.secondsLeft = ShuttleSettings.shotLifetime;
    bolts.push_back(bolt);
    shuttle.nextShotTime = time + ShuttleSettings.shotCooldown;
}

void updateDrones(std::vector<Drone>& drones, float time)
{
    // Повторно активируем дрон по истечении времени его восстановления.
    for (Drone& drone : drones) {
        if (!drone.active && time >= drone.respawnTime) {
            drone.active = true;
        }
    }
}

void updateBolts(std::vector<Bolt>& bolts, std::vector<Drone>& drones,
                 World& world, float time, float deltaTime)
{
    // erase возвращает следующий итератор; удаление не пропускает соседние снаряды.
    for (auto boltIterator = bolts.begin(); boltIterator != bolts.end();) {
        Bolt& bolt = *boltIterator;
        bolt.position.x += bolt.velocity.x * deltaTime;
        bolt.position.y += bolt.velocity.y * deltaTime;
        bolt.secondsLeft -= deltaTime;

        bool hitDrone = false;
        for (Drone& drone : drones) {
            if (!drone.active) {
                continue;
            }

            const Point target = dronePosition(drone, time);
            const float offsetX = bolt.position.x - target.x;
            const float offsetY = bolt.position.y - target.y;
            // Сравниваем квадраты расстояний: корень не нужен для проверки попадания.
            if (offsetX * offsetX + offsetY * offsetY < DroneSettings.hitRadius * DroneSettings.hitRadius) {
                drone.active = false;
                drone.respawnTime = time + DroneSettings.respawnDelay;
                hitDrone = true;
                break;
            }
        }

        const bool outsideScreen = bolt.position.x < 0.0f || bolt.position.x > WorldSettings.width ||
                                   bolt.position.y < 0.0f || bolt.position.y > WorldSettings.height;
        if (hitDrone || bolt.secondsLeft <= 0.0f || outsideScreen) {
            if (hitDrone) {
                ++world.score;
            }
            boltIterator = bolts.erase(boltIterator);
        } else {
            ++boltIterator;
        }
    }
}

bool distanceIsLessThan(Point first, Point second, float distance)
{
    const float offsetX = first.x - second.x;
    const float offsetY = first.y - second.y;
    // Квадраты сравнимы без sqrt: это короче и дешевле вычислительно.
    return offsetX * offsetX + offsetY * offsetY < distance * distance;
}

void usePortals(Shuttle& shuttle, const std::array<Portal, PortalCount>& portals, World& world, float time, bool portalsOpen)
{
    // Телепортация блокируется, пока порталы закрыты или действует общий cooldown.
    if (!portalsOpen || time < world.nextPortalTime) {
        return;
    }

    for (const Portal& portal : portals) {
        if (distanceIsLessThan(shuttle.position, portal.position, PortalSettings.triggerRadius)) {
            shuttle.position = portal.destination;
            world.nextPortalTime = time + PortalSettings.cooldown;
            return;
        }
    }
}
}

int Game::run()
{
    Game::State state;
    Platform::Error error;
    if (!Platform::initialize(static_cast<int>(WorldSettings.width),
                              static_cast<int>(WorldSettings.height), error)) {
        Platform::reportError(error);
        std::vector<Platform::Error> cleanupErrors;
        Platform::shutdown(cleanupErrors);
        for (const Platform::Error& cleanupError : cleanupErrors) {
            Platform::reportError(cleanupError, false);
        }
        return 1;
    }

    // glViewport задаёт прямоугольную область окна, в которую выводится кадр.
    glViewport(0, 0,
               static_cast<GLsizei>(WorldSettings.width),
               static_cast<GLsizei>(WorldSettings.height));
    // Проекция задаёт координаты игрового поля. Начало находится сверху слева:
    // верхняя граница Y равна 0, нижняя — высоте мира.
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    // glOrtho задаёт ортографическую проекцию: координаты не уменьшаются с глубиной.
    glOrtho(0, WorldSettings.width, WorldSettings.height, 0, NearDepth, FarDepth);

    // glMatrixMode выбирает матрицу объектов; glLoadIdentity очищает старые преобразования.
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    // Сцена плоская, поэтому глубина не нужна: порядок рисования задаёт перекрытие объектов.
    glDisable(GL_DEPTH_TEST);

    // Разрешаем смешивание полупрозрачных цветов с уже нарисованным фоном.
    glEnable(GL_BLEND);
    // Итоговый цвет = цвет объекта * его альфа + цвет фона * оставшаяся доля.
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    const std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
    std::chrono::steady_clock::time_point previousFrame = start;
    std::chrono::steady_clock::time_point fpsUpdateTime = start;
    constexpr double TargetFramesPerSecond = 120.0;
    const std::chrono::steady_clock::duration targetFrameDuration =
        std::chrono::duration_cast<std::chrono::steady_clock::duration>(
            std::chrono::duration<double>(1.0 / TargetFramesPerSecond));
    double framesSinceTitleUpdate = 0.0;

    int exitCode = 0;
    while (true) {
        const Platform::EventResult eventResult = Platform::processEvents(error);
        if (eventResult == Platform::EventResult::Failed) {
            Platform::reportError(error);
            exitCode = 1;
            break;
        }
        if (eventResult == Platform::EventResult::Quit) {
            break;
        }
        // deltaTime измеряет время между кадрами; его ограничение предотвращает
        // слишком большой скачок симуляции после задержки или переключения окна.
        const std::chrono::steady_clock::time_point frameStart = std::chrono::steady_clock::now();
        const float time = std::chrono::duration<float>(frameStart - start).count();
        float deltaTime = std::chrono::duration<float>(frameStart - previousFrame).count();
        previousFrame = frameStart;
        if (deltaTime > WorldSettings.maxFrameTime) {
            deltaTime = WorldSettings.maxFrameTime;
        }

        // Сначала обновляем состояние сущностей, затем вычисляем состояние мира для отрисовки.
        const Game::InputState input = Game::readInput();
        Game::updateShuttle(state.shuttle, input, deltaTime);
        Game::fireBolt(state.shuttle, state.bolts, input, time);
        Game::updateDrones(state.drones, time);
        Game::updateBolts(state.bolts, state.drones, state.world, time, deltaTime);

        // fmod возвращает положение внутри повторяющегося цикла порталов.
        const float portalCycle = std::fmod(time, WorldSettings.portalCycleDuration);
        const bool portalsOpen = portalCycle > WorldSettings.portalOpenTime &&
                                 portalCycle < WorldSettings.portalCloseTime;
        Game::usePortals(state.shuttle, state.portals, state.world, time, portalsOpen);
        const bool locked = Game::distanceIsLessThan(
            state.shuttle.position, StationSettings.position, StationSettings.lockRadius);

        // glClearColor сохраняет цвет, которым glClear заполнит цветовой буфер.
        glClearColor(Palette::Clear.red, Palette::Clear.green,
                     Palette::Clear.blue, Palette::Clear.alpha);
        glClear(GL_COLOR_BUFFER_BIT);
        // Обнуляем матрицу модели перед рисованием сцены, чтобы не наследовать
        // преобразования от предыдущих объектов.
        glLoadIdentity();
        Game::drawScene(state, time, portalsOpen, locked);
        if (!Platform::present(error)) {
            Platform::reportError(error);
            exitCode = 1;
            break;
        }

        // FPS — число кадров за прошедший интервал; заголовок обновляется раз в секунду.
        ++framesSinceTitleUpdate;
        const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
        const float fpsUpdateInterval =
            std::chrono::duration<float>(now - fpsUpdateTime).count();
        if (fpsUpdateInterval >= 1.0f) {
            const int framesPerSecond = static_cast<int>(
                framesSinceTitleUpdate / fpsUpdateInterval + 0.5);
            const std::string title = "ORBITAL // SECTOR 09 | FPS: " + std::to_string(framesPerSecond);
            if (!Platform::setWindowTitle(title.c_str(), error)) {
                // Отказ обновления заголовка не мешает продолжать игру.
                Platform::reportError(error, false);
            }
            fpsUpdateTime = now;
            framesSinceTitleUpdate = 0.0;
        }

        // Ждём только остаток интервала кадра; если отрисовка уже заняла больше,
        // цикл сразу переходит к следующему кадру.
        const std::chrono::steady_clock::duration frameWorkDuration =
            std::chrono::steady_clock::now() - frameStart;
        if (frameWorkDuration < targetFrameDuration) {
            std::this_thread::sleep_for(targetFrameDuration - frameWorkDuration);
        }
    }

    std::vector<Platform::Error> cleanupErrors;
    Platform::shutdown(cleanupErrors);
    for (const Platform::Error& cleanupError : cleanupErrors) {
        Platform::reportError(cleanupError, false);
    }
    return cleanupErrors.empty() ? exitCode : 1;
}

#if defined(_WIN32)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    return Game::run();
}
#else
int main()
{
    return Game::run();
}
#endif
