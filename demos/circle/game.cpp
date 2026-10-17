#include "platform.h"
#include "game.h"
#include <GL/gl.h>
#include <chrono>
#include <cmath>
#include <cstdio>
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
const Point Hull[] = {
    { 0.0f, -20.0f }, { 7.0f, -5.0f }, { 17.0f, 9.0f }, { 8.0f, 7.0f },
    { 0.0f, 13.0f }, { -8.0f, 7.0f }, { -17.0f, 9.0f }, { -7.0f, -5.0f }
};
const Point Window[] = {
    { -3.0f, 1.0f }, { 3.0f, 1.0f }, { 5.0f, 7.0f }, { -5.0f, 7.0f }
};
const int HullVertexCount = sizeof(Hull) / sizeof(Hull[0]);
const int WindowVertexCount = sizeof(Window) / sizeof(Window[0]);
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
enum PortalIndex
{
    FirstPortal,
    SecondPortal
};
const float Pi = 3.14159265f;
const float RadiansPerFullTurn = 2.0f * Pi;
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

struct Shuttle
{
    Point position;
    float heading;
    float headingOffset;
    bool thrusting;

    // Управление и границы игровой области.
    float movementSpeed;
    float diagonalSpeedFactor;
    float leftLimit;
    float rightLimit;
    float topLimit;
    float bottomLimit;

    // Таймеры и параметры снаряда.
    float nextShotTime;
    float shotSpeed;
    float shotSpawnOffset;
    float shotLifetime;
    float shotCooldown;

    // Масштаб модели, защитный контур и анимация реактивного пламени.
    float scale;
    float shieldRadius;
    float flameBase;
    float flamePulse;
    float flameSpeed;

    Shuttle()
        : position{170.0f, 390.0f}
        , heading(90.0f)
        , headingOffset(90.0f)
        , thrusting(false)
        , movementSpeed(205.0f)
        , diagonalSpeedFactor(0.7071f)
        , leftLimit(38.0f)
        , rightLimit(762.0f)
        , topLimit(76.0f)
        , bottomLimit(505.0f)
        , nextShotTime(0.0f)
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

struct Bolt
{
    Point position;
    Point velocity;
    float secondsLeft;

    // Настройки свечения, следа и толщины отрисовки снаряда.
    float glowRadius;
    float trailLength;
    float lineWidth;
    int glowSides;

    Bolt()
        : secondsLeft(0.0f)
        , glowRadius(8.0f)
        , trailLength(0.025f)
        , lineWidth(3.0f)
        , glowSides(12)
    {
    }
};

struct Drone
{
    Point orbitCenter;
    float phase;

    // Траектория патруля — эллипс с независимыми радиусами по осям.
    float orbitRadiusX;
    float orbitRadiusY;
    float orbitSpeed;
    float hitRadius;

    // Время повторного появления и размеры частей модели.
    float respawnDelay;
    float respawnTime;
    float glowRadius;
    float bodyRadius;
    float outlineRadius;
    float coreRadius;
    bool active;

    explicit Drone(float startPhase = 0.0f)
        : orbitCenter{655.0f, 340.0f}
        , phase(startPhase)
        , orbitRadiusX(150.0f)
        , orbitRadiusY(47.0f)
        , orbitSpeed(0.42f)
        , hitRadius(17.0f)
        , respawnDelay(4.0f)
        , respawnTime(0.0f)
        , glowRadius(19.0f)
        , bodyRadius(11.0f)
        , outlineRadius(13.0f)
        , coreRadius(3.0f)
        , active(true)
    {
    }
};

struct Portal
{
    Point position;
    Point destination;

    // Радиус срабатывания и время до следующего переноса.
    float triggerRadius;
    float cooldown;

    // Пульсация, яркость и размеры визуальных слоёв портала.
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

    Portal()
        : position{0.0f, 0.0f}
        , destination{0.0f, 0.0f}
        , triggerRadius(28.0f)
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

    Portal(Point entrance, Point exit)
        : Portal()
    {
        position = entrance;
        destination = exit;
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

struct StarField
{
    // Количество точек, их размер и диапазон яркости.
    int count;
    float pointSize;
    float glowBase;
    float glowRange;

    StarField()
        : count(130)
        , pointSize(2.0f)
        , glowBase(0.65f)
        , glowRange(0.35f)
    {
    }
};

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

struct World
{
    // Размер игрового поля и верхняя граница шага симуляции.
    float width;
    float height;
    float maxFrameTime;

    // Расписание порталов и счёт.
    float portalCycleDuration;
    float portalOpenTime;
    float portalCloseTime;
    float nextPortalTime;
    int score;
    int startingDroneCount;
    float dronePhaseSpacing;

    World()
        : width(800.0f)
        , height(600.0f)
        , maxFrameTime(0.08f)
        , portalCycleDuration(24.0f)
        , portalOpenTime(7.0f)
        , portalCloseTime(14.0f)
        , nextPortalTime(0.0f)
        , score(0)
        , startingDroneCount(3)
        , dronePhaseSpacing(2.094f)
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
    Portal portals[PortalCount];
    Planet planet;
    Station station;
    StarField stars;
    Nebula nebula;

    State()
    {
        // Порталы соединены попарно: вход каждого ведёт к выходу другого.
        portals[FirstPortal] = Portal(Point{138.0f, 184.0f}, Point{738.0f, 432.0f});
        portals[SecondPortal] = Portal(Point{690.0f, 432.0f}, Point{90.0f, 184.0f});

        // Фаза разносит дроны по орбите, чтобы они не летели одной группой.
        for (int drone = 0; drone < world.startingDroneCount; ++drone) {
            drones.push_back(Drone(drone * world.dronePhaseSpacing));
        }
    }
};

Point dronePosition(const Drone& drone, float time)
{
    // Синус и косинус задают эллиптическую траекторию; phase сдвигает дроны друг относительно друга.
    const float angle = time * drone.orbitSpeed + drone.phase;
    Point position = { drone.orbitCenter.x + std::cos(angle) * drone.orbitRadiusX,
                       drone.orbitCenter.y + std::sin(angle) * drone.orbitRadiusY };
    return position;
}

// Для простоты учебного примера геометрия передаётся старым непосредственным
// способом OpenGL: glBegin выбирает примитив, glVertex задаёт вершины, glEnd его завершает.
void disc(float x, float y, float radiusX, float radiusY, int sides = DefaultDiscSides)
{
    // GL_TRIANGLE_FAN строит заполненный круг веером треугольников от центра к контуру.
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(x, y);
    for (int side = 0; side <= sides; ++side) {
        const float angle = RadiansPerFullTurn * side / sides;
        glVertex2f(x + std::cos(angle) * radiusX, y + std::sin(angle) * radiusY);
    }
    glEnd();
}

void ring(float x, float y, float radiusX, float radiusY, int sides = DefaultRingSides)
{
    // GL_LINE_LOOP соединяет вершины по кругу и автоматически замыкает последнюю с первой.
    glBegin(GL_LINE_LOOP);
    for (int side = 0; side < sides; ++side) {
        const float angle = RadiansPerFullTurn * side / sides;
        glVertex2f(x + std::cos(angle) * radiusX, y + std::sin(angle) * radiusY);
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
    const float brightness = open ? portal.openBrightness : portal.closedBrightness;
    const float pulse = portal.pulseBase + portal.pulseSize * std::sin(time * portal.pulseSpeed);

    // Слои идут от мягкого свечения к яркому центру; каждый следующий слой
    // перекрывает предыдущий благодаря включённому альфа-смешиванию.
    setColor(Palette::PortalGlow, Palette::PortalGlow.alpha * brightness);
    disc(portal.position.x, portal.position.y, portal.glowRadiusX * pulse, portal.glowRadiusY * pulse);
    setColor(Palette::PortalOuterRing, Palette::PortalOuterRing.alpha * brightness);
    ring(portal.position.x, portal.position.y, portal.outerRadiusX, portal.outerRadiusY);
    setColor(Palette::PortalInnerRing, Palette::PortalInnerRing.alpha * brightness);
    ring(portal.position.x, portal.position.y, portal.innerRadiusX, portal.innerRadiusY);
    setColor(Palette::PortalCore);
    disc(portal.position.x, portal.position.y, portal.coreRadiusX, portal.coreRadiusY);
}

void drawStars(float time, const StarField& stars, const World& world)
{
    const unsigned int SeedMultiplier = 7919u;
    const unsigned int SeedOffset = 17u;
    const unsigned int XMultiplier = 37u;
    const unsigned int XOffset = 91u;
    const unsigned int YMultiplier = 53u;
    const unsigned int YOffset = 29u;
    const unsigned int screenWidth = static_cast<unsigned int>(world.width);
    const unsigned int screenHeight = static_cast<unsigned int>(world.height);

    // Фиксированные множители дают псевдослучайное, но повторяемое расположение звёзд
    // без отдельного массива координат.
    // GL_POINTS рисует каждую вершину как отдельную точку заданного размера.
    glPointSize(stars.pointSize);
    glBegin(GL_POINTS);
    for (int star = 0; star < stars.count; ++star) {
        // Простая арифметика даёт каждой звезде повторяемую координату.
        const unsigned int seed = static_cast<unsigned int>(star) * SeedMultiplier + SeedOffset;
        const float x = static_cast<float>((seed * XMultiplier + XOffset) % screenWidth);
        const float y = static_cast<float>((seed * YMultiplier + YOffset) % screenHeight);
        const float glow = stars.glowBase + stars.glowRange * std::sin(time + star);
        setColor(Palette::Star, glow);
        glVertex2f(x, y);
    }
    glEnd();
}

void drawNebula(float time, const Nebula& nebula)
{
    // Несколько прозрачных овалов накладываются друг на друга и изображают туманность.
    for (int cloud = 0; cloud < nebula.cloudCount; ++cloud) {
        const float x = nebula.startX + cloud * nebula.spacing;
        // Синус смещает облака по вертикали с разной фазой, сохраняя шаг по X.
        const float y = nebula.startY + nebula.sway * std::sin(time * nebula.swaySpeed + cloud);
        setColor(Palette::Nebula);
        disc(x, y, nebula.radiusX, nebula.radiusY);
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
    const int lastBand = firstBand + planet.bandCount - 1;
    // Устанавливаем толщину линий атмосферы до построения полос.
    glLineWidth(planet.bandLineWidth);
    for (int band = firstBand; band <= lastBand; ++band) {
        const float y = planet.position.y + band * planet.bandSpacing;
        setColor(Palette::PlanetBand,
                 Palette::PlanetBand.alpha + planet.bandPulseAlpha * std::sin(time + band));
        // GL_LINES трактует каждую пару вершин как отдельный отрезок.
        glBegin(GL_LINES);
        glVertex2f(planet.position.x - planet.bandHalfWidth, y);
        glVertex2f(planet.position.x + planet.bandHalfWidth, y);
        glEnd();
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
    for (int module = 0; module < station.moduleCount; ++module) {
        const int column = module % station.moduleColumns;
        const int row = module / station.moduleColumns;
        const float moduleX = x + (column - (station.moduleColumns - 1) * 0.5f) * station.moduleSpacingX;
        const float moduleY = y + (row - (station.moduleCount / station.moduleColumns - 1) * 0.5f) *
            station.moduleSpacingY;
        setColor(Palette::StationModule);
        disc(moduleX, moduleY, station.moduleRadiusX, station.moduleRadiusY, station.moduleSides);
        setColor(Palette::StationModuleRing);
        ring(moduleX, moduleY, station.moduleRadiusX, station.moduleRadiusY, station.moduleSides);
        setColor(Palette::StationModuleLight);
        disc(moduleX, moduleY, station.moduleLightRadius, station.moduleLightRadius, station.moduleSides);
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
    disc(position.x, position.y, drone.glowRadius, drone.glowRadius);
    setColor(Palette::DroneBody);
    disc(position.x, position.y, drone.bodyRadius, drone.bodyRadius, OctagonSides);
    setColor(Palette::DroneRing);
    ring(position.x, position.y, drone.outlineRadius, drone.outlineRadius, OctagonSides);
    setColor(Palette::DroneCore);
    disc(position.x, position.y, drone.coreRadius, drone.coreRadius, OctagonSides);
}

void drawBolts(const std::vector<Bolt>& bolts)
{
    // У каждого снаряда своя толщина линии; после цикла восстанавливаем значение по умолчанию.
    for (std::size_t index = 0; index < bolts.size(); ++index) {
        const Bolt& bolt = bolts[index];
        glLineWidth(bolt.lineWidth);
        setColor(Palette::BoltGlow);
        ring(bolt.position.x, bolt.position.y, bolt.glowRadius, bolt.glowRadius, bolt.glowSides);
        setColor(Palette::Bolt);
        // Отрезок начинается позади снаряда в направлении, обратном его скорости.
        glBegin(GL_LINES);
        glVertex2f(bolt.position.x - bolt.velocity.x * bolt.trailLength,
                   bolt.position.y - bolt.velocity.y * bolt.trailLength);
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
    glScalef(shuttle.scale, shuttle.scale, 1.0f);

    if (shuttle.thrusting) {
        const float flame = shuttle.flameBase + shuttle.flamePulse * std::sin(time * shuttle.flameSpeed);
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
    for (int vertex = 0; vertex < ShuttleModel::HullVertexCount; ++vertex) {
        glVertex2f(ShuttleModel::Hull[vertex].x, ShuttleModel::Hull[vertex].y);
    }
    glEnd();

    setColor(Palette::ShuttleWindow);
    // Иллюминатор задан четырьмя вершинами и рисуется как четырёхугольник.
    glBegin(GL_QUADS);
    for (int vertex = 0; vertex < ShuttleModel::WindowVertexCount; ++vertex) {
        glVertex2f(ShuttleModel::Window[vertex].x, ShuttleModel::Window[vertex].y);
    }
    glEnd();
    setColor(Palette::ShuttleShield);
    ring(0.0f, 0.0f, shuttle.shieldRadius, shuttle.shieldRadius);

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
        (state.shuttle.position.x - state.station.position.x) * HudLayout::RadarMapScale;
    const float shuttleBlipY = HudLayout::RadarCenterY +
        (state.shuttle.position.y - state.station.position.y) * HudLayout::RadarMapScale;
    disc(shuttleBlipX, shuttleBlipY,
         HudLayout::BlipRadius, HudLayout::BlipRadius, HudLayout::FineCircleSides);
    setColor(Palette::BeaconBlip);
    const float beaconOrbit = time * state.station.beaconSpeed;
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
    for (int cell = 0; cell < HudLayout::BarCellCount; ++cell) {
        const float shieldAlpha = cell < HudLayout::ShieldCellCount
            ? Palette::ShieldCell.alpha
            : InactiveHudAlpha;
        setColor(Palette::ShieldCell, shieldAlpha);
        rectangle(HudLayout::BarStartX + cell * HudLayout::BarCellSpacing,
                  HudLayout::BarShieldY, HudLayout::BarCellSize, HudLayout::BarCellSize);
        const float energyAlpha = cell < HudLayout::EnergyCellCount
            ? Palette::EnergyCell.alpha
            : InactiveHudAlpha;
        setColor(Palette::EnergyCell, energyAlpha);
        rectangle(HudLayout::BarStartX + cell * HudLayout::BarCellSpacing,
                  HudLayout::BarEnergyY, HudLayout::BarCellSize, HudLayout::BarCellSize);
    }
}

void drawScore(const State& state)
{
    // Первые score огоньков активны, остальные остаются тусклыми.
    for (int point = 0; point < HudLayout::ScoreLightCount; ++point) {
        const float alpha = point < state.world.score
            ? Palette::Score.alpha
            : InactiveHudAlpha;
        setColor(Palette::Score, alpha);
        disc(HudLayout::ScoreStartX + point * HudLayout::ScoreSpacing,
             HudLayout::ScoreY, HudLayout::ScoreLightRadius, HudLayout::ScoreLightRadius,
             HudLayout::FineCircleSides);
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
    const float horizontalScale = state.world.width / ReferenceScreenWidth;
    const float verticalScale = state.world.height / ReferenceScreenHeight;
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
    glVertex2f(state.world.width, 0.0f);
    setColor(Palette::BackgroundBottom);
    glVertex2f(state.world.width, state.world.height);
    glVertex2f(0.0f, state.world.height);
    glEnd();

    // Порядок важен: дальние фоновые объекты рисуются раньше ближайших.
    drawNebula(time, state.nebula);
    drawStars(time, state.stars, state.world);
    drawPlanet(time, state.planet);
    drawStation(time, state.station);
    for (int portal = 0; portal < PortalCount; ++portal) {
        drawPortal(state.portals[portal], time + portal, portalsOpen);
    }

    for (std::size_t index = 0; index < state.drones.size(); ++index) {
        drawDrone(state.drones[index], time);
    }

    drawBolts(state.bolts);
    drawShuttle(state.shuttle, time);
    drawHud(state, time, locked);
}

void updateShuttle(Shuttle& shuttle, float deltaTime)
{
    // Направление движения сначала собирается из независимых нажатий клавиш.
    Point movement = { 0.0f, 0.0f };
    if (Platform::isKeyDown(Platform::Key::Up)) {
        movement.y -= 1.0f;
    }
    if (Platform::isKeyDown(Platform::Key::Down)) {
        movement.y += 1.0f;
    }
    if (Platform::isKeyDown(Platform::Key::Left)) {
        movement.x -= 1.0f;
    }
    if (Platform::isKeyDown(Platform::Key::Right)) {
        movement.x += 1.0f;
    }

    shuttle.thrusting = movement.x != 0.0f || movement.y != 0.0f;
    if (shuttle.thrusting) {
        float speed = shuttle.movementSpeed;

        // При движении по диагонали уменьшаем скорость, чтобы длина вектора
        // не была больше скорости движения по одной оси.
        if (movement.x != 0.0f && movement.y != 0.0f) {
            speed *= shuttle.diagonalSpeedFactor;
        }
        shuttle.position.x += movement.x * speed * deltaTime;
        shuttle.position.y += movement.y * speed * deltaTime;

        // atan2 возвращает угол в радианах; переводим его в градусы для OpenGL.
        shuttle.heading = std::atan2(movement.y, movement.x) *
            DegreesPerHalfTurn / Pi + shuttle.headingOffset;
    }

    // Ограничиваем позицию после перемещения, чтобы шаттл не покидал игровую область.
    if (shuttle.position.x < shuttle.leftLimit) {
        shuttle.position.x = shuttle.leftLimit;
    }
    if (shuttle.position.x > shuttle.rightLimit) {
        shuttle.position.x = shuttle.rightLimit;
    }
    if (shuttle.position.y < shuttle.topLimit) {
        shuttle.position.y = shuttle.topLimit;
    }
    if (shuttle.position.y > shuttle.bottomLimit) {
        shuttle.position.y = shuttle.bottomLimit;
    }
}

void fireBolt(Shuttle& shuttle, std::vector<Bolt>& bolts, float time)
{
    // Выстрел возможен только при удержании Fire и после окончания перезарядки.
    if (!Platform::isKeyDown(Platform::Key::Fire) || time < shuttle.nextShotTime) {
        return;
    }

    // Угол шаттла задан в градусах, а sin/cos принимают радианы.
    const float angle = (shuttle.heading - shuttle.headingOffset) *
        Pi / DegreesPerHalfTurn;
    const Point direction = { std::cos(angle), std::sin(angle) };
    Bolt bolt;
    bolt.position = Point{ shuttle.position.x + direction.x * shuttle.shotSpawnOffset,
                           shuttle.position.y + direction.y * shuttle.shotSpawnOffset };
    bolt.velocity = Point{ direction.x * shuttle.shotSpeed, direction.y * shuttle.shotSpeed };
    bolt.secondsLeft = shuttle.shotLifetime;
    bolts.push_back(bolt);
    shuttle.nextShotTime = time + shuttle.shotCooldown;
}

void updateDrones(std::vector<Drone>& drones, float time)
{
    // Повторно активируем дрон по истечении времени его восстановления.
    for (std::size_t index = 0; index < drones.size(); ++index) {
        if (!drones[index].active && time >= drones[index].respawnTime) {
            drones[index].active = true;
        }
    }
}

void updateBolts(std::vector<Bolt>& bolts, std::vector<Drone>& drones,
                 World& world, float time, float deltaTime)
{
    // Индексный цикл позволяет удалить снаряд на месте и не пропустить следующий элемент.
    for (std::size_t boltIndex = 0; boltIndex < bolts.size();) {
        Bolt& bolt = bolts[boltIndex];
        bolt.position.x += bolt.velocity.x * deltaTime;
        bolt.position.y += bolt.velocity.y * deltaTime;
        bolt.secondsLeft -= deltaTime;

        bool hitDrone = false;
        for (std::size_t droneIndex = 0; droneIndex < drones.size() && !hitDrone; ++droneIndex) {
            Drone& drone = drones[droneIndex];
            if (!drone.active) {
                continue;
            }

            const Point target = dronePosition(drone, time);
            const float offsetX = bolt.position.x - target.x;
            const float offsetY = bolt.position.y - target.y;
            // Сравниваем квадраты расстояний: корень не нужен для проверки попадания.
            if (offsetX * offsetX + offsetY * offsetY < drone.hitRadius * drone.hitRadius) {
                drone.active = false;
                drone.respawnTime = time + drone.respawnDelay;
                hitDrone = true;
            }
        }

        const bool outsideScreen = bolt.position.x < 0.0f || bolt.position.x > world.width ||
                                   bolt.position.y < 0.0f || bolt.position.y > world.height;
        if (hitDrone || bolt.secondsLeft <= 0.0f || outsideScreen) {
            if (hitDrone) {
                ++world.score;
            }
            bolts.erase(bolts.begin() + boltIndex);
        } else {
            ++boltIndex;
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

void usePortals(Shuttle& shuttle, Portal portals[PortalCount], World& world, float time, bool portalsOpen)
{
    // Телепортация блокируется, пока порталы закрыты или действует общий cooldown.
    if (!portalsOpen || time < world.nextPortalTime) {
        return;
    }

    for (int index = 0; index < PortalCount; ++index) {
        Portal& portal = portals[index];
        if (distanceIsLessThan(shuttle.position, portal.position, portal.triggerRadius)) {
            shuttle.position = portal.destination;
            world.nextPortalTime = time + portal.cooldown;
            return;
        }
    }
}
}

int Game::run()
{
    Game::State state;
    Platform::Error error;
    if (!Platform::initialize(static_cast<int>(state.world.width),
                              static_cast<int>(state.world.height), error)) {
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
               static_cast<GLsizei>(state.world.width),
               static_cast<GLsizei>(state.world.height));
    // Проекция задаёт координаты игрового поля. Начало находится сверху слева:
    // верхняя граница Y равна 0, нижняя — высоте мира.
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    // glOrtho задаёт ортографическую проекцию: координаты не уменьшаются с глубиной.
    glOrtho(0, state.world.width, state.world.height, 0, NearDepth, FarDepth);

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
    const std::chrono::steady_clock::duration targetFrameDuration =
        std::chrono::duration_cast<std::chrono::steady_clock::duration>(
            std::chrono::duration<double>(1.0 / 120.0));
    int framesSinceTitleUpdate = 0;

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
        if (deltaTime > state.world.maxFrameTime) {
            deltaTime = state.world.maxFrameTime;
        }

        // Сначала обновляем состояние сущностей, затем вычисляем состояние мира для отрисовки.
        Game::updateShuttle(state.shuttle, deltaTime);
        Game::fireBolt(state.shuttle, state.bolts, time);
        Game::updateDrones(state.drones, time);
        Game::updateBolts(state.bolts, state.drones, state.world, time, deltaTime);

        // fmod возвращает положение внутри повторяющегося цикла порталов.
        const float portalCycle = std::fmod(time, state.world.portalCycleDuration);
        const bool portalsOpen = portalCycle > state.world.portalOpenTime &&
                                 portalCycle < state.world.portalCloseTime;
        Game::usePortals(state.shuttle, state.portals, state.world, time, portalsOpen);
        const bool locked = Game::distanceIsLessThan(
            state.shuttle.position, state.station.position, state.station.lockRadius);

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
                framesSinceTitleUpdate / fpsUpdateInterval + 0.5f);
            char title[64];
            std::snprintf(title, sizeof(title), "ORBITAL // SECTOR 09 | FPS: %d", framesPerSecond);
            if (!Platform::setWindowTitle(title, error)) {
                // Отказ обновления заголовка не мешает продолжать игру.
                Platform::reportError(error, false);
            }
            fpsUpdateTime = now;
            framesSinceTitleUpdate = 0;
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
