#include "BodyFactory.hpp"

#include <cmath>

namespace bag {
namespace {

bool finite(Vec3 value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

DataResult<Body> validateAndCreate(const std::string& id, const std::string& name, const std::string& type,
                                   double massKg, double radiusM, Vec3 positionM, Vec3 velocityMps) {
    if (id.empty() || name.empty() || type.empty()) return DataResult<Body>::failure("body id, name, and type are required");
    if (!std::isfinite(massKg) || massKg <= 0.0) return DataResult<Body>::failure("body mass_kg must be finite and positive");
    if (!std::isfinite(radiusM) || radiusM <= 0.0) return DataResult<Body>::failure("body radius_m must be finite and positive");
    if (!finite(positionM) || !finite(velocityMps)) return DataResult<Body>::failure("body position and velocity must be finite");

    Body body;
    body.id = id;
    body.name = name;
    body.type = type;
    body.mass = massKg;
    body.realRadius = radiusM;
    body.position = positionM;
    body.velocity = velocityMps;
    return DataResult<Body>::success(std::move(body));
}

} // namespace

DataResult<Body> BodyFactory::create(const BodyDefinition& definition) {
    DataResult<Body> result = validateAndCreate(definition.id, definition.name, definition.type,
                                                definition.massKg, definition.radiusM,
                                                definition.initialPositionM, definition.initialVelocityMps);
    if (!result) return result;
    Body body = *result.value;
    body.color = definition.color;
    body.accent = definition.accent;
    body.radius = definition.displayRadiusPx;
    body.luminous = definition.luminous;
    body.ringed = definition.ringed;
    body.parentId = definition.parentId;
    body.semiMajorAxis = definition.semiMajorAxisM;
    body.eccentricity = definition.eccentricity;
    body.orbitalPeriod = definition.orbitalPeriodS;
    body.rotationPeriod = definition.rotationPeriodS;
    body.axialTilt = definition.axialTiltDeg;
    return DataResult<Body>::success(std::move(body));
}

DataResult<Body> BodyFactory::createCustom(const CustomBodyData& data) {
    DataResult<Body> result = validateAndCreate(data.id, data.name, data.type, data.massKg, data.radiusM,
                                                data.positionM, data.velocityMps);
    if (!result) return result;
    Body body = *result.value;
    body.radius = 6.0;
    body.color = {180, 200, 220, 255};
    body.accent = {100, 220, 255, 255};
    body.userCreated = true;
    return DataResult<Body>::success(std::move(body));
}

} // namespace bag
