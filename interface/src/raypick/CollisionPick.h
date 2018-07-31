//
//  Created by Sabrina Shanman 7/11/2018
//  Copyright 2018 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//
#ifndef hifi_CollisionPick_h
#define hifi_CollisionPick_h

#include <PhysicsEngine.h>
#include <model-networking/ModelCache.h>
#include <RegisteredMetaTypes.h>
#include <Pick.h>

class CollisionPickResult : public PickResult {
public:
    CollisionPickResult() {}
    CollisionPickResult(const QVariantMap& pickVariant) : PickResult(pickVariant) {}

    CollisionPickResult(const CollisionRegion& searchRegion, const std::vector<ContactTestResult>& entityIntersections, const std::vector<ContactTestResult>& avatarIntersections) :
        PickResult(searchRegion.toVariantMap()),
        intersects(entityIntersections.size() || avatarIntersections.size()),
        entityIntersections(entityIntersections),
        avatarIntersections(avatarIntersections) {
    }

    CollisionPickResult(const CollisionPickResult& collisionPickResult) : PickResult(collisionPickResult.pickVariant) {
        avatarIntersections = collisionPickResult.avatarIntersections;
        entityIntersections = collisionPickResult.entityIntersections;
        intersects = avatarIntersections.size() || entityIntersections.size();
    }

    bool intersects { false };
    std::vector<ContactTestResult> entityIntersections;
    std::vector<ContactTestResult> avatarIntersections;

    QVariantMap CollisionPickResult::toVariantMap() const override;

    bool doesIntersect() const override { return intersects; }
    bool checkOrFilterAgainstMaxDistance(float maxDistance) override { return true; }

    PickResultPointer compareAndProcessNewResult(const PickResultPointer& newRes) override {
        const std::shared_ptr<CollisionPickResult> newCollisionResult = std::static_pointer_cast<CollisionPickResult>(newRes);

        for (ContactTestResult& entityIntersection : newCollisionResult->entityIntersections) {
            entityIntersections.push_back(entityIntersection);
        }
        for (ContactTestResult& avatarIntersection : newCollisionResult->avatarIntersections) {
            avatarIntersections.push_back(avatarIntersection);
        }

        intersects = entityIntersections.size() || avatarIntersections.size();

        return std::make_shared<CollisionPickResult>(*this);
    }
};

class CollisionPick : public Pick<CollisionRegion> {
public:
    CollisionPick(const PickFilter& filter, float maxDistance, bool enabled, CollisionRegion collisionRegion, PhysicsEnginePointer physicsEngine) :
        Pick(filter, maxDistance, enabled),
        _mathPick(collisionRegion),
        _physicsEngine(physicsEngine) {
    }

    CollisionRegion getMathematicalPick() const override;
    PickResultPointer getDefaultResult(const QVariantMap& pickVariant) const override { return std::make_shared<CollisionPickResult>(pickVariant); }
    PickResultPointer getEntityIntersection(const CollisionRegion& pick) override;
    PickResultPointer getOverlayIntersection(const CollisionRegion& pick) override;
    PickResultPointer getAvatarIntersection(const CollisionRegion& pick) override;
    PickResultPointer getHUDIntersection(const CollisionRegion& pick) override;

protected:
    // Returns true if pick.shapeInfo is valid. Otherwise, attempts to get the shapeInfo ready for use.
    bool isShapeInfoReady(CollisionRegion& pick);
    void computeShapeInfo(CollisionRegion& pick, ShapeInfo& shapeInfo, QSharedPointer<GeometryResource> resource);

    CollisionRegion _mathPick;
    PhysicsEnginePointer _physicsEngine;
    QSharedPointer<GeometryResource> _cachedResource;
};

#endif // hifi_CollisionPick_h