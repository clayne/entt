#ifndef ENTT_ENTITY_REGISTRY_HPP
#define ENTT_ENTITY_REGISTRY_HPP

#include <algorithm>
#include <cstddef>
#include <functional>
#include <iterator>
#include <memory>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>
#include "../config/config.h"
#include "../container/dense_map.hpp"
#include "../core/algorithm.hpp"
#include "../core/any.hpp"
#include "../core/fwd.hpp"
#include "../core/iterator.hpp"
#include "../core/memory.hpp"
#include "../core/type_info.hpp"
#include "../core/type_traits.hpp"
#include "../core/utility.hpp"
#include "entity.hpp"
#include "fwd.hpp"
#include "group.hpp"
#include "mixin.hpp"
#include "sparse_set.hpp"
#include "storage.hpp"
#include "view.hpp"

namespace entt {

/**
 * @cond TURN_OFF_DOXYGEN
 * Internal details not to be documented.
 */

namespace internal {

template<typename It>
class registry_storage_iterator final {
    template<typename Other>
    friend class registry_storage_iterator;

    using mapped_type = std::remove_reference_t<decltype(std::declval<It>()->second)>;

public:
    using value_type = std::pair<id_type, constness_as_t<typename mapped_type::element_type, mapped_type> &>;
    using pointer = input_iterator_pointer<value_type>;
    using reference = value_type;
    using difference_type = std::ptrdiff_t;
    using iterator_category = std::input_iterator_tag;

    constexpr registry_storage_iterator() noexcept
        : it{} {}

    constexpr registry_storage_iterator(It iter) noexcept
        : it{iter} {}

    template<typename Other, typename = std::enable_if_t<!std::is_same_v<It, Other> && std::is_constructible_v<It, Other>>>
    constexpr registry_storage_iterator(const registry_storage_iterator<Other> &other) noexcept
        : registry_storage_iterator{other.it} {}

    constexpr registry_storage_iterator &operator++() noexcept {
        return ++it, *this;
    }

    constexpr registry_storage_iterator operator++(int) noexcept {
        registry_storage_iterator orig = *this;
        return ++(*this), orig;
    }

    constexpr registry_storage_iterator &operator--() noexcept {
        return --it, *this;
    }

    constexpr registry_storage_iterator operator--(int) noexcept {
        registry_storage_iterator orig = *this;
        return operator--(), orig;
    }

    constexpr registry_storage_iterator &operator+=(const difference_type value) noexcept {
        it += value;
        return *this;
    }

    constexpr registry_storage_iterator operator+(const difference_type value) const noexcept {
        registry_storage_iterator copy = *this;
        return (copy += value);
    }

    constexpr registry_storage_iterator &operator-=(const difference_type value) noexcept {
        return (*this += -value);
    }

    constexpr registry_storage_iterator operator-(const difference_type value) const noexcept {
        return (*this + -value);
    }

    [[nodiscard]] constexpr reference operator[](const difference_type value) const noexcept {
        return {it[value].first, *it[value].second};
    }

    [[nodiscard]] constexpr reference operator*() const noexcept {
        return {it->first, *it->second};
    }

    [[nodiscard]] constexpr pointer operator->() const noexcept {
        return operator*();
    }

    template<typename Lhs, typename Rhs>
    friend constexpr std::ptrdiff_t operator-(const registry_storage_iterator<Lhs> &, const registry_storage_iterator<Rhs> &) noexcept;

    template<typename Lhs, typename Rhs>
    friend constexpr bool operator==(const registry_storage_iterator<Lhs> &, const registry_storage_iterator<Rhs> &) noexcept;

    template<typename Lhs, typename Rhs>
    friend constexpr bool operator<(const registry_storage_iterator<Lhs> &, const registry_storage_iterator<Rhs> &) noexcept;

private:
    It it;
};

template<typename Lhs, typename Rhs>
[[nodiscard]] constexpr std::ptrdiff_t operator-(const registry_storage_iterator<Lhs> &lhs, const registry_storage_iterator<Rhs> &rhs) noexcept {
    return lhs.it - rhs.it;
}

template<typename Lhs, typename Rhs>
[[nodiscard]] constexpr bool operator==(const registry_storage_iterator<Lhs> &lhs, const registry_storage_iterator<Rhs> &rhs) noexcept {
    return lhs.it == rhs.it;
}

template<typename Lhs, typename Rhs>
[[nodiscard]] constexpr bool operator!=(const registry_storage_iterator<Lhs> &lhs, const registry_storage_iterator<Rhs> &rhs) noexcept {
    return !(lhs == rhs);
}

template<typename Lhs, typename Rhs>
[[nodiscard]] constexpr bool operator<(const registry_storage_iterator<Lhs> &lhs, const registry_storage_iterator<Rhs> &rhs) noexcept {
    return lhs.it < rhs.it;
}

template<typename Lhs, typename Rhs>
[[nodiscard]] constexpr bool operator>(const registry_storage_iterator<Lhs> &lhs, const registry_storage_iterator<Rhs> &rhs) noexcept {
    return rhs < lhs;
}

template<typename Lhs, typename Rhs>
[[nodiscard]] constexpr bool operator<=(const registry_storage_iterator<Lhs> &lhs, const registry_storage_iterator<Rhs> &rhs) noexcept {
    return !(lhs > rhs);
}

template<typename Lhs, typename Rhs>
[[nodiscard]] constexpr bool operator>=(const registry_storage_iterator<Lhs> &lhs, const registry_storage_iterator<Rhs> &rhs) noexcept {
    return !(lhs < rhs);
}

template<typename Allocator>
class registry_context {
    using alloc_traits = std::allocator_traits<Allocator>;
    using allocator_type = typename alloc_traits::template rebind_alloc<std::pair<const id_type, basic_any<0u>>>;

public:
    explicit registry_context(const allocator_type &allocator)
        : ctx{allocator} {}

    template<typename Type, typename... Args>
    Type &emplace_as(const id_type id, Args &&...args) {
        return any_cast<Type &>(ctx.try_emplace(id, std::in_place_type<Type>, std::forward<Args>(args)...).first->second);
    }

    template<typename Type, typename... Args>
    Type &emplace(Args &&...args) {
        return emplace_as<Type>(type_id<Type>().hash(), std::forward<Args>(args)...);
    }

    template<typename Type>
    Type &insert_or_assign(const id_type id, Type &&value) {
        return any_cast<std::remove_cv_t<std::remove_reference_t<Type>> &>(ctx.insert_or_assign(id, std::forward<Type>(value)).first->second);
    }

    template<typename Type>
    Type &insert_or_assign(Type &&value) {
        return insert_or_assign(type_id<Type>().hash(), std::forward<Type>(value));
    }

    template<typename Type>
    bool erase(const id_type id = type_id<Type>().hash()) {
        const auto it = ctx.find(id);
        return it != ctx.end() && it->second.type() == type_id<Type>() ? (ctx.erase(it), true) : false;
    }

    template<typename Type>
    [[nodiscard]] const Type &get(const id_type id = type_id<Type>().hash()) const {
        return any_cast<const Type &>(ctx.at(id));
    }

    template<typename Type>
    [[nodiscard]] Type &get(const id_type id = type_id<Type>().hash()) {
        return any_cast<Type &>(ctx.at(id));
    }

    template<typename Type>
    [[nodiscard]] const Type *find(const id_type id = type_id<Type>().hash()) const {
        const auto it = ctx.find(id);
        return it != ctx.cend() ? any_cast<const Type>(&it->second) : nullptr;
    }

    template<typename Type>
    [[nodiscard]] Type *find(const id_type id = type_id<Type>().hash()) {
        const auto it = ctx.find(id);
        return it != ctx.end() ? any_cast<Type>(&it->second) : nullptr;
    }

    template<typename Type>
    [[nodiscard]] bool contains(const id_type id = type_id<Type>().hash()) const {
        const auto it = ctx.find(id);
        return it != ctx.cend() && it->second.type() == type_id<Type>();
    }

private:
    dense_map<id_type, basic_any<0u>, identity, std::equal_to<id_type>, allocator_type> ctx;
};

} // namespace internal

/**
 * Internal details not to be documented.
 * @endcond
 */

/**
 * @brief Fast and reliable entity-component system.
 * @tparam Entity A valid entity type.
 * @tparam Allocator Type of allocator used to manage memory and elements.
 */
template<typename Entity, typename Allocator>
class basic_registry {
    using base_type = basic_sparse_set<Entity, Allocator>;

    using alloc_traits = std::allocator_traits<Allocator>;
    static_assert(std::is_same_v<typename alloc_traits::value_type, Entity>, "Invalid value type");

    template<typename Type>
    using storage_for_type = typename storage_for<Type, Entity, typename alloc_traits::template rebind_alloc<std::remove_const_t<Type>>>::type;

    // std::shared_ptr because of its type erased allocator which is useful here
    using pool_container_type = dense_map<id_type, std::shared_ptr<base_type>, identity, std::equal_to<id_type>, typename alloc_traits::template rebind_alloc<std::pair<const id_type, std::shared_ptr<base_type>>>>;
    using owning_group_container_type = dense_map<id_type, std::shared_ptr<internal::owning_group_descriptor>, identity, std::equal_to<id_type>, typename alloc_traits::template rebind_alloc<std::pair<const id_type, std::shared_ptr<internal::owning_group_descriptor>>>>;
    using non_owning_group_container_type = dense_map<id_type, std::shared_ptr<void>, identity, std::equal_to<id_type>, typename alloc_traits::template rebind_alloc<std::pair<const id_type, std::shared_ptr<void>>>>;

    template<typename Type>
    [[nodiscard]] auto &assure(const id_type id = type_hash<Type>::value()) {
        static_assert(std::is_same_v<Type, std::decay_t<Type>>, "Non-decayed types not allowed");
        auto &cpool = pools[id];

        if(!cpool) {
            using alloc_type = typename storage_for_type<std::remove_const_t<Type>>::allocator_type;

            if constexpr(std::is_same_v<Type, void> && !std::is_constructible_v<alloc_type, allocator_type>) {
                // std::allocator<void> has no cross constructors (waiting for C++20)
                cpool = std::allocate_shared<storage_for_type<std::remove_const_t<Type>>>(get_allocator(), alloc_type{});
            } else {
                cpool = std::allocate_shared<storage_for_type<std::remove_const_t<Type>>>(get_allocator(), get_allocator());
            }

            cpool->bind(forward_as_any(*this));
        }

        ENTT_ASSERT(cpool->type() == type_id<Type>(), "Unexpected type");
        return static_cast<storage_for_type<Type> &>(*cpool);
    }

    template<typename Type>
    [[nodiscard]] const auto &assure(const id_type id = type_hash<Type>::value()) const {
        static_assert(std::is_same_v<Type, std::decay_t<Type>>, "Non-decayed types not allowed");

        if(const auto it = pools.find(id); it != pools.cend()) {
            ENTT_ASSERT(it->second->type() == type_id<Type>(), "Unexpected type");
            return static_cast<const storage_for_type<Type> &>(*it->second);
        }

        static storage_for_type<Type> placeholder{};
        return placeholder;
    }

    void rebind() {
        for(auto &&curr: pools) {
            curr.second->bind(forward_as_any(*this));
        }
    }

public:
    /*! @brief Entity traits. */
    using traits_type = typename base_type::traits_type;
    /*! @brief Allocator type. */
    using allocator_type = Allocator;
    /*! @brief Underlying entity identifier. */
    using entity_type = typename traits_type::value_type;
    /*! @brief Underlying version type. */
    using version_type = typename traits_type::version_type;
    /*! @brief Unsigned integer type. */
    using size_type = std::size_t;
    /*! @brief Common type among all storage types. */
    using common_type = base_type;
    /*! @brief Context type. */
    using context = internal::registry_context<allocator_type>;

    /*! @brief Default constructor. */
    basic_registry()
        : basic_registry{allocator_type{}} {}

    /**
     * @brief Constructs an empty registry with a given allocator.
     * @param allocator The allocator to use.
     */
    explicit basic_registry(const allocator_type &allocator)
        : basic_registry{0u, allocator} {}

    /**
     * @brief Allocates enough memory upon construction to store `count` pools.
     * @param count The number of pools to allocate memory for.
     * @param allocator The allocator to use.
     */
    basic_registry(const size_type count, const allocator_type &allocator = allocator_type{})
        : vars{allocator},
          pools{allocator},
          owning_groups{allocator},
          non_owning_groups{allocator},
          shortcut{&assure<entity_type>()} {
        pools.reserve(count);
        rebind();
    }

    /**
     * @brief Move constructor.
     * @param other The instance to move from.
     */
    basic_registry(basic_registry &&other) noexcept
        : vars{std::move(other.vars)},
          pools{std::move(other.pools)},
          owning_groups{std::move(other.owning_groups)},
          non_owning_groups{std::move(other.non_owning_groups)},
          shortcut{std::move(other.shortcut)} {
        rebind();
    }

    /**
     * @brief Move assignment operator.
     * @param other The instance to move from.
     * @return This registry.
     */
    basic_registry &operator=(basic_registry &&other) noexcept {
        vars = std::move(other.vars);
        pools = std::move(other.pools);
        owning_groups = std::move(other.owning_groups);
        non_owning_groups = std::move(other.non_owning_groups);
        shortcut = std::move(other.shortcut);

        rebind();

        return *this;
    }

    /**
     * @brief Exchanges the contents with those of a given registry.
     * @param other Registry to exchange the content with.
     */
    void swap(basic_registry &other) {
        using std::swap;

        swap(vars, other.vars);
        swap(pools, other.pools);
        swap(owning_groups, other.owning_groups);
        swap(non_owning_groups, other.non_owning_groups);
        swap(shortcut, other.shortcut);

        rebind();
        other.rebind();
    }

    /**
     * @brief Returns the associated allocator.
     * @return The associated allocator.
     */
    [[nodiscard]] constexpr allocator_type get_allocator() const noexcept {
        return pools.get_allocator();
    }

    /**
     * @brief Returns an iterable object to use to _visit_ a registry.
     *
     * The iterable object returns a pair that contains the name and a reference
     * to the current storage.
     *
     * @return An iterable object to use to _visit_ the registry.
     */
    [[nodiscard]] auto storage() noexcept {
        return iterable_adaptor{internal::registry_storage_iterator{pools.begin()}, internal::registry_storage_iterator{pools.end()}};
    }

    /*! @copydoc storage */
    [[nodiscard]] auto storage() const noexcept {
        return iterable_adaptor{internal::registry_storage_iterator{pools.cbegin()}, internal::registry_storage_iterator{pools.cend()}};
    }

    /**
     * @brief Finds the storage associated with a given name, if any.
     * @param id Name used to map the storage within the registry.
     * @return A pointer to the storage if it exists, a null pointer otherwise.
     */
    [[nodiscard]] common_type *storage(const id_type id) {
        return const_cast<common_type *>(std::as_const(*this).storage(id));
    }

    /**
     * @brief Finds the storage associated with a given name, if any.
     * @param id Name used to map the storage within the registry.
     * @return A pointer to the storage if it exists, a null pointer otherwise.
     */
    [[nodiscard]] const common_type *storage(const id_type id) const {
        const auto it = pools.find(id);
        return it == pools.cend() ? nullptr : it->second.get();
    }

    /**
     * @brief Returns the storage for a given component type.
     * @tparam Type Type of component of which to return the storage.
     * @param id Optional name used to map the storage within the registry.
     * @return The storage for the given component type.
     */
    template<typename Type>
    decltype(auto) storage(const id_type id = type_hash<Type>::value()) {
        return assure<Type>(id);
    }

    /**
     * @brief Returns the storage for a given component type.
     *
     * @warning
     * If a storage for the given component doesn't exist yet, a temporary
     * placeholder is returned instead.
     *
     * @tparam Type Type of component of which to return the storage.
     * @param id Optional name used to map the storage within the registry.
     * @return The storage for the given component type.
     */
    template<typename Type>
    decltype(auto) storage(const id_type id = type_hash<Type>::value()) const {
        return assure<Type>(id);
    }

    /**
     * @brief Returns the number of entities created so far.
     * @return Number of entities created so far.
     */
    [[deprecated("use .storage<Entity>().size() instead")]] [[nodiscard]] size_type size() const noexcept {
        return shortcut->size();
    }

    /**
     * @brief Returns the number of entities still in use.
     * @return Number of entities still in use.
     */
    [[deprecated("use .storage<Entity>().in_use() instead")]] [[nodiscard]] size_type alive() const {
        return shortcut->in_use();
    }

    /**
     * @brief Increases the capacity (number of entities) of the registry.
     * @param cap Desired capacity.
     */
    [[deprecated("use .storage<Entity>().reserve(cap) instead")]] void reserve(const size_type cap) {
        shortcut->reserve(cap);
    }

    /**
     * @brief Returns the number of entities that a registry has currently
     * allocated space for.
     * @return Capacity of the registry.
     */
    [[deprecated("use .storage<Entity>().capacity() instead")]] [[nodiscard]] size_type capacity() const noexcept {
        return shortcut->capacity();
    }

    /**
     * @brief Checks whether the registry is empty (no entities still in use).
     * @return True if the registry is empty, false otherwise.
     */
    [[deprecated("use .storage<Entity>().in_use() instead")]] [[nodiscard]] bool empty() const {
        return !alive();
    }

    /**
     * @brief Direct access to the list of entities of a registry.
     *
     * The returned pointer is such that range `[data(), data() + size())` is
     * always a valid range, even if the registry is empty.
     *
     * @warning
     * This list contains both valid and destroyed entities and isn't suitable
     * for direct use.
     *
     * @return A pointer to the array of entities.
     */
    [[deprecated("use .storage<Entity>().data() instead")]] [[nodiscard]] const entity_type *data() const noexcept {
        return shortcut->data();
    }

    /**
     * @brief Returns the number of released entities.
     * @return The number of released entities.
     */
    [[deprecated("use .storage<Entity>().size() and .storage<Entity>().in_use() instead")]] [[nodiscard]] size_type released() const noexcept {
        return (shortcut->size() - shortcut->in_use());
    }

    /**
     * @brief Checks if an identifier refers to a valid entity.
     * @param entt An identifier, either valid or not.
     * @return True if the identifier is valid, false otherwise.
     */
    [[nodiscard]] bool valid(const entity_type entt) const {
        return shortcut->contains(entt);
    }

    /**
     * @brief Returns the actual version for an identifier.
     * @param entt A valid identifier.
     * @return The version for the given identifier if valid, the tombstone
     * version otherwise.
     */
    [[nodiscard]] version_type current(const entity_type entt) const {
        return shortcut->current(entt);
    }

    /**
     * @brief Creates a new entity or recycles a destroyed one.
     * @return A valid identifier.
     */
    [[nodiscard]] entity_type create() {
        return shortcut->emplace();
    }

    /**
     * @copybrief create
     *
     * If the requested entity isn't in use, the suggested identifier is used.
     * Otherwise, a new identifier is generated.
     *
     * @param hint Required identifier.
     * @return A valid identifier.
     */
    [[nodiscard]] entity_type create(const entity_type hint) {
        return shortcut->emplace(hint);
    }

    /**
     * @brief Assigns each element in a range an identifier.
     *
     * @sa create
     *
     * @tparam It Type of forward iterator.
     * @param first An iterator to the first element of the range to generate.
     * @param last An iterator past the last element of the range to generate.
     */
    template<typename It>
    void create(It first, It last) {
        shortcut->insert(std::move(first), std::move(last));
    }

    /**
     * @brief Assigns identifiers to an empty registry.
     *
     * This function is intended for use in conjunction with `data`, `size` and
     * `released`.<br/>
     * Don't try to inject ranges of randomly generated entities nor the _wrong_
     * head for the list of destroyed entities. There is no guarantee that a
     * registry will continue to work properly in this case.
     *
     * @warning
     * There must be no entities still alive for this to work properly.
     *
     * @tparam It Type of input iterator.
     * @param first An iterator to the first element of the range of entities.
     * @param last An iterator past the last element of the range of entities.
     * @param destroyed The number of released entities.
     */
    template<typename It>
    void assign(It first, It last, const size_type destroyed) {
        ENTT_ASSERT(!shortcut->in_use(), "Non-empty registry");
        shortcut->push(first, last);
        shortcut->in_use(shortcut->size() - destroyed);
    }

    /**
     * @brief Releases an identifier.
     *
     * The version is updated and the identifier can be recycled at any time.
     *
     * @warning
     * Attempting to use an invalid entity results in undefined behavior.
     *
     * @param entt A valid identifier.
     * @return The version of the recycled entity.
     */
    [[deprecated("use .orphan(entt) and .storage<Entity>().erase(entt) instead")]] version_type release(const entity_type entt) {
        ENTT_ASSERT(orphan(entt), "Non-orphan entity");
        shortcut->erase(entt);
        return shortcut->current(entt);
    }

    /**
     * @brief Releases an identifier.
     *
     * The suggested version or the valid version closest to the suggested one
     * is used instead of the implicitly generated version.
     *
     * @sa release
     *
     * @param entt A valid identifier.
     * @param version A desired version upon destruction.
     * @return The version actually assigned to the entity.
     */
    [[deprecated("use .orphan(entt), then .storage<Entity>().erase(entt)/.bump(next) instead")]] version_type release(const entity_type entt, const version_type version) {
        ENTT_ASSERT(orphan(entt), "Non-orphan entity");
        shortcut->erase(entt);
        const auto elem = traits_type::construct(traits_type::to_entity(entt), version);
        return shortcut->bump((elem == tombstone) ? traits_type::next(elem) : elem);
    }

    /**
     * @brief Releases all identifiers in a range.
     *
     * @sa release
     *
     * @tparam It Type of input iterator.
     * @param first An iterator to the first element of the range of entities.
     * @param last An iterator past the last element of the range of entities.
     */
    template<typename It>
    [[deprecated("use .orphan(entt) and .storage<Entity>().erase(first, last) instead")]] void release(It first, It last) {
        ENTT_ASSERT(std::all_of(first, last, [this](const auto entt) { return orphan(entt); }), "Non-orphan entity");
        shortcut->erase(std::move(first), std::move(last));
    }

    /**
     * @brief Destroys an entity and releases its identifier.
     *
     * @sa release
     *
     * @warning
     * Adding or removing components to an entity that is being destroyed can
     * result in undefined behavior. Attempting to use an invalid entity results
     * in undefined behavior.
     *
     * @param entt A valid identifier.
     * @return The version of the recycled entity.
     */
    version_type destroy(const entity_type entt) {
        return destroy(entt, traits_type::to_version(traits_type::next(entt)));
    }

    /**
     * @brief Destroys an entity and releases its identifier.
     *
     * The suggested version or the valid version closest to the suggested one
     * is used instead of the implicitly generated version.
     *
     * @sa destroy
     *
     * @param entt A valid identifier.
     * @param version A desired version upon destruction.
     * @return The version actually assigned to the entity.
     */
    version_type destroy(const entity_type entt, const version_type version) {
        ENTT_ASSERT(!pools.empty() && (pools.begin()->second.get() == shortcut), "Misplaced entity pool");
        ENTT_ASSERT(shortcut->contains(entt), "Invalid entity");

        for(size_type pos = pools.size(); pos; --pos) {
            pools.begin()[pos - 1u].second->remove(entt);
        }

        const auto elem = traits_type::construct(traits_type::to_entity(entt), version);
        return shortcut->bump((elem == tombstone) ? traits_type::next(elem) : elem);
    }

    /**
     * @brief Destroys all entities in a range and releases their identifiers.
     *
     * @sa destroy
     *
     * @tparam It Type of input iterator.
     * @param first An iterator to the first element of the range of entities.
     * @param last An iterator past the last element of the range of entities.
     */
    template<typename It>
    void destroy(It first, It last) {
        ENTT_ASSERT(!pools.empty() && (pools.begin()->second.get() == shortcut), "Misplaced entity pool");
        const auto from = shortcut->each().cbegin().base();
        const auto to = from + shortcut->pack(first, last);

        for(size_type pos = pools.size(); pos; --pos) {
            pools.begin()[pos - 1u].second->remove(from, to);
        }
    }

    /**
     * @brief Assigns the given component to an entity.
     *
     * The component must have a proper constructor or be of aggregate type.
     *
     * @warning
     * Attempting to assign a component to an entity that already owns it
     * results in undefined behavior.
     *
     * @tparam Type Type of component to create.
     * @tparam Args Types of arguments to use to construct the component.
     * @param entt A valid identifier.
     * @param args Parameters to use to initialize the component.
     * @return A reference to the newly created component.
     */
    template<typename Type, typename... Args>
    decltype(auto) emplace(const entity_type entt, Args &&...args) {
        return assure<Type>().emplace(entt, std::forward<Args>(args)...);
    }

    /**
     * @brief Assigns each entity in a range the given component.
     *
     * @sa emplace
     *
     * @tparam Type Type of component to create.
     * @tparam It Type of input iterator.
     * @param first An iterator to the first element of the range of entities.
     * @param last An iterator past the last element of the range of entities.
     * @param value An instance of the component to assign.
     */
    template<typename Type, typename It>
    void insert(It first, It last, const Type &value = {}) {
        assure<Type>().insert(std::move(first), std::move(last), value);
    }

    /**
     * @brief Assigns each entity in a range the given components.
     *
     * @sa emplace
     *
     * @tparam Type Type of component to create.
     * @tparam EIt Type of input iterator.
     * @tparam CIt Type of input iterator.
     * @param first An iterator to the first element of the range of entities.
     * @param last An iterator past the last element of the range of entities.
     * @param from An iterator to the first element of the range of components.
     */
    template<typename Type, typename EIt, typename CIt, typename = std::enable_if_t<std::is_same_v<typename std::iterator_traits<CIt>::value_type, Type>>>
    void insert(EIt first, EIt last, CIt from) {
        assure<Type>().insert(first, last, from);
    }

    /**
     * @brief Assigns or replaces the given component for an entity.
     *
     * @sa emplace
     * @sa replace
     *
     * @tparam Type Type of component to assign or replace.
     * @tparam Args Types of arguments to use to construct the component.
     * @param entt A valid identifier.
     * @param args Parameters to use to initialize the component.
     * @return A reference to the newly created component.
     */
    template<typename Type, typename... Args>
    decltype(auto) emplace_or_replace(const entity_type entt, Args &&...args) {
        if(auto &cpool = assure<Type>(); cpool.contains(entt)) {
            return cpool.patch(entt, [&args...](auto &...curr) { ((curr = Type{std::forward<Args>(args)...}), ...); });
        } else {
            return cpool.emplace(entt, std::forward<Args>(args)...);
        }
    }

    /**
     * @brief Patches the given component for an entity.
     *
     * The signature of the function should be equivalent to the following:
     *
     * @code{.cpp}
     * void(Type &);
     * @endcode
     *
     * @note
     * Empty types aren't explicitly instantiated and therefore they are never
     * returned. However, this function can be used to trigger an update signal
     * for them.
     *
     * @warning
     * Attempting to to patch a component of an entity that doesn't own it
     * results in undefined behavior.
     *
     * @tparam Type Type of component to patch.
     * @tparam Func Types of the function objects to invoke.
     * @param entt A valid identifier.
     * @param func Valid function objects.
     * @return A reference to the patched component.
     */
    template<typename Type, typename... Func>
    decltype(auto) patch(const entity_type entt, Func &&...func) {
        return assure<Type>().patch(entt, std::forward<Func>(func)...);
    }

    /**
     * @brief Replaces the given component for an entity.
     *
     * The component must have a proper constructor or be of aggregate type.
     *
     * @warning
     * Attempting to replace a component of an entity that doesn't own it
     * results in undefined behavior.
     *
     * @tparam Type Type of component to replace.
     * @tparam Args Types of arguments to use to construct the component.
     * @param entt A valid identifier.
     * @param args Parameters to use to initialize the component.
     * @return A reference to the component being replaced.
     */
    template<typename Type, typename... Args>
    decltype(auto) replace(const entity_type entt, Args &&...args) {
        return patch<Type>(entt, [&args...](auto &...curr) { ((curr = Type{std::forward<Args>(args)...}), ...); });
    }

    /**
     * @brief Removes the given components from an entity.
     *
     * @tparam Type Type of component to remove.
     * @tparam Other Other types of components to remove.
     * @param entt A valid identifier.
     * @return The number of components actually removed.
     */
    template<typename Type, typename... Other>
    size_type remove(const entity_type entt) {
        return (assure<Type>().remove(entt) + ... + assure<Other>().remove(entt));
    }

    /**
     * @brief Removes the given components from all the entities in a range.
     *
     * @sa remove
     *
     * @tparam Type Type of component to remove.
     * @tparam Other Other types of components to remove.
     * @tparam It Type of input iterator.
     * @param first An iterator to the first element of the range of entities.
     * @param last An iterator past the last element of the range of entities.
     * @return The number of components actually removed.
     */
    template<typename Type, typename... Other, typename It>
    size_type remove(It first, It last) {
        size_type count{};

        if constexpr(sizeof...(Other) == 0u) {
            count += assure<Type>().remove(std::move(first), std::move(last));
        } else if constexpr(std::is_same_v<It, typename common_type::iterator>) {
            constexpr size_type len = sizeof...(Other) + 1u;
            common_type *cpools[len]{&assure<Type>(), &assure<Other>()...};

            for(size_type pos{}; pos < len; ++pos) {
                if(cpools[pos]->data() == first.data()) {
                    std::swap(cpools[pos], cpools[len - 1u]);
                }

                count += cpools[pos]->remove(first, last);
            }
        } else {
            for(auto cpools = std::forward_as_tuple(assure<Type>(), assure<Other>()...); first != last; ++first) {
                count += std::apply([entt = *first](auto &...curr) { return (curr.remove(entt) + ... + 0u); }, cpools);
            }
        }

        return count;
    }

    /**
     * @brief Erases the given components from an entity.
     *
     * @warning
     * Attempting to erase a component from an entity that doesn't own it
     * results in undefined behavior.
     *
     * @tparam Type Types of components to erase.
     * @tparam Other Other types of components to erase.
     * @param entt A valid identifier.
     */
    template<typename Type, typename... Other>
    void erase(const entity_type entt) {
        (assure<Type>().erase(entt), (assure<Other>().erase(entt), ...));
    }

    /**
     * @brief Erases the given components from all the entities in a range.
     *
     * @sa erase
     *
     * @tparam Type Types of components to erase.
     * @tparam Other Other types of components to erase.
     * @tparam It Type of input iterator.
     * @param first An iterator to the first element of the range of entities.
     * @param last An iterator past the last element of the range of entities.
     */
    template<typename Type, typename... Other, typename It>
    void erase(It first, It last) {
        if constexpr(sizeof...(Other) == 0u) {
            assure<Type>().erase(std::move(first), std::move(last));
        } else if constexpr(std::is_same_v<It, typename common_type::iterator>) {
            constexpr size_type len = sizeof...(Other) + 1u;
            common_type *cpools[len]{&assure<Type>(), &assure<Other>()...};

            for(size_type pos{}; pos < len; ++pos) {
                if(cpools[pos]->data() == first.data()) {
                    std::swap(cpools[pos], cpools[len - 1u]);
                }

                cpools[pos]->erase(first, last);
            }
        } else {
            for(auto cpools = std::forward_as_tuple(assure<Type>(), assure<Other>()...); first != last; ++first) {
                std::apply([entt = *first](auto &...curr) { (curr.erase(entt), ...); }, cpools);
            }
        }
    }

    /**
     * @brief Removes all tombstones from a registry or only the pools for the
     * given components.
     * @tparam Type Types of components for which to clear all tombstones.
     */
    template<typename... Type>
    void compact() {
        if constexpr(sizeof...(Type) == 0) {
            for(auto &&curr: pools) {
                curr.second->compact();
            }
        } else {
            (assure<Type>().compact(), ...);
        }
    }

    /**
     * @brief Check if an entity is part of all the given storage.
     * @tparam Type Type of storage to check for.
     * @param entt A valid identifier.
     * @return True if the entity is part of all the storage, false otherwise.
     */
    template<typename... Type>
    [[nodiscard]] bool all_of(const entity_type entt) const {
        return (assure<std::remove_const_t<Type>>().contains(entt) && ...);
    }

    /**
     * @brief Check if an entity is part of at least one given storage.
     * @tparam Type Type of storage to check for.
     * @param entt A valid identifier.
     * @return True if the entity is part of at least one storage, false
     * otherwise.
     */
    template<typename... Type>
    [[nodiscard]] bool any_of(const entity_type entt) const {
        return (assure<std::remove_const_t<Type>>().contains(entt) || ...);
    }

    /**
     * @brief Returns references to the given components for an entity.
     *
     * @warning
     * Attempting to get a component from an entity that doesn't own it results
     * in undefined behavior.
     *
     * @tparam Type Types of components to get.
     * @param entt A valid identifier.
     * @return References to the components owned by the entity.
     */
    template<typename... Type>
    [[nodiscard]] decltype(auto) get([[maybe_unused]] const entity_type entt) const {
        if constexpr(sizeof...(Type) == 1u) {
            return (assure<std::remove_const_t<Type>>().get(entt), ...);
        } else {
            return std::forward_as_tuple(get<Type>(entt)...);
        }
    }

    /*! @copydoc get */
    template<typename... Type>
    [[nodiscard]] decltype(auto) get([[maybe_unused]] const entity_type entt) {
        if constexpr(sizeof...(Type) == 1u) {
            return (const_cast<Type &>(std::as_const(*this).template get<Type>(entt)), ...);
        } else {
            return std::forward_as_tuple(get<Type>(entt)...);
        }
    }

    /**
     * @brief Returns a reference to the given component for an entity.
     *
     * In case the entity doesn't own the component, the parameters provided are
     * used to construct it.
     *
     * @sa get
     * @sa emplace
     *
     * @tparam Type Type of component to get.
     * @tparam Args Types of arguments to use to construct the component.
     * @param entt A valid identifier.
     * @param args Parameters to use to initialize the component.
     * @return Reference to the component owned by the entity.
     */
    template<typename Type, typename... Args>
    [[nodiscard]] decltype(auto) get_or_emplace(const entity_type entt, Args &&...args) {
        if(auto &cpool = assure<Type>(); cpool.contains(entt)) {
            return cpool.get(entt);
        } else {
            return cpool.emplace(entt, std::forward<Args>(args)...);
        }
    }

    /**
     * @brief Returns pointers to the given components for an entity.
     *
     * @note
     * The registry retains ownership of the pointed-to components.
     *
     * @tparam Type Types of components to get.
     * @param entt A valid identifier.
     * @return Pointers to the components owned by the entity.
     */
    template<typename... Type>
    [[nodiscard]] auto try_get([[maybe_unused]] const entity_type entt) const {
        if constexpr(sizeof...(Type) == 1) {
            const auto &cpool = assure<std::remove_const_t<Type>...>();
            return cpool.contains(entt) ? std::addressof(cpool.get(entt)) : nullptr;
        } else {
            return std::make_tuple(try_get<Type>(entt)...);
        }
    }

    /*! @copydoc try_get */
    template<typename... Type>
    [[nodiscard]] auto try_get([[maybe_unused]] const entity_type entt) {
        if constexpr(sizeof...(Type) == 1) {
            return (const_cast<Type *>(std::as_const(*this).template try_get<Type>(entt)), ...);
        } else {
            return std::make_tuple(try_get<Type>(entt)...);
        }
    }

    /**
     * @brief Clears a whole registry or the pools for the given components.
     * @tparam Type Types of components to remove from their entities.
     */
    template<typename... Type>
    void clear() {
        if constexpr(sizeof...(Type) == 0) {
            ENTT_ASSERT(!pools.empty() && (pools.begin()->second.get() == shortcut), "Misplaced entity pool");

            for(size_type pos = pools.size() - 1u; pos; --pos) {
                pools.begin()[pos].second->clear();
            }

            auto iterable = shortcut->each();
            shortcut->erase(iterable.begin().base(), iterable.end().base());
        } else {
            (assure<Type>().clear(), ...);
        }
    }

    /**
     * @brief Iterates all the entities that are still in use.
     *
     * The signature of the function should be equivalent to the following:
     *
     * @code{.cpp}
     * void(const Entity);
     * @endcode
     *
     * It's not defined whether entities created during iteration are returned.
     *
     * @tparam Func Type of the function object to invoke.
     * @param func A valid function object.
     */
    template<typename Func>
    void each(Func func) const {
        for(auto [entt]: shortcut->each()) {
            func(entt);
        }
    }

    /**
     * @brief Checks if an entity has components assigned.
     * @param entt A valid identifier.
     * @return True if the entity has no components assigned, false otherwise.
     */
    [[nodiscard]] bool orphan(const entity_type entt) const {
        return std::none_of(++pools.cbegin(), pools.cend(), [entt](auto &&curr) { return curr.second->contains(entt); });
    }

    /**
     * @brief Returns a sink object for the given component.
     *
     * Use this function to receive notifications whenever a new instance of the
     * given component is created and assigned to an entity.<br/>
     * The function type for a listener is equivalent to:
     *
     * @code{.cpp}
     * void(basic_registry<Entity> &, Entity);
     * @endcode
     *
     * Listeners are invoked **after** assigning the component to the entity.
     *
     * @sa sink
     *
     * @tparam Type Type of component of which to get the sink.
     * @return A temporary sink object.
     */
    template<typename Type>
    [[nodiscard]] auto on_construct() {
        return assure<Type>().on_construct();
    }

    /**
     * @brief Returns a sink object for the given component.
     *
     * Use this function to receive notifications whenever an instance of the
     * given component is explicitly updated.<br/>
     * The function type for a listener is equivalent to:
     *
     * @code{.cpp}
     * void(basic_registry<Entity> &, Entity);
     * @endcode
     *
     * Listeners are invoked **after** updating the component.
     *
     * @sa sink
     *
     * @tparam Type Type of component of which to get the sink.
     * @return A temporary sink object.
     */
    template<typename Type>
    [[nodiscard]] auto on_update() {
        return assure<Type>().on_update();
    }

    /**
     * @brief Returns a sink object for the given component.
     *
     * Use this function to receive notifications whenever an instance of the
     * given component is removed from an entity and thus destroyed.<br/>
     * The function type for a listener is equivalent to:
     *
     * @code{.cpp}
     * void(basic_registry<Entity> &, Entity);
     * @endcode
     *
     * Listeners are invoked **before** removing the component from the entity.
     *
     * @sa sink
     *
     * @tparam Type Type of component of which to get the sink.
     * @return A temporary sink object.
     */
    template<typename Type>
    [[nodiscard]] auto on_destroy() {
        return assure<Type>().on_destroy();
    }

    /**
     * @brief Returns a view for the given components.
     * @tparam Type Type of component used to construct the view.
     * @tparam Other Other types of components used to construct the view.
     * @tparam Exclude Types of components used to filter the view.
     * @return A newly created view.
     */
    template<typename Type, typename... Other, typename... Exclude>
    [[nodiscard]] basic_view<get_t<storage_for_type<const Type>, storage_for_type<const Other>...>, exclude_t<storage_for_type<const Exclude>...>>
    view(exclude_t<Exclude...> = exclude_t{}) const {
        return {assure<std::remove_const_t<Type>>(), assure<std::remove_const_t<Other>>()..., assure<std::remove_const_t<Exclude>>()...};
    }

    /*! @copydoc view */
    template<typename Type, typename... Other, typename... Exclude>
    [[nodiscard]] basic_view<get_t<storage_for_type<Type>, storage_for_type<Other>...>, exclude_t<storage_for_type<Exclude>...>>
    view(exclude_t<Exclude...> = exclude_t{}) {
        return {assure<std::remove_const_t<Type>>(), assure<std::remove_const_t<Other>>()..., assure<std::remove_const_t<Exclude>>()...};
    }

    /**
     * @brief Returns a group for the given components.
     *
     * Group owned component pools can no longer be sorted.<br/>
     * The group takes the ownership of the pools and arrange components so as
     * to iterate them as fast as possible.
     *
     * @tparam Owned Types of storage _owned_ by the group.
     * @tparam Get Types of storage _observed_ by the group, if any.
     * @tparam Exclude Types of storage used to filter the group, if any.
     * @return A newly created group.
     */
    template<typename... Owned, typename... Get, typename... Exclude>
    basic_group<owned_t<storage_for_type<Owned>...>, get_t<storage_for_type<Get>...>, exclude_t<storage_for_type<Exclude>...>>
    group(get_t<Get...> = get_t{}, exclude_t<Exclude...> = exclude_t{}) {
        using handler_type = typename basic_group<owned_t<storage_for_type<Owned>...>, get_t<storage_for_type<Get>...>, exclude_t<storage_for_type<Exclude>...>>::handler;

        if constexpr(sizeof...(Owned) == 0u) {
            if(auto it = non_owning_groups.find(type_hash<handler_type>::value()); it != non_owning_groups.cend()) {
                return {*std::static_pointer_cast<handler_type>(it->second)};
            }

            auto handler = std::allocate_shared<handler_type>(get_allocator(), get_allocator(), assure<std::remove_const_t<Get>>()..., assure<std::remove_const_t<Exclude>>()...);
            non_owning_groups.emplace(type_hash<handler_type>::value(), handler);
            return {*handler};
        } else {
            if(auto it = owning_groups.find(type_hash<handler_type>::value()); it != owning_groups.cend()) {
                return {*std::static_pointer_cast<handler_type>(it->second)};
            }

            const id_type elem[]{type_hash<std::remove_const_t<Owned>>::value()..., type_hash<std::remove_const_t<Get>>::value()..., type_hash<std::remove_const_t<Exclude>>::value()...};
            auto handler = std::allocate_shared<handler_type>(get_allocator(), assure<std::remove_const_t<Owned>>()..., assure<std::remove_const_t<Get>>()..., assure<std::remove_const_t<Exclude>>()...);
            owning_groups.emplace(type_hash<handler_type>::value(), handler);

            ENTT_ASSERT(std::all_of(owning_groups.cbegin(), owning_groups.cend(), [&elem, hsize = handler->size()](const auto &data) {
                            const auto overlapping = data.second->check(elem, sizeof...(Owned), 0u, 0u);
                            const auto sz = data.second->check(elem, sizeof...(Owned), sizeof...(Get), sizeof...(Exclude));
                            return !overlapping || ((sz == hsize) || (sz == data.second->size()));
                        }),
                        "Conflicting groups");

            const internal::owning_group_descriptor *prev = nullptr;
            const internal::owning_group_descriptor *next = nullptr;

            for(auto &&data: owning_groups) {
                if(const auto sz = data.second->size(); data.second->check(elem, sizeof...(Owned), 0u, 0u)) {
                    if(sz < handler->size() && (prev == nullptr || prev->size() < sz)) {
                        prev = data.second.get();
                    }

                    if(sz > handler->size() && (next == nullptr || next->size() > sz)) {
                        next = data.second.get();
                    }
                }
            }

            if(prev) {
                handler->previous(*prev);
            }

            if(next) {
                handler->next(*next);
            }

            return {*handler};
        }
    }

    /*! @copydoc group */
    template<typename... Owned, typename... Get, typename... Exclude>
    basic_group<owned_t<storage_for_type<const Owned>...>, get_t<storage_for_type<const Get>...>, exclude_t<storage_for_type<const Exclude>...>>
    group_if_exists(get_t<Get...> = get_t{}, exclude_t<Exclude...> = exclude_t{}) const {
        using handler_type = typename basic_group<owned_t<storage_for_type<const Owned>...>, get_t<storage_for_type<const Get>...>, exclude_t<storage_for_type<const Exclude>...>>::handler;

        if constexpr(sizeof...(Owned) == 0u) {
            if(auto it = non_owning_groups.find(type_hash<handler_type>::value()); it != non_owning_groups.cend()) {
                return {*std::static_pointer_cast<handler_type>(it->second)};
            }
        } else {
            if(auto it = owning_groups.find(type_hash<handler_type>::value()); it != owning_groups.cend()) {
                return {*std::static_pointer_cast<handler_type>(it->second)};
            }
        }

        return {};
    }

    /**
     * @brief Checks whether the given components belong to any group.
     * @tparam Type Type of component in which one is interested.
     * @tparam Other Other types of components in which one is interested.
     * @return True if the pools of the given components are _free_, false
     * otherwise.
     */
    template<typename Type, typename... Other>
    [[nodiscard]] bool owned() const {
        const id_type elem[]{type_hash<std::remove_const_t<Type>>::value(), type_hash<std::remove_const_t<Other>>::value()...};
        return std::any_of(owning_groups.cbegin(), owning_groups.cend(), [&elem](auto &&data) { return data.second->check(elem, 1u + sizeof...(Other), 0u, 0u); });
    }

    /**
     * @brief Checks whether a group can be sorted.
     * @tparam Owned Type of storage _owned_ by the group.
     * @tparam Get Type of storage _observed_ by the group.
     * @tparam Exclude Type of storage used to filter the group.
     * @return True if the group can be sorted, false otherwise.
     */
    template<typename... Owned, typename... Get, typename... Exclude>
    [[nodiscard]] bool sortable(const basic_group<owned_t<Owned...>, get_t<Get...>, exclude_t<Exclude...>> &) noexcept {
        constexpr auto size = sizeof...(Owned) + sizeof...(Get) + sizeof...(Exclude);
        const id_type elem[]{type_hash<typename Owned::value_type>::value()...};
        auto pred = [&elem, size](const auto &data) { return data.second->check(elem, sizeof...(Owned), 0u, 0u) && (size < data.second->size()); };
        return std::find_if(owning_groups.cbegin(), owning_groups.cend(), std::move(pred)) == owning_groups.cend();
    }

    /**
     * @brief Sorts the elements of a given component.
     *
     * The order remains valid until a component of the given type is assigned
     * to or removed from an entity.<br/>
     * The comparison function object returns `true` if the first element is
     * _less_ than the second one, `false` otherwise. Its signature is also
     * equivalent to one of the following:
     *
     * @code{.cpp}
     * bool(const Entity, const Entity);
     * bool(const Type &, const Type &);
     * @endcode
     *
     * Moreover, it shall induce a _strict weak ordering_ on the values.<br/>
     * The sort function object offers an `operator()` that accepts:
     *
     * * An iterator to the first element of the range to sort.
     * * An iterator past the last element of the range to sort.
     * * A comparison function object to use to compare the elements.
     *
     * The comparison function object hasn't necessarily the type of the one
     * passed along with the other parameters to this member function.
     *
     * @warning
     * Pools of components owned by a group cannot be sorted.
     *
     * @tparam Type Type of components to sort.
     * @tparam Compare Type of comparison function object.
     * @tparam Sort Type of sort function object.
     * @tparam Args Types of arguments to forward to the sort function object.
     * @param compare A valid comparison function object.
     * @param algo A valid sort function object.
     * @param args Arguments to forward to the sort function object, if any.
     */
    template<typename Type, typename Compare, typename Sort = std_sort, typename... Args>
    void sort(Compare compare, Sort algo = Sort{}, Args &&...args) {
        ENTT_ASSERT(!owned<Type>(), "Cannot sort owned storage");
        auto &cpool = assure<Type>();

        if constexpr(std::is_invocable_v<Compare, decltype(cpool.get({})), decltype(cpool.get({}))>) {
            auto comp = [&cpool, compare = std::move(compare)](const auto lhs, const auto rhs) { return compare(std::as_const(cpool.get(lhs)), std::as_const(cpool.get(rhs))); };
            cpool.sort(std::move(comp), std::move(algo), std::forward<Args>(args)...);
        } else {
            cpool.sort(std::move(compare), std::move(algo), std::forward<Args>(args)...);
        }
    }

    /**
     * @brief Sorts two pools of components in the same way.
     *
     * Being `To` and `From` the two sets, after invoking this function an
     * iterator for `To` returns elements according to the following rules:
     *
     * * All entities in `To` that are also in `From` are returned first
     *   according to the order they have in `From`.
     * * All entities in `To` that are not in `From` are returned in no
     *   particular order after all the other entities.
     *
     * Any subsequent change to `From` won't affect the order in `To`.
     *
     * @warning
     * Pools of components owned by a group cannot be sorted.
     *
     * @tparam To Type of components to sort.
     * @tparam From Type of components to use to sort.
     */
    template<typename To, typename From>
    void sort() {
        ENTT_ASSERT(!owned<To>(), "Cannot sort owned storage");
        assure<To>().sort_as(assure<From>());
    }

    /**
     * @brief Returns the context object, that is, a general purpose container.
     * @return The context object, that is, a general purpose container.
     */
    context &ctx() noexcept {
        return vars;
    }

    /*! @copydoc ctx */
    const context &ctx() const noexcept {
        return vars;
    }

private:
    context vars;
    pool_container_type pools;
    owning_group_container_type owning_groups;
    non_owning_group_container_type non_owning_groups;
    storage_for_type<entity_type> *shortcut;
};

} // namespace entt

#endif
