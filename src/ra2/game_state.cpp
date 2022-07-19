#include "ra2/game_state.hpp"

using namespace ra2::game_state;

void GameState::add_AbstractTypeClass(std::unique_ptr<AbstractTypeClass> a,
                                      const std::uintptr_t* real_address) {
  // FIXME: use and fix contains
  if (abstract_type_classes_.find((u32)real_address) !=
      abstract_type_classes_.end()) {
    throw std::runtime_error(std::string("Duplicate key ") +
                             yrclient::to_hex((u32)real_address));
  }

  abstract_type_classes_.try_emplace((u32)real_address, std::move(a));
}

void GameState::add_ObjectClass(std::unique_ptr<objects::ObjectClass> a) {
  object_classes_.emplace_back(std::move(a));
}

void GameState::add_HouseClass(std::unique_ptr<objects::HouseClass> h) {
  house_classes_.emplace_back(std::move(h));
}

void GameState::add_FactoryClass(std::unique_ptr<objects::FactoryClass> f) {
  factory_classes_.emplace_back(std::move(f));
}

atc_map_t& GameState::abstract_type_classes() { return abstract_type_classes_; }

object_vec_t& GameState::object_classes() { return object_classes_; }

houseclass_vec_t& GameState::house_classes() { return house_classes_; }

factoryclass_vec_t& GameState::factory_classes() { return factory_classes_; }
