#include "platform.h"
#include "game.h"
#include "application.h"
#include "math2d.h"
#include "geometry.h"
#include "transform.h"

#if defined(_WIN32)
#include <windows.h>
#endif

#include <GL/gl.h>
#include <array>
#include <cmath>
#include <optional>
#include <string>
#include <vector>

namespace Game
{
using Engine::Vector2;
using Engine::Color;
using Engine::Size2;
using Engine::Rect;
using Engine::Circle;
using Engine::Transform2;
using Engine::Affine2;
using Engine::Pi;

namespace Palette
{
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

// Раскладка интерфейса задана в опорном разрешении ReferenceScreenSize.
namespace HudLayout
{
// Координаты и размеры элементов HUD в опорном разрешении 800x600.
const Rect Frame = { 22, 22, 756, 500 };
const Vector2 RadarCenter = { 720, 470 };
const float RadarOuterRadiusX = 44.0f;
const float RadarOuterRadiusY = 22.0f;
const float RadarInnerRadiusX = 28.0f;
const float RadarInnerRadiusY = 14.0f;
const float RadarSweepRadiusX = 42.0f;
const float RadarSweepRadiusY = 21.0f;
const float RadarMapScale = 0.2f;
const float BlipRadius = 2.5f;
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

namespace ShuttleModel
{
// Контур корпуса, иллюминатора и пламени в локальных координатах шаттла.
const Vector2 FlameLeft = { -7.0f, 13.0f };
const Vector2 FlameRight = { 7.0f, 13.0f };
const std::array<Vector2, 8> Hull = {{
    { 0.0f, -20.0f }, { 7.0f, -5.0f }, { 17.0f, 9.0f }, { 8.0f, 7.0f },
    { 0.0f, 13.0f }, { -8.0f, 7.0f }, { -17.0f, 9.0f }, { -7.0f, -5.0f }
}};
const std::array<Vector2, 4> Window = {{
    { -3.0f, 1.0f }, { 3.0f, 1.0f }, { 5.0f, 7.0f }, { -5.0f, 7.0f }
}};
}

void setColor(const Color& color)
{
    glColor4f(color.red, color.green, color.blue, color.alpha);
}

void setColor(const Color& color, float alpha)
{
    glColor4f(color.red, color.green, color.blue, alpha);
}

void vertex(Vector2 point)
{
    glVertex2f(point.x, point.y);
}

void applyTransform(const Affine2& transform)
{
    // OpenGL хранит матрицу по столбцам; математические коэффициенты остаются в Affine2.
    const std::array<GLfloat, 16> matrix = {
        transform.m00, transform.m10, 0, 0,
        transform.m01, transform.m11, 0, 0,
        0, 0, 1, 0,
        transform.m02, transform.m12, 0, 1
    };
    glMultMatrixf(matrix.data());
}

const int PortalCount = 2;
const int DefaultDiscSides = 32;
const int DefaultRingSides = 48;
const int OctagonSides = 8;
const int DodecagonSides = 12;
const Size2 ReferenceScreenSize = { 800, 600 };
const float HudFrameCornerLength = 50.0f;
const float InactiveHudAlpha = 0.18f;
const float InactiveTargetAlpha = 0.45f;
const float DefaultLineWidth = 1.0f;
const float NearDepth = -1.0f;
const float FarDepth = 1.0f;

struct ShuttleConfig
{
    float headingOffset;
    float movementSpeed;
    Rect movementBounds;

    float shotSpeed;
    float shotSpawnOffset;
    float shotLifetime;
    float shotCooldown;

    float scale;
    float shieldRadius;
    float flameBase;
    float flamePulse;
    float flameSpeed;

    ShuttleConfig()
        : headingOffset(Pi / 2)
        , movementSpeed(205.0f)
        , movementBounds(38, 76, 724, 429)
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

struct Shuttle
{
    Transform2 transform;
    bool thrusting;
    float nextShotTime;

    Shuttle()
        : transform{ { 170, 390 }, Pi / 2, { ShuttleSettings.scale, ShuttleSettings.scale }, {} }
        , thrusting(false)
        , nextShotTime(0.0f)
    {
    }
};

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

struct Bolt
{
    Vector2 position;
    Vector2 velocity;
    float secondsLeft;

    Bolt()
        : position{0.0f, 0.0f}
        , velocity{0.0f, 0.0f}
        , secondsLeft(0.0f)
    {
    }
};

struct DroneConfig
{
    Vector2 orbitCenter;
    float orbitRadiusX;
    float orbitRadiusY;
    float orbitSpeed;
    float hitRadius;
    float respawnDelay;

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

struct PortalConfig
{
    float triggerRadius;
    float cooldown;

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

struct Portal
{
    Vector2 position;
    Vector2 destination;

    Portal(Vector2 entrance, Vector2 exit)
        : position(entrance)
        , destination(exit)
    {
    }
};

struct Planet
{
    Vector2 position;
    float atmosphereRadius;
    float radius;

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
    Vector2 position;
    float lockRadius;

    float outerRadiusX;
    float outerRadiusY;
    float reactorRadius;
    float reactorRingRadius;

    int moduleCount;
    int moduleColumns;
    float moduleSpacingX;
    float moduleSpacingY;
    float moduleRadiusX;
    float moduleRadiusY;
    float moduleLightRadius;

    float beaconSpeed;
    float beaconOrbitX;
    float beaconOrbitY;
    float beaconGlowRadius;
    float beaconRadius;

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

struct WorldConfig
{
    Size2 size;
    float maxFrameTime;
    float portalCycleDuration;
    float portalOpenTime;
    float portalCloseTime;
    int startingDroneCount;
    float dronePhaseSpacing;

    WorldConfig()
        : size(800, 600)
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
    World world;
    Shuttle shuttle;
    std::vector<Drone> drones;
    std::vector<Bolt> bolts;
    // Порталы соединены попарно; их расположение не меняется во время игры.
    const std::array<Portal, PortalCount> portals;

    State()
        : portals{{
            Portal(Vector2{138.0f, 184.0f}, Vector2{738.0f, 432.0f}),
            Portal(Vector2{690.0f, 432.0f}, Vector2{90.0f, 184.0f})
        }}
    {

        float phase = 0.0f;
        for (int drone = 0; drone < WorldSettings.startingDroneCount; ++drone) {
            drones.emplace_back(phase);
            phase += WorldSettings.dronePhaseSpacing;
        }
    }
};

Vector2 dronePosition(const Drone& drone, float time)
{
    const float angle = time * DroneSettings.orbitSpeed + drone.phase;
    const Transform2 orbit = { DroneSettings.orbitCenter, 0,
        { DroneSettings.orbitRadiusX, DroneSettings.orbitRadiusY }, {} };
    return orbit.toAffine().transformPoint(Engine::rotated({ 1, 0 }, angle));
}

Vector2 beaconPosition(const Station& station, float time)
{
    const Transform2 orbit = { station.position, 0, { station.beaconOrbitX, station.beaconOrbitY }, {} };
    return orbit.toAffine().transformPoint(Engine::rotated({ 1, 0 }, time * station.beaconSpeed));
}

void disc(Vector2 center, Vector2 radii, int sides = DefaultDiscSides)
{
    const Affine2 transform = Transform2{ center, 0, radii, {} }.toAffine();
    glBegin(GL_TRIANGLE_FAN);
    vertex(center);
    for (int side = 0; side <= sides; ++side) {
        const float angle = 2 * Pi * static_cast<float>(side) / static_cast<float>(sides);
        vertex(transform.transformPoint(Engine::rotated({ 1, 0 }, angle)));
    }
    glEnd();
}

void disc(const Circle& circle, int sides = DefaultDiscSides)
{
    disc(circle.center, { circle.radius, circle.radius }, sides);
}

void ring(Vector2 center, Vector2 radii, int sides = DefaultRingSides)
{
    const Affine2 transform = Transform2{ center, 0, radii, {} }.toAffine();
    glBegin(GL_LINE_LOOP);
    for (int side = 0; side < sides; ++side) {
        const float angle = 2 * Pi * static_cast<float>(side) / static_cast<float>(sides);
        vertex(transform.transformPoint(Engine::rotated({ 1, 0 }, angle)));
    }
    glEnd();
}

void ring(const Circle& circle, int sides = DefaultRingSides)
{
    ring(circle.center, { circle.radius, circle.radius }, sides);
}

void rectangle(const Rect& rectangle)
{
    glBegin(GL_QUADS);
    vertex(rectangle.position);
    vertex(rectangle.position + Vector2{ rectangle.size.width, 0 });
    vertex(rectangle.position + Vector2{ rectangle.size.width, rectangle.size.height });
    vertex(rectangle.position + Vector2{ 0, rectangle.size.height });
    glEnd();
}

void drawPortal(const Portal& portal, float time, bool open)
{
    const float brightness = open ? PortalSettings.openBrightness : PortalSettings.closedBrightness;
    const float pulse = PortalSettings.pulseBase + PortalSettings.pulseSize * std::sin(time * PortalSettings.pulseSpeed);

    setColor(Palette::PortalGlow, Palette::PortalGlow.alpha * brightness);
    disc(portal.position, { PortalSettings.glowRadiusX * pulse, PortalSettings.glowRadiusY * pulse });
    setColor(Palette::PortalOuterRing, Palette::PortalOuterRing.alpha * brightness);
    ring(portal.position, { PortalSettings.outerRadiusX, PortalSettings.outerRadiusY });
    setColor(Palette::PortalInnerRing, Palette::PortalInnerRing.alpha * brightness);
    ring(portal.position, { PortalSettings.innerRadiusX, PortalSettings.innerRadiusY });
    setColor(Palette::PortalCore);
    disc(portal.position, { PortalSettings.coreRadiusX, PortalSettings.coreRadiusY });
}

void drawStars(float time, const StarField& stars)
{
    const unsigned int SeedMultiplier = 7919u;
    const unsigned int SeedOffset = 17u;
    const unsigned int XMultiplier = 37u;
    const unsigned int XOffset = 91u;
    const unsigned int YMultiplier = 53u;
    const unsigned int YOffset = 29u;
    const unsigned int screenWidth = static_cast<unsigned int>(WorldSettings.size.width);
    const unsigned int screenHeight = static_cast<unsigned int>(WorldSettings.size.height);

    glPointSize(stars.pointSize);
    glBegin(GL_POINTS);
    for (unsigned int star = 0; star < stars.count; ++star) {
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
    float x = nebula.startX;
    float phase = time * nebula.swaySpeed;
    for (int cloud = 0; cloud < nebula.cloudCount; ++cloud) {
        const float y = nebula.startY + nebula.sway * std::sin(phase);
        setColor(Palette::Nebula);
        disc({ x, y }, { nebula.radiusX, nebula.radiusY });
        x += nebula.spacing;
        phase += 1.0f;
    }
}

void drawPlanet(float time, const Planet& planet)
{
    setColor(Palette::PlanetAtmosphere);
    disc(Circle{ planet.position, planet.atmosphereRadius });
    setColor(Palette::PlanetSurface);
    disc(Circle{ planet.position, planet.radius });

    const int firstBand = -planet.bandCount / 2;
    const float firstBandOffset = static_cast<float>(firstBand);
    float bandY = planet.position.y + firstBandOffset * planet.bandSpacing;
    float bandPhase = time + firstBandOffset;
    glLineWidth(planet.bandLineWidth);
    for (int band = 0; band < planet.bandCount; ++band) {
        setColor(Palette::PlanetBand,
                 Palette::PlanetBand.alpha + planet.bandPulseAlpha * std::sin(bandPhase));
        glBegin(GL_LINES);
        glVertex2f(planet.position.x - planet.bandHalfWidth, bandY);
        glVertex2f(planet.position.x + planet.bandHalfWidth, bandY);
        glEnd();
        bandY += planet.bandSpacing;
        bandPhase += 1.0f;
    }

    glLineWidth(planet.innerRingLineWidth);
    setColor(Palette::PlanetRing);
    ring(planet.position, { planet.innerRingRadiusX, planet.innerRingRadiusY });
    glLineWidth(planet.outerRingLineWidth);
    setColor(Palette::PlanetRingHighlight);
    ring(planet.position, { planet.outerRingRadiusX, planet.outerRingRadiusY });
}

void drawStation(float time, const Station& station)
{
    const float x = station.position.x;
    const float y = station.position.y;
    const float glow = station.reactorGlowBase +
        station.reactorGlowRange * std::sin(time * station.reactorGlowSpeed);
    setColor(Palette::StationOuterRing);
    ring(station.position, { station.outerRadiusX, station.outerRadiusY });
    setColor(Palette::StationBody);
    disc(Circle{ station.position, station.reactorRadius });
    setColor(Palette::StationRing);
    ring(Circle{ station.position, station.reactorRingRadius });
    setColor(Palette::StationReactor, glow);
    disc(Circle{ station.position, station.reactorGlowRadius }, station.reactorSides);

    const int moduleRows = station.moduleCount / station.moduleColumns;
    const float firstModuleX = x - static_cast<float>(station.moduleColumns - 1) * 0.5f * station.moduleSpacingX;
    float moduleY = y - static_cast<float>(moduleRows - 1) * 0.5f * station.moduleSpacingY;
    int module = 0;
    while (module < station.moduleCount) {
        float moduleX = firstModuleX;
        for (int column = 0; column < station.moduleColumns && module < station.moduleCount; ++column) {
            setColor(Palette::StationModule);
            disc({ moduleX, moduleY }, { station.moduleRadiusX, station.moduleRadiusY }, station.moduleSides);
            setColor(Palette::StationModuleRing);
            ring({ moduleX, moduleY }, { station.moduleRadiusX, station.moduleRadiusY }, station.moduleSides);
            setColor(Palette::StationModuleLight);
            disc(Circle{ { moduleX, moduleY }, station.moduleLightRadius }, station.moduleSides);
            moduleX += station.moduleSpacingX;
            ++module;
        }
        moduleY += station.moduleSpacingY;
    }

    const Vector2 beacon = beaconPosition(station, time);
    setColor(Palette::BeaconGlow);
    disc(Circle{ beacon, station.beaconGlowRadius });
    setColor(Palette::Beacon);
    disc(Circle{ beacon, station.beaconRadius }, DodecagonSides);
}

void drawDrone(const Drone& drone, float time)
{
    if (!drone.active) {
        return;
    }

    const Vector2 position = dronePosition(drone, time);
    setColor(Palette::DroneGlow);
    disc(Circle{ position, DroneSettings.glowRadius });
    setColor(Palette::DroneBody);
    disc(Circle{ position, DroneSettings.bodyRadius }, OctagonSides);
    setColor(Palette::DroneRing);
    ring(Circle{ position, DroneSettings.outlineRadius }, OctagonSides);
    setColor(Palette::DroneCore);
    disc(Circle{ position, DroneSettings.coreRadius }, OctagonSides);
}

void drawBolts(const std::vector<Bolt>& bolts)
{
    for (const Bolt& bolt : bolts) {
        glLineWidth(BoltSettings.lineWidth);
        setColor(Palette::BoltGlow);
        ring(Circle{ bolt.position, BoltSettings.glowRadius }, BoltSettings.glowSides);
        setColor(Palette::Bolt);
        glBegin(GL_LINES);
        vertex(bolt.position - bolt.velocity * BoltSettings.trailLength);
        vertex(bolt.position);
        glEnd();
    }
    glLineWidth(DefaultLineWidth);
}

void drawShuttle(const Shuttle& shuttle, float time)
{
    glPushMatrix();

    applyTransform(shuttle.transform.toAffine());

    if (shuttle.thrusting) {
        const float flame = ShuttleSettings.flameBase + ShuttleSettings.flamePulse * std::sin(time * ShuttleSettings.flameSpeed);
        setColor(Palette::ShuttleFlame);
        glBegin(GL_TRIANGLES);
        glVertex2f(ShuttleModel::FlameLeft.x, ShuttleModel::FlameLeft.y);
        glVertex2f(0.0f, ShuttleModel::FlameLeft.y + flame);
        glVertex2f(ShuttleModel::FlameRight.x, ShuttleModel::FlameRight.y);
        glEnd();
    }

    setColor(Palette::ShuttleHull);
    glBegin(GL_POLYGON);
    for (Vector2 point : ShuttleModel::Hull) {
        vertex(point);
    }
    glEnd();

    setColor(Palette::ShuttleWindow);
    glBegin(GL_QUADS);
    for (Vector2 point : ShuttleModel::Window) {
        vertex(point);
    }
    glEnd();
    setColor(Palette::ShuttleShield);
    ring(Circle{ {}, ShuttleSettings.shieldRadius });

    glPopMatrix();
}

void drawHudFrame()
{
    const Vector2 start = HudLayout::Frame.position;
    const Vector2 end = start + Vector2{ HudLayout::Frame.size.width, HudLayout::Frame.size.height };
    setColor(Palette::HudFrame);
    glBegin(GL_LINES);
    vertex(start);
    vertex({ end.x, start.y });
    vertex({ start.x, end.y });
    vertex(end);
    vertex(start);
    vertex(start + Vector2{ 0, HudFrameCornerLength });
    vertex({ end.x, start.y });
    vertex({ end.x, start.y + HudFrameCornerLength });
    glEnd();
}

void drawRadar(const State& state, float time)
{
    setColor(Palette::Radar);
    ring(HudLayout::RadarCenter, { HudLayout::RadarOuterRadiusX, HudLayout::RadarOuterRadiusY });
    ring(HudLayout::RadarCenter, { HudLayout::RadarInnerRadiusX, HudLayout::RadarInnerRadiusY });
    setColor(Palette::RadarSweep);
    glBegin(GL_LINES);
    const Transform2 sweep = { HudLayout::RadarCenter, 0,
        { HudLayout::RadarSweepRadiusX, HudLayout::RadarSweepRadiusY }, {} };
    vertex(HudLayout::RadarCenter);
    vertex(sweep.toAffine().transformPoint(Engine::rotated({ 1, 0 }, time)));
    glEnd();

    // Положение шаттла показывается относительно станции, а не края всего уровня.
    setColor(Palette::ShuttleBlip);
    const Affine2 radar = Transform2{ HudLayout::RadarCenter, 0,
        { HudLayout::RadarMapScale, HudLayout::RadarMapScale }, StationSettings.position }.toAffine();
    const Vector2 shuttleBlip = radar.transformPoint(state.shuttle.transform.position);
    disc(Circle{ shuttleBlip, HudLayout::BlipRadius }, HudLayout::FineCircleSides);
    setColor(Palette::BeaconBlip);
    const Vector2 beaconBlip = radar.transformPoint(beaconPosition(StationSettings, time));
    disc(Circle{ beaconBlip, HudLayout::BlipRadius }, HudLayout::FineCircleSides);
}

void drawStatusBars()
{
    float cellX = HudLayout::BarStartX;
    for (int cell = 0; cell < HudLayout::BarCellCount; ++cell) {
        const float shieldAlpha = cell < HudLayout::ShieldCellCount
            ? Palette::ShieldCell.alpha
            : InactiveHudAlpha;
        setColor(Palette::ShieldCell, shieldAlpha);
        rectangle({ cellX, HudLayout::BarShieldY, HudLayout::BarCellSize, HudLayout::BarCellSize });
        const float energyAlpha = cell < HudLayout::EnergyCellCount
            ? Palette::EnergyCell.alpha
            : InactiveHudAlpha;
        setColor(Palette::EnergyCell, energyAlpha);
        rectangle({ cellX, HudLayout::BarEnergyY, HudLayout::BarCellSize, HudLayout::BarCellSize });
        cellX += HudLayout::BarCellSpacing;
    }
}

void drawScore(const State& state)
{
    float lightX = HudLayout::ScoreStartX;
    for (int point = 0; point < HudLayout::ScoreLightCount; ++point) {
        const float alpha = point < state.world.score
            ? Palette::Score.alpha
            : InactiveHudAlpha;
        setColor(Palette::Score, alpha);
        disc(Circle{ { lightX, HudLayout::ScoreY }, HudLayout::ScoreLightRadius }, HudLayout::FineCircleSides);
        lightX += HudLayout::ScoreSpacing;
    }
}

void drawTarget(const State& state, bool locked)
{
    setColor(Palette::Target, locked ? Palette::Target.alpha : InactiveTargetAlpha);
    ring(Circle{ state.shuttle.transform.position, HudLayout::TargetRadius });
    if (locked) {
        glBegin(GL_LINES);
        const float x = state.shuttle.transform.position.x;
        const float y = state.shuttle.transform.position.y;
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
    const Transform2 transform = { {}, 0,
        { WorldSettings.size.width / ReferenceScreenSize.width, WorldSettings.size.height / ReferenceScreenSize.height }, {} };
    glPushMatrix();
    applyTransform(transform.toAffine());

    drawHudFrame();
    drawRadar(state, time);
    drawStatusBars();
    drawScore(state);
    drawTarget(state, locked);

    glPopMatrix();
}

void drawScene(const State& state, float time, bool portalsOpen, bool locked)
{
    glBegin(GL_QUADS);
    setColor(Palette::BackgroundTop);
    glVertex2f(0.0f, 0.0f);
    glVertex2f(WorldSettings.size.width, 0.0f);
    setColor(Palette::BackgroundBottom);
    glVertex2f(WorldSettings.size.width, WorldSettings.size.height);
    glVertex2f(0.0f, WorldSettings.size.height);
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
        Platform::isKeyDown(Platform::Key::ArrowUp) || Platform::isKeyDown(Platform::Key::W),
        Platform::isKeyDown(Platform::Key::ArrowDown) || Platform::isKeyDown(Platform::Key::S),
        Platform::isKeyDown(Platform::Key::ArrowLeft) || Platform::isKeyDown(Platform::Key::A),
        Platform::isKeyDown(Platform::Key::ArrowRight) || Platform::isKeyDown(Platform::Key::D),
        Platform::isKeyDown(Platform::Key::Space)
    };
}

void updateShuttle(Shuttle& shuttle, const InputState& input, float deltaTime)
{
    Vector2 movement = { 0.0f, 0.0f };
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
        shuttle.transform.position += Engine::normalized(movement) * ShuttleSettings.movementSpeed * deltaTime;
        shuttle.transform.rotation = std::atan2(movement.y, movement.x) + ShuttleSettings.headingOffset;
    }

    shuttle.transform.position = Engine::closestPoint(ShuttleSettings.movementBounds, shuttle.transform.position);
}

void fireBolt(Shuttle& shuttle, std::vector<Bolt>& bolts, const InputState& input, float time)
{
    if (!input.fire || time < shuttle.nextShotTime) {
        return;
    }

    const Affine2 transform = shuttle.transform.toAffine();
    const Vector2 direction = Engine::normalized(transform.transformVector({ 0, -1 }));
    Bolt bolt;
    bolt.position = transform.transformPoint({ 0, -ShuttleSettings.shotSpawnOffset });
    bolt.velocity = direction * ShuttleSettings.shotSpeed;
    bolt.secondsLeft = ShuttleSettings.shotLifetime;
    bolts.push_back(bolt);
    shuttle.nextShotTime = time + ShuttleSettings.shotCooldown;
}

void updateDrones(std::vector<Drone>& drones, float time)
{
    for (Drone& drone : drones) {
        if (!drone.active && time >= drone.respawnTime) {
            drone.active = true;
        }
    }
}

void updateBolts(std::vector<Bolt>& bolts, std::vector<Drone>& drones,
                 World& world, float time, float deltaTime)
{
    for (auto boltIterator = bolts.begin(); boltIterator != bolts.end();) {
        Bolt& bolt = *boltIterator;
        bolt.position += bolt.velocity * deltaTime;
        bolt.secondsLeft -= deltaTime;

        bool hitDrone = false;
        for (Drone& drone : drones) {
            if (!drone.active) {
                continue;
            }

            const Circle target = { dronePosition(drone, time), DroneSettings.hitRadius };
            if (Engine::contains(target, bolt.position)) {
                drone.active = false;
                drone.respawnTime = time + DroneSettings.respawnDelay;
                hitDrone = true;
                break;
            }
        }

        const bool outsideScreen = !Engine::contains(Rect{ {}, WorldSettings.size }, bolt.position);
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

void usePortals(Shuttle& shuttle, const std::array<Portal, PortalCount>& portals, World& world, float time, bool portalsOpen)
{
    if (!portalsOpen || time < world.nextPortalTime) {
        return;
    }

    for (const Portal& portal : portals) {
        if (Engine::contains(Circle{ portal.position, PortalSettings.triggerRadius }, shuttle.transform.position)) {
            shuttle.transform.position = portal.destination;
            world.nextPortalTime = time + PortalSettings.cooldown;
            return;
        }
    }
}
}

namespace
{
class CircleApplication final : public Engine::Application
{
private:
    Game::State state;
    bool portalsOpen = false;
    bool locked = false;
    bool titleDirty = true;
    std::optional<bool> verticalSync;
    bool verticalSyncFailed = false;

    bool onStart(Platform::Error&) override
    {
        // Начало координат игрового поля — сверху слева.
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0, Game::WorldSettings.size.width, Game::WorldSettings.size.height, 0,
            Game::NearDepth, Game::FarDepth);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        // Плоская сцена перекрывается в порядке рисования, без буфера глубины.
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        return true;
    }

    bool onInput(const Engine::FrameInfo&, Platform::Error&) override
    {
        if (Platform::isKeyDown(Platform::Key::Escape)) {
            requestExit();
        }
        if (Platform::isKeyPressed(Platform::Key::P)) {
            setPaused(!isPaused());
            titleDirty = true;
        }
        if (Platform::isKeyPressed(Platform::Key::V)) {
            const bool enabled = !verticalSync.value_or(false);
            Platform::Error error;
            verticalSyncFailed = !setVSync(enabled, error);
            if (verticalSyncFailed) {
                Platform::reportError(error, false);
            } else {
                verticalSync = enabled;
            }
            titleDirty = true;
        }
        return true;
    }

    bool onUpdate(const Engine::FrameInfo& frame, Platform::Error&) override
    {
        // Движение, перезарядка, восстановление дронов и порталы используют
        // одно игровое время: все они останавливаются и продолжаются вместе.
        const float time = static_cast<float>(frame.elapsedTime);
        const Game::InputState input = Game::readInput();
        Game::updateShuttle(state.shuttle, input, frame.deltaTime);
        Game::fireBolt(state.shuttle, state.bolts, input, time);
        Game::updateDrones(state.drones, time);
        Game::updateBolts(state.bolts, state.drones, state.world, time, frame.deltaTime);
        const float portalCycle = std::fmod(time, Game::WorldSettings.portalCycleDuration);
        portalsOpen = portalCycle > Game::WorldSettings.portalOpenTime &&
            portalCycle < Game::WorldSettings.portalCloseTime;
        Game::usePortals(state.shuttle, state.portals, state.world, time, portalsOpen);
        return true;
    }

    bool onPrepareFrame(const Engine::FrameInfo& frame, Platform::Error&) override
    {
        locked = Engine::contains(Engine::Circle{ Game::StationSettings.position, Game::StationSettings.lockRadius },
            state.shuttle.transform.position);
        if (frame.fpsUpdated || titleDirty) {
            const int fps = static_cast<int>(frame.framesPerSecond + 0.5);
            const std::string syncStatus = verticalSyncFailed ? "error" :
                (verticalSync.has_value() ? (*verticalSync ? "on" : "off") : "driver");
            const std::string title = "ORBITAL // SECTOR 09 | FPS: " + std::to_string(fps) +
                (frame.paused ? " | PAUSED" : "") + " | VSync: " + syncStatus + " | P: pause | V: VSync";
            Platform::Error error;
            if (!Platform::setWindowTitle(title, error)) {
                Platform::reportError(error, false);
            }
            titleDirty = false;
        }
        return true;
    }

    bool onRender(const Engine::FrameInfo& frame, Platform::Error&) override
    {
        glClearColor(Game::Palette::Clear.red, Game::Palette::Clear.green,
            Game::Palette::Clear.blue, Game::Palette::Clear.alpha);
        glClear(GL_COLOR_BUFFER_BIT);
        glLoadIdentity();
        Game::drawScene(state, static_cast<float>(frame.elapsedTime), portalsOpen, locked);
        return true;
    }
};
}

int Game::run()
{
    Engine::ApplicationOptions options;
    options.window.width = static_cast<int>(WorldSettings.size.width);
    options.window.height = static_cast<int>(WorldSettings.size.height);
    options.window.resizable = false;
    options.window.title = "ORBITAL // SECTOR 09";
    options.maxDeltaTime = WorldSettings.maxFrameTime;
    options.targetFramesPerSecond = 120.0;
    CircleApplication application;
    std::vector<Platform::Error> errors;
    const bool success = application.run(options, errors);
    bool showDialog = true;
    for (const Platform::Error& error : errors) {
        Platform::reportError(error, showDialog);
        showDialog = false;
    }
    return success ? 0 : 1;
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
