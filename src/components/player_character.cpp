// Copyright 2014 Google Inc. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <memory>
#include "character.h"
#include "game_state.h"
#include "pie_noon_common_generated.h"
#include "player_character.h"
#include "scene_object.h"

CORGI_DEFINE_COMPONENT(fpl::pie_noon::PlayerCharacterComponent,
                       fpl::pie_noon::PlayerCharacterData)

using mathfu::vec3;
using mathfu::mat4;
using motive::Angle;
using motive::kDegreesToRadians;

namespace fpl {
namespace pie_noon {

void PlayerCharacterComponent::UpdateAllEntities(
    corgi::WorldTime /*delta_time*/) {
  for (auto iter = component_data_.begin(); iter != component_data_.end();
       ++iter) {
    corgi::EntityRef entity = iter->entity;
    int num_accessories = 0;
    UpdateCharacterFacing(entity);
    UpdateCharacterTint(entity);
    UpdateUiArrow(entity);
    UpdateVisibility(entity);
    num_accessories += PopulatePieAccessories(entity, num_accessories);
    num_accessories += PopulateHealthAccessories(entity, num_accessories);
  }
}

// Make sure the character is correctly positioned and facing the correct way:
void PlayerCharacterComponent::UpdateCharacterFacing(corgi::EntityRef entity) {
  SceneObjectData* so_data = Data<SceneObjectData>(entity);
  std::vector<std::unique_ptr<Character>>& character_vector =
      gamestate_ptr_->characters();

  PlayerCharacterData* pc_data = GetComponentData(entity);

  std::unique_ptr<Character>& character =
      character_vector[pc_data->character_id];

  const Angle towards_camera_angle = Angle::FromXZVector(
      gamestate_ptr_->camera().Position() - character->position());
  Angle character_face_angle = character->FaceAngle();
  const Angle face_to_camera_angle =
      character_face_angle - towards_camera_angle;
  const bool facing_camera = face_to_camera_angle.ToRadians() < 0.0f;

  so_data->SetScaleZ(facing_camera ? 1.0f : -1.0f);
  so_data->SetRotationAboutY((-character_face_angle).ToRadians());

  const WorldTime anim_time = gamestate_ptr_->GetAnimationTime(*character);
  const uint16_t renderable_id = character->RenderableId(anim_time);
  const uint16_t variant = character->Variant();

  so_data->set_renderable_id(renderable_id);
  so_data->set_variant(variant);
  so_data->SetTranslation(character->position());
}

void PlayerCharacterComponent::UpdateCharacterTint(corgi::EntityRef entity) {
  PlayerCharacterData* pc_data = GetComponentData(entity);
  SceneObjectData* so_data = Data<SceneObjectData>(entity);
  std::vector<std::unique_ptr<Character>>& character_vector =
      gamestate_ptr_->characters();
  std::unique_ptr<Character>& character =
      character_vector[pc_data->character_id];
  so_data->set_tint(character->Color());
}

// Keep the circle underfoot up to date and pointing the right way:
void PlayerCharacterComponent::UpdateUiArrow(corgi::EntityRef entity) {
  PlayerCharacterData* pc_data = GetComponentData(entity);
  SceneObjectData* so_data = Data<SceneObjectData>(entity);
  std::vector<std::unique_ptr<Character>>& character_vector =
      gamestate_ptr_->characters();
  std::unique_ptr<Character>& character =
      character_vector[pc_data->character_id];

  // Base UI arrow circle:
  SceneObjectData* circle_so_data = Data<SceneObjectData>(pc_data->base_circle);
  const Angle arrow_angle = gamestate_ptr_->TargetFaceAngle(character->id());
  circle_so_data->SetRotationAboutY((-arrow_angle).ToRadians());
  circle_so_data->SetTranslation(so_data->Translation());
  circle_so_data->SetOriginPoint(LoadVec3(config_->ui_arrow_offset()));
  circle_so_data->SetScale(LoadVec3(config_->ui_arrow_scale()));
  circle_so_data->set_visible(DrawBaseCircle(entity));
}

// Keep the scene object visible flag up to date
void PlayerCharacterComponent::UpdateVisibility(corgi::EntityRef entity) {
  SceneObjectData* so_data = Data<SceneObjectData>(entity);
  std::vector<std::unique_ptr<Character>>& character_vector =
      gamestate_ptr_->characters();

  PlayerCharacterData* pc_data = GetComponentData(entity);

  std::unique_ptr<Character>& character =
      character_vector[pc_data->character_id];

  so_data->set_visible(character->visible());
}

// Add the accessories that are part of the character's timeline animation.
// Pies, and Pie Block Pans, mostly.
int PlayerCharacterComponent::PopulatePieAccessories(corgi::EntityRef entity,
                                                     int num_accessories) {
  std::vector<std::unique_ptr<Character>>& character_vector =
      gamestate_ptr_->characters();

  PlayerCharacterData* pc_data = GetComponentData(entity);
  std::unique_ptr<Character>& character =
      character_vector[pc_data->character_id];

  // Accessories:
  const Timeline* const timeline = character->CurrentTimeline();
  const WorldTime anim_time = gamestate_ptr_->GetAnimationTime(*character);

  if (timeline) {
    // Get accessories that are valid for the current time.
    const std::vector<int> accessory_indices =
        TimelineIndicesWithTime(timeline->accessories(), anim_time);

    for (auto it = accessory_indices.begin(); it != accessory_indices.end();
         ++it) {
      const TimelineAccessory& accessory = *timeline->accessories()->Get(*it);

      corgi::EntityRef& accessory_entity =
          pc_data->accessories[num_accessories];

      SceneObjectData* accessory_so_data =
          Data<SceneObjectData>(accessory_entity);

      accessory_so_data->set_visible(true);
      const vec3 offset(
          accessory.offset().x() * config_->pixel_to_world_scale(),
          accessory.offset().y() * config_->pixel_to_world_scale(),
          (num_accessories + 1) * config_->accessory_z_increment());
      accessory_so_data->SetTranslation(offset);

      accessory_so_data->set_renderable_id(accessory.renderable());
      accessory_so_data->SetScale(mathfu::kOnes3f);

      num_accessories++;
    }
  }
  return num_accessories;
}

// Populate the health and splatter damage accessories:
int PlayerCharacterComponent::PopulateHealthAccessories(
    corgi::EntityRef entity, int num_accessories) {
  std::vector<std::unique_ptr<Character>>& character_vector =
      gamestate_ptr_->characters();

  PlayerCharacterData* pc_data = GetComponentData(entity);
  std::unique_ptr<Character>& character =
      character_vector[pc_data->character_id];

  // Accessories:
  const Timeline* const timeline = character->CurrentTimeline();
  const WorldTime anim_time = gamestate_ptr_->GetAnimationTime(*character);

  const uint16_t renderable_id = character->RenderableId(anim_time);

  // Accessories:
  if (timeline) {
    // Now for the hearts and splatters and what not.
    CharacterHealth health = character->health();
    const CharacterHealth damage =
        config_->character_health() - character->health();

    auto renderable = config_->renderables()->Get(renderable_id);

    // Loop twice. First for damage splatters, then for health hearts.
    struct {
      int key;
      vec2i offset;
      const flatbuffers::Vector<flatbuffers::Offset<AccessoryGroup>>* indices;
      const flatbuffers::Vector<flatbuffers::Offset<FixedAccessory>>*
          fixed_accessories;
    } accessories[] = {
        {damage, LoadVec2i(renderable->splatter_offset()),
         config_->splatter_map(), config_->splatter_accessories()},
        {health, LoadVec2i(renderable->health_offset()), config_->health_map(),
         config_->health_accessories()}};

    for (size_t j = 0; j < PIE_ARRAYSIZE(accessories); ++j) {
      // Get the set of indices into the fixed_accessories array.
      const int max_key = accessories[j].indices->Length() - 1;
      const int key = mathfu::Clamp(accessories[j].key, 0, max_key);
      auto indices = accessories[j].indices->Get(key)->indices();
      const int num_fixed_accessories = static_cast<int>(indices->Length());

      // Add each accessory slightly in front of the character, with a
      // slight z-offset so that they don't z-fight when they overlap, and
      // for a nice parallax look.
      for (int i = 0; i < num_fixed_accessories; ++i) {
        const int index = indices->Get(i);
        const FixedAccessory* accessory =
            accessories[j].fixed_accessories->Get(index);
        const vec2 location(LoadVec2i(accessory->location()) +
                            accessories[j].offset);
        const vec2 scale(LoadVec2(accessory->scale()));

        corgi::EntityRef& accessory_entity =
            pc_data->accessories[num_accessories];

        SceneObjectData* accessory_so_data =
            Data<SceneObjectData>(accessory_entity);

        accessory_so_data->set_visible(true);
        const vec3 offset(
            location.x() * config_->pixel_to_world_scale(),
            location.y() * config_->pixel_to_world_scale(),
            (num_accessories + 1) * config_->accessory_z_increment());
        accessory_so_data->SetTranslation(offset);

        accessory_so_data->set_renderable_id(accessory->renderable());
        accessory_so_data->SetScale(vec3(scale.x(), scale.y(), 1));

        num_accessories++;
      }
    }

    assert(num_accessories < kMaxAccessories);
    // make sure everything else is turned off:
    for (; num_accessories < kMaxAccessories; num_accessories++) {
      SceneObjectData* accessory_so_data =
          Data<SceneObjectData>(pc_data->accessories[num_accessories]);
      accessory_so_data->set_visible(false);
    }
  }
  return num_accessories;
}

void PlayerCharacterComponent::AddFromRawData(corgi::EntityRef& entity,
                                              const void* /*raw_data*/) {
  entity_manager_->AddEntityToComponent<PlayerCharacterComponent>(entity);
}

Controller::ControllerType PlayerCharacterComponent::ControllerType(
    const corgi::EntityRef& entity) const {
  const PlayerCharacterData* pc_data = GetComponentData(entity);
  return gamestate_ptr_->characters()[pc_data->character_id]
      ->controller()
      ->controller_type();
}

bool PlayerCharacterComponent::DrawBaseCircle(
    const corgi::EntityRef& entity) const {
  // Output the base circle only for certain controller types. For others
  // (for example, for touch) it causes confusion, since players think they
  // can interact with it.
  const Controller::ControllerType controller_type = ControllerType(entity);
  return controller_type == Controller::kTypeGamepad ||
         controller_type == Controller::kTypeCardboard;
}

void PlayerCharacterComponent::InitEntity(corgi::EntityRef& entity) {
  entity_manager_->AddEntityToComponent<SceneObjectComponent>(entity);

  PlayerCharacterData* pc_data = GetComponentData(entity);

  pc_data->base_circle = entity_manager_->AllocateNewEntity();
  entity_manager_->AddEntityToComponent<SceneObjectComponent>(
      pc_data->base_circle);

  SceneObjectData* circle_so_data = Data<SceneObjectData>(pc_data->base_circle);

  circle_so_data->set_renderable_id(RenderableId_UiArrow);
  circle_so_data->SetPreRotationAboutX(kDegreesToRadians * 90);

  // set up slots for accessories:
  for (int i = 0; i < kMaxAccessories; i++) {
    corgi::EntityRef& accessory = pc_data->accessories[i];
    accessory = entity_manager_->AllocateNewEntity();
    entity_manager_->AddEntityToComponent<SceneObjectComponent>(accessory);
    SceneObjectData* accessory_so_data = Data<SceneObjectData>(accessory);

    accessory_so_data->set_visible(false);
    accessory_so_data->set_parent(entity);
  }
}

}  // pie noon
}  // fpl
