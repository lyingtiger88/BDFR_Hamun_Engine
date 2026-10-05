#include <Hamun/World/World.hpp>

#include <algorithm>
#include <utility>

namespace Hamun::World {

EntityId World::CreateEntity(
    std::string name,
    EntityId parent)
{
    if (parent != InvalidEntity &&
        !Find(parent)) {
        return InvalidEntity;
    }

    EntityRecord entity;
    entity.id =
        nextEntityId_++;
    entity.parent =
        parent;
    entity.name =
        std::move(name);

    entities_.push_back(
        std::move(entity));

    return entities_.back().id;
}

bool World::DestroyEntity(
    EntityId entity)
{
    if (!Find(entity))
        return false;

    std::vector<EntityId>
        pending{
            entity
        };

    for (std::size_t i = 0;
         i < pending.size();
         ++i) {
        const EntityId parent =
            pending[i];

        for (const EntityRecord& record :
             entities_) {
            if (record.parent ==
                parent) {
                pending.push_back(
                    record.id);
            }
        }
    }

    entities_.erase(
        std::remove_if(
            entities_.begin(),
            entities_.end(),
            [&](const EntityRecord& record) {
                return
                    std::find(
                        pending.begin(),
                        pending.end(),
                        record.id) !=
                    pending.end();
            }),
        entities_.end());

    return true;
}

bool World::WouldCreateCycle(
    EntityId entity,
    EntityId parent) const noexcept
{
    EntityId current =
        parent;

    while (current !=
           InvalidEntity) {
        if (current ==
            entity) {
            return true;
        }

        const EntityRecord* record =
            Find(current);

        if (!record)
            break;

        current =
            record->parent;
    }

    return false;
}

bool World::SetParent(
    EntityId entity,
    EntityId parent)
{
    EntityRecord* record =
        Find(entity);

    if (!record)
        return false;

    if (parent != InvalidEntity &&
        !Find(parent)) {
        return false;
    }

    if (entity == parent ||
        WouldCreateCycle(
            entity,
            parent)) {
        return false;
    }

    record->parent =
        parent;

    return true;
}

EntityRecord* World::Find(
    EntityId entity) noexcept
{
    const auto iterator =
        std::find_if(
            entities_.begin(),
            entities_.end(),
            [&](const EntityRecord& record) {
                return
                    record.id ==
                    entity;
            });

    return iterator ==
        entities_.end()
            ? nullptr
            : &*iterator;
}

const EntityRecord* World::Find(
    EntityId entity) const noexcept
{
    const auto iterator =
        std::find_if(
            entities_.begin(),
            entities_.end(),
            [&](const EntityRecord& record) {
                return
                    record.id ==
                    entity;
            });

    return iterator ==
        entities_.end()
            ? nullptr
            : &*iterator;
}

void World::Clear() noexcept
{
    entities_.clear();
    nextEntityId_ = 1;
}

} // namespace Hamun::World
